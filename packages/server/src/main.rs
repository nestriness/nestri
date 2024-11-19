mod args;
mod enc_helper;
mod gpu;

use std::collections::HashSet;
use std::error::Error;
use std::sync::{Arc};
use tokio_tungstenite::{connect_async, tungstenite::protocol::Message, MaybeTlsStream, WebSocketStream};

use gst::prelude::*;
use serde::{Deserialize, Serialize};
use std::time::{Instant};
use futures_util::{SinkExt, StreamExt};
use futures_util::stream::SplitSink;
use gst::Pipeline;
use serde_json::from_str;
use tokio::net::TcpStream;
use tokio::sync::{mpsc, Mutex};
use crate::args::{encoding_args, output_args};
use tokio::time::Duration;

#[derive(Serialize, Deserialize, Debug)]
#[serde(tag = "type")]
enum InputMessage {
    #[serde(rename = "mousemove")]
    MouseMove { x: i32, y: i32 },

    #[serde(rename = "mousemoveabs")]
    MouseMoveAbs { x: i32, y: i32 },

    #[serde(rename = "wheel")]
    Wheel { x: f64, y: f64 },

    #[serde(rename = "mousedown")]
    MouseDown { key: i32 },
    // Add other variants as needed
    #[serde(rename = "mouseup")]
    MouseUp { key: i32 },

    #[serde(rename = "keydown")]
    KeyDown { key: i32 },

    #[serde(rename = "keyup")]
    KeyUp { key: i32 },
}

// Handles gathering GPU information and selecting the most suitable GPU
fn handle_gpus(args: &args::Args) -> Option<gpu::GPUInfo> {
    println!("Gathering GPU information..");
    let gpus = gpu::get_gpus();
    if gpus.is_empty() {
        println!("No GPUs found");
        return None;
    }
    for gpu in &gpus {
        println!(
            "> [GPU] Vendor: '{}', Card Path: '{}', Render Path: '{}', Device Name: '{}'",
            gpu.vendor_string(),
            gpu.card_path(),
            gpu.render_path(),
            gpu.device_name()
        );
    }

    // Based on available arguments, pick a GPU
    let gpu;
    if !args.device.gpu_card_path.is_empty() {
        gpu = gpu::get_gpu_by_card_path(&gpus, &args.device.gpu_card_path);
    } else {
        // Run all filters that are not empty
        let mut filtered_gpus = gpus.clone();
        if !args.device.gpu_vendor.is_empty() {
            filtered_gpus = gpu::get_gpus_by_vendor(&filtered_gpus, &args.device.gpu_vendor);
        }
        if !args.device.gpu_name.is_empty() {
            filtered_gpus = gpu::get_gpus_by_device_name(&filtered_gpus, &args.device.gpu_name);
        }
        if args.device.gpu_index != 0 {
            // get single GPU by index
            gpu = filtered_gpus.get(args.device.gpu_index as usize).cloned();
        } else {
            // get first GPU
            gpu = filtered_gpus.get(0).cloned();
        }
    }
    if gpu.is_none() {
        println!("No GPU found with the specified parameters: vendor='{}', name='{}', index='{}', card_path='{}'",
                 args.device.gpu_vendor, args.device.gpu_name, args.device.gpu_index, args.device.gpu_card_path);
        return None;
    }
    let gpu = gpu.unwrap();
    println!("Selected GPU: '{}'", gpu.device_name());
    Some(gpu)
}

