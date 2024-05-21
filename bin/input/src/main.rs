use moq_transport::{serve, session::Subscriber};
use moq_native::quic;
use std::net;
use url::Url;

use anyhow::Context;
use clap::Parser;

mod input;


#[derive(Parser, Clone)]
pub struct Cli {
    /// Listen for UDP packets on the given address.
    #[arg(long, default_value = "[::]:0")]
    pub bind: net::SocketAddr,

    /// Connect to the given URL starting with https://
    #[arg()]
    pub url: Url,

    /// The TLS configuration.
    #[command(flatten)]
    pub tls: moq_native::tls::Cli,

    // /// Publish the current time to the relay, otherwise only subscribe.
    // #[arg(long)]
    // pub publish: bool,
    /// The name of the input track.
    #[arg(long, default_value = "netris")]
    pub namespace: String,

    /// The name of the input track.
    #[arg(long, default_value = ".catalog")]
    pub track: String,
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    env_logger::init();

    // Disable tracing so we don't get a bunch of Quinn spam.
    let tracer = tracing_subscriber::FmtSubscriber::builder()
        .with_max_level(tracing::Level::WARN)
        .finish();
    tracing::subscriber::set_global_default(tracer).unwrap();

    let config = Cli::parse();
    let tls = config.tls.load()?;

    let quic = quic::Endpoint::new(quic::Config {
        bind: config.bind,
        tls,
    })?;

    log::info!("connecting to server: url={}", config.url);

    let session = quic.client.connect(&config.url).await?;

    let (session, mut subscriber) = Subscriber::connect(session)
        .await
        .context("failed to create MoQ Transport session")?;

    let namespace = format!("{}input", config.namespace);

    let (prod, sub) = serve::Track::new(namespace, config.track).produce();

    let input = input::Subscriber::new(sub);

    //TODO: Make sure to retry until the input server comes [Use Supervisord for now]
    tokio::select! {
        res = session.run() => res.context("session error")?,
        res = input.run() => res.context("input error")?,
        res = subscriber.subscribe(prod) => res.context("failed to subscribe to track")?,
    }

    Ok(())
}
