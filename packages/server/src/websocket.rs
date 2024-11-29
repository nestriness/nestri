use futures_util::sink::SinkExt;
use futures_util::stream::{SplitSink, SplitStream};
use futures_util::StreamExt;
use log::{Level, Log, Metadata, Record};
use std::collections::HashMap;
use std::error::Error;
use std::sync::Arc;
use std::time::Duration;
use tokio::net::TcpStream;
use tokio::sync::Mutex;
use tokio::sync::RwLock;
use tokio::time::sleep;
use tokio_tungstenite::tungstenite::Message;
use tokio_tungstenite::{connect_async, MaybeTlsStream, WebSocketStream};
use crate::messages::{decode_message, encode_message, MessageBase, MessageLog};

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
            let log_message = MessageLog {
                base: MessageBase {
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

