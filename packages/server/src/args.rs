use clap::{Arg, Command};

pub struct Args {
    /// Verbose output mode
    pub verbose: bool,
    /// Relay server URL (e.g. "https://relay.example.com")
    pub relay_url: String,
    /// Relay path/namespace (e.g. "teststream")
    pub relay_path: String,
    /// Video/display resolution (e.g. "1920x1080")
    pub resolution: (u32, u32),
    /// Video framerate (e.g. 60)
    pub framerate: u32,
    /// GPU vendor (e.g. "intel")
    pub gpu_vendor: String,
    /// GPU name (e.g. "a770")
    pub gpu_name: String,
    /// GPU index, if multiple same GPUs are present
    pub gpu_index: u32,
    /// GPU card/render path, sets card explicitly from such path
    pub gpu_card_path: String,
    /// Encoder video codec (e.g. "h264")
    pub encoder_vcodec: String,
    /// Encoder type (e.g. "hardware")
    pub encoder_type: String,
    /// Encoder name (e.g. "vah264lpenc")
    pub encoder_name: String,
    /// Encoder CQP quality level (e.g. 25)
    pub encoder_cqp: u32,
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
                Arg::new("relay-url")
                    .short('u')
                    .long("relay-url")
                    .env("RELAY_URL")
                    .help("Relay server URL")
                    .default_value("https://relay.dathorse.com:8443"),
            )
            .arg(
                Arg::new("relay-path")
                    .short('p')
                    .long("relay-path")
                    .env("RELAY_PATH")
                    .help("Relay namespace/path")
                    .required(false),
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
                    .help("Encoder CQP quality level, lower values mean higher quality but with higher bitrate, recommended to keep it between 20-30")
                    .default_value("25"),
            )
            .get_matches();

        Self {
            verbose: matches.get_one::<String>("verbose").unwrap() == "true"
                || matches.get_one::<String>("verbose").unwrap() == "1",
            relay_url: matches.get_one::<String>("relay-url").unwrap().clone(),
            // generate a random relay namespace/path starting with "teststream", e.g. "teststream-1234"
            relay_path: matches
                .get_one::<String>("relay-path")
                .unwrap_or(&format!("teststream-{}", rand::random::<u32>()).clone())
                .clone(),
            resolution: {
                let res = matches.get_one::<String>("resolution").unwrap().clone();
                let parts: Vec<&str> = res.split('x').collect();
                (
                    parts[0].parse::<u32>().unwrap(),
                    parts[1].parse::<u32>().unwrap(),
                )
            },
            framerate: matches
                .get_one::<String>("framerate")
                .unwrap()
                .parse::<u32>()
                .unwrap(),
            gpu_vendor: matches
                .get_one::<String>("gpu-vendor")
                .unwrap_or(&"".to_string())
                .clone(),
            gpu_name: matches
                .get_one::<String>("gpu-name")
                .unwrap_or(&"".to_string())
                .clone(),
            gpu_index: matches
                .get_one::<String>("gpu-index")
                .unwrap()
                .parse::<u32>()
                .unwrap(),
            gpu_card_path: matches
                .get_one::<String>("gpu-card-path")
                .unwrap_or(&"".to_string())
                .clone(),
            encoder_vcodec: matches.get_one::<String>("encoder-vcodec").unwrap().clone(),
            encoder_type: matches.get_one::<String>("encoder-type").unwrap().clone(),
            encoder_name: matches
                .get_one::<String>("encoder-name")
                .unwrap_or(&"".to_string())
                .clone(),
            encoder_cqp: matches
                .get_one::<String>("encoder-cqp")
                .unwrap()
                .parse::<u32>()
                .unwrap(),
        }
    }

    pub fn print(&self) {
        println!("Arguments:");
        println!("> Verbose: {}", self.verbose);
        println!("> Relay URL: {}", self.relay_url);
        println!("> Relay Path: {}", self.relay_path);
        println!("> Resolution: {}x{}", self.resolution.0, self.resolution.1);
        println!("> Framerate: {}", self.framerate);
        println!("> GPU Vendor: {}", self.gpu_vendor);
        println!("> GPU Name: {}", self.gpu_name);
        println!("> GPU Index: {}", self.gpu_index);
        println!("> GPU Card Path: {}", self.gpu_card_path);
        println!("> Encoder Video Codec: {}", self.encoder_vcodec);
        println!("> Encoder Type: {}", self.encoder_type);
        println!("> Encoder Name: {}", self.encoder_name);
    }
}
