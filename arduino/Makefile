device = /dev/ttyACM0
baudrate = 57600
microcontroller = atmega328p
programmer = arduino
frequency = 16000000
flags = -DF_CPU=$(frequency) -mmcu=$(microcontroller) -Wall -Wextra -O3

.PHONY: flash clean

all: build/arduino.hex

build/arduino.o: arduino.c
	mkdir -p build
	avr-gcc $(flags) -c $< -o $@

build/arduino.elf: build/arduino.o
	avr-gcc $(flags) -o $@ $^

build/arduino.hex: build/arduino.elf
	avr-objcopy -j .text -j .data -O ihex $^ $@

flash: build/arduino.hex
	avrdude -p$(microcontroller) -c$(programmer) -P$(device) -b$(baudrate) -D -Uflash:w:$<:i

clean:
	rm -rf build
