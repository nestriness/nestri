use reqwest;
use std::collections::HashSet;
use std::io;
use std::sync::Arc;
use tokio::sync::mpsc;
use tokio::sync::Mutex;
use tokio::time::Duration;
use webrtc::api::interceptor_registry::register_default_interceptors;
// use std::collections::HashSet;
use serde::{Deserialize, Serialize};
use serde_json::from_str;
use webrtc::api::media_engine::MediaEngine;
use webrtc::api::APIBuilder;
use webrtc::data_channel::data_channel_message::DataChannelMessage;
use webrtc::ice_transport::ice_server::RTCIceServer;
use webrtc::interceptor::registry::Registry;
use webrtc::peer_connection::configuration::RTCConfiguration;
use webrtc::peer_connection::math_rand_alpha;
use webrtc::peer_connection::peer_connection_state::RTCPeerConnectionState;
use webrtc::peer_connection::sdp::session_description::RTCSessionDescription;

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

pub struct Room {
    peer_connection: Arc<webrtc::peer_connection::RTCPeerConnection>,
    data_channel: Arc<webrtc::data_channel::RTCDataChannel>,
    done_tx: mpsc::Sender<()>,
    done_rx: mpsc::Receiver<()>,
    base_url: String,
    stream_name: String,
    // pipeline: Arc<Mutex<gst::Pipeline>>,
}

impl Room {
    pub async fn new(
        stream_name: &str,
        base_url: &str,
        // pipeline: Arc<Mutex<gst::Pipeline>>,
    ) -> io::Result<Self> {
        // Create a MediaEngine object to configure the supported codec
        let mut m = MediaEngine::default();

        // Register default codecs
        let _ = m.register_default_codecs().map_err(map_to_io_error)?;

        let mut registry = Registry::new();

        // Use the default set of Interceptors
        registry = register_default_interceptors(registry, &mut m).map_err(map_to_io_error)?;

        // Create the API object with the MediaEngine
        let api = APIBuilder::new()
            .with_media_engine(m)
            .with_interceptor_registry(registry)
            .build();

        // Prepare the configuration
        let config = RTCConfiguration {
            ice_servers: vec![RTCIceServer {
                urls: vec!["stun:stun.l.google.com:19302".to_owned()],
                ..Default::default()
            }],
            ..Default::default()
        };

        // Create a new RTCPeerConnection
        let peer_connection = Arc::new(
            api.new_peer_connection(config)
                .await
                .map_err(map_to_io_error)?,
        );

        // Create a datachannel with label 'data'
        let data_channel = peer_connection
            .create_data_channel("input", None)
            .await
            .map_err(map_to_io_error)?;

        let (done_tx, done_rx) = mpsc::channel::<()>(1);

        let done_tx_clone = done_tx.clone();
        // Peer connection state change handler
        peer_connection.on_peer_connection_state_change(Box::new(
            move |s: RTCPeerConnectionState| {
                println!("Peer Connection State has changed: {s}");

                if s == RTCPeerConnectionState::Failed {
                    println!("Peer Connection has gone to failed exiting");
                    let _ = done_tx_clone.try_send(());
                }

                Box::pin(async {})
            },
        ));

        Ok(Self {
            peer_connection,
            // pipeline,
            data_channel,
            done_tx,
            done_rx,
            base_url: base_url.to_string(),
            stream_name: stream_name.to_string(),
        })
    }

