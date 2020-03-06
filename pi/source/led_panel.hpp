#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <stdexcept>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <vector>

/// led_panel controls the communication with an array of 32 x 16 LED panels.
/// The constructor's width and height correspond to a number of led_panels, not pixels.
class led_panel {
    public:
    led_panel(uint8_t width, uint8_t height) :
        _width(width), _height(height), _memory_file_descriptor(open("/dev/gpiomem", O_RDWR | O_SYNC)) {
        if (_memory_file_descriptor < 0) {
            throw std::logic_error("'/dev/gpiomem' could not be opened in read and write mode");
        }
        auto map = mmap(nullptr, 180, PROT_READ | PROT_WRITE, MAP_SHARED, _memory_file_descriptor, 0);
        if (map == MAP_FAILED) {
            close(_memory_file_descriptor);
            throw std::logic_error("mmap failed");
        }
        _gpios = reinterpret_cast<volatile uint32_t*>(const_cast<volatile void*>(map));
        //                --999888777666555444333222------
        *(_gpios + 0) = 0b00000000000001001000000000000000u;
        //                --999888777666555444333222111000
        *(_gpios + 1) = 0b00001000000001000000001000000000u;
        //                --------777666555444333222111000
        *(_gpios + 2) = 0b00000000001001000000000000001001u;
        *(_gpios + clear_offset) = _byte_to_mask[255] | request_mask;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (uint8_t index = 0; index < 8; ++index) {
            send(std::vector<uint8_t>(64 * _width * _height + 1, 0));
        }
    }
    led_panel(const led_panel&) = delete;
    led_panel(led_panel&& other) = delete;
    led_panel& operator=(const led_panel&) = delete;
    led_panel& operator=(led_panel&& other) = delete;
    virtual ~led_panel() {
        *(_gpios + clear_offset) = _byte_to_mask[255] | request_mask;
        *(_gpios + 2) = 0u;
        *(_gpios + 1) = 0u;
        *(_gpios + 0) = 0u;
        munmap(const_cast<void*>(reinterpret_cast<volatile void*>(_gpios)), 180);
        close(_memory_file_descriptor);
    }

    /// send transmits a frame to the led_panel.
    /// The frame must have width * height * 64 bytes.
    virtual void send(const std::vector<uint8_t>& frame) {
        if (frame.size() != 64 * _width * _height + 1) {
            throw std::logic_error("bad frame size");
        }
        auto request = true;
        auto acknowledge = true;
        send_byte(frame[0], request, acknowledge, true); // send the duty cycle
        for (uint8_t ab = 0; ab < 4; ++ab) {
            for (uint8_t panel = 0; panel < _width * _height; ++panel) {
                for (uint8_t column = 0; column < 4; ++column) {
                    for (uint8_t row = 0; row < 4; ++row) {
                        send_byte(
                            frame[display_coordinates_to_frame_index(_width, _height, ab, panel, row, column) + 1],
                            request,
                            acknowledge);
                    }
                }
            }
        }
        send_byte(0, request, acknowledge); // send an extra byte to even the payload
        _previous_write = std::chrono::high_resolution_clock::now();
    }

    protected:
    /// display_coordinates_to_frame_index converts a display byte position to a frame index.
    /// The frame must be in row major order.
    ///     - width is the number of panels (horizontaly)
    ///     - height is the number of panels (verticaly)
    ///     - ab is the row block index, in the range [0, 4[
    ///     - panel is the s-pattern panel index, in the range [0, width * height[
    ///     - column is the column index, in the range [0, 4[
    ///     - row is the interlaced row index, in the range [0, 4[
    /// The Arduino is connected to the panel with coordinates (width - 1, height - 1)
    /// Both the panel and pixel coordinates use the conventional frame coordinate system, with the origin at the top
    /// left.
    const uint16_t display_coordinates_to_frame_index(
        uint8_t width,
        uint8_t height,
        uint8_t ab,
        uint8_t panel,
        uint8_t row,
        uint8_t column) {
        return column + (panel % width) * 4 + ((3 - row) * 4 + ab + (panel / width) * 16) * width * 4;
    }

