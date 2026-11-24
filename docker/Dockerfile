# Native arm64 build environment matching the CardputerZero image (Debian 13 trixie).
# On Apple Silicon this runs without emulation.
FROM --platform=linux/arm64 debian:trixie

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git ca-certificates file dpkg-dev \
        libfreetype-dev libpng-dev libjpeg62-turbo-dev zlib1g-dev libfmt-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
