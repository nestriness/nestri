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

async fn hello_world() -> impl Responder {
    "Hello world!"
}

async fn handle_events(
    req: HttpRequest,
    stream: web::Payload,
    state: web::Data<AppState>,
) -> Result<HttpResponse, Error> {
    let (res, mut session, mut stream) = actix_ws::handle(&req, stream)?;
    // start task but don't wait for it
    rt::spawn(async move {
        // receive messages from websocket
        let state = state.into_inner();
        let pipeline = state.pipeline.lock().unwrap();

        let mut last_heartbeat = Instant::now();
        let mut interval = interval(HEARTBEAT_INTERVAL);

        let reason = loop {
            // create "next client timeout check" future
            let tick = interval.tick();
            // required for select()
            pin!(tick);

            // waits for either `msg_stream` to receive a message from the client or the heartbeat
            // interval timer to tick, yielding the value of whichever one is ready first
            match future::select(stream.next(), tick).await {
                // received message from WebSocket client
                Either::Left((Some(Ok(msg)), _)) => {
                    match msg {
                        Message::Text(text) => {
                            // session.text(text).await.unwrap();
                            match serde_json::from_str::<InputMessage>(&text) {
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
                                    eprintln!("Failed to parse input message: {}", e);
                                    // Optionally, send an error response or handle the error
                                }
                            }
                        }

                        Message::Binary(bin) => {
                            session.binary(bin).await.unwrap();
                        }

                        Message::Close(reason) => {
                            break reason;
                        }

                        Message::Ping(bytes) => {
                            last_heartbeat = Instant::now();
                            let _ = session.pong(&bytes).await;
                        }

                        Message::Pong(_) => {
                            last_heartbeat = Instant::now();
                        }

                        Message::Continuation(_) => {
                            println!("no support for continuation frames");
                        }
                        // no-op; ignore
                        Message::Nop => {}
                    };
                }

                Either::Left((Some(Err(err)), _)) => {
                    println!("{}", err);
                    break None;
                }

                // client WebSocket stream ended
                Either::Left((None, _)) => break None,

                // heartbeat interval ticked
                Either::Right((_inst, _)) => {
                    // if no heartbeat ping/pong received recently, close the connection
                    if Instant::now().duration_since(last_heartbeat) > CLIENT_TIMEOUT {
                        println!(
                            "client has not sent heartbeat in over {CLIENT_TIMEOUT:?}; disconnecting"
                        );

                        break None;
                    }

                    // send heartbeat ping
                    let _ = session.ping(b"").await;
                }
            }
        };
        // attempt to close connection gracefully
        let _ = session.close(reason).await;
    });

    // respond immediately with response connected to WS session
    Ok(res)
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    let args = args::Args::new();
    if args.app.verbose {
        args.debug_print();
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
        println!("No GPU found with the specified parameters: vendor='{}', name='{}', index='{}', card_path='{}'. Exiting..",
                 args.device.gpu_vendor, args.device.gpu_name, args.device.gpu_index, args.device.gpu_card_path);
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
    if !args.encoding.encoder_name.is_empty() {
        encoder = enc_helper::get_encoder_by_name(&encoders, &args.encoding.encoder_name);
    } else {
        encoder = enc_helper::get_best_compatible_encoder(
            &encoders,
            enc_helper::VideoCodec::from_str(&args.encoding.encoder_vcodec),
            enc_helper::EncoderType::from_str(&args.encoding.encoder_type),
        );
    }
    if encoder.is_none() {
        println!("No encoder found with the specified parameters: name='{}', vcodec='{}', type='{}'. Exiting..",
                 args.encoding.encoder_name, args.encoding.encoder_vcodec, args.encoding.encoder_type);
        return Ok(());
    }
    let encoder = encoder.unwrap();
    println!("Selected encoder: '{}'", encoder.name);

    println!(
        "Optimizing encoder parameters with CQP of: {}..",
        args.encoding.encoder_cqp
    );
    let mut optimized_encoder = enc_helper::encoder_cqp_params(&encoder, args.encoding.encoder_cqp);
    println!("Optimizing encoder parameters for low latency..");
    optimized_encoder = enc_helper::encoder_low_latency_params(&optimized_encoder);
    println!(
        "Optimized encoder parameters: '{}'",
        optimized_encoder.get_parameters_string()
    );

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
    let mut audio_pipeline = "";
    if !args.encoding.no_audio {
        audio_pipeline = "
        pipewiresrc \
        ! queue2 max-size-time=1000000 ! audioconvert \
        ! opusenc bitrate=196000 \
        ! pipend.";
    }

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
        optimized_encoder.name,
        optimized_encoder.get_parameters_string(),
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
