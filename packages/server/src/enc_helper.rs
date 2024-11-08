use crate::gpu;

#[derive(Debug, Eq, PartialEq)]
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

#[derive(Debug, Eq, PartialEq)]
pub enum EncoderAPI {
    VAAPI,
    NVENC,
    QSV,
    AMF,
    UNKNOWN,
}
impl EncoderAPI {
    pub fn to_str(&self) -> &'static str {
        match self {
            EncoderAPI::VAAPI => "VA-API",
            EncoderAPI::NVENC => "NVENC",
            EncoderAPI::QSV => "QSV",
            EncoderAPI::AMF => "AMF",
            EncoderAPI::UNKNOWN => "UNKNOWN",
        }
    }
}

#[derive(Debug)]
pub struct EncoderInfo {
    pub name: String,
    pub codec: VideoCodec,
    pub api: EncoderAPI,
    pub parameters: Vec<(String, String)>,
}

impl EncoderInfo {
    pub fn new(name: String, codec: VideoCodec, api: EncoderAPI) -> Self {
        Self {
            name,
            codec,
            api,
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

/// Returns gstreamer encoder for the given GPU vendor and video codec.
/// # Returns
/// * `String` - The name of the gstreamer encoder.
fn get_vendor_encoder(vendor: &gpu::GPUVendor, codec: &VideoCodec) -> String {
    match vendor {
        gpu::GPUVendor::Intel => match codec {
            VideoCodec::H264 => "qsvh264enc".to_string(),
            VideoCodec::AV1 => "qsvav1enc".to_string(),
        },
        gpu::GPUVendor::NVIDIA => match codec {
            VideoCodec::H264 => "nvcudah264enc".to_string(),
            VideoCodec::AV1 => "nvav1enc".to_string(),
        },
        // TODO: AMF whenever it's supported widely?
        gpu::GPUVendor::AMD => match codec {
            VideoCodec::H264 => "vah264enc".to_string(),
            VideoCodec::AV1 => "vaav1enc".to_string(),
        },
        // Fallback to software encoders
        gpu::GPUVendor::Unknown => match codec {
            VideoCodec::H264 => "x264enc".to_string(),
            VideoCodec::AV1 => "svtav1enc".to_string(),
        },
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
fn get_encoder_api(encoder: &String) -> EncoderAPI {
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
}

/// Returns true if system supports given encoder.
/// # Returns
/// * `bool` - True if encoder is supported, false otherwise.
fn is_encoder_supported(encoder: &String) -> bool {
    gst::ElementFactory::find(encoder.as_str()).is_some()
}

/// Picks the most approriate encoder for the given GPU and codecs.
/// # Arguments
/// * `gpu` - Information about the GPU.
/// * `codecs` - Vector of allowed codecs in order of preference.
/// # Returns
/// * `Option<EncoderInfo>` - Information about the selected encoder.
pub fn pick_encoder(gpu: &gpu::GPUInfo, codecs: Vec<VideoCodec>) -> Option<EncoderInfo> {
    let mut encoder: Option<EncoderInfo> = None;
    for codec in codecs {
        let encoder_name = get_vendor_encoder(gpu.vendor(), &codec);
        let encoder_api = get_encoder_api(&encoder_name);
        if is_encoder_supported(&encoder_name) {
            encoder = Some(EncoderInfo::new(encoder_name, codec, encoder_api));
            break;
        } else if encoder_api == EncoderAPI::VAAPI {
            // Try using low-power variant of the encoder
            let low_power_encoder = get_low_power_encoder(&encoder_name);
            if is_encoder_supported(&low_power_encoder) {
                encoder = Some(EncoderInfo::new(low_power_encoder, codec, encoder_api));
                break;
            }
        }
    }

    encoder
}

/// Sets parameters of known encoders for low latency operation and specified CQP quality.
/// # Arguments
/// * `encoder` - Information about the encoder.
/// * `quality` - Constant quantization parameter (CQP) quality, recommended values are between 20-30.
/// # Returns
/// * `EncoderInfo` - Information about the encoder with low latency parameters.
pub fn encoder_low_latency_cqp_params(encoder: EncoderInfo, quality: u32) -> EncoderInfo {
    let mut encoder = encoder;
    match encoder.api {
        EncoderAPI::QSV => {
            encoder.set_parameter("gop-size", "30");
            encoder.set_parameter("low-latency", "true");
            encoder.set_parameter("target-usage", "7");
            encoder.set_parameter("rate-control", "cqp");
            encoder.set_parameter("qp-i", &quality.to_string());
            // P-frame QP is offset by +2 from base (I-frame) QP
            encoder.set_parameter("qp-p", &(quality + 2).to_string());
        }
        EncoderAPI::VAAPI => {
            encoder.set_parameter("key-int-max", "30");
            encoder.set_parameter("target-usage", "7");
            encoder.set_parameter("rate-control", "cqp");
            encoder.set_parameter("qpi", &quality.to_string());
            encoder.set_parameter("qpp", &(quality + 2).to_string());
        }
        EncoderAPI::NVENC => {
            match encoder.codec {
                // nvcudah264enc supports newer presets and tunes
                VideoCodec::H264 => {
                    encoder.set_parameter("gop-size", "30");
                    encoder.set_parameter("multi-pass", "disabled");
                    encoder.set_parameter("preset", "p1");
                    encoder.set_parameter("tune", "ultra-low-latency");
                    encoder.set_parameter("rc-mode", "constqp");
                    encoder.set_parameter("qp-const-i", &quality.to_string());
                    encoder.set_parameter("qp-const-p", &(quality + 2).to_string());
                }
                // nvav1enc only supports older presets
                VideoCodec::AV1 => {
                    encoder.set_parameter("gop-size", "30");
                    encoder.set_parameter("preset", "low-latency-hp");
                    encoder.set_parameter("rc-mode", "constqp");
                    encoder.set_parameter("qp-const-i", &quality.to_string());
                    encoder.set_parameter("qp-const-p", &(quality + 2).to_string());
                }
            }
        }
        EncoderAPI::AMF => {
            encoder.set_parameter("gop-size", "30");
            encoder.set_parameter("preset", "speed");
            encoder.set_parameter("rate-control", "cqp");
            encoder.set_parameter("qp-i", &quality.to_string());
            encoder.set_parameter("qp-p", &(quality + 2).to_string());
            match encoder.codec {
                // Only H.264 supports "ultra-low-latency" usage
                VideoCodec::H264 => {
                    encoder.set_parameter("usage", "ultra-low-latency");
                }
                VideoCodec::AV1 => {
                    encoder.set_parameter("usage", "low-latency");
                }
            }
        }
        EncoderAPI::UNKNOWN => {} // TODO: Fallback software encoders
    }

    encoder
}
