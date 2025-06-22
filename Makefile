CXX = g++
CXXFLAGS = -O2 -std=c++11
LDFLAGS = -lSDL2 -lGLESv2

SRC = GameLaunchSplash.cpp
OUT = game_launch_splash

all: $(OUT)

$(OUT): $(SRC)
    $(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
    rm -f $(OUT)