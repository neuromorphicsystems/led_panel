flags = -std=c++17 -O3

.PHONY: clean

all: source/led_panel_sink.cpp source/led_panel.hpp
	mkdir -p build
	g++ $(flags) source/led_panel_sink.cpp -o build/led_panel_sink

clean:
	rm -rf build
