mod room;

#[tokio::main]
async fn main() -> std::io::Result<()> {
    let room = "test";
    let base_url = "http://localhost:8088";
    let mut room_handler = room::Room::new(room, base_url).await?;
    room_handler.run().await?;

    Ok(())
}
