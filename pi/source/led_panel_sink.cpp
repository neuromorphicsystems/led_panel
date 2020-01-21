#include "led_panel.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

uint8_t string_to_uint8(const std::string& name, const std::string& input) {
    const auto candidate = stoul(input);
    if (candidate > 255) {
        throw std::out_of_range(name + " must be smaller than 256");
    }
    return static_cast<uint8_t>(candidate);
}

int main(int argc, char* argv[]) {
    uint8_t width = 1;
    uint8_t height = 1;
    try {
        if (argc != 3) {
            throw std::runtime_error("bad number of arguments");
        }
        width = string_to_uint8("width", argv[1]);
        height = string_to_uint8("height", argv[2]);
    } catch (const std::exception& error) {
        std::cerr << error.what()
                  << "\nsyntax: led_panel_sink width height\n    width and height are a number of panels, not a number "
                     "of pixels"
                  << std::endl;
        return 1;
    }
    led_panel display(width, height);
    std::vector<uint8_t> frame(64 * width * height + 1);
    for (;;) {
        std::cin.read(reinterpret_cast<char*>(frame.data()), frame.size());
        if (!std::cin.good()) {
            break;
        }
        display.send(frame);
    }
    return 0;
}
