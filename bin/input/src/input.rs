use anyhow::Context;
use enigo::{
    Axis::Horizontal,
    Coordinate::Rel,
    Direction::{Press, Release},
    Enigo, Keyboard, Mouse, Settings,
};
use moq_transport::serve::{
    DatagramsReader, GroupsReader, ObjectsReader, StreamReader, TrackReader, TrackReaderMode,
};
use serde::{Deserialize, Serialize};

pub struct Subscriber {
    track: TrackReader,
}

#[derive(Debug, Serialize, Deserialize)]
struct MessageObject {
    input_type: String,
    delta_y: Option<i32>,
    delta_x: Option<i32>,
    button: Option<i32>,
    key_code: Option<i32>,
}

impl Subscriber {
    pub fn new(track: TrackReader) -> Self {
        Self { track }
    }

    pub async fn run(self) -> anyhow::Result<()> {
        loop {

        match self.track.mode().await {
           Ok(mode) => {
            TrackReaderMode::Stream(stream) => Self::recv_stream(stream).await,
            TrackReaderMode::Groups(groups) => Self::recv_groups(groups).await,
            TrackReaderMode::Objects(objects) => Self::recv_objects(objects).await,
            TrackReaderMode::Datagrams(datagrams) => Self::recv_datagrams(datagrams).await,
           },
           Err(err) => {
                // if err.to_string().contains("failed to get mode") {
                    tracing::warn!("Failed to get mode, retrying...");
                    tokio::time::sleep(std::time::Duration::from_millis(100)).await; // adjust the delay as needed
                // } else {
                //     return Err(err);
            }
        }

        }
    
        }
    }

    async fn recv_stream(mut track: StreamReader) -> anyhow::Result<()> {
        while let Some(mut group) = track.next().await? {
            println!("received a stream");
            while let Some(object) = group.read_next().await? {
                println!("received a stream 1");
                let str = String::from_utf8_lossy(&object);
                println!("{}", str);
            }
        }
        Ok(())
    }

    async fn recv_groups(mut groups: GroupsReader) -> anyhow::Result<()> {
        while let Some(mut group) = groups.next().await? {
            let base = group
                .read_next()
                .await
                .context("failed to get first object")?
                .context("empty group")?;

            //TODO: Use this to allow for authorisation (admin, player or guest) etc etc
            let _base = String::from_utf8_lossy(&base);
            // let json = serde_json::from_str(&str)?;

            //TODO: Handle clipboard
            let mut enigo = Enigo::new(&Settings::default()).unwrap();
            while let Some(object) = group.read_next().await? {
                let str = String::from_utf8_lossy(&object);
                let parsed: MessageObject = serde_json::from_str(&str)?;
                match parsed.input_type.as_str() {
                    "mouse_move" => {
                        if let (Some(x), Some(y)) = (parsed.delta_x, parsed.delta_y) {
                            // println!("Handling mouse_move with delta_x: {}, delta_y: {}", x, y);
                            //TODO: This feels laggy , needs fixing
                            enigo.move_mouse(x, y, Rel).unwrap();
                        }
                    }
                    "mouse_key_down" => {
                        if let Some(button) = parsed.button {
                            // println!("Handling mouse_key_down with key: {}", button);
                            if let Some(key) = mouse_key_to_enigo(button) {
                                enigo.button(key, Press).unwrap();
                            }
                        }
                    }
                    "mouse_key_up" => {
                        if let Some(button) = parsed.button {
                            // println!("Handling mouse_key_up with key: {}", button);
                            if let Some(key) = mouse_key_to_enigo(button) {
                                enigo.button(key, Release).unwrap();
                            }
                        }
                    }
                    "mouse_wheel_up" => {
                        //TODO: handle vertical scrolling
                        // println!("Handling mouse_wheel_up with key");
                        enigo.scroll(-2, Horizontal).unwrap();
                    }
                    "mouse_wheel_down" => {
                        //TODO: handle vertical scrolling
                        // println!("Handling mouse_wheel_down with key");
                        enigo.scroll(2, Horizontal).unwrap();
                    }
                    "key_up" => {
                        if let Some(key_code) = parsed.key_code {
                            // println!("Handling key_up with key: {}", key_code);
                            if let Some(key) = key_to_enigo(key_code as u8) {
                                enigo.key(key, Release).unwrap();
                            }
                        }
                    }
                    "key_down" => {
                        if let Some(key_code) = parsed.key_code {
                            // println!("Handling key_down with key: {}", key_code);
                            if let Some(key) = key_to_enigo(key_code as u8) {
                                enigo.key(key, Press).unwrap();
                            }
                        }
                    }
                    _ => {
                        println!("Unknown input_type: {}", parsed.input_type);
                    }
                }
            }
        }

        Ok(())
    }

    async fn recv_objects(mut objects: ObjectsReader) -> anyhow::Result<()> {
        while let Some(mut object) = objects.next().await? {
            let payload = object.read_all().await?;
            let str = String::from_utf8_lossy(&payload);
            println!("{}", str);
        }

        Ok(())
    }

    async fn recv_datagrams(mut datagrams: DatagramsReader) -> anyhow::Result<()> {
        while let Some(datagram) = datagrams.read().await? {
            let str = String::from_utf8_lossy(&datagram.payload);
            println!("{}", str);
        }

        Ok(())
    }
}