// Handles picking video encoder
fn handle_encoder_video(args: &args::Args) -> Option<enc_helper::VideoEncoderInfo> {
    println!("Getting compatible video encoders..");
    let video_encoders = enc_helper::get_compatible_encoders();
    if video_encoders.is_empty() {
        println!("No compatible video encoders found");
        return None;
    }
    for encoder in &video_encoders {
        println!(
            "> [Video Encoder] Name: '{}', Codec: '{}', API: '{}', Type: '{}'",
            encoder.name,
            encoder.codec.to_str(),
            encoder.encoder_api.to_str(),
            encoder.encoder_type.to_str()
        );
    }
    // Pick most suitable video encoder based on given arguments
    let video_encoder;
    if !args.encoding.video.encoder.is_empty() {
        video_encoder = enc_helper::get_encoder_by_name(&video_encoders, &args.encoding.video.encoder);
    } else {
        video_encoder = enc_helper::get_best_compatible_encoder(
            &video_encoders,
            enc_helper::VideoCodec::from_str(&args.encoding.video.codec),
            enc_helper::EncoderType::from_str(&args.encoding.video.encoder_type),
        );
    }
    if video_encoder.is_none() {
        println!("No video encoder found with the specified parameters: name='{}', vcodec='{}', type='{}'",
                 args.encoding.video.encoder, args.encoding.video.codec, args.encoding.video.encoder_type);
        return None;
    }
    let video_encoder = video_encoder.unwrap();
    println!("Selected video encoder: '{}'", video_encoder.name);
    Some(video_encoder)
}

// Handles picking preferred settings for video encoder
fn handle_encoder_video_settings(args: &args::Args, video_encoder: &enc_helper::VideoEncoderInfo) -> enc_helper::VideoEncoderInfo {
    let mut optimized_encoder = enc_helper::encoder_low_latency_params(&video_encoder);
    // Handle rate-control method
    match &args.encoding.video.rate_control {
        encoding_args::RateControl::CQP(cqp) => {
            optimized_encoder = enc_helper::encoder_cqp_params(&optimized_encoder, cqp.quality);
        }
        encoding_args::RateControl::VBR(vbr) => {
            optimized_encoder = enc_helper::encoder_vbr_params(&optimized_encoder, vbr.target_bitrate, vbr.max_bitrate);
        }
        encoding_args::RateControl::CBR(cbr) => {
            optimized_encoder = enc_helper::encoder_cbr_params(&optimized_encoder, cbr.target_bitrate);
        }
    }
    println!("Selected video encoder settings: '{}'", optimized_encoder.get_parameters_string());
    optimized_encoder
}

