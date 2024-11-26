use flate2::read::GzDecoder;
use flate2::write::GzEncoder;
use flate2::Compression;
use futures_util::sink::SinkExt;
use futures_util::stream::{SplitSink, SplitStream};
use futures_util::StreamExt;
use log::{Level, Log, Metadata, Record};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::error::Error;
use std::io::{Read, Write};
use std::sync::Arc;
use std::time::Duration;
use tokio::net::TcpStream;
use tokio::sync::Mutex;
use tokio::sync::RwLock;
use tokio::time::sleep;
use tokio_tungstenite::tungstenite::Message;
use tokio_tungstenite::{connect_async, MaybeTlsStream, WebSocketStream};
use webrtc::ice_transport::ice_candidate::RTCIceCandidateInit;
use webrtc::peer_connection::sdp::session_description::RTCSessionDescription;

#[derive(Serialize, Deserialize, Debug)]
pub struct WSMessageBase
{
    pub payload_type: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct WSMessageLog {
    #[serde(flatten)]
    pub base: WSMessageBase,
    pub level: String,
    pub message: String,
    pub time: String,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct WSMessageMetrics {
    #[serde(flatten)]
    pub base: WSMessageBase,
    pub usage_cpu: f64,
    pub usage_memory: f64,
    pub uptime: u64,
    pub pipeline_latency: f64,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct WSMessageICE {
    #[serde(flatten)]
    pub base: WSMessageBase,
    pub candidate: RTCIceCandidateInit,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct WSMessageSDP {
    #[serde(flatten)]
    pub base: WSMessageBase,
    pub sdp: RTCSessionDescription,
}

pub fn encode_message<T: Serialize>(message: &T) -> Result<Vec<u8>, Box<dyn std::error::Error>> {
    // Serialize the message to JSON
    let json = serde_json::to_string(message)?;

    // Compress the JSON using gzip
    let mut encoder = GzEncoder::new(Vec::new(), Compression::default());
    encoder.write_all(json.as_bytes())?;
    let compressed_data = encoder.finish()?;

    Ok(compressed_data)
}

pub fn decode_message(data: &[u8]) -> Result<WSMessageBase, Box<dyn std::error::Error + Send + Sync>> {
    let mut decoder = GzDecoder::new(data);
    let mut decompressed_data = String::new();
    decoder.read_to_string(&mut decompressed_data)?;

    let base_message: WSMessageBase = serde_json::from_str(&decompressed_data)?;
    Ok(base_message)
}

pub fn decode_message_as<T: for<'de> Deserialize<'de>>(
    data: Vec<u8>,
) -> Result<T, Box<dyn std::error::Error + Send + Sync>> {
    let mut decoder = GzDecoder::new(data.as_slice());
    let mut decompressed_data = String::new();
    decoder.read_to_string(&mut decompressed_data)?;

    let message: T = serde_json::from_str(&decompressed_data)?;
    Ok(message)
}

type Callback = Box<dyn Fn(Vec<u8>) + Send + Sync>;
type WSRead = SplitStream<WebSocketStream<MaybeTlsStream<TcpStream>>>;
type WSWrite = SplitSink<WebSocketStream<MaybeTlsStream<TcpStream>>, Message>;

#[derive(Clone)]
pub struct NestriWebSocket {
    ws_url: String,
    reader: Arc<Mutex<WSRead>>,
    writer: Arc<Mutex<WSWrite>>,
    callbacks: Arc<RwLock<HashMap<String, Callback>>>,
}
impl NestriWebSocket {
    pub async fn new(
        ws_url: String,
    ) -> Result<NestriWebSocket, Box<dyn Error>> {
        // Attempt to connect to the WebSocket
        let ws_stream = NestriWebSocket::do_connect(&ws_url).await.unwrap();

        // If the connection is successful, split the stream
        let (write, read) = ws_stream.split();
        let mut ws = NestriWebSocket {
            ws_url,
            reader: Arc::new(Mutex::new(read)),
            writer: Arc::new(Mutex::new(write)),
            callbacks: Arc::new(RwLock::new(HashMap::new())),
        };

        // Spawn the read loop
        ws.spawn_read_loop();

        Ok(ws)
    }

    async fn do_connect(
        ws_url: &str,
    ) -> Result<WebSocketStream<MaybeTlsStream<TcpStream>>, Box<dyn Error + Send + Sync>> {
        loop {
            match connect_async(ws_url).await {
                Ok((ws_stream, _)) => {
                    return Ok(ws_stream);
                }
                Err(e) => {
                    eprintln!("Failed to connect to WebSocket, retrying: {:?}", e);
                    sleep(Duration::from_secs(1)).await; // Wait before retrying
                }
            }
        }
    }

    // Handles message -> callback calls and reconnects on error/disconnect
    fn spawn_read_loop(&mut self) {
        let reader = self.reader.clone();
        let callbacks = self.callbacks.clone();

        let mut self_clone = self.clone();

        tokio::spawn(async move {
            loop {
                let message = reader.lock().await.next().await;
                match message {
                    Some(Ok(message)) => {
                        let data = message.into_data();
                        let base_message = match decode_message(&data) {
                            Ok(base_message) => base_message,
                            Err(e) => {
                                eprintln!("Failed to decode message: {:?}", e);
                                continue;
                            }
                        };

                        let callbacks_lock = callbacks.read().await;
                        if let Some(callback) = callbacks_lock.get(&base_message.payload_type) {
                            let data = data.clone();
                            callback(data);
                        }
                    }
                    Some(Err(e)) => {
                        eprintln!("Error receiving message: {:?}", e);
                        self_clone.reconnect().await.unwrap();
                    }
                    None => {
                        eprintln!("WebSocket connection closed, reconnecting...");
                        self_clone.reconnect().await.unwrap();
                    }
                }
            }
        });
    }

    async fn reconnect(&mut self) -> Result<(), Box<dyn Error + Send + Sync>> {
        // Keep trying to reconnect until successful
        loop {
            match NestriWebSocket::do_connect(&self.ws_url).await {
                Ok(ws_stream) => {
                    let (write, read) = ws_stream.split();
                    *self.reader.lock().await = read;
                    *self.writer.lock().await = write;
                    return Ok(());
                }
                Err(e) => {
                    eprintln!("Failed to reconnect to WebSocket: {:?}", e);
                    sleep(Duration::from_secs(2)).await; // Wait before retrying
                }
            }
        }
    }

    pub async fn send_message(&self, message: Vec<u8>) -> Result<(), Box<dyn Error>> {
        let mut writer_lock = self.writer.lock().await;
        writer_lock
            .send(Message::Binary(message))
            .await
            .map_err(|e| format!("Error sending message: {:?}", e).into())
    }

    pub async fn register_callback<F>(&self, response_type: &str, callback: F)
    where
        F: Fn(Vec<u8>) + Send + Sync + 'static,
    {
        let mut callbacks_lock = self.callbacks.write().await;
        callbacks_lock.insert(response_type.to_string(), Box::new(callback));
    }
}
impl Log for NestriWebSocket {
    fn enabled(&self, metadata: &Metadata) -> bool {
        // Filter logs by level
        metadata.level() <= Level::Info
    }

    fn log(&self, record: &Record) {
        if self.enabled(record.metadata()) {
            let level = record.level().to_string();
            let message = record.args().to_string();
            let time = chrono::Local::now().to_rfc3339();

            // Print to console as well
            println!("{}: {}", level, message);

            // Encode and send the log message
            let log_message = WSMessageLog {
                base: WSMessageBase {
                    payload_type: "log".to_string(),
                },
                level,
                message,
                time,
            };
            if let Ok(encoded_message) = encode_message(&log_message) {
                let _ = self.send_message(encoded_message);
            }
        }
    }

    fn flush(&self) {
        // No-op for this logger
    }
}

