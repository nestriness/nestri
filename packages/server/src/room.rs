use reqwest;
use serde::{Deserialize, Serialize};
use serde_json::from_str;
use std::collections::{HashSet, VecDeque};
use std::error::Error;
use std::sync::Arc;
use tokio::sync::Mutex;
use tokio::sync::{mpsc, Notify};
use webrtc::api::interceptor_registry::register_default_interceptors;
use webrtc::api::media_engine::MediaEngine;
use webrtc::api::APIBuilder;
use webrtc::data_channel::data_channel_message::DataChannelMessage;
use webrtc::data_channel::data_channel_state::RTCDataChannelState;
use webrtc::ice_transport::ice_candidate::{RTCIceCandidate, RTCIceCandidateInit};
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
    relay_url: String,
    webrtc_api: webrtc::api::API,
    webrtc_config: RTCConfiguration,
    input_notify: Arc<Notify>,
    input_buffer: Arc<Mutex<VecDeque<gst::Event>>>,
    audio_buffer: Arc<Mutex<VecDeque<Vec<u8>>>>,
    video_buffer: Arc<Mutex<VecDeque<Vec<u8>>>>,
}

impl Room {
    pub async fn new(
        relay_url: String,
        input_notify: Arc<Notify>,
        input_buffer: Arc<Mutex<VecDeque<gst::Event>>>,
        audio_buffer: Arc<Mutex<VecDeque<Vec<u8>>>>,
        video_buffer: Arc<Mutex<VecDeque<Vec<u8>>>>,
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

        Ok(Self {
            relay_url,
            webrtc_api: api,
            webrtc_config: config,
            input_notify,
            input_buffer,
            audio_buffer,
            video_buffer,
        })
    }

    pub async fn run(
        &mut self,
        audio_codec: &str,
        video_codec: &str,
    ) -> Result<(), Box<dyn Error>> {
        // Create a new RTCPeerConnection
        let config = self.webrtc_config.clone();
        let peer_connection = Arc::new(self.webrtc_api.new_peer_connection(config).await?);

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

        // Cancellation token to stop spawned tasks after peer connection is closed
        let cancel_token = tokio_util::sync::CancellationToken::new();

        // Add audio track to peer connection
        let audio_sender = peer_connection.add_track(audio_track.clone()).await?;
        let audio_sender_token = cancel_token.child_token();
        tokio::spawn(async move {
            loop {
                let mut rtcp_buf = vec![0u8; 1500];
                tokio::select! {
                    _ = audio_sender_token.cancelled() => {
                        break;
                    }
                    _ = audio_sender.read(&mut rtcp_buf) => {}
                }
            }
        });

        // Add video track to peer connection
        let video_sender = peer_connection.add_track(video_track.clone()).await?;
        let video_sender_token = cancel_token.child_token();
        tokio::spawn(async move {
            loop {
                let mut rtcp_buf = vec![0u8; 1500];
                tokio::select! {
                    _ = video_sender_token.cancelled() => {
                        break;
                    }
                    _ = video_sender.read(&mut rtcp_buf) => {}
                }
            }
        });

        // Create a datachannel with label 'input'
        let data_channel = peer_connection.create_data_channel("input", None).await?;

        // A shared state to track currently pressed keys
        let pressed_keys = Arc::new(Mutex::new(HashSet::new()));

        // PeerConnection state change tracker
        let (pc_sndr, mut pc_recv) = mpsc::channel(1);

        // Peer connection state change handler
        peer_connection.on_peer_connection_state_change(Box::new(
            move |s: RTCPeerConnectionState| {
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
            },
        ));

        peer_connection.on_ice_gathering_state_change(Box::new(move |s| {
            Box::pin(async move {
                println!("ICE Gathering State has changed: {s}");
            })
        }));

        peer_connection.on_ice_connection_state_change(Box::new(move |s| {
            Box::pin(async move {
                println!("ICE Connection State has changed: {s}");
            })
        }));

        // TODO: Trickle ICE
        /*peer_connection.on_ice_candidate(Box::new(move |c| {
        }));*/

        // Data channel message handler
        let input_notify = self.input_notify.clone();
        let input_buffer = self.input_buffer.clone();
        data_channel.on_message(Box::new(move |msg: DataChannelMessage| {
            let pressed_keys = pressed_keys.clone();
            Box::pin({
                let input_notify = input_notify.clone();
                let input_buffer = input_buffer.clone();
                async move {
                    if msg.is_string {
                        let msg_str = String::from_utf8(msg.data.to_vec()).unwrap();
                        if let Ok(input_msg) = from_str::<InputMessage>(&msg_str) {
                            if let Some(event) =
                                handle_input_message(input_msg, &pressed_keys).await
                            {
                                let mut input_buf = input_buffer.lock().await;
                                // Remove oldest if buffer is full
                                if input_buf.len() >= input_buf.capacity() {
                                    input_buf.pop_front();
                                }
                                input_buf.push_back(event);
                                input_notify.notify_one();
                            }
                        }
                    }
                }
            })
        }));

        println!("Creating offer...");

        // Create an offer to send to the browser
        let offer = peer_connection.create_offer(None).await?;

        println!("Setting local description...");

        // Create channel that is blocked until ICE Gathering is complete
        let mut gather_complete = peer_connection.gathering_complete_promise().await;

        // Sets the LocalDescription, and starts our UDP listeners
        peer_connection.set_local_description(offer).await?;

        // Block until ICE Gathering is complete
        let _ = gather_complete.recv().await;

        println!("Local description set, waiting for remote description...");

        if let Some(local_description) = peer_connection.local_description().await {
            let url = format!("{}", self.relay_url);
            let response = reqwest::Client::new()
                .post(&url)
                .header("Content-Type", "application/sdp")
                .body(local_description.sdp.clone())
                .send()
                .await?;

            let answer = response.json::<RTCSessionDescription>().await?;

            println!("Setting remote description...");

            peer_connection.set_remote_description(answer).await?;

            println!("Remote description set...");
        } else {
            println!("generate local_description failed!");
            cancel_token.cancel();
            return Err("generate local_description failed!".into());
        };

        // Send video and audio data
        let audio_track = audio_track.clone();
        let audio_buf = self.audio_buffer.clone();
        let audio_token = cancel_token.child_token();
        tokio::spawn(async move {
            loop {
                tokio::select! {
                    _ = audio_token.cancelled() => {
                        break;
                    }
                    _ = async {
                        let mut audio_buf = audio_buf.lock().await;
                        if let Some(data) = audio_buf.pop_front() {
                            audio_track.write(data.as_slice()).await.unwrap();
                        }
                    } => {}
                }
            }
        });
        let video_track = video_track.clone();
        let video_buf = self.video_buffer.clone();
        let video_token = cancel_token.child_token();
        tokio::spawn(async move {
            loop {
                tokio::select! {
                    _ = video_token.cancelled() => {
                        break;
                    }
                    _ = async {
                        let mut video_buf = video_buf.lock().await;
                        if let Some(data) = video_buf.pop_front() {
                            video_track.write(data.as_slice()).await.unwrap();
                        }
                    } => {}
                }
            }
        });

        // Block until closed or error
        tokio::select! {
            _ = pc_recv.recv() => {
                println!("Peer connection closed with state: {:?}", peer_connection.connection_state());
            }
        }

        cancel_token.cancel();

        // Make double-sure to close the peer connection
        if let Err(e) = peer_connection.close().await {
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
