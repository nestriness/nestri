use reqwest;
use serde::{Deserialize, Serialize};
use serde_json::from_str;
use std::collections::HashSet;
use std::error::Error;
use std::sync::Arc;
use tokio::sync::mpsc;
use tokio::sync::Mutex;
use webrtc::api::interceptor_registry::register_default_interceptors;
use webrtc::api::media_engine::MediaEngine;
use webrtc::api::APIBuilder;
use webrtc::data_channel::data_channel_message::DataChannelMessage;
use webrtc::ice_transport::ice_server::RTCIceServer;
use webrtc::interceptor::registry::Registry;
use webrtc::peer_connection::configuration::RTCConfiguration;
use webrtc::peer_connection::peer_connection_state::RTCPeerConnectionState;
use webrtc::peer_connection::sdp::session_description::RTCSessionDescription;
use webrtc::rtp_transceiver::rtp_codec::RTCRtpCodecCapability;
use webrtc::track::track_local::track_local_static_rtp::TrackLocalStaticRTP;
use webrtc::track::track_local::TrackLocalWriter;

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
    audio_track: Arc<TrackLocalStaticRTP>,
    video_track: Arc<TrackLocalStaticRTP>,
    relay_url: String,
    event_tx: mpsc::Sender<gst::Event>,
}

impl Room {
    pub async fn new(
        relay_url: String,
        event_tx: mpsc::Sender<gst::Event>,
        audio_codec: &str,
        video_codec: &str,
    ) -> Result<Room, Box<dyn Error>> {
        // Create media engine and register default codecs
        let mut media_engine = MediaEngine::default();
        media_engine.register_default_codecs()?;

        // Registry
        let mut registry = Registry::new();
        registry = register_default_interceptors(registry, &mut media_engine)?;

        // Create the API object with the MediaEngine
        let api = APIBuilder::new()
            .with_media_engine(media_engine)
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
        let peer_connection = Arc::new(api.new_peer_connection(config).await?);

        // Create audio track
        let audio_track = Arc::new(TrackLocalStaticRTP::new(
            RTCRtpCodecCapability {
                mime_type: audio_codec.to_owned(),
                ..Default::default()
            },
            "audio".to_owned(),
            "audio-nestri-server".to_owned(),
        ));

        // Create video track
        let video_track = Arc::new(TrackLocalStaticRTP::new(
            RTCRtpCodecCapability {
                mime_type: video_codec.to_owned(),
                ..Default::default()
            },
            "video".to_owned(),
            "video-nestri-server".to_owned(),
        ));

        // Add audio track to peer connection
        let audio_sender = peer_connection.add_track(audio_track.clone()).await?;
        tokio::spawn(async move {
            let mut rtcp_buf = vec![0u8; 1500];
            while let Ok((_, _)) = audio_sender.read(&mut rtcp_buf).await {}
        });

        // Add video track to peer connection
        let video_sender = peer_connection.add_track(video_track.clone()).await?;
        tokio::spawn(async move {
            let mut rtcp_buf = vec![0u8; 1500];
            while let Ok((_, _)) = video_sender.read(&mut rtcp_buf).await {}
        });

        // Create a datachannel with label 'input'
        let data_channel = peer_connection.create_data_channel("input", None).await?;

        Ok(Self {
            peer_connection,
            data_channel,
            audio_track,
            video_track,
            relay_url,
            event_tx,
        })
    }

