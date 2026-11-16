FROM debian:trixie-slim
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends python3 python3-venv build-essential ninja-build git libfreetype-dev libpng-dev libjpeg-dev zlib1g-dev libfmt-dev nftables sudo adduser systemd dpkg-dev file ca-certificates && rm -rf /var/lib/apt/lists/*
RUN python3 -m venv /opt/test && /opt/test/bin/pip install --no-cache-dir cmake==3.31.6 meshtastic==2.7.11
ENV PATH="/opt/test/bin:${PATH}"
