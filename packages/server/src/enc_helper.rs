use gst::prelude::{GstObjectExt, ObjectExt};

#[derive(Debug, Eq, PartialEq, Clone)]
pub enum VideoCodec {
    H264,
    AV1,
    UNKNOWN,
}
impl VideoCodec {
    pub fn to_str(&self) -> &'static str {
        match self {
            VideoCodec::H264 => "H.264",
            VideoCodec::AV1 => "AV1",
            VideoCodec::UNKNOWN => "Unknown",
        }
    }

    pub fn from_str(s: &str) -> Self {
        match s.to_lowercase().as_str() {
            "h264" => VideoCodec::H264,
            "h.264" => VideoCodec::H264,
            "avc" => VideoCodec::H264,
            "av1" => VideoCodec::AV1,
            _ => VideoCodec::UNKNOWN,
        }
    }
}

#[derive(Debug, Eq, PartialEq, Clone)]
pub enum EncoderAPI {
    QSV,
    VAAPI,
    NVENC,
    AMF,
    SOFTWARE,
    UNKNOWN,
}
impl EncoderAPI {
    pub fn to_str(&self) -> &'static str {
        match self {
            EncoderAPI::QSV => "Intel QuickSync Video",
            EncoderAPI::VAAPI => "Video Acceleration API",
            EncoderAPI::NVENC => "NVIDIA NVENC",
            EncoderAPI::AMF => "AMD Media Framework",
            EncoderAPI::SOFTWARE => "Software",
            EncoderAPI::UNKNOWN => "Unknown",
        }
    }
}

#[derive(Debug, Eq, PartialEq, Clone)]
pub enum EncoderType {
    SOFTWARE,
    HARDWARE,
    UNKNOWN,
}
impl EncoderType {
    pub fn to_str(&self) -> &'static str {
        match self {
            EncoderType::SOFTWARE => "Software",
            EncoderType::HARDWARE => "Hardware",
            EncoderType::UNKNOWN => "Unknown",
        }
    }

    pub fn from_str(s: &str) -> Self {
        match s.to_lowercase().as_str() {
            "software" => EncoderType::SOFTWARE,
            "hardware" => EncoderType::HARDWARE,
            _ => EncoderType::UNKNOWN,
        }
    }
}

#[derive(Debug, Clone)]
pub struct VideoEncoderInfo {
    pub name: String,
    pub codec: VideoCodec,
    pub encoder_type: EncoderType,
    pub encoder_api: EncoderAPI,
    pub parameters: Vec<(String, String)>,
}

impl VideoEncoderInfo {
    pub fn new(
        name: String,
        codec: VideoCodec,
        encoder_type: EncoderType,
        encoder_api: EncoderAPI,
    ) -> Self {
        Self {
            name,
            codec,
            encoder_type,
            encoder_api,
            parameters: Vec::new(),
        }
    }

    pub fn get_parameters_string(&self) -> String {
        self.parameters
            .iter()
            .map(|(key, value)| format!("{}={}", key, value))
            .collect::<Vec<String>>()
            .join(" ")
    }

    pub fn set_parameter(&mut self, key: &str, value: &str) {
        self.parameters.push((key.to_string(), value.to_string()));
    }
}

/// Converts VA-API encoder name to low-power variant.
/// # Arguments
/// * `encoder` - The name of the VA-API encoder.
/// # Returns
/// * `&str` - The name of the low-power variant of the encoder.
fn get_low_power_encoder(encoder: &String) -> String {
    if encoder.starts_with("va") && !encoder.ends_with("enc") && !encoder.ends_with("lpenc") {
        // Replace "enc" substring at end with "lpenc"
        let mut encoder = encoder.to_string();
        encoder.truncate(encoder.len() - 3);
        encoder.push_str("lpenc");
        encoder
    } else {
        encoder.to_string()
    }
}

/// Returns best guess for encoder API based on the encoder name.
/// # Arguments
/// * `encoder` - The name of the encoder.
/// # Returns
/// * `EncoderAPI` - The best guess for the encoder API.
fn get_encoder_api(encoder: &String, encoder_type: &EncoderType) -> EncoderAPI {
    if *encoder_type == EncoderType::HARDWARE {
        if encoder.starts_with("qsv") {
            EncoderAPI::QSV
        } else if encoder.starts_with("va") {
            EncoderAPI::VAAPI
        } else if encoder.starts_with("nv") {
            EncoderAPI::NVENC
        } else if encoder.starts_with("amf") {
            EncoderAPI::AMF
        } else {
            EncoderAPI::UNKNOWN
        }
    } else if *encoder_type == EncoderType::SOFTWARE {
        EncoderAPI::SOFTWARE
    } else {
        EncoderAPI::UNKNOWN
    }
}

