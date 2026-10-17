#!/usr/bin/env bash
# build deps for debian / ubuntu / mint
set -euo pipefail

sudo apt update
sudo apt install -y \
    build-essential cmake ninja-build git gdb pkg-config \
    libgl-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libsystemd-dev \
    strace clang-format clangd
