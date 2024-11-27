use serde::{Deserialize, Serialize};
use serde_json::from_str;
use std::collections::{HashSet, VecDeque};
use std::error::Error;
use std::sync::Arc;
use tokio::sync::{oneshot, Mutex};
use tokio::sync::{mpsc, Notify};
use webrtc::api::interceptor_registry::register_default_interceptors;
use webrtc::api::media_engine::MediaEngine;
use webrtc::api::APIBuilder;
use webrtc::data_channel::data_channel_init::RTCDataChannelInit;
use webrtc::data_channel::data_channel_message::DataChannelMessage;
use webrtc::ice_transport::ice_candidate::RTCIceCandidateInit;
use webrtc::ice_transport::ice_gathering_state::RTCIceGatheringState;
use webrtc::ice_transport::ice_server::RTCIceServer;
use webrtc::interceptor::registry::Registry;
use webrtc::peer_connection::configuration::RTCConfiguration;
use webrtc::peer_connection::peer_connection_state::RTCPeerConnectionState;
use webrtc::rtp_transceiver::rtp_codec::RTCRtpCodecCapability;
use webrtc::track::track_local::track_local_static_rtp::TrackLocalStaticRTP;
use webrtc::track::track_local::TrackLocalWriter;
use crate::websocket::{decode_message_as, encode_message, AnswerType, JoinerType, NestriWebSocket, WSMessageAnswer, WSMessageBase, WSMessageICE, WSMessageJoin, WSMessageSDP};

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
    nestri_ws: Arc<NestriWebSocket>,
    webrtc_api: webrtc::api::API,
    webrtc_config: RTCConfiguration,
    input_notify: Arc<Notify>,
    input_buffer: Arc<Mutex<VecDeque<gst::Event>>>,
    audio_buffer: Arc<Mutex<VecDeque<Vec<u8>>>>,
    video_buffer: Arc<Mutex<VecDeque<Vec<u8>>>>,
}