/// Returns true if system supports given encoder.
/// # Returns
/// * `bool` - True if encoder is supported, false otherwise.
fn is_encoder_supported(encoder: &String) -> bool {
    gst::ElementFactory::find(encoder.as_str()).is_some()
}

/// Helper to set CQP value of known encoder
/// # Arguments
/// * `encoder` - Information about the encoder.
/// * `quality` - Constant quantization parameter (CQP) quality, recommended values are between 20-30.
/// # Returns
/// * `EncoderInfo` - Encoder with maybe updated parameters.
pub fn encoder_cqp_params(encoder: &VideoEncoderInfo, quality: u32) -> VideoEncoderInfo {
    let mut encoder_optz = encoder.clone();

    // Look for known keys by factory creation
    let encoder = gst::ElementFactory::make(encoder_optz.name.as_str())
        .build()
        .unwrap();

    // Get properties of the encoder
    for prop in encoder.list_properties() {
        let prop_name = prop.name();

        // Look for known keys
        if prop_name.to_lowercase().contains("qp")
            && (prop_name.to_lowercase().contains("i") || prop_name.to_lowercase().contains("min"))
        {
            encoder_optz.set_parameter(prop_name, &quality.to_string());
        } else if prop_name.to_lowercase().contains("qp")
            && (prop_name.to_lowercase().contains("p") || prop_name.to_lowercase().contains("max"))
        {
            encoder_optz.set_parameter(prop_name, &(quality + 2).to_string());
        }
    }

    encoder_optz
}

/// Helper to set VBR values of known encoder
/// # Arguments
/// * `encoder` - Information about the encoder.
/// * `bitrate` - Target bitrate in bits per second.
/// * `max_bitrate` - Maximum bitrate in bits per second.
/// # Returns
/// * `EncoderInfo` - Encoder with maybe updated parameters.
pub fn encoder_vbr_params(encoder: &VideoEncoderInfo, bitrate: u32, max_bitrate: u32) -> VideoEncoderInfo {
    let mut encoder_optz = encoder.clone();

    // Look for known keys by factory creation
    let encoder = gst::ElementFactory::make(encoder_optz.name.as_str())
        .build()
        .unwrap();

    // Get properties of the encoder
    for prop in encoder.list_properties() {
        let prop_name = prop.name();

        // Look for known keys
        if prop_name.to_lowercase().contains("bitrate")
            && !prop_name.to_lowercase().contains("max")
        {
            encoder_optz.set_parameter(prop_name, &bitrate.to_string());
        } else if prop_name.to_lowercase().contains("bitrate")
            && prop_name.to_lowercase().contains("max")
        {
            encoder_optz.set_parameter(prop_name, &max_bitrate.to_string());
        }
    }

    encoder_optz
}

/// Helper to set CBR value of known encoder
/// # Arguments
/// * `encoder` - Information about the encoder.
/// * `bitrate` - Target bitrate in bits per second.
/// # Returns
/// * `EncoderInfo` - Encoder with maybe updated parameters.
pub fn encoder_cbr_params(encoder: &VideoEncoderInfo, bitrate: u32) -> VideoEncoderInfo {
    let mut encoder_optz = encoder.clone();

    // Look for known keys by factory creation
    let encoder = gst::ElementFactory::make(encoder_optz.name.as_str())
        .build()
        .unwrap();

    // Get properties of the encoder
    for prop in encoder.list_properties() {
        let prop_name = prop.name();

        // Look for known keys
        if prop_name.to_lowercase().contains("bitrate")
            && !prop_name.to_lowercase().contains("max")
        {
            encoder_optz.set_parameter(prop_name, &bitrate.to_string());
        }
    }

    encoder_optz
}

/// Helper to set GOP size of known encoder
/// # Arguments
/// * `encoder` - Information about the encoder.
/// * `gop_size` - Group of pictures (GOP) size.
/// # Returns
/// * `EncoderInfo` - Encoder with maybe updated parameters.
pub fn encoder_gop_params(encoder: &VideoEncoderInfo, gop_size: u32) -> VideoEncoderInfo {
    let mut encoder_optz = encoder.clone();

    // Look for known keys by factory creation
    let encoder = gst::ElementFactory::make(encoder_optz.name.as_str())
        .build()
        .unwrap();

    // Get properties of the encoder
    for prop in encoder.list_properties() {
        let prop_name = prop.name();

        // Look for known keys
        if prop_name.to_lowercase().contains("gop")
            || prop_name.to_lowercase().contains("int-max")
            || prop_name.to_lowercase().contains("max-dist")
            || prop_name.to_lowercase().contains("intra-period-length")
        {
            encoder_optz.set_parameter(prop_name, &gop_size.to_string());
        }
    }

    encoder_optz
}

