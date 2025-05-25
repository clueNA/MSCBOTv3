CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
LDFLAGS = -ldpp -ljsoncpp -lpthread

# Source files
SOURCES = musicbot.cpp
TARGET = discord-musicbot

# Build target
$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# Clean target
clean:
	rm -f $(TARGET)

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y build-essential cmake pkg-config libssl-dev libopus-dev libsodium-dev zlib1g-dev libjsoncpp-dev ffmpeg python3-pip
	sudo pip3 install yt-dlp
	# Install DPP
	git clone --depth 1 https://github.com/brainboxdotcc/DPP.git /tmp/DPP
	cd /tmp/DPP && mkdir build && cd build && cmake .. -DDPP_BUILD_TEST=OFF && make -j$$(nproc) && sudo make install

.PHONY: clean install-deps