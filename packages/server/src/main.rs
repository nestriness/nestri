use actix_web::{rt, web, App, Error, HttpRequest, HttpResponse, HttpServer, Responder};
use actix_ws::Message;
use futures_util::{
    future::{self, Either},
    StreamExt as _,
};
use gst::prelude::*;
use serde::{Deserialize, Serialize};
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};
use tokio::{pin, time::interval};

const HEARTBEAT_INTERVAL: Duration = Duration::from_secs(5);
/// How long before lack of client response causes a timeout.
const CLIENT_TIMEOUT: Duration = Duration::from_secs(10);
struct AppState {
    pipeline: Arc<Mutex<gst::Pipeline>>,
}

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
    let _ = gst::init();

    let _ = gstmoq::plugin_register_static();
    let _ = gstwaylanddisplaysrc::plugin_register_static();

    let pipeline = gst::parse::launch(
        "
        waylanddisplaysrc \
        ! video/x-raw,width=1280,height=720,format=RGBx,framerate=60/1 \
        ! videoconvertscale\
        ! qsvh264enc low-latency=true \
        ! h264parse \
        ! isofmp4mux name=mux chunk-duration=1 fragment-duration=1 \
        ! moqsink url=https://relay.fst.so namespace=testing
    ",
    )
    .unwrap()
    .downcast::<gst::Pipeline>()
    .unwrap();

    let _ = pipeline.set_state(gst::State::Playing);

    let app_state = web::Data::new(AppState {
        pipeline: Arc::new(Mutex::new(pipeline.clone())),
    });

    let pipeline_clone = pipeline.clone();

    std::thread::spawn(move || {
        let bus = pipeline_clone
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
                    let _ = pipeline_clone.set_state(gst::State::Null);
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

    HttpServer::new(move || {
        App::new()
            .app_data(app_state.clone())
            .service(web::resource("/ws").route(web::get().to(handle_events)))
            .service(web::resource("/").route(web::get().to(hello_world)))
    })
    .bind("0.0.0.0:8081")?
    .run()
    .await
}