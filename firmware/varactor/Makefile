ALL_SRCS = $(wildcard *.c)
TEST_SRCS = $(wildcard *_test.c)
SRCS = $(filter-out ${TEST_SRCS},${ALL_SRCS})
OBJS = $(SRCS:.c=.o)
TEST_BINS = $(TEST_SRCS:%_test.c=%_test.exe)
MCU  = attiny84
F_CPU = 8000000
CC = avr-gcc
CFLAGS = -Wall -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU) -g -DNDEBUG -MMD -MP
TEST_FLAGS = -Wall -g
HDRS = $(wildcard *.h) ../shared/*.h ../test.h

default: firmware.hex

upload: firmware.hex
	avrdude -c avrisp -P /dev/ttyUSB0 -b 250000 -p $(MCU) -U flash:w:firmware.hex:i

firmware.elf firmware.hex: $(OBJS)
	avr-gcc $(CFLAGS) -o firmware.elf $(OBJS)
	avr-objcopy -j .text -j .data -O ihex firmware.elf firmware.hex

disasm: firmware.elf
	avr-objdump -d -S firmware.elf

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%_test.exe: %_test.c ${SRCS} ${HDRS}
	cc $(TEST_FLAGS) -o $@ $<

test: $(TEST_BINS)
	@for t in $^; do \
		echo "TEST --- $$t ---"; \
		./$$t && echo "Passed" || exit 1; \
	done


-include ${OBJS:.o=.d}

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TEST_BINS) firmware.elf firmware.hex *.d

size: firmware.elf
	avr-size -C --mcu=${MCU} firmware.elf

size-info: firmware.elf
	avr-nm -t d --size-sort -C firmware.elf
