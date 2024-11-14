mod args;
mod enc_helper;
mod gpu;

use std::sync::{Arc, Mutex};
use tungstenite::{connect, Message};
use url::Url;

use gst::prelude::*;
use serde::{Deserialize, Serialize};
use std::time::{Duration, Instant};

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

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let args = args::Args::new();
    if args.verbose {
        args.print();
    }

    let _ = gst::init();
    let _ = gstmoq::plugin_register_static();

    println!("Gathering GPU information..");
    let gpus = gpu::get_gpus();
    if gpus.is_empty() {
        println!("No GPUs found. Exiting..");
        return Ok(());
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
    if !args.gpu_card_path.is_empty() {
        gpu = gpu::get_gpu_by_card_path(&gpus, &args.gpu_card_path);
    } else {
        // Run all filters that are not empty
        let mut filtered_gpus = gpus.clone();
        if !args.gpu_vendor.is_empty() {
            filtered_gpus = gpu::get_gpus_by_vendor(&filtered_gpus, &args.gpu_vendor);
        }
        if !args.gpu_name.is_empty() {
            filtered_gpus = gpu::get_gpus_by_device_name(&filtered_gpus, &args.gpu_name);
        }
        if args.gpu_index != 0 {
            // get single GPU by index
            gpu = filtered_gpus.get(args.gpu_index as usize).cloned();
        } else {
            // get first GPU
            gpu = filtered_gpus.get(0).cloned();
        }
    }
    if gpu.is_none() {
        println!("No GPU found with the specified parameters: vendor='{}', name='{}', index='{}', card_path='{}'. Exiting..",
                 args.gpu_vendor, args.gpu_name, args.gpu_index, args.gpu_card_path);
        return Ok(());
    }
    let gpu = gpu.unwrap();
    println!("Selected GPU: '{}'", gpu.device_name());

    println!("Getting compatible encoders..");
    let encoders = enc_helper::get_compatible_encoders();
    if encoders.is_empty() {
        println!("No compatible encoders found. Exiting..");
        return Ok(());
    }
    for encoder in &encoders {
        println!(
            "> [Encoder] Name: '{}', Codec: '{}', API: '{}', Type: '{}'",
            encoder.name,
            encoder.codec.to_str(),
            encoder.encoder_api.to_str(),
            encoder.encoder_type.to_str()
        );
    }
    // Pick most suitable encoder based on given arguments
    let mut encoder = encoders.get(0).cloned();
    if !args.encoder_name.is_empty() {
        encoder = enc_helper::get_encoder_by_name(&encoders, &args.encoder_name);
    } else {
        encoder = enc_helper::get_best_compatible_encoder(
            &encoders,
            enc_helper::VideoCodec::from_str(&args.encoder_vcodec),
            enc_helper::EncoderType::from_str(&args.encoder_type),
        );
    }
    if encoder.is_none() {
        println!("No encoder found with the specified parameters: name='{}', vcodec='{}', type='{}'. Exiting..",
                 args.encoder_name, args.encoder_vcodec, args.encoder_type);
        return Ok(());
    }
    let encoder = encoder.unwrap();
    println!("Selected encoder: '{}'", encoder.name);

    println!(
        "Optimizing encoder parameters with CQP of: {}..",
        args.encoder_cqp
    );
    let mut optimized_encoder = enc_helper::encoder_cqp_params(&encoder, args.encoder_cqp);
    println!("Optimizing encoder parameters for low latency..");
    optimized_encoder = enc_helper::encoder_low_latency_params(&optimized_encoder);
    println!(
        "Optimized encoder parameters: '{}'",
        optimized_encoder.get_parameters_string()
    );

    // Notify of relay path used
    println!("Starting stream with relay path: '{}'", args.relay_path);

    // Debug-feed string
    let mut debug_feed = "";
    if args.debug_feed {
        debug_feed = "! timeoverlay halignment=right valignment=bottom ! tee name=dfee"
    }

    // Additional sink for debugging
    let mut debug_sink = "";
    if args.debug_feed {
        debug_sink = "dfee. ! queue2 max-size-time=1000000 ! videoconvert ! ximagesink"
    }

    // Construct the pipeline string
    let pipeline_str = format!(
        "
        waylanddisplaysrc \
        ! video/x-raw,width={},height={},framerate={}/1,format=RGBx \
        {debug_feed} \
        ! queue2 max-size-time=1000000 ! videoconvert \
        ! {} {} \
        ! {} \
        ! isofmp4mux chunk-duration=1 fragment-duration=1 name=mux \
        ! moqsink url={} broadcast={} \
        pipewiresrc ! audioconvert ! faac bitrate=196000 ! aacparse ! mux. \
        {debug_sink}
        ",
        args.resolution.0,
        args.resolution.1,
        args.framerate,
        optimized_encoder.name,
        optimized_encoder.get_parameters_string(),
        (optimized_encoder.codec == enc_helper::VideoCodec::AV1)
            .then(|| "av1parse")
            .unwrap_or("h264parse"),
        args.relay_url,
        args.relay_path,
    );

    // If verbose, print out the pipeline string
    if args.verbose {
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
