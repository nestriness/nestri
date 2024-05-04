use anyhow::Context;
use moq_transport::serve::{
    DatagramsReader, GroupsReader, ObjectsReader, StreamReader, TrackReader, TrackReaderMode,
};

pub struct Subscriber {
    track: TrackReader,
}

impl Subscriber {
    pub fn new(track: TrackReader) -> Self {
        Self { track }
    }

    pub async fn run(self) -> anyhow::Result<()> {
        match self.track.mode().await.context("failed to get mode")? {
            TrackReaderMode::Stream(stream) => Self::recv_stream(stream).await,
            TrackReaderMode::Groups(groups) => Self::recv_groups(groups).await,
            TrackReaderMode::Objects(objects) => Self::recv_objects(objects).await,
            TrackReaderMode::Datagrams(datagrams) => Self::recv_datagrams(datagrams).await,
        }
    }

    async fn recv_stream(mut track: StreamReader) -> anyhow::Result<()> {
        while let Some(mut group) = track.next().await? {
            while let Some(object) = group.read_next().await? {
                let str = String::from_utf8_lossy(&object);
                println!("{}", str);
            }
        }

        Ok(())
    }

    async fn recv_groups(mut groups: GroupsReader) -> anyhow::Result<()> {
        while let Some(mut group) = groups.next().await? {
            let base = group
                .read_next()
                .await
                .context("failed to get first object")?
                .context("empty group")?;

            let base = String::from_utf8_lossy(&base);

            while let Some(object) = group.read_next().await? {
                let str = String::from_utf8_lossy(&object);
                println!("{}{}", base, str);
            }
        }

        Ok(())
    }

    async fn recv_objects(mut objects: ObjectsReader) -> anyhow::Result<()> {
        while let Some(mut object) = objects.next().await? {
            let payload = object.read_all().await?;
            let str = String::from_utf8_lossy(&payload);
            println!("{}", str);
        }

        Ok(())
    }

    async fn recv_datagrams(mut datagrams: DatagramsReader) -> anyhow::Result<()> {
        while let Some(datagram) = datagrams.read().await? {
            let str = String::from_utf8_lossy(&datagram.payload);
            println!("{}", str);
        }

        Ok(())
    }
}