/// Sets parameters of known encoders for low latency operation.
/// # Arguments
/// * `encoder` - Information about the encoder.
/// # Returns
/// * `EncoderInfo` - Encoder with maybe updated parameters.
pub fn encoder_low_latency_params(encoder: &VideoEncoderInfo) -> VideoEncoderInfo {
    let mut encoder_optz = encoder.clone();
    encoder_optz = encoder_gop_params(&encoder_optz, 30);
    match encoder_optz.encoder_api {
        EncoderAPI::QSV => {
            encoder_optz.set_parameter("low-latency", "true");
            encoder_optz.set_parameter("target-usage", "7");
        }
        EncoderAPI::VAAPI => {
            encoder_optz.set_parameter("target-usage", "7");
        }
        EncoderAPI::NVENC => {
            match encoder_optz.codec {
                // nvcudah264enc supports newer presets and tunes
                VideoCodec::H264 => {
                    encoder_optz.set_parameter("multi-pass", "disabled");
                    encoder_optz.set_parameter("preset", "p1");
                    encoder_optz.set_parameter("tune", "ultra-low-latency");
                }
                // nvav1enc only supports older presets
                VideoCodec::AV1 => {
                    encoder_optz.set_parameter("preset", "low-latency-hp");
                }
                _ => {}
            }
        }
        EncoderAPI::AMF => {
            encoder_optz.set_parameter("preset", "speed");
            match encoder_optz.codec {
                // Only H.264 supports "ultra-low-latency" usage
                VideoCodec::H264 => {
                    encoder_optz.set_parameter("usage", "ultra-low-latency");
                }
                VideoCodec::AV1 => {
                    encoder_optz.set_parameter("usage", "low-latency");
                }
                _ => {}
            }
        }
        EncoderAPI::SOFTWARE => {
            // Check encoder name for software encoders
            match encoder_optz.name.as_str() {
                "openh264enc" => {
                    encoder_optz.set_parameter("complexity", "low");
                    encoder_optz.set_parameter("usage-type", "screen");
                }
                "x264enc" => {
                    encoder_optz.set_parameter("rc-lookahead", "0");
                    encoder_optz.set_parameter("speed-preset", "ultrafast");
                    encoder_optz.set_parameter("tune", "zerolatency");
                }
                "svtav1enc" => {
                    encoder_optz.set_parameter("preset", "12");
                    encoder_optz.set_parameter("parameters-string", "pred-struct=1:lookahead=0");
                }
                "av1enc" => {
                    encoder_optz.set_parameter("usage-profile", "realtime");
                    encoder_optz.set_parameter("cpu-used", "10");
                    encoder_optz.set_parameter("lag-in-frames", "0");
                }
                _ => {}
            }
        }
        _ => {}
    }

    encoder_optz
}

/// Returns all compatible encoders for the system.
/// # Returns
/// * `Vec<EncoderInfo>` - List of compatible encoders.
pub fn get_compatible_encoders() -> Vec<VideoEncoderInfo> {
    let mut encoders: Vec<VideoEncoderInfo> = Vec::new();

    let registry = gst::Registry::get();
    let plugins = registry.plugins();
    for plugin in plugins {
        let features = registry.features_by_plugin(plugin.plugin_name().as_str());
        for feature in features {
            let encoder = feature.name().to_string();
            let factory = gst::ElementFactory::find(encoder.as_str());
            if factory.is_some() {
                let factory = factory.unwrap();
                // Get klass metadata
                let klass = factory.metadata("klass");
                if klass.is_some() {
                    // Make sure klass contains "Encoder/Video/..."
                    let klass = klass.unwrap().to_string();
                    if !klass.to_lowercase().contains("encoder/video") {
                        continue;
                    }

                    // If contains "/hardware" in klass, it's a hardware encoder
                    let encoder_type = if klass.to_lowercase().contains("/hardware") {
                        EncoderType::HARDWARE
                    } else {
                        EncoderType::SOFTWARE
                    };

                    let api = get_encoder_api(&encoder, &encoder_type);
                    if is_encoder_supported(&encoder) {
                        // Match codec by looking for "264" or "av1" in encoder name
                        let codec = if encoder.contains("264") {
                            VideoCodec::H264
                        } else if encoder.contains("av1") {
                            VideoCodec::AV1
                        } else {
                            continue;
                        };
                        let encoder_info = VideoEncoderInfo::new(encoder, codec, encoder_type, api);
                        encoders.push(encoder_info);
                    } else if api == EncoderAPI::VAAPI {
                        // Try low-power variant of VA-API encoder
                        let low_power_encoder = get_low_power_encoder(&encoder);
                        if is_encoder_supported(&low_power_encoder) {
                            let codec = if low_power_encoder.contains("264") {
                                VideoCodec::H264
                            } else if low_power_encoder.contains("av1") {
                                VideoCodec::AV1
                            } else {
                                continue;
                            };
                            let encoder_info =
                                VideoEncoderInfo::new(low_power_encoder, codec, encoder_type, api);
                            encoders.push(encoder_info);
                        }
                    }
                }
            }
        }
    }

    encoders
}

