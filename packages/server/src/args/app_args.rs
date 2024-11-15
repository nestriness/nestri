pub struct AppArgs {
    /// Verbose output mode
    pub verbose: bool,
    /// Debug the pipeline by showing a window on host
    pub debug_feed: bool,
    /// Debug the latency by showing time in stream
    pub debug_latency: bool,

    /// Virtual display resolution
    pub resolution: (u32, u32),
    /// Virtual display framerate
    pub framerate: u32,
}
impl AppArgs {
    pub fn from_matches(matches: &clap::ArgMatches) -> Self {
        Self {
            verbose: matches.get_one::<String>("verbose").unwrap() == "true"
                || matches.get_one::<String>("verbose").unwrap() == "1",
            debug_feed: matches.get_one::<String>("debug-feed").unwrap() == "true"
                || matches.get_one::<String>("debug-feed").unwrap() == "1",
            debug_latency: matches.get_one::<String>("debug-latency").unwrap() == "true"
                || matches.get_one::<String>("debug-latency").unwrap() == "1",
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
        }
    }

    pub fn debug_print(&self) {
        println!("AppArgs:");
        println!("> verbose: {}", self.verbose);
        println!("> debug_feed: {}", self.debug_feed);
        println!("> debug_latency: {}", self.debug_latency);
        println!("> resolution: {}x{}", self.resolution.0, self.resolution.1);
        println!("> framerate: {}", self.framerate);
    }
}
