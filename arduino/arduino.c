// The display period is cycles * rows * timer0_ticks * clock_period = 256 * 4 * (1255 + 1) * (1 / 16e6) = 0.009984 s
// The display framerate is 100.2 Hz.

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>

#define frame_size 128
const uint8_t oe_pin = DDB2;
const uint8_t a_pin = DDB1;
const uint8_t b_pin = DDB0;
const uint8_t l_pin = DDC0;
const uint8_t pi_request_pin = DDC1;
const uint8_t pi_acknowledge_pin = DDC2;

volatile uint8_t frame_buffer[8][frame_size + 1];
volatile union {
    struct {
        uint8_t read : 3;
        uint8_t write : 3;
        uint8_t reserved : 2;
    };
    uint8_t value;
} frame_buffer_index = {
    .read = 0,
    .write = 1,
};
volatile uint8_t frame_tick = 0;
ISR(TIMER0_COMPA_vect) {
    static uint8_t count = 0;
    static union {
        struct {
            uint8_t ab : 2;
            uint8_t reserved : 6;
        };
        uint8_t value;
    } state = {
        .ab = 0,
    };
    static uint8_t oe_count = 0;
    if (count < frame_size / 4) {
        SPDR = frame_buffer[frame_buffer_index.read][count + (frame_size / 4) * state.ab + 1];
        if (oe_count == count) {
            PORTB &= ~(1 << oe_pin);
        }
    } else if (count == frame_size / 4) {
        PORTC |= (1 << l_pin);
        PORTC &= ~(1 << l_pin);
        if (oe_count == frame_size / 4 || oe_count == frame_size / 4 + 1) {
            PORTB &= ~(1 << oe_pin);
        }
    } else if (count == frame_size / 4 + 1) {
        switch (state.ab) {
            case 0:
                PORTB &= ~((1 << a_pin) | (1 << b_pin));
                break;
            case 1:
                PORTB |= (1 << a_pin);
                break;
            case 2:
                PORTB &= ~(1 << a_pin);
                PORTB |= (1 << b_pin);
                break;
            case 3:
                PORTB |= (1 << a_pin);
                break;
        }
        ++state.ab;
        if (state.ab == 0) {
            ++frame_tick;
            if ((frame_buffer_index.read + 1) % 8 != frame_buffer_index.write) {
                ++frame_buffer_index.read;
            }
        }
    } else if (count == frame_size / 4 + 2) {
        oe_count = (frame_buffer[frame_buffer_index.read][0] + frame_size / 4 + 2) % 256;
        if (oe_count != (frame_size / 4 + 2)) {
            PORTB |= (1 << oe_pin);
        }
    } else {
        if (oe_count == count) {
            PORTB &= ~(1 << oe_pin);
        }
    }
    ++count;
}

int main(void) {
    wdt_reset();
    wdt_disable();
    for (uint8_t frame_index = 0; frame_index < 8; ++frame_index) {
        frame_buffer[frame_index][0] = 0;
        for (uint16_t index = 1; index < frame_size + 1; ++index) {
            frame_buffer[frame_index][index] = 255;
        }
    }

    // disable USART
    UCSR0B &= ~((1 << RXEN0) | (1 << TXEN0));

    // trigger an interrupt every 155 clock ticks with Timer0
    OCR0A = 155;
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS00);
    TIMSK0 = (1 << OCIE0A);

    // configure pins
    DDRB = (1 << oe_pin) | (1 << a_pin) | (1 << b_pin);
    PORTB &= ~((1 << oe_pin) | (1 << a_pin) | (1 << b_pin));
    DDRC = (1 << l_pin) | (1 << pi_acknowledge_pin);
    PORTC &= ~((1 << l_pin) | (1 << pi_acknowledge_pin));
    DDRD = 0;
    PORTD = 0;

    // enable SPI
    DDRB |= (1 << DDB3) | (1 << DDB5);
    PORTB |= (1 << DDB3) | (1 << DDB5);
    SPCR = (1 << SPR0) | (1 << MSTR) | (1 << SPE);

    sei();
    uint8_t read_state = 0;
    uint16_t read_index = 0;
    uint8_t previous_frame_tick = 0;
    for (;;) {
        switch (read_state) {
            case 0:
                if ((PINC >> pi_request_pin) & 1) {
                    const uint8_t read = frame_buffer_index.read;
                    if (frame_buffer_index.write == read) {
                        read_state = 1;
                    } else {
                        frame_buffer[frame_buffer_index.write][0] = PIND;
                        PORTC |= (1 << pi_acknowledge_pin);
                        previous_frame_tick = frame_tick;
                        read_index = 1;
                        read_state = 2;
                    }
                }
                break;
            case 1: {
                const uint8_t read = frame_buffer_index.read;
                if (frame_buffer_index.write != read) {
                    frame_buffer[frame_buffer_index.write][0] = PIND;
                    PORTC |= (1 << pi_acknowledge_pin);
                    previous_frame_tick = frame_tick;
                    read_index = 1;
                    read_state = 2;
                }
                break;
            }
            case 2:
                if (((PINC >> pi_request_pin) & 1) != (read_index & 1)) {
                    frame_buffer[frame_buffer_index.write][read_index] = PIND;
                    if (read_index & 1) {
                        PORTC &= ~(1 << pi_acknowledge_pin);
                    } else {
                        PORTC |= (1 << pi_acknowledge_pin);
                    }
                    if (read_index < frame_size) {
                        previous_frame_tick = frame_tick;
                        ++read_index;
                    } else {
                        ++frame_buffer_index.write;
                        read_state = 3;
                    }
                } else {
                    const uint8_t ellapsed = frame_tick - previous_frame_tick;
                    if (ellapsed > 8) {
                        PORTC &= ~(1 << pi_acknowledge_pin);
                        read_state = 0;
                    }
                }
                break;
            case 3:
                if (((PINC >> pi_request_pin) & 1) == 0) {
                    PORTC &= ~(1 << pi_acknowledge_pin);
                    read_state = 0;
                } else {
                    const uint8_t ellapsed = frame_tick - previous_frame_tick;
                    if (ellapsed > 8) {
                        PORTC &= ~(1 << pi_acknowledge_pin);
                        read_state = 4;
                    }
                }
                break;
            case 4:
                if (((PINC >> pi_request_pin) & 1) == 0) {
                    read_state = 0;
                }
                break;
            default:
                break;
        }
    }
}
