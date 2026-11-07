#!/bin/bash
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
    exec sudo "$0" "$@"
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    live-build \
    debootstrap \
    xorriso \
    squashfs-tools \
    mtools \
    dosfstools \
    isolinux \
    syslinux-common \
    grub-efi-amd64-bin \
    grub-pc-bin \
    rsync \
    ca-certificates \
    curl \
    gnupg \
    python3 \
    python3-yaml \
    python3-pil \
    nodejs
