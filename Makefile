CXX ?= c++
VERSION := $(shell cat VERSION)
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null || pkg-config --libs sdl2)
BINARY ?= build/nightfall
.PHONY: all test clean
all: $(BINARY)
$(BINARY): src/main.cpp src/game.hpp src/network.hpp src/render.hpp VERSION
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DNIGHTFALL_VERSION='"$(VERSION)"' $(SDL_CFLAGS) src/main.cpp -o $@ $(SDL_LIBS)
test: all
	./$(BINARY) --self-test
	python3 tests/integration.py ./$(BINARY)
	./$(BINARY) --headless --ui-test --fast --frames 70
	python3 tests/updater.py
clean:
	rm -rf build
