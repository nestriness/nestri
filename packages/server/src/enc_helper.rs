use gst::prelude::GstObjectExt;

#[derive(Debug, Eq, PartialEq, Clone)]
pub enum VideoCodec {
    H264,
    AV1,
}
impl VideoCodec {
    pub fn to_str(&self) -> &'static str {
        match self {
            VideoCodec::H264 => "H.264",
            VideoCodec::AV1 => "AV1",
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

/// Sets parameters of known encoders for low latency operation and specified CQP quality.
/// # Arguments
/// * `encoder` - Information about the encoder.
/// * `quality` - Constant quantization parameter (CQP) quality, recommended values are between 20-30.
/// # Returns
/// * `Option<EncoderInfo>` - Encoder with updated parameters if supported, None otherwise.
pub fn encoder_low_latency_cqp_params(
    encoder: &VideoEncoderInfo,
    quality: u32,
) -> Option<VideoEncoderInfo> {
    let mut encoder_optz = encoder.clone();
    match encoder_optz.encoder_api {
        EncoderAPI::QSV => {
            encoder_optz.set_parameter("gop-size", "30");
            encoder_optz.set_parameter("low-latency", "true");
            encoder_optz.set_parameter("target-usage", "7");
            encoder_optz.set_parameter("rate-control", "cqp");
            encoder_optz.set_parameter("qp-i", &quality.to_string());
            // P-frame QP is offset by +2 from base (I-frame) QP
            encoder_optz.set_parameter("qp-p", &(quality + 2).to_string());
        }
        EncoderAPI::VAAPI => {
            encoder_optz.set_parameter("key-int-max", "30");
            encoder_optz.set_parameter("target-usage", "7");
            encoder_optz.set_parameter("rate-control", "cqp");
            encoder_optz.set_parameter("qpi", &quality.to_string());
            encoder_optz.set_parameter("qpp", &(quality + 2).to_string());
        }
        EncoderAPI::NVENC => {
            match encoder_optz.codec {
                // nvcudah264enc supports newer presets and tunes
                VideoCodec::H264 => {
                    encoder_optz.set_parameter("gop-size", "30");
                    encoder_optz.set_parameter("multi-pass", "disabled");
                    encoder_optz.set_parameter("preset", "p1");
                    encoder_optz.set_parameter("tune", "ultra-low-latency");
                    encoder_optz.set_parameter("rc-mode", "constqp");
                    encoder_optz.set_parameter("qp-const-i", &quality.to_string());
                    encoder_optz.set_parameter("qp-const-p", &(quality + 2).to_string());
                }
                // nvav1enc only supports older presets
                VideoCodec::AV1 => {
                    encoder_optz.set_parameter("gop-size", "30");
                    encoder_optz.set_parameter("preset", "low-latency-hp");
                    encoder_optz.set_parameter("rc-mode", "constqp");
                    encoder_optz.set_parameter("qp-const-i", &quality.to_string());
                    encoder_optz.set_parameter("qp-const-p", &(quality + 2).to_string());
                }
            }
        }
        EncoderAPI::AMF => {
            encoder_optz.set_parameter("gop-size", "30");
            encoder_optz.set_parameter("preset", "speed");
            encoder_optz.set_parameter("rate-control", "cqp");
            encoder_optz.set_parameter("qp-i", &quality.to_string());
            encoder_optz.set_parameter("qp-p", &(quality + 2).to_string());
            match encoder_optz.codec {
                // Only H.264 supports "ultra-low-latency" usage
                VideoCodec::H264 => {
                    encoder_optz.set_parameter("usage", "ultra-low-latency");
                }
                VideoCodec::AV1 => {
                    encoder_optz.set_parameter("usage", "low-latency");
                }
            }
        }
        EncoderAPI::SOFTWARE => {
            // Check encoder name for software encoders
            match encoder_optz.name.as_str() {
                "openh264enc" => {
                    encoder_optz.set_parameter("gop-size", "30");
                    encoder_optz.set_parameter("complexity", "low");
                    encoder_optz.set_parameter("usage-type", "screen");
                    encoder_optz.set_parameter("rate-control", "quality");
                    encoder_optz.set_parameter("qp-min", &quality.to_string());
                    encoder_optz.set_parameter("qp-max", &(quality + 2).to_string());
                }
                "x264enc" => {
                    encoder_optz.set_parameter("key-int-max", "30");
                    encoder_optz.set_parameter("rc-lookahead", "0");
                    encoder_optz.set_parameter("speed-preset", "ultrafast");
                    encoder_optz.set_parameter("tune", "zerolatency");
                    encoder_optz.set_parameter("pass", "quant");
                    encoder_optz.set_parameter("quantizer", &quality.to_string());
                }
                "svtav1enc" => {
                    encoder_optz.set_parameter("preset", "12");
                    encoder_optz.set_parameter("intra-period-length", "30");
                    encoder_optz.set_parameter("cqp", &quality.to_string());
                    encoder_optz.set_parameter("parameters-string", "pred-struct=1:lookahead=0");
                }
                "av1enc" => {
                    encoder_optz.set_parameter("usage-profile", "realtime");
                    encoder_optz.set_parameter("cpu-used", "10");
                    encoder_optz.set_parameter("end-usage", "q");
                    encoder_optz.set_parameter("lag-in-frames", "0");
                    encoder_optz.set_parameter("keyframe-max-dist", "30");
                    encoder_optz.set_parameter("min-quantizer", &quality.to_string());
                    encoder_optz.set_parameter("max-quantizer", &(quality + 2).to_string());
                }
                _ => {
                    return None;
                }
            }
        }
        EncoderAPI::UNKNOWN => {
            return None;
        }
    }

    Some(encoder_optz)
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

    for encoder in encoders {
        // Skip if encoder codec doesn't match desired codec
        if encoder.codec != codec {
            continue;
        }

        // Skip if encoder type doesn't match desired encoder type
        if encoder.encoder_type != encoder_type {
            continue;
        }

        // Local score
        let mut score = 0;

        // API score
        score += match encoder.encoder_api {
            EncoderAPI::VAAPI => 2,
            EncoderAPI::QSV => 3,
            EncoderAPI::NVENC => 3,
            EncoderAPI::AMF => 3,
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