    pub async fn run(
        &mut self,
        audio_tx: Arc<Mutex<mpsc::Receiver<Vec<u8>>>>,
        video_tx: Arc<Mutex<mpsc::Receiver<Vec<u8>>>>,
    ) -> Result<(), Box<dyn Error>> {
        let data_channel = self.data_channel.clone();

        // A shared state to track currently pressed keys
        let pressed_keys = Arc::new(Mutex::new(HashSet::new()));
        let event_tx = self.event_tx.clone();

        // PeerConnection state change tracker
        let (pc_sndr, mut pc_recv) = mpsc::channel(1);

        // Peer connection state change handler
        self.peer_connection
            .on_peer_connection_state_change(Box::new(move |s: RTCPeerConnectionState| {
                let pc_sndr = pc_sndr.clone();
                Box::pin(async move {
                    println!("PeerConnection State has changed: {s}");

                    if s == RTCPeerConnectionState::Failed
                        || s == RTCPeerConnectionState::Disconnected
                        || s == RTCPeerConnectionState::Closed
                    {
                        // Notify pc_state that the peer connection has closed
                        if let Err(e) = pc_sndr.send(s).await {
                            eprintln!("Failed to send PeerConnection state: {}", e);
                        }
                    }
                })
            }));

        self.peer_connection
            .on_ice_gathering_state_change(Box::new(move |s| {
                Box::pin(async move {
                    println!("ICE Gathering State has changed: {s}");
                })
            }));

        self.peer_connection
            .on_ice_connection_state_change(Box::new(move |s| {
                Box::pin(async move {
                    println!("ICE Connection State has changed: {s}");
                })
            }));

        self.peer_connection.on_ice_candidate(Box::new(move |c| {
            Box::pin(async move {
                if let Some(candidate) = c {
                    let jsoned = candidate.to_json().unwrap();
                    let string_json = serde_json::to_string(&jsoned).unwrap();
                    println!("ICE Candidate: {string_json}");
                }
            })
        }));

        // Data channel message handler
        data_channel.on_message(Box::new(move |msg: DataChannelMessage| {
            let pressed_keys = pressed_keys.clone();
            Box::pin({
                let event_tx = event_tx.clone();
                async move {
                    if msg.is_string {
                        let msg_str = String::from_utf8(msg.data.to_vec()).unwrap();
                        if let Ok(input_msg) = from_str::<InputMessage>(&msg_str) {
                            if let Some(event) =
                                handle_input_message(input_msg, &pressed_keys).await
                            {
                                if let Err(e) = event_tx.send(event).await {
                                    eprintln!("Failed to send event: {}", e);
                                }
                            }
                        }
                    }
                }
            })
        }));

        println!("Creating offer...");

        // Create an offer to send to the browser
        let offer = self.peer_connection.create_offer(None).await?;

        println!("Setting local description...");

        // Create channel that is blocked until ICE Gathering is complete
        let mut gather_complete = self.peer_connection.gathering_complete_promise().await;

        // Sets the LocalDescription, and starts our UDP listeners
        self.peer_connection.set_local_description(offer).await?;

        // Block until ICE Gathering is complete
        let _ = gather_complete.recv().await;

        println!("Local description set, waiting for remote description...");

        if let Some(local_description) = self.peer_connection.local_description().await {
            let url = format!("{}", self.relay_url);
            let response = reqwest::Client::new()
                .post(&url)
                .header("Content-Type", "application/sdp")
                .body(local_description.sdp.clone())
                .send()
                .await?;

            let answer = response.json::<RTCSessionDescription>().await?;

            println!("Setting remote description...");

            self.peer_connection.set_remote_description(answer).await?;

            println!("Remote description set...");
        } else {
            println!("generate local_description failed!");
            return Err("generate local_description failed!".into());
        };

        // Send video and audio data
        let audio_track = self.audio_track.clone();
        tokio::spawn(async move {
            loop {
                let audio_tx = audio_tx.clone();
                let audio_frame = audio_tx.lock().await.recv().await.unwrap();
                audio_track.write(audio_frame.as_slice()).await.unwrap();
            }
        });
        let video_track = self.video_track.clone();
        tokio::spawn(async move {
            loop {
                let video_tx = video_tx.clone();
                let video_frame = video_tx.lock().await.recv().await.unwrap();
                video_track.write(video_frame.as_slice()).await.unwrap();
            }
        });

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