// Handles picking audio encoder
// TODO: Expand enc_helper with audio types, for now just AAC or opus
fn handle_encoder_audio(args: &args::Args, output_option: &output_args::OutputOption) -> String {
    let audio_encoder = if args.encoding.audio.encoder.is_empty() {
        if let output_args::OutputOption::MoQ(_) = output_option {
            "faac".to_string()
        } else {
            "opusenc".to_string()
        }
    } else {
        args.encoding.audio.encoder.clone()
    };
    println!("Selected audio encoder: '{}'", audio_encoder);
    audio_encoder
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    let args = args::Args::new();
    if args.app.verbose {
        args.debug_print();
    }

    let _ = gst::init();
    let _ = gstmoq::plugin_register_static();

    // Handle GPU selection
    let gpu = handle_gpus(&args);
    if gpu.is_none() {
        println!("Failed to find a suitable GPU. Exiting..");
        return Ok(());
    }
    let gpu = gpu.unwrap();

    // Handle video encoder selection
    let video_encoder = handle_encoder_video(&args);
    if video_encoder.is_none() {
        println!("Failed to find a suitable video encoder. Exiting..");
        return Ok(());
    }
    let mut video_encoder = video_encoder.unwrap();
    // Handle video encoder settings
    video_encoder = handle_encoder_video_settings(&args, &video_encoder);

    // Handle audio encoder selection
    let audio_encoder = handle_encoder_audio(&args, &args.output);


    // Get output option
    let mut output_pipeline: String = "".to_string();
    if let output_args::OutputOption::MoQ(moqargs) = &args.output {
        output_pipeline = format!(
            "
            ! isofmp4mux chunk-duration=1 fragment-duration=1 name=pipend \
            ! moqsink url={} broadcast={}
            ",
            moqargs.relay_url, args.app.room
        );
    } else if let output_args::OutputOption::WHIP(whipargs) = &args.output {
        output_pipeline = format!(
            "
            ! whipclientsink name=pipend signaller::whip-endpoint=\"{}/api/whip/{}\" signaller::auth-token=\"{}\" congestion-control=disabled
            ",
            whipargs.endpoint, args.app.room, whipargs.auth_token
        );
    }

    // Debug-latency
    let mut debug_feed = "";
    if args.app.debug_latency {
        debug_feed = "! timeoverlay halignment=right valignment=bottom"
    }

    // Additional sink for debugging
    let mut debug_sink = "";
    if args.app.debug_feed {
        debug_sink = "dfee. ! queue2 max-size-time=1000000 ! videoconvert ! ximagesink"
    }

    // Audio sub-pipeline
    let audio_pipeline = format!("
        {}
        ! queue2 max-size-time=1000000 ! audioconvert \
        ! {} bitrate={}000 \
        ! pipend.",
                                 if args.encoding.audio.capture_method == encoding_args::AudioCaptureMethod::PulseAudio {
                                     "pulsesrc"
                                 } else if args.encoding.audio.capture_method == encoding_args::AudioCaptureMethod::PipeWire {
                                     "pipewiresrc"
                                 } else {
                                     "alsasrc"
                                 },
                                 audio_encoder,
                                 match &args.encoding.audio.rate_control {
                                     encoding_args::RateControl::CBR(cbr) => cbr.target_bitrate,
                                     encoding_args::RateControl::VBR(vbr) => vbr.target_bitrate,
                                     _ => 128,
                                 }
    ).to_string();

    // Construct the pipeline string
    let pipeline_str = format!(
        "
        waylanddisplaysrc render-node={} \
        ! video/x-raw,width={},height={},framerate={}/1,format=RGBx \
        {debug_feed} ! tee name=dfee \
        ! queue2 max-size-time=1000000 ! videoconvert \
        ! {} {} \
        {output_pipeline} \
        {audio_pipeline} \
        {debug_sink}
        ",
        gpu.render_path(),
        args.app.resolution.0,
        args.app.resolution.1,
        args.app.framerate,
        video_encoder.name,
        video_encoder.get_parameters_string(),
    );

    // If verbose, print out the pipeline string
    if args.app.verbose {
        println!("Constructed pipeline string: {}", pipeline_str);
    }

    //TODO: Get this from the CLI
    let server = "server";
    let url_string = format!("{}/api/room/{}?name={}", args.app.input_server, args.app.room, server);

    // Set up a channel for communication with the pipeline
    let (event_tx, event_rx) = mpsc::channel(50);
    let event_rx = Arc::new(Mutex::new(event_rx));

    // Run both pipeline and websocket tasks concurrently
    let result = tokio::try_join!(
        run_pipeline(pipeline_str, event_rx),
        run_websocket(url_string, event_tx),
    );

    match result {
        Ok(_) => println!("Both tasks completed successfully."),
        Err(e) => eprintln!("One of the tasks failed: {}", e),
    }

    Ok(())
}

async fn run_pipeline(
    pipeline_str: String,
    event_rx: Arc<Mutex<mpsc::Receiver<gst::Event>>>,
) -> Result<(), Box<dyn Error>> {
    loop {
        // Create the pipeline
        let pipeline = gst::parse::launch(&pipeline_str)?
            .downcast::<Pipeline>()
            .map_err(|_| "Failed to downcast pipeline")?;
        pipeline.set_state(gst::State::Playing)?;

        let pipeline = Arc::new(Mutex::new(pipeline));

        // Spawn the event handling task
        let (error_tx, mut error_rx) = mpsc::channel(1);
        let pipeline_clone = pipeline.clone();
        tokio::spawn(async move {
            if let Err(e) = handle_pipeline_errors(pipeline_clone, error_tx).await {
                eprintln!("Error handling pipeline errors: {}", e);
            }
        });

        // Spawn the pipeline event task
        let pipeline_event_task = spawn_pipeline_event_task(pipeline.clone(), event_rx.clone());

        // Wait for an error or the event task to complete
        tokio::select! {
            res = pipeline_event_task => {
                if let Err(e) = res {
                    eprintln!("Pipeline event task failed: {}", e);
                }
                break Err("Pipeline event task ended unexpectedly".into());
            }
            Some(_) = error_rx.recv() => {
                eprintln!("Pipeline error occurred. Restarting...");
                continue; // Restart the pipeline loop
            }
        }
    }
}

