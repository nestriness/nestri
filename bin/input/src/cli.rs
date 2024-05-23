use clap::Parser;
use std::{net, path};
use url::Url;

#[derive(Parser, Clone, Debug)]
pub struct Config {
	/// Listen for UDP packets on the given address.
	#[arg(long, default_value = "[::]:8080")]
	pub bind: net::SocketAddr,

	/// Connect to the given URL starting with https://
	#[arg(value_parser = moq_url)]
	pub url: Url,

	/// Use the TLS root CA at this path, encoded as PEM.
	///
	/// This value can be provided multiple times for multiple roots.
	/// If this is empty, system roots will be used instead
	#[arg(long)]
	pub tls_root: Vec<path::PathBuf>,

	/// Danger: Disable TLS certificate verification.
	///
	/// Fine for local development, but should be used in caution in production.
	#[arg(long)]
	pub tls_disable_verify: bool,

	/// Publish the current time to the relay, otherwise only subscribe.
	// #[arg(long)]
	// pub publish: bool,

	/// The name of the input track.
	#[arg(long, default_value = "netris")]
	pub namespace: String,

	/// The name of the input track.
	#[arg(long, default_value = ".catalog")]
	pub track: String,
}

fn moq_url(s: &str) -> Result<Url, String> {
	Url::try_from(s).map_err(|e| e.to_string())
}