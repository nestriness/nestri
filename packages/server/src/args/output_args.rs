pub struct MoQOutputArgs {
    /// Relay server URL (e.g. "https://relay.example.com")
    pub relay_url: String,
}
impl MoQOutputArgs {
    pub fn from_matches(matches: &clap::ArgMatches) -> Self {
        Self {
            relay_url: matches.get_one::<String>("moq-relay").unwrap().clone(),
        }
    }
}

pub struct WebRTCWHIPOutputArgs {
    /// WHIP endpoint
    pub endpoint: String,
    /// WHIP auth token
    pub auth_token: String,
}
impl WebRTCWHIPOutputArgs {
    pub fn from_matches(matches: &clap::ArgMatches) -> Self {
        Self {
            endpoint: matches.get_one::<String>("whip-endpoint").unwrap().clone(),
            auth_token: matches.get_one::<String>("whip-auth-token").unwrap().clone(),
        }
    }
}

pub enum OutputOption {
    /// MoQ (Media over QUIC) output
    MoQ(MoQOutputArgs),
    /// WebRTC WHIP output
    WHIP(WebRTCWHIPOutputArgs),
}
impl OutputOption {
    pub fn from_matches(matches: &clap::ArgMatches) -> Self {
        match matches.get_one::<String>("output").unwrap().as_str() {
            "moq" => OutputOption::MoQ(MoQOutputArgs::from_matches(matches)),
            "whip" => OutputOption::WHIP(WebRTCWHIPOutputArgs::from_matches(matches)),
            // Default to WHIP
            _ => OutputOption::WHIP(WebRTCWHIPOutputArgs::from_matches(matches)),
        }
    }

    pub fn debug_print(&self) {
        println!("OutputArgs:");
        match self {
            OutputOption::MoQ(args) => {
                println!("> MoQ:");
                println!("-> relay_url: {}", args.relay_url);
            }
            OutputOption::WHIP(args) => {
                println!("> WHIP:");
                println!("-> endpoint: {}", args.endpoint);
                println!("-> auth_token: {}", args.auth_token);
            }
        }
    }
}
