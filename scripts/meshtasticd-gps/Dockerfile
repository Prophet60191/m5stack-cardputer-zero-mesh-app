# Native build dependencies follow the pinned upstream Dockerfile/debian control.
FROM debian:13-slim
ENV DEBIAN_FRONTEND=noninteractive PIP_BREAK_SYSTEM_PACKAGES=1
RUN apt-get update && apt-get install -y --no-install-recommends \
 ca-certificates curl git g++ make zip pkg-config python3-pip python3-grpc-tools \
 dpkg-dev libgpiod-dev libyaml-cpp-dev libbluetooth-dev libi2c-dev libuv1-dev \
 libusb-1.0-0-dev libulfius-dev liborcania-dev libssl-dev libbsd-dev \
 libx11-dev libinput-dev libxkbcommon-x11-dev libsqlite3-dev libsdl2-dev \
 && python3 -m pip install --no-cache-dir platformio==6.2.0 meshtastic==2.7.11
ENV PLATFORMIO_SETTING_ENABLE_TELEMETRY=no
WORKDIR /work