async fn handle_pipeline_errors(
    pipeline: Arc<Mutex<Pipeline>>,
    error_tx: mpsc::Sender<()>,
) -> Result<(), Box<dyn Error>> {
    let bus = pipeline.lock().await.bus().expect("Pipeline without bus. Shouldn't happen!");
    for msg in bus.iter_timed(gst::ClockTime::NONE) {
        use gst::MessageView;
        match msg.view() {
            MessageView::Eos(..) => {
                println!("Pipeline reached EOS.");
                break;
            }
            MessageView::Error(err) => {
                let _ = pipeline.lock().await.set_state(gst::State::Null);
                eprintln!(
                    "Pipeline error: {} ({})",
                    err.error(),
                    err.debug().unwrap_or_else(|| "".into())
                );
                let _ = error_tx.send(()).await;
                break;
            }
            _ => (),
        }
    }
    Ok(())
}

async fn spawn_pipeline_event_task(
    pipeline: Arc<Mutex<Pipeline>>,
    event_rx: Arc<Mutex<mpsc::Receiver<gst::Event>>>,
) -> Result<(), Box<dyn Error>> {
    while let Some(event) = event_rx.lock().await.recv().await {
        let pipeline = pipeline.lock().await;
        if !pipeline.send_event(event) {
            // No need to spam this message, it's normal for some events to be dropped
            //eprintln!("Failed to send event to the pipeline.");
        }
    }
    Ok(())
}

async fn run_websocket(
    url_string: String,
    event_tx: mpsc::Sender<gst::Event>,
) -> Result<(), Box<dyn Error>> {
    loop {
        let mut socket = None;
        let start_time = Instant::now();
        let retry_duration = Duration::from_secs(30);

        // Attempt to connect to the WebSocket server within the retry duration
        while socket.is_none() && start_time.elapsed() < retry_duration {
            match connect_async(&url_string).await {
                Ok((ws_stream, _)) => {
                    println!("Connected to WebSocket server.");
                    socket = Some(ws_stream);
                }
                Err(e) => {
                    eprintln!("WebSocket connection error: {}", e);
                    tokio::time::sleep(Duration::from_secs(1)).await;
                }
            }
        }

        if let Some(socket) = socket {
            // Process messages from WebSocket
            if let Err(e) = process_websocket(socket, event_tx.clone()).await {
                eprintln!("Error during WebSocket processing: {}", e);
            }
        } else {
            eprintln!("Failed to connect to WebSocket server within the timeout.");
            return Ok(());  // Exit if unable to connect after retries
        }

        // Wait before attempting to reconnect
        eprintln!("Reconnecting to WebSocket server...");
        tokio::time::sleep(Duration::from_secs(2)).await;
    }
}

