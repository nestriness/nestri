use anyhow::Context;
use chrono::prelude::*;
use clap::Parser;
use moq_native::quic;
use moq_transport::{serve, session::Subscriber};
use std::net;
use url::Url;

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

    let start = Utc::now();
    let mut now = start;

    loop {
        log::info!("connecting to server: url={}", config.url);

        let session = quic.client.connect(&config.url).await?;

        let (session, mut subscriber) = Subscriber::connect(session)
            .await
            .context("failed to create MoQ Transport session")?;

        let namespace = format!("{}input", config.namespace);

        let (prod, sub) = serve::Track::new(namespace, config.track.clone()).produce();

        let input = input::Subscriber::new(sub);

        // let (session, mut publisher) = Publisher::connect(session)
        //     .await
        //     .context("failed to create MoQ Transport session")?;

        // let (mut writer, _, reader) = serve::Tracks {
        //     namespace: config.namespace.clone(),
        // }
        // .produce();

        // let track = writer.create(&config.track).unwrap();
        // let input_publisher = input::Publisher::new(track.groups()?);

        tokio::select! {
            res = session.run() => {
                if let Err(e) = res {
                    log::error!("session error: {}", e);
                    continue;
                }
            },
            res = input.run() => {
                if let Err(e) = res {
                    log::error!("input subscriber error: {}", e);
                    continue;
                }
            },
            // res = publisher.announce(reader) => res.context("failed to serve tracks")?,
            res = subscriber.subscribe(prod) => {
                if let Err(e) = res {
                    log::error!("failed to subscribe to track: {}", e);
                    continue;
                }
            },
        }

        let next = now + chrono::Duration::try_minutes(1).unwrap();
        let next = next.with_second(0).unwrap().with_nanosecond(0).unwrap();

        let delay = (next - now).to_std().unwrap();
        tokio::time::sleep(delay).await;

        // if next.minute() - now.minute() == 10 {
        //     return Ok(());
        // }

        now = next; // just assume we didn't undersleep
    }

    // Ok(())
}
