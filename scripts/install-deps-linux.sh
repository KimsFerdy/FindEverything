#!/usr/bin/env bash
# Installs build tools + the X11/OpenGL headers GLFW needs (Debian/Ubuntu/Mint).
set -euo pipefail

sudo apt update
sudo apt install -y \
    build-essential cmake ninja-build git gdb \
    libgl-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    strace clang-format clangd
