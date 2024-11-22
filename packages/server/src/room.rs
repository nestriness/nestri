use reqwest;
use serde::{Deserialize, Serialize};
use serde_json::from_str;
use std::collections::HashSet;
use std::error::Error;
use std::sync::Arc;
use tokio::sync::mpsc;
use tokio::sync::Mutex;
use webrtc::api::APIBuilder;
use webrtc::data_channel::data_channel_message::DataChannelMessage;
use webrtc::ice_transport::ice_server::RTCIceServer;
use webrtc::peer_connection::configuration::RTCConfiguration;
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
    relay_url: String,
    event_tx: mpsc::Sender<gst::Event>,
}

impl Room {
    pub async fn new(
        relay_url: String,
        event_tx: mpsc::Sender<gst::Event>,
    ) -> Result<Room, Box<dyn Error>> {
        // Create the API object with the MediaEngine
        let api = APIBuilder::new().build();

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
                .await?
        );

        // Create a datachannel with label 'input'
        let data_channel = peer_connection
            .create_data_channel("input", None)
            .await?;

        Ok(Self {
            peer_connection,
            data_channel,
            relay_url,
            event_tx,
        })
    }

    pub async fn run(&mut self) -> Result<(), Box<dyn Error>> {
        let data_channel = self.data_channel.clone();

        // A shared state to track currently pressed keys
        let pressed_keys = Arc::new(Mutex::new(HashSet::new()));
        let event_tx = self.event_tx.clone();

        // PeerConnection state change tracker
        let (pc_sndr, mut pc_recv) = mpsc::channel(1);

        // Peer connection state change handler
        self.peer_connection.on_peer_connection_state_change(Box::new(
            move |s: RTCPeerConnectionState| {
                let pc_sndr = pc_sndr.clone();
                Box::pin(async move {
                    println!("PeerConnection State has changed: {s}");

                    if s == RTCPeerConnectionState::Failed
                        || s == RTCPeerConnectionState::Disconnected
                        || s == RTCPeerConnectionState::Closed {
                        // Notify pc_state that the peer connection has closed
                        if let Err(e) = pc_sndr.send(s).await {
                            eprintln!("Failed to send PeerConnection state: {}", e);
                        }
                    }
                })
            },
        ));

        // Data channel message handler
        data_channel.on_message(Box::new(move |msg: DataChannelMessage| {
            let pressed_keys = pressed_keys.clone();
            Box::pin({
                let event_tx = event_tx.clone();
                async move {
                    if msg.is_string {
                        let msg_str = String::from_utf8(msg.data.to_vec()).unwrap();
                        if let Ok(input_msg) = from_str::<InputMessage>(&msg_str) {
                            if let Some(event) = handle_input_message(input_msg, &pressed_keys).await {
                                if let Err(e) = event_tx.send(event).await {
                                    eprintln!("Failed to send event: {}", e);
                                }
                            }
                        }
                    }
                }
            })
        }));

        // Create an offer to send to the browser
        let offer = self
            .peer_connection
            .create_offer(None)
            .await?;

        // Create channel that is blocked until ICE Gathering is complete
        let mut gather_complete = self.peer_connection.gathering_complete_promise().await;

        // Sets the LocalDescription, and starts our UDP listeners
        self.peer_connection
            .set_local_description(offer)
            .await?;

        // Block until ICE Gathering is complete, disabling trickle ICE
        // we do this because we only can exchange one signaling message
        // in a production application you should exchange ICE Candidates via OnICECandidate
        let _ = gather_complete.recv().await;

        if let Some(local_description) = self.peer_connection.local_description().await {
            let url = format!("{}", self.relay_url);
            let response = reqwest::Client::new()
                .post(&url)
                .header("Content-Type", "application/sdp")
                .body(local_description.sdp.clone()) // clone if you don't want to move offer.sdp
                .send()
                .await?;

            let answer = response
                .json::<RTCSessionDescription>()
                .await?;

            self.peer_connection
                .set_remote_description(answer)
                .await?;
        } else {
            println!("generate local_description failed!");
        };

        // Block until closed or error
        tokio::select! {
            _ = pc_recv.recv() => {
                println!("Peer connection closed with state: {:?}", self.peer_connection.connection_state());
            }
        }

        // Make double-sure to close the peer connection
        if let Err(e) = self.peer_connection.close().await {
            eprintln!("Failed to close peer connection: {}", e);
        }

        Ok(())
    }
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
