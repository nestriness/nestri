FROM ubuntu:23.10

RUN apt-get -yqq update \
    && apt-get install -yq --no-install-recommends \
    software-properties-common \
    ca-certificates \
    && apt-get autoremove -y \
    && apt-get clean -y

RUN apt-get update -y \
    && apt-get upgrade -y \
    && add-apt-repository ppa:savoury1/ffmpeg4 \
    && add-apt-repository ppa:savoury1/ffmpeg6 \
    && apt-get update -y \
    && apt-get upgrade -y \
    && apt-get dist-upgrade -y \
    && apt-get install ffmpeg -y \
    && ffmpeg -version