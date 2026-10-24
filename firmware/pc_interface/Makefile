ALL_SRCS = $(wildcard *.c)
TEST_SRCS = $(wildcard *_test.c)
SRCS = $(filter-out ${TEST_SRCS},${ALL_SRCS})
OBJS = $(SRCS:.c=.o)
TEST_BINS = $(TEST_SRCS:%_test.c=%_test.exe)
MCU  = atmega328p
F_CPU = 16000000
CC = avr-gcc
CFLAGS = -Wall -Werror -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU) -g -DNDEBUG -MMD -MP
TEST_FLAGS = -Wall -g
HDRS = $(wildcard *.h) ../shared/*.h ../test.h
PORT = /dev/ttyUSB1

default: pc_interface.hex

upload: pc_interface.hex
	avrdude -c arduino -P $(PORT) -b 115200 -p $(MCU) -U flash:w:pc_interface.hex:i

pc_interface.elf pc_interface.hex: $(OBJS)
	avr-gcc $(CFLAGS) -o pc_interface.elf $(OBJS)
	avr-objcopy -j .text -j .data -O ihex pc_interface.elf pc_interface.hex

disasm: pc_interface.elf
	avr-objdump -d -S pc_interface.elf

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
	rm -f $(OBJS) $(TEST_OBJS) $(TEST_BINS) pc_interface.elf pc_interface.hex *.d

size: pc_interface.elf
	avr-size -C --mcu=${MCU} pc_interface.elf

size-info: pc_interface.elf
	avr-nm -t d --size-sort -C pc_interface.elf