    pub async fn run(&mut self) -> io::Result<()> {
        // Create an async channel for sending events to the pipeline
        let (event_tx, mut event_rx) = mpsc::channel(10);

        // A shared state to track currently pressed keys
        let pressed_keys = Arc::new(tokio::sync::Mutex::new(HashSet::new()));

        // Spawn a task to process events for the pipeline
        let pipeline_task = {
            // let pipeline = Arc::clone(self.pipeline);
            // let pipeline_clone = self.pipeline.clone();
            tokio::spawn(async move {
                while let Some(event) = event_rx.recv().await {
                    // let pipeline = pipeline_clone.lock().await;
                    // pipeline.send_event(event);
                    println!("Invoked an event: {}", event)
                }
            })
        };

        let data_channel = self.data_channel.clone();
        //TODO: Handle heartbeats here
        let d1 = Arc::clone(&self.data_channel);
        data_channel.on_open(Box::new(move || {
            println!("Data channel '{}'-'{}' open. Random messages will now be sent to any connected DataChannels every 5 seconds", d1.label(), d1.id());

            let d2 = Arc::clone(&d1);
            Box::pin(async move {
                let mut result = std::io::Result::<usize>::Ok(0);
                while result.is_ok() {
                    let timeout = tokio::time::sleep(Duration::from_secs(5));
                    tokio::pin!(timeout);

                    tokio::select! {
                        _ = timeout.as_mut() =>{
                            let message = math_rand_alpha(15);
                            println!("Sending '{message}'");
                            result = d2.send_text(message).await.map_err(map_to_io_error);
                        }
                    };
                }
            })
        }));

        // Data channel message handler
        let d_label = data_channel.label().to_owned();
        data_channel.on_message(Box::new(move |msg: DataChannelMessage| {
            let msg_str = String::from_utf8(msg.data.to_vec()).unwrap();
            println!("Message from DataChannel '{d_label}': '{msg_str}'");

            let event_tx = event_tx.clone();
            let pressed_keys = Arc::clone(&pressed_keys);

            tokio::spawn(async move {
                if let Ok(input_msg) = from_str::<InputMessage>(&msg_str) {
                    if let Some(event) = handle_input_message(input_msg, &pressed_keys).await {
                        event_tx.send(event).await.unwrap();
                    }
                }
            });

            Box::pin(async {})
        }));

        // Create an offer to send to the browser
        let offer = self
            .peer_connection
            .create_offer(None)
            .await
            .map_err(map_to_io_error)?;

        // Create channel that is blocked until ICE Gathering is complete
        let mut gather_complete = self.peer_connection.gathering_complete_promise().await;

        // Sets the LocalDescription, and starts our UDP listeners
        self.peer_connection
            .set_local_description(offer)
            .await
            .map_err(map_to_io_error)?;

        // Block until ICE Gathering is complete, disabling trickle ICE
        // we do this because we only can exchange one signaling message
        // in a production application you should exchange ICE Candidates via OnICECandidate
        let _ = gather_complete.recv().await;

        if let Some(local_description) = self.peer_connection.local_description().await {
            let url = format!("{}/api/whep/{}", self.base_url, self.stream_name);
            let response = reqwest::Client::new()
                .post(&url)
                .header("Content-Type", "application/sdp")
                .body(local_description.sdp.clone()) // clone if you don't want to move offer.sdp
                .send()
                .await
                .map_err(map_to_io_error)?;

            let answer = response
                .json::<RTCSessionDescription>()
                .await
                .map_err(map_to_io_error)?;

            self.peer_connection
                .set_remote_description(answer)
                .await
                .map_err(map_to_io_error)?;
        } else {
            println!("generate local_description failed!");
        };

        println!("Press ctrl-c to stop");

        tokio::select! {
            _ = self.done_rx.recv() => {
                println!("received done signal!");
            }
            _ = tokio::signal::ctrl_c() => {
                println!();
            }
        };
        self.peer_connection
            .close()
            .await
            .map_err(map_to_io_error)?;

        //FIXME: Ctr + C is not working... i suspect it has something to do with this guy -- Do not forget to fix packages/server/room.rs as well

        pipeline_task.await?;
        Ok(())
    }
}

fn map_to_io_error<E: std::fmt::Display>(e: E) -> io::Error {
    io::Error::new(io::ErrorKind::Other, format!("{}", e))
}

async fn handle_input_message(
    input_msg: InputMessage,
    pressed_keys: &Arc<tokio::sync::Mutex<HashSet<i32>>>,
) -> Option<String> {
    match input_msg {
        InputMessage::MouseMove { x, y } => Some("MouseMoved".to_string()),
        InputMessage::MouseMoveAbs { x, y } => Some("MouseMoveAbsolute".to_string()),
        InputMessage::KeyDown { key } => {
            let mut keys = pressed_keys.lock().await;
            // If the key is already pressed, return to prevent key lockup
            if keys.contains(&key) {
                return None;
            }
            keys.insert(key);

            Some("KeyDown".to_string())
        }
        InputMessage::KeyUp { key } => {
            let mut keys = pressed_keys.lock().await;
            // Remove the key from the pressed state when released
            keys.remove(&key);

            Some("KeyUp".to_string())
        }
        InputMessage::Wheel { x, y } => Some("Wheel".to_string()),
        InputMessage::MouseDown { key } => Some("MouseDown".to_string()),
        InputMessage::MouseUp { key } => Some("MouseUp".to_string()),
    }
}
