FROM ubuntu:20.04 as runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt update && \
   apt install -y x11-apps

CMD /usr/bin/xclock