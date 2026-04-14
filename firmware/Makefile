SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
MCU  = attiny84
F_CPU = 8000000
CC = avr-gcc
CFLAGS = -Wall -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU)

default: firmware.hex

upload: firmware.hex
	avrdude -c avrisp -P /dev/ttyUSB0 -b 19200 -p $(MCU) -U flash:w:firmware.hex:i

firmware.hex: $(OBJS)
	avr-gcc $(CFLAGS) -o firmware.elf $(OBJS)
	avr-objcopy -j .text -j .data -O ihex firmware.elf firmware.hex

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) firmware.elf firmware.hex
