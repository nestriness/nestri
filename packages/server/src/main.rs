mod args;
mod enc_helper;
mod gpu;

use std::sync::{Arc, Mutex};
use tungstenite::{connect, Message};
use url::Url;

use gst::prelude::*;
use serde::{Deserialize, Serialize};
use std::time::{Duration, Instant};
use crate::args::{encoding_args, output_args};

#[derive(Serialize, Deserialize, Debug)]
#[serde(tag = "type")]
enum InputMessage {
    #[serde(rename = "mousemove")]
    MouseMove { x: i32, y: i32 },

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
    let mut gpu = gpus.get(0).cloned();
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
    let mut video_encoder = video_encoders.get(0).cloned();
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
            optimized_encoder = enc_helper::encoder_cqp_params(&video_encoder, cqp.quality);
        }
        encoding_args::RateControl::VBR(vbr) => {
            optimized_encoder = enc_helper::encoder_vbr_params(&video_encoder, vbr.target_bitrate, vbr.max_bitrate);
        }
        encoding_args::RateControl::CBR(cbr) => {
            optimized_encoder = enc_helper::encoder_cbr_params(&video_encoder, cbr.target_bitrate);
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

#[actix_web::main]
async fn main() -> std::io::Result<()> {
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
    if let output_args::OutputOption::MoQ(args) = &args.output {
        output_pipeline = format!(
            "
            ! isofmp4mux chunk-duration=1 fragment-duration=1 name=pipend \
            ! moqsink url={} broadcast={}
            ",
            args.relay_url, args.relay_path
        );
    } else if let output_args::OutputOption::WHIP(args) = &args.output {
        output_pipeline = format!(
            "
            ! whipclientsink name=pipend signaller::whip-endpoint=\"{}\" signaller::auth-token=\"{}\" congestion-control=disabled
            ",
            args.endpoint, args.auth_token
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

    // Create the pipeline
    let pipeline = gst::parse::launch(pipeline_str.as_str())
        .unwrap()
        .downcast::<gst::Pipeline>()
        .unwrap();

    let _ = pipeline.set_state(gst::State::Playing);
    let pipeline_clone = Arc::new(Mutex::new(pipeline.clone()));

    // let app_state = web::Data::new(AppState {
    //     pipeline: Arc::new(Mutex::new(pipeline.clone())),
    // });

    let pipeline_thread = pipeline.clone();

    std::thread::spawn(move || {
        let bus = pipeline_thread
            .bus()
            .expect("Pipeline without bus. Shouldn't happen!");

        for msg in bus.iter_timed(gst::ClockTime::NONE) {
            use gst::MessageView;

            match msg.view() {
                MessageView::Eos(..) => {
                    println!("EOS");
                    break;
                }
                MessageView::Error(err) => {
                    let _ = pipeline_thread.set_state(gst::State::Null);
                    eprintln!(
                        "Got error from {}: {} ({})",
                        msg.src()
                            .map(|s| String::from(s.path_string()))
                            .unwrap_or_else(|| "None".into()),
                        err.error(),
                        err.debug().unwrap_or_else(|| "".into()),
                    );
                    break;
                }
                _ => (),
            }
        }

        let _ = pipeline.set_state(gst::State::Null);
    });

    //TODO: Get this from the CLI
    let server = "server";
    let ws_relay = "localhost:1999";
    let url_string = format!("ws://{}/parties/main/testing?_pk={}", ws_relay, server);

    let mut socket = None;
    let start_time = Instant::now();
    let retry_duration = Duration::from_secs(30);

    while socket.is_none() && start_time.elapsed() < retry_duration {
        match Url::parse(&url_string) {
            Ok(url) => {
                match connect(url.as_str()) {
                    Ok((s, _response)) => {
                        println!("[websocket]: Connected to the server");
                        socket = Some(s);
                    }
                    Err(e) => {
                        eprintln!("[websocket]: Error connecting: {}", e);
                        std::thread::sleep(Duration::from_secs(1)); // Wait before retrying
                    }
                }
            }
            Err(e) => {
                eprintln!("Invalid URL: {}", e);
                break; // Exit if the URL is invalid
            }
        }
    }


    if let Some(mut socket) = socket {
        socket.send(Message::Text("Hello There".into())).unwrap();

        let pipeline = pipeline_clone.lock().unwrap();

        let reason = loop {
            let _handle_msg = match socket.read() {
                Ok(msg) => {
                    match msg {
                        Message::Text(txt) => {
                            match serde_json::from_str::<InputMessage>(&txt) {
                                Ok(input_msg) => match input_msg {
                                    InputMessage::MouseMove { x, y } => {
                                        let structure =
                                            gst::Structure::builder("MouseMoveRelative")
                                                .field("pointer_x", x as f64)
                                                .field("pointer_y", y as f64)
                                                .build();

                                        let event = gst::event::CustomUpstream::new(structure);
                                        pipeline.send_event(event);
                                    }
                                    InputMessage::KeyDown { key } => {
                                        let structure = gst::Structure::builder("KeyboardKey")
                                            .field("key", key as u32)
                                            .field("pressed", true)
                                            .build();

                                        let event = gst::event::CustomUpstream::new(structure);
                                        pipeline.send_event(event);
                                    }

                                    InputMessage::KeyUp { key } => {
                                        let structure: gst::Structure =
                                            gst::Structure::builder("KeyboardKey")
                                                .field("key", key as u32)
                                                .field("pressed", false)
                                                .build();

                                        let event = gst::event::CustomUpstream::new(structure);
                                        pipeline.send_event(event);
                                    }

                                    InputMessage::Wheel { x, y } => {
                                        let structure = gst::Structure::builder("MouseAxis")
                                            .field("x", x as f64)
                                            .field("y", y as f64)
                                            .build();

                                        let event = gst::event::CustomUpstream::new(structure);
                                        pipeline.send_event(event);
                                    }

                                    InputMessage::MouseDown { key } => {
                                        let structure = gst::Structure::builder("MouseButton")
                                            .field("button", key as u32)
                                            .field("pressed", true)
                                            .build();

                                        let event = gst::event::CustomUpstream::new(structure);
                                        pipeline.send_event(event);
                                    }

                                    InputMessage::MouseUp { key } => {
                                        let structure = gst::Structure::builder("MouseButton")
                                            .field("button", key as u32)
                                            .field("pressed", false)
                                            .build();

                                        let event = gst::event::CustomUpstream::new(structure);
                                        pipeline.send_event(event);
                                    }
                                },

                                Err(e) => {
                                    // eprintln!("Failed to parse input message: {}", e);
                                    println!("{}", e);
                                    // Optionally, send an error response or handle the error
                                }
                            }
                        }

                        Message::Binary(_) => {
                            // session.binary(bin).await.unwrap();
                        }

                        Message::Close(reason) => {
                            //TODO: Add a retry method for the websocket server, when it goes down because of a graceful connection close
                            break reason;
                        }

                        Message::Ping(bytes) => {
                            socket.send(Message::Pong(bytes)).unwrap();
                        }

                        Message::Pong(bytes) => {
                            socket.send(Message::Ping(bytes)).unwrap();
                        }

                        // ignore
                        Message::Frame(_) => {}
                    }
                }
                Err(e) => {
                    //TODO: Add a retry method for the websocket server, when it goes down because of ungraceful connection close
                    eprintln!("Error reading message: {}", e);
                }
            };
        };

        let _ = socket.close(reason);
    } else {
        eprintln!("[websocket]: Could not connect to the server within 30 seconds.");
    }

    Ok(())
}