    /// request_mask selects the request signal pin.
    static constexpr uint32_t request_mask = (1u << 27);

    /// acknowledge_pin is the acknowledge signal pin.
    static constexpr uint32_t acknowledge_pin = 22;

    /// set_offset is the gpios set register offset.
    static constexpr uint8_t set_offset = 7;

    /// clear_offset is the gpios clear register offset.
    static constexpr uint8_t clear_offset = 10;

    /// level_offset is the gpios level register offset.
    static constexpr uint8_t level_offset = 13;

    /// byte_to_mask associates a byte value to a set mask.
    /// The clear mask can be calculated from the set mask with the expression:
    ///     clear_mask = (~set_mask) & set_mask[255]
    static constexpr std::array<uint32_t, 256> _byte_to_mask = {
        0b00000000000000000000000000000000, 0b00000000000100000000000000000000, 0b00000000001000000000000000000000,
        0b00000000001100000000000000000000, 0b00000100000000000000000000000000, 0b00000100000100000000000000000000,
        0b00000100001000000000000000000000, 0b00000100001100000000000000000000, 0b00000000000000010000000000000000,
        0b00000000000100010000000000000000, 0b00000000001000010000000000000000, 0b00000000001100010000000000000000,
        0b00000100000000010000000000000000, 0b00000100000100010000000000000000, 0b00000100001000010000000000000000,
        0b00000100001100010000000000000000, 0b00000000000010000000000000000000, 0b00000000000110000000000000000000,
        0b00000000001010000000000000000000, 0b00000000001110000000000000000000, 0b00000100000010000000000000000000,
        0b00000100000110000000000000000000, 0b00000100001010000000000000000000, 0b00000100001110000000000000000000,
        0b00000000000010010000000000000000, 0b00000000000110010000000000000000, 0b00000000001010010000000000000000,
        0b00000000001110010000000000000000, 0b00000100000010010000000000000000, 0b00000100000110010000000000000000,
        0b00000100001010010000000000000000, 0b00000100001110010000000000000000, 0b00000000000000000010000000000000,
        0b00000000000100000010000000000000, 0b00000000001000000010000000000000, 0b00000000001100000010000000000000,
        0b00000100000000000010000000000000, 0b00000100000100000010000000000000, 0b00000100001000000010000000000000,
        0b00000100001100000010000000000000, 0b00000000000000010010000000000000, 0b00000000000100010010000000000000,
        0b00000000001000010010000000000000, 0b00000000001100010010000000000000, 0b00000100000000010010000000000000,
        0b00000100000100010010000000000000, 0b00000100001000010010000000000000, 0b00000100001100010010000000000000,
        0b00000000000010000010000000000000, 0b00000000000110000010000000000000, 0b00000000001010000010000000000000,
        0b00000000001110000010000000000000, 0b00000100000010000010000000000000, 0b00000100000110000010000000000000,
        0b00000100001010000010000000000000, 0b00000100001110000010000000000000, 0b00000000000010010010000000000000,
        0b00000000000110010010000000000000, 0b00000000001010010010000000000000, 0b00000000001110010010000000000000,
        0b00000100000010010010000000000000, 0b00000100000110010010000000000000, 0b00000100001010010010000000000000,
        0b00000100001110010010000000000000, 0b00000000000000000000000001000000, 0b00000000000100000000000001000000,
        0b00000000001000000000000001000000, 0b00000000001100000000000001000000, 0b00000100000000000000000001000000,
        0b00000100000100000000000001000000, 0b00000100001000000000000001000000, 0b00000100001100000000000001000000,
        0b00000000000000010000000001000000, 0b00000000000100010000000001000000, 0b00000000001000010000000001000000,
        0b00000000001100010000000001000000, 0b00000100000000010000000001000000, 0b00000100000100010000000001000000,
        0b00000100001000010000000001000000, 0b00000100001100010000000001000000, 0b00000000000010000000000001000000,
        0b00000000000110000000000001000000, 0b00000000001010000000000001000000, 0b00000000001110000000000001000000,
        0b00000100000010000000000001000000, 0b00000100000110000000000001000000, 0b00000100001010000000000001000000,
        0b00000100001110000000000001000000, 0b00000000000010010000000001000000, 0b00000000000110010000000001000000,
        0b00000000001010010000000001000000, 0b00000000001110010000000001000000, 0b00000100000010010000000001000000,
        0b00000100000110010000000001000000, 0b00000100001010010000000001000000, 0b00000100001110010000000001000000,
        0b00000000000000000010000001000000, 0b00000000000100000010000001000000, 0b00000000001000000010000001000000,
        0b00000000001100000010000001000000, 0b00000100000000000010000001000000, 0b00000100000100000010000001000000,
        0b00000100001000000010000001000000, 0b00000100001100000010000001000000, 0b00000000000000010010000001000000,
        0b00000000000100010010000001000000, 0b00000000001000010010000001000000, 0b00000000001100010010000001000000,
        0b00000100000000010010000001000000, 0b00000100000100010010000001000000, 0b00000100001000010010000001000000,
        0b00000100001100010010000001000000, 0b00000000000010000010000001000000, 0b00000000000110000010000001000000,
        0b00000000001010000010000001000000, 0b00000000001110000010000001000000, 0b00000100000010000010000001000000,
        0b00000100000110000010000001000000, 0b00000100001010000010000001000000, 0b00000100001110000010000001000000,
        0b00000000000010010010000001000000, 0b00000000000110010010000001000000, 0b00000000001010010010000001000000,
        0b00000000001110010010000001000000, 0b00000100000010010010000001000000, 0b00000100000110010010000001000000,
        0b00000100001010010010000001000000, 0b00000100001110010010000001000000, 0b00000000000000000000000000100000,
        0b00000000000100000000000000100000, 0b00000000001000000000000000100000, 0b00000000001100000000000000100000,
        0b00000100000000000000000000100000, 0b00000100000100000000000000100000, 0b00000100001000000000000000100000,
        0b00000100001100000000000000100000, 0b00000000000000010000000000100000, 0b00000000000100010000000000100000,
        0b00000000001000010000000000100000, 0b00000000001100010000000000100000, 0b00000100000000010000000000100000,
        0b00000100000100010000000000100000, 0b00000100001000010000000000100000, 0b00000100001100010000000000100000,
        0b00000000000010000000000000100000, 0b00000000000110000000000000100000, 0b00000000001010000000000000100000,
        0b00000000001110000000000000100000, 0b00000100000010000000000000100000, 0b00000100000110000000000000100000,
        0b00000100001010000000000000100000, 0b00000100001110000000000000100000, 0b00000000000010010000000000100000,
        0b00000000000110010000000000100000, 0b00000000001010010000000000100000, 0b00000000001110010000000000100000,
        0b00000100000010010000000000100000, 0b00000100000110010000000000100000, 0b00000100001010010000000000100000,
        0b00000100001110010000000000100000, 0b00000000000000000010000000100000, 0b00000000000100000010000000100000,
        0b00000000001000000010000000100000, 0b00000000001100000010000000100000, 0b00000100000000000010000000100000,
        0b00000100000100000010000000100000, 0b00000100001000000010000000100000, 0b00000100001100000010000000100000,
        0b00000000000000010010000000100000, 0b00000000000100010010000000100000, 0b00000000001000010010000000100000,
        0b00000000001100010010000000100000, 0b00000100000000010010000000100000, 0b00000100000100010010000000100000,
        0b00000100001000010010000000100000, 0b00000100001100010010000000100000, 0b00000000000010000010000000100000,
        0b00000000000110000010000000100000, 0b00000000001010000010000000100000, 0b00000000001110000010000000100000,
        0b00000100000010000010000000100000, 0b00000100000110000010000000100000, 0b00000100001010000010000000100000,
        0b00000100001110000010000000100000, 0b00000000000010010010000000100000, 0b00000000000110010010000000100000,
        0b00000000001010010010000000100000, 0b00000000001110010010000000100000, 0b00000100000010010010000000100000,
        0b00000100000110010010000000100000, 0b00000100001010010010000000100000, 0b00000100001110010010000000100000,
        0b00000000000000000000000001100000, 0b00000000000100000000000001100000, 0b00000000001000000000000001100000,
        0b00000000001100000000000001100000, 0b00000100000000000000000001100000, 0b00000100000100000000000001100000,
        0b00000100001000000000000001100000, 0b00000100001100000000000001100000, 0b00000000000000010000000001100000,
        0b00000000000100010000000001100000, 0b00000000001000010000000001100000, 0b00000000001100010000000001100000,
        0b00000100000000010000000001100000, 0b00000100000100010000000001100000, 0b00000100001000010000000001100000,
        0b00000100001100010000000001100000, 0b00000000000010000000000001100000, 0b00000000000110000000000001100000,
        0b00000000001010000000000001100000, 0b00000000001110000000000001100000, 0b00000100000010000000000001100000,
        0b00000100000110000000000001100000, 0b00000100001010000000000001100000, 0b00000100001110000000000001100000,
        0b00000000000010010000000001100000, 0b00000000000110010000000001100000, 0b00000000001010010000000001100000,
        0b00000000001110010000000001100000, 0b00000100000010010000000001100000, 0b00000100000110010000000001100000,
        0b00000100001010010000000001100000, 0b00000100001110010000000001100000, 0b00000000000000000010000001100000,
        0b00000000000100000010000001100000, 0b00000000001000000010000001100000, 0b00000000001100000010000001100000,
        0b00000100000000000010000001100000, 0b00000100000100000010000001100000, 0b00000100001000000010000001100000,
        0b00000100001100000010000001100000, 0b00000000000000010010000001100000, 0b00000000000100010010000001100000,
        0b00000000001000010010000001100000, 0b00000000001100010010000001100000, 0b00000100000000010010000001100000,
        0b00000100000100010010000001100000, 0b00000100001000010010000001100000, 0b00000100001100010010000001100000,
        0b00000000000010000010000001100000, 0b00000000000110000010000001100000, 0b00000000001010000010000001100000,
        0b00000000001110000010000001100000, 0b00000100000010000010000001100000, 0b00000100000110000010000001100000,
        0b00000100001010000010000001100000, 0b00000100001110000010000001100000, 0b00000000000010010010000001100000,
        0b00000000000110010010000001100000, 0b00000000001010010010000001100000, 0b00000000001110010010000001100000,
        0b00000100000010010010000001100000, 0b00000100000110010010000001100000, 0b00000100001010010010000001100000,
        0b00000100001110010010000001100000};

