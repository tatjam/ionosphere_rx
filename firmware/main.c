#include <avr/io.h>
#include <util/delay.h>

#define LED PA1

int main() {
  uint8_t high = 0;

  DDRA |= (1 << LED);
  PORTA &= ~(1 << LED);

  while(1) {
    high = !high;

    if(high) {
      PORTA |= (1 << LED);
    } else {
      PORTA &= ~(1 << LED);
    }

    for(int i = 0; i < 10; i++) {
      _delay_ms(10);
    }
  }

  return 0;
}