pub fn mouse_key_to_enigo(key: i32) -> Option<enigo::Button> {
    match key {
        0 => Some(enigo::Button::Left),
        1 => Some(enigo::Button::Middle),
        2 => Some(enigo::Button::Right),
        _ => None,
    }
}

pub fn key_to_enigo(key: u8) -> Option<enigo::Key> {
    match key {
        27 => Some(enigo::Key::Escape),
        112 => Some(enigo::Key::F1),
        113 => Some(enigo::Key::F2),
        114 => Some(enigo::Key::F3),
        115 => Some(enigo::Key::F4),
        116 => Some(enigo::Key::F5),
        117 => Some(enigo::Key::F6),
        118 => Some(enigo::Key::F7),
        119 => Some(enigo::Key::F8),
        120 => Some(enigo::Key::F9),
        121 => Some(enigo::Key::F10),
        122 => Some(enigo::Key::F11),
        123 => Some(enigo::Key::F12),
        // 19 => Some(enigo::Key::Pause), // Pause
        // 97 => Some(enigo::Key::Print), // Print
        46 => Some(enigo::Key::Delete),
        35 => Some(enigo::Key::End),
        192 => Some(enigo::Key::Unicode('`')),
        48 => Some(enigo::Key::Unicode('0')),
        49 => Some(enigo::Key::Unicode('1')),
        50 => Some(enigo::Key::Unicode('2')),
        51 => Some(enigo::Key::Unicode('3')),
        52 => Some(enigo::Key::Unicode('4')),
        53 => Some(enigo::Key::Unicode('5')),
        54 => Some(enigo::Key::Unicode('6')),
        55 => Some(enigo::Key::Unicode('7')),
        56 => Some(enigo::Key::Unicode('8')),
        57 => Some(enigo::Key::Unicode('9')),
        189 => Some(enigo::Key::Unicode('-')),
        187 => Some(enigo::Key::Unicode('=')),
        8 => Some(enigo::Key::Backspace),
        9 => Some(enigo::Key::Tab),
        219 => Some(enigo::Key::Unicode('[')),
        221 => Some(enigo::Key::Unicode(']')),
        220 => Some(enigo::Key::Unicode('\\')),
        20 => Some(enigo::Key::CapsLock),
        186 => Some(enigo::Key::Unicode(';')),
        222 => Some(enigo::Key::Unicode('\'')),
        13 => Some(enigo::Key::Return),
        16 => Some(enigo::Key::Shift), // ShiftL
        188 => Some(enigo::Key::Unicode(',')),
        190 => Some(enigo::Key::Unicode('.')),
        191 => Some(enigo::Key::Unicode('/')),
        161 => Some(enigo::Key::Shift), // ShiftR
        38 => Some(enigo::Key::UpArrow),
        17 => Some(enigo::Key::Control), // ControlL
        18 => Some(enigo::Key::Alt),     // AltL
        32 => Some(enigo::Key::Space),
        165 => Some(enigo::Key::Alt), // AltR
        // 103 => Some(enigo::Key::Menu),
        163 => Some(enigo::Key::Control), // ControlR
        37 => Some(enigo::Key::LeftArrow),
        40 => Some(enigo::Key::DownArrow),
        39 => Some(enigo::Key::RightArrow),
        // 99 => Some(enigo::Key::Raw(45)), // Insert
        34 => Some(enigo::Key::PageDown),
        36 => Some(enigo::Key::Home),
        33 => Some(enigo::Key::PageUp),
        a if a >= 65 && a <= 90 => Some(enigo::Key::Unicode((a - 65 + ('a' as u8)) as char)),
        _ => None,
    }
}
