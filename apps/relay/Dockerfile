#Git clone kixelated's moq-rs then build the moq-relay and push it to the final image
#Copied from https://github.com/kixelated/moq-rs/blob/main/Dockerfile
FROM rust:bookworm as builder

LABEL maintainer="https://github.com/nestriness,https://github.com/wanjohiryan"

WORKDIR /build

RUN apt-get install -y git

RUN git clone https://github.com/kixelated/moq-rs .

RUN --mount=type=cache,target=/usr/local/cargo/registry \
    --mount=type=cache,target=/build/target \
    cargo build --release && cp /build/target/release/moq-* /usr/local/cargo/bin

FROM debian:bookworm-slim

RUN apt-get update && \
	apt-get install -y --no-install-recommends ca-certificates curl libssl3 && \
	rm -rf /var/lib/apt/lists/*

COPY --from=builder /usr/local/cargo/bin/moq-relay /usr/local/bin

CMD ["moq-relay"]