    /// send_byte sends a single byte to the display.
    void send_byte(uint8_t byte, bool& request, bool& acknowledge, bool first = false) {
        for (uint8_t index = 0; index < 64; ++index) {
            asm volatile("nop");
        }
        *(_gpios + (first ? set_offset : clear_offset)) = _byte_to_mask[byte];
        *(_gpios + (first ? clear_offset : set_offset)) = (~_byte_to_mask[byte]) & std::get<255>(_byte_to_mask);
        for (uint8_t index = 0; index < 64; ++index) {
            asm volatile("nop");
        }
        *(_gpios + (request ? set_offset : clear_offset)) = request_mask;
        request = !request;
        if (first) {
            std::this_thread::sleep_until(_previous_write + std::chrono::microseconds(100));
            if (((*(_gpios + level_offset) >> acknowledge_pin) & 1) != acknowledge) {
                std::this_thread::sleep_until(_previous_write + std::chrono::milliseconds(15));
            }
        }
        while ((((*(_gpios + level_offset) >> acknowledge_pin) & 1) == 1) != acknowledge) {
        }
        acknowledge = !acknowledge;
    }

    const uint8_t _width;
    const uint8_t _height;
    int32_t _memory_file_descriptor;
    volatile uint32_t* _gpios;
    std::chrono::high_resolution_clock::time_point _previous_write;
};