/// Helper to return encoder from vector by name (case-insensitive).
/// # Arguments
/// * `encoders` - A vector containing information about each encoder.
/// * `name` - A string slice that holds the encoder name.
/// # Returns
/// * `Option<EncoderInfo>` - A reference to an EncoderInfo struct if found.
pub fn get_encoder_by_name(
    encoders: &Vec<VideoEncoderInfo>,
    name: &str,
) -> Option<VideoEncoderInfo> {
    let name = name.to_lowercase();
    encoders
        .iter()
        .find(|encoder| encoder.name.to_lowercase() == name)
        .cloned()
}

/// Helper to get encoders from vector by video codec.
/// # Arguments
/// * `encoders` - A vector containing information about each encoder.
/// * `codec` - The codec of the encoder.
/// # Returns
/// * `Vec<EncoderInfo>` - A vector containing EncoderInfo structs if found.
pub fn get_encoders_by_videocodec(
    encoders: &Vec<VideoEncoderInfo>,
    codec: &VideoCodec,
) -> Vec<VideoEncoderInfo> {
    encoders
        .iter()
        .filter(|encoder| encoder.codec == *codec)
        .cloned()
        .collect()
}

/// Helper to get encoders from vector by encoder type.
/// # Arguments
/// * `encoders` - A vector containing information about each encoder.
/// * `encoder_type` - The type of the encoder.
/// # Returns
/// * `Vec<EncoderInfo>` - A vector containing EncoderInfo structs if found.
pub fn get_encoders_by_type(
    encoders: &Vec<VideoEncoderInfo>,
    encoder_type: &EncoderType,
) -> Vec<VideoEncoderInfo> {
    encoders
        .iter()
        .filter(|encoder| encoder.encoder_type == *encoder_type)
        .cloned()
        .collect()
}

/// Returns best-case compatible encoder given desired codec and encoder type.
/// # Arguments
/// * `encoders` - List of encoders to pick from.
/// * `codec` - Desired codec.
/// * `encoder_type` - Desired encoder type.
/// # Returns
/// * `Option<EncoderInfo>` - Best-case compatible encoder.
pub fn get_best_compatible_encoder(
    encoders: &Vec<VideoEncoderInfo>,
    codec: VideoCodec,
    encoder_type: EncoderType,
) -> Option<VideoEncoderInfo> {
    let mut best_encoder: Option<VideoEncoderInfo> = None;
    let mut best_score: i32 = 0;

    // Filter by codec and type first
    let encoders = get_encoders_by_videocodec(encoders, &codec);
    let encoders = get_encoders_by_type(&encoders, &encoder_type);

    for encoder in encoders {
        // Local score
        let mut score = 0;

        // API score
        score += match encoder.encoder_api {
            EncoderAPI::NVENC => 3,
            EncoderAPI::QSV => 3,
            EncoderAPI::AMF => 3,
            EncoderAPI::VAAPI => 2,
            EncoderAPI::SOFTWARE => 1,
            EncoderAPI::UNKNOWN => 0,
        };

        // If software, score also based on name to get most compatible software encoder for low latency
        if encoder.encoder_type == EncoderType::SOFTWARE {
            score += match encoder.name.as_str() {
                "openh264enc" => 2,
                "x264enc" => 1,
                "svtav1enc" => 2,
                "av1enc" => 1,
                _ => 0,
            };
        }

        // Update best encoder based on score
        if score > best_score {
            best_encoder = Some(encoder.clone());
            best_score = score;
        }
    }

    best_encoder
}
