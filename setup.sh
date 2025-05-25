#!/bin/bash

# Setup script for Discord Music Bot C++

echo "Setting up Discord Music Bot (C++ with D++)"

# Check if running on supported OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Detected Linux system"
    
    # Update package manager
    sudo apt-get update
    
    # Install build dependencies
    echo "Installing build dependencies..."
    sudo apt-get install -y \
        build-essential \
        cmake \
        pkg-config \
        libssl-dev \
        libopus-dev \
        libsodium-dev \
        zlib1g-dev \
        libjsoncpp-dev \
        ffmpeg \
        python3 \
        python3-pip \
        git
    
    # Install yt-dlp
    echo "Installing yt-dlp..."
    pip3 install --user yt-dlp
    
    # Install DPP library
    echo "Installing D++ library..."
    if [ ! -d "/tmp/DPP" ]; then
        git clone --depth 1 https://github.com/brainboxdotcc/DPP.git /tmp/DPP
    fi
    
    cd /tmp/DPP
    mkdir -p build
    cd build
    cmake .. -DDPP_BUILD_TEST=OFF
    make -j$(nproc)
    sudo make install
    
    # Update library cache
    sudo ldconfig
    
    echo "Dependencies installed successfully!"
    echo "Now you can build the bot with: make"
    echo "Don't forget to set your DISCORD_TOKEN environment variable!"
    
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected macOS system"
    
    # Check if Homebrew is installed
    if ! command -v brew &> /dev/null; then
        echo "Homebrew not found. Please install Homebrew first."
        echo "Visit: https://brew.sh/"
        exit 1
    fi
    
    # Install dependencies with Homebrew
    echo "Installing dependencies with Homebrew..."
    brew install cmake pkg-config openssl opus libsodium jsoncpp ffmpeg python3
    
    # Install yt-dlp
    echo "Installing yt-dlp..."
    pip3 install yt-dlp
    
    # Install DPP library
    echo "Installing D++ library..."
    if [ ! -d "/tmp/DPP" ]; then
        git clone --depth 1 https://github.com/brainboxdotcc/DPP.git /tmp/DPP
    fi
    
    cd /tmp/DPP
    mkdir -p build
    cd build
    cmake .. -DDPP_BUILD_TEST=OFF
    make -j$(sysctl -n hw.ncpu)
    sudo make install
    
    echo "Dependencies installed successfully!"
    echo "Now you can build the bot with: make"
    echo "Don't forget to set your DISCORD_TOKEN environment variable!"
    
else
    echo "Unsupported operating system: $OSTYPE"
    echo "This script supports Linux and macOS only."
    exit 1
fi