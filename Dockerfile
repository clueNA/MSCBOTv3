FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
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
    git \
    && rm -rf /var/lib/apt/lists/*

# Install yt-dlp
RUN pip3 install yt-dlp

# Install DPP
WORKDIR /tmp
RUN git clone --depth 1 https://github.com/brainboxdotcc/DPP.git
WORKDIR /tmp/DPP
RUN mkdir build && cd build && \
    cmake .. -DDPP_BUILD_TEST=OFF && \
    make -j$(nproc) && \
    make install

# Copy source code
WORKDIR /app
COPY . .

# Build the bot
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Set executable permissions
RUN chmod +x build/discord-musicbot

CMD ["./build/discord-musicbot"]