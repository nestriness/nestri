use clap::{Arg, Command};

pub mod app_args;
pub mod output_args;
pub mod device_args;
pub mod encoding_args;

pub struct Args {
    pub app: app_args::AppArgs,
    pub output: output_args::OutputOption,
    pub device: device_args::DeviceArgs,
    pub encoding: encoding_args::EncodingArgs,
}
impl Args {
    pub fn new() -> Self {
        let matches = Command::new("nestri-server")
            .arg(
                Arg::new("verbose")
                    .short('v')
                    .long("verbose")
                    .env("VERBOSE")
                    .help("Enable verbose output")
                    .default_value("false"),
            )
            .arg(
                Arg::new("debug-feed")
                    .short('d')
                    .long("debug-feed")
                    .env("DEBUG_FEED")
                    .help("Debug by showing a window on host")
                    .default_value("false"),
            )
            .arg(
                Arg::new("debug-latency")
                    .short('l')
                    .long("debug-latency")
                    .env("DEBUG_LATENCY")
                    .help("Debug latency by showing time on feed")
                    .default_value("false"),
            )
            .arg(
                Arg::new("resolution")
                    .short('r')
                    .long("resolution")
                    .env("RESOLUTION")
                    .help("Display/stream resolution in 'WxH' format")
                    .default_value("1280x720"),
            )
            .arg(
                Arg::new("framerate")
                    .short('f')
                    .long("framerate")
                    .env("FRAMERATE")
                    .help("Display/stream framerate")
                    .default_value("60"),
            )
            .arg(
                Arg::new("gpu-vendor")
                    .short('g')
                    .long("gpu-vendor")
                    .env("GPU_VENDOR")
                    .help("GPU to find by vendor (e.g. 'nvidia')")
                    .required(false),
            )
            .arg(
                Arg::new("gpu-name")
                    .short('n')
                    .long("gpu-name")
                    .env("GPU_NAME")
                    .help("GPU to find by name (e.g. 'rtx 3060')")
                    .required(false),
            )
            .arg(
                Arg::new("gpu-index")
                    .short('i')
                    .long("gpu-index")
                    .env("GPU_INDEX")
                    .help("GPU index, if multiple similar GPUs are present")
                    .default_value("0"),
            )
            .arg(
                Arg::new("gpu-card-path")
                    .short('a')
                    .long("gpu-card-path")
                    .env("GPU_CARD_PATH")
                    .help("Force a specific GPU by card/render path (e.g. '/dev/dri/card0')")
                    .required(false)
                    .conflicts_with_all(["gpu-vendor", "gpu-name", "gpu-index"]),
            )
            .arg(
                Arg::new("encoder-vcodec")
                    .short('c')
                    .long("encoder-vcodec")
                    .env("ENCODER_VCODEC")
                    .help("Preferred encoder video codec (e.g. 'h264')")
                    .default_value("h264"),
            )
            .arg(
                Arg::new("encoder-type")
                    .short('t')
                    .long("encoder-type")
                    .env("ENCODER_TYPE")
                    .help("Preferred encoder type (e.g. 'hardware')")
                    .default_value("hardware"),
            )
            .arg(
                Arg::new("encoder-name")
                    .short('e')
                    .long("encoder-name")
                    .env("ENCODER_NAME")
                    .help("Force an encoder to use (e.g. 'vah264lpenc')")
                    .required(false)
                    .conflicts_with_all(["encoder-type", "encoder-vcodec"]),
            )
            .arg(
                Arg::new("encoder-cqp")
                    .short('q')
                    .long("encoder-cqp")
                    .env("ENCODER_CQP")
                    .help("Encoder CQP quality level, lower values mean higher quality at cost of higher bitrate")
                    .default_value("25"),
            )
            .arg(
                Arg::new("no-audio")
                    .short('x')
                    .long("no-audio")
                    .env("NO_AUDIO")
                    .help("Disable audio output")
                    .default_value("false"),
            )
            .arg(
                Arg::new("output")
                    .short('o')
                    .long("output")
                    .env("OUTPUT")
                    .help("Output type (e.g. 'moq', 'whip')")
                    .default_value("whip"),
            )
            .arg(
                Arg::new("moq-relay")
                    .short('m')
                    .long("moq-relay")
                    .env("MOQ_RELAY")
                    .help("MoQ relay URL")
                    .default_value("https://relay.dathorse.com:8443"),
            )
            .arg(
                Arg::new("moq-path")
                    .short('p')
                    .long("moq-path")
                    .env("MOQ_PATH")
                    .help("MoQ relay path/namespace/broadcast")
                    .default_value("teststream"),
            )
            .arg(
                Arg::new("whip-endpoint")
                    .short('w')
                    .long("whip-endpoint")
                    .env("WHIP_ENDPOINT")
                    .help("WebRTC WHIP endpoint")
                    .default_value("https://relay.dathorse.com/whip"),
            )
            .arg(
                Arg::new("whip-auth-token")
                    .short('y')
                    .long("whip-auth-token")
                    .env("WHIP_AUTH_TOKEN")
                    .help("WebRTC WHIP auth token")
                    .default_value(""),
            )
            .get_matches();

        Self {
            app: app_args::AppArgs::from_matches(&matches),
            output: output_args::OutputOption::from_matches(&matches),
            device: device_args::DeviceArgs::from_matches(&matches),
            encoding: encoding_args::EncodingArgs::from_matches(&matches),
        }
    }

    pub fn debug_print(&self) {
        self.app.debug_print();
        self.output.debug_print();
        self.device.debug_print();
        self.encoding.debug_print();
    }
}
