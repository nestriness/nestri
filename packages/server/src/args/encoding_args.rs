pub struct EncodingArgs {
    /// Encoder video codec (e.g. "h264")
    pub encoder_vcodec: String,
    /// Encoder type (e.g. "hardware")
    pub encoder_type: String,
    /// Encoder name (e.g. "vah264lpenc")
    pub encoder_name: String,
    /// Encoder CQP quality level (e.g. 25)
    pub encoder_cqp: u32,
    /// Whether to disable audio output
    pub no_audio: bool,
}
impl EncodingArgs {
    pub fn from_matches(matches: &clap::ArgMatches) -> Self {
        Self {
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
            no_audio: matches.get_one::<String>("no-audio").unwrap() == "true"
                || matches.get_one::<String>("no-audio").unwrap() == "1",
        }
    }

    pub fn debug_print(&self) {
        println!("Encoding Arguments:");
        println!("> Encoder Video Codec: {}", self.encoder_vcodec);
        println!("> Encoder Type: {}", self.encoder_type);
        println!("> Encoder Name: {}", self.encoder_name);
        println!("> Encoder CQP: {}", self.encoder_cqp);
        println!("> No Audio: {}", self.no_audio);
    }
}