impl Room {
    pub async fn new(
        nestri_ws: Arc<NestriWebSocket>,
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
            nestri_ws,
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
        let (tx, rx) = oneshot::channel();
        let tx = Arc::new(std::sync::Mutex::new(Some(tx)));
        self.nestri_ws
            .register_callback("answer",  {
                let tx = tx.clone();
                move |data| {
                    if let Ok(answer) = decode_message_as::<WSMessageAnswer>(data) {
                        log::info!("Received answer: {:?}", answer);
                        match answer.answer_type {
                            AnswerType::AnswerOffline => {
                                log::warn!("Room is offline, we shouldn't be receiving this");
                            }
                            AnswerType::AnswerInUse => {
                                log::error!("Room is in use by another node!");
                            }
                            AnswerType::AnswerOK => {
                                // Notify that we got an OK answer
                                if let Some(tx) = tx.lock().unwrap().take() {
                                    if let Err(_) = tx.send(()) {
                                        log::error!("Failed to send OK answer signal");
                                    }
                                }
                            }
                        }
                    } else {
                        log::error!("Failed to decode answer");
                    }
                }
            })
            .await;

        // Send a request to join the room
        let join_msg = WSMessageJoin {
            base: WSMessageBase {
                payload_type: "join".to_string(),
            },
            joiner_type: JoinerType::JoinerNode,
        };
        if let Ok(encoded) = encode_message(&join_msg) {
            self.nestri_ws.send_message(encoded).await?;
        } else {
            log::error!("Failed to encode join message");
            return Err("Failed to encode join message".into());
        }

        // Wait for the signal indicating that we have received an OK answer
        match rx.await {
            Ok(()) => {
                log::info!("Received OK answer, proceeding...");
            }
            Err(_) => {
                log::error!("Oneshot channel closed unexpectedly");
                return Err("Unexpected error while waiting for OK answer".into());
            }
        }

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
        let data_channel_opts = Some(RTCDataChannelInit {
            ordered: Some(false),
            max_retransmits: Some(0),
            ..Default::default()
        });
        let data_channel
            = peer_connection.create_data_channel("input", data_channel_opts).await?;

        // PeerConnection state change tracker
        let (pc_sndr, mut pc_recv) = mpsc::channel(1);

        // Peer connection state change handler
        peer_connection.on_peer_connection_state_change(Box::new(
            move |s: RTCPeerConnectionState| {
                let pc_sndr = pc_sndr.clone();
                Box::pin(async move {
                    log::info!("PeerConnection State has changed: {s}");

                    if s == RTCPeerConnectionState::Failed
                        || s == RTCPeerConnectionState::Disconnected
                        || s == RTCPeerConnectionState::Closed
                    {
                        // Notify pc_state that the peer connection has closed
                        if let Err(e) = pc_sndr.send(s).await {
                            log::error!("Failed to send PeerConnection state: {}", e);
                        }
                    }
                })
            },
        ));

        peer_connection.on_ice_gathering_state_change(Box::new(move |s| {
            Box::pin(async move {
                log::info!("ICE Gathering State has changed: {s}");
            })
        }));

        peer_connection.on_ice_connection_state_change(Box::new(move |s| {
            Box::pin(async move {
                log::info!("ICE Connection State has changed: {s}");
            })
        }));

        // Trickle ICE over WebSocket
        let ws = self.nestri_ws.clone();
        peer_connection.on_ice_candidate(Box::new(move |c| {
            let nestri_ws = ws.clone();
            Box::pin(async move {
                if let Some(candidate) = c {
                    let candidate_json = candidate.to_json().unwrap();
                    let ice_msg = WSMessageICE {
                        base: WSMessageBase {
                            payload_type: "ice".to_string(),
                        },
                        candidate: candidate_json,
                    };
                    if let Ok(encoded) = encode_message(&ice_msg) {
                        let _ = nestri_ws.send_message(encoded);
                    }
                }
            })
        }));

        // Temporary ICE candidate buffer until remote description is set
        let ice_holder: Arc<Mutex<Vec<RTCIceCandidateInit>>> = Arc::new(Mutex::new(Vec::new()));
        // Register set_response_callback for ICE candidate
        let pc = peer_connection.clone();
        let ice_clone = ice_holder.clone();
        self.nestri_ws.register_callback("ice", move |data| {
            match decode_message_as::<WSMessageICE>(data) {
                Ok(message) => {
                    log::info!("Received ICE message");
                    let candidate = RTCIceCandidateInit::from(message.candidate);
                    let pc = pc.clone();
                    let ice_clone = ice_clone.clone();
                    tokio::spawn(async move {
                        // If remote description is not set, buffer ICE candidates
                        if pc.remote_description().await.is_none() {
                            let mut ice_holder = ice_clone.lock().await;
                            ice_holder.push(candidate);
                        } else {
                            if let Err(e) = pc.add_ice_candidate(candidate).await {
                                log::error!("Failed to add ICE candidate: {}", e);
                            } else {
                                // Add any held ICE candidates
                                let mut ice_holder = ice_clone.lock().await;
                                for candidate in ice_holder.drain(..) {
                                    if let Err(e) = pc.add_ice_candidate(candidate).await {
                                        log::error!("Failed to add ICE candidate: {}", e);
                                    }
                                }
                            }
                        }
                    });
                }
                Err(e) => eprintln!("Failed to decode callback message: {:?}", e),
            }
        }).await;

        // A shared state to track currently pressed keys
        let pressed_keys = Arc::new(Mutex::new(HashSet::new()));
        let pressed_buttons = Arc::new(Mutex::new(HashSet::new()));

        // Data channel message handler
        let input_notify = self.input_notify.clone();
        let input_buffer = self.input_buffer.clone();
        data_channel.on_message(Box::new(move |msg: DataChannelMessage| {
            let pressed_keys = pressed_keys.clone();
            let pressed_buttons = pressed_buttons.clone();
            Box::pin({
                let input_notify = input_notify.clone();
                let input_buffer = input_buffer.clone();
                async move {
                    if msg.is_string {
                        let msg_str = String::from_utf8(msg.data.to_vec()).unwrap();
                        if let Ok(input_msg) = from_str::<InputMessage>(&msg_str) {
                            if let Some(event) =
                                handle_input_message(input_msg, &pressed_keys, &pressed_buttons).await
                            {
                                let mut input_buf = input_buffer.lock().await;
                                // Remove oldest if buffer is full
                                if input_buf.len() >= input_buf.capacity() {
                                    log::info!("Input buffer full, removing oldest event");
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

        log::info!("Creating offer...");

        // Create an offer to send to the browser
        let offer = peer_connection.create_offer(None).await?;

        log::info!("Setting local description...");

        // Sets the LocalDescription, and starts our UDP listeners
        peer_connection.set_local_description(offer).await?;

        log::info!("Local description set...");

        if let Some(local_description) = peer_connection.local_description().await {
            // Wait until we have gathered all ICE candidates
            while peer_connection.ice_gathering_state() != RTCIceGatheringState::Complete {
                tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;
            }

            // Register set_response_callback for SDP answer
            let pc = peer_connection.clone();
            self.nestri_ws.register_callback("sdp", move |data| {
                match decode_message_as::<WSMessageSDP>(data) {
                    Ok(message) => {
                        log::info!("Received SDP message");
                        let sdp = message.sdp;
                        let pc = pc.clone();
                        tokio::spawn(async move {
                            if let Err(e) = pc.set_remote_description(sdp).await {
                                log::error!("Failed to set remote description: {}", e);
                            }
                        });
                    }
                    Err(e) => eprintln!("Failed to decode callback message: {:?}", e),
                }
            }).await;

            log::info!("Sending local description to remote...");
            // Encode and send the local description via WebSocket
            let sdp_msg = WSMessageSDP {
                base: WSMessageBase {
                    payload_type: "sdp".to_string(),
                },
                sdp: local_description,
            };
            let encoded = encode_message(&sdp_msg)?;
            self.nestri_ws.send_message(encoded).await?;
        } else {
            log::error!("generate local_description failed!");
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
                log::info!("Peer connection closed with state: {:?}", peer_connection.connection_state());
            }
        }

        cancel_token.cancel();

        // Make double-sure to close the peer connection
        if let Err(e) = peer_connection.close().await {
            log::error!("Failed to close peer connection: {}", e);
        }

        Ok(())
    }
}

async fn handle_input_message(
    input_msg: InputMessage,
    pressed_keys: &Arc<Mutex<HashSet<i32>>>,
    pressed_buttons: &Arc<Mutex<HashSet<i32>>>,
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
            let mut buttons = pressed_buttons.lock().await;
            // If the button is already pressed, return to prevent button lockup
            if buttons.contains(&key) {
                return None;
            }
            buttons.insert(key);

            let structure = gst::Structure::builder("MouseButton")
                .field("button", key as u32)
                .field("pressed", true)
                .build();

            Some(gst::event::CustomUpstream::new(structure))
        }
        InputMessage::MouseUp { key } => {
            let mut buttons = pressed_buttons.lock().await;
            // Remove the button from the pressed state when released
            buttons.remove(&key);

            let structure = gst::Structure::builder("MouseButton")
                .field("button", key as u32)
                .field("pressed", false)
                .build();

            Some(gst::event::CustomUpstream::new(structure))
        }
    }
}
