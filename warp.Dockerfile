FROM rust:bookworm as builder

# Create a build directory and copy over all of the files
WORKDIR /build
COPY ./moq-server/* ./
# Reuse a cache between builds.
# I tried to `cargo install`, but it doesn't seem to work with workspaces.
# There's also issues with the cache mount since it builds into /usr/local/cargo/bin
# We can't mount that without clobbering cargo itself.
# We instead we build the binaries and copy them to the cargo bin directory.
RUN ls -la 

RUN --mount=type=cache,target=/usr/local/cargo/registry \
    --mount=type=cache,target=/build/target \
    cargo build --manifest-path ./moq-pub/Cargo.toml --target x86_64-unknown-linux-gnu --release && cp target/x86_64-unknown-linux-gnu/release/moq-pub /usr/bin/warp