async fn process_websocket(
    socket: WebSocketStream<MaybeTlsStream<TcpStream>>,
    event_tx: mpsc::Sender<gst::Event>,
) -> Result<(), Box<dyn Error>> {
    let (ws_sink, mut ws_stream) = socket.split();

    // We need sink clone for the heartbeat task
    let ws_sink = Arc::new(Mutex::new(ws_sink));

    // Spawn heartbeat task
    let heartbeat_task = spawn_heartbeat_task(Arc::clone(&ws_sink));

    // A shared state to track currently pressed keys
    let pressed_keys = Arc::new(Mutex::new(HashSet::new()));

    // Handle incoming messages
    while let Some(msg) = ws_stream.next().await {
        match msg {
            Ok(Message::Text(txt)) => {
                let event_tx = event_tx.clone();
                let pressed_keys = Arc::clone(&pressed_keys);

                tokio::spawn(async move {
                    if let Ok(input_msg) = from_str::<InputMessage>(&txt) {
                        if let Some(event) = handle_input_message(input_msg, &pressed_keys).await {
                            if let Err(e) = event_tx.send(event).await {
                                eprintln!("Failed to send event: {}", e);
                            }
                        }
                    }
                });
            }
            Ok(Message::Ping(data)) => {
                if let Err(e) = ws_sink.lock().await.send(Message::Pong(data)).await {
                    eprintln!("Failed to send WebSocket Pong: {}", e);
                    break;
                }
            }
            Ok(Message::Close(reason)) => {
                println!("WebSocket closed: {:?}", reason);
                break;
            }
            Err(e) => {
                eprintln!("Error reading WebSocket message: {}", e);
            }
            _ => {}
        }
    }

    heartbeat_task.abort(); // Ensure the heartbeat task is stopped
    Ok(())
}

fn spawn_heartbeat_task(
    ws_sink: Arc<Mutex<SplitSink<WebSocketStream<MaybeTlsStream<TcpStream>>, Message>>>,
) -> tokio::task::JoinHandle<()> {
    tokio::spawn(async move {
        let mut interval = tokio::time::interval(Duration::from_secs(10));
        let timeout = Duration::from_secs(20);

        loop {
            interval.tick().await;

            if let Err(e) = tokio::time::timeout(timeout, ws_sink.lock().await.send(Message::Ping(vec![]))).await {
                eprintln!("Heartbeat task error: {:?}", e);
                break;
            }
        }
    })
}

async fn handle_input_message(
    input_msg: InputMessage,
    pressed_keys: &Arc<Mutex<HashSet<i32>>>,
) -> Option<gst::Event> {
    match input_msg {
        InputMessage::MouseMove { x, y } => {
            let structure = gst::Structure::builder("MouseMoveRelative")
                .field("pointer_x", x as f64)
                .field("pointer_y", y as f64)
                .build();

            Some(gst::event::CustomUpstream::new(structure))
        }
        InputMessage::MouseMoveAbs { x, y } => {
            let structure = gst::Structure::builder("MouseMoveAbsolute")
                .field("pointer_x", x as f64)
                .field("pointer_y", y as f64)
                .build();

            Some(gst::event::CustomUpstream::new(structure))
        }
        InputMessage::KeyDown { key } => {
            let mut keys = pressed_keys.lock().await;
            // If the key is already pressed, return to prevent key lockup
            if keys.contains(&key) {
                return None;
            }
            keys.insert(key);

            let structure = gst::Structure::builder("KeyboardKey")
                .field("key", key as u32)
                .field("pressed", true)
                .build();

            Some(gst::event::CustomUpstream::new(structure))
        }
        InputMessage::KeyUp { key } => {
            let mut keys = pressed_keys.lock().await;
            // Remove the key from the pressed state when released
            keys.remove(&key);

            let structure = gst::Structure::builder("KeyboardKey")
                .field("key", key as u32)
                .field("pressed", false)
                .build();

            Some(gst::event::CustomUpstream::new(structure))
        }
        InputMessage::Wheel { x, y } => {
            let structure = gst::Structure::builder("MouseAxis")
                .field("x", x as f64)
                .field("y", y as f64)
                .build();

            Some(gst::event::CustomUpstream::new(structure))
        }
        InputMessage::MouseDown { key } => {
            let structure = gst::Structure::builder("MouseButton")
                .field("button", key as u32)
                .field("pressed", true)
                .build();

            Some(gst::event::CustomUpstream::new(structure))
        }
        InputMessage::MouseUp { key } => {
            let structure = gst::Structure::builder("MouseButton")
                .field("button", key as u32)
                .field("pressed", false)
                .build();

            Some(gst::event::CustomUpstream::new(structure))
        }
    }
}
