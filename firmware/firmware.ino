#include <avr/delay.h>
#include <avr/interrupt.h>

#define F_CPU 1000000UL   // 1 MHz CPU

#define M1_PIN    0
#define M2_PIN    1
#define M3_PIN    2
#define M4_PIN    3
#define BTN_PIN   4

#define I_TIMER   0
#define I_BTN     1

uint8_t INTERRUPTS = 0;       // No interrupt is triggered by default

uint8_t INTERRUPT_MASK = 0;   // No interrupt is enabled by default

uint8_t LED_SHOW_IDX = 0;

bool BTN_DOWN = false;

struct led_config {
  uint8_t vcc_pin;
  uint8_t gnd_pin;
  uint8_t pin_mask_out;
  uint8_t port_mask_high;
};

const struct led_config LED_CONFIGS[12] = {
  {M2_PIN, M1_PIN, (1 << M2_PIN) | (1 << M1_PIN), (1 << M2_PIN)},   // D1:  M2 -> M1
  {M3_PIN, M1_PIN, (1 << M3_PIN) | (1 << M1_PIN), (1 << M3_PIN)},   // D2:  M3 -> M1
  {M4_PIN, M1_PIN, (1 << M4_PIN) | (1 << M1_PIN), (1 << M4_PIN)},   // D3:  M4 -> M1
  {M1_PIN, M2_PIN, (1 << M1_PIN) | (1 << M2_PIN), (1 << M1_PIN)},   // D4:  M1 -> M2
  {M3_PIN, M2_PIN, (1 << M3_PIN) | (1 << M2_PIN), (1 << M3_PIN)},   // D5:  M3 -> M2
  {M4_PIN, M2_PIN, (1 << M4_PIN) | (1 << M2_PIN), (1 << M4_PIN)},   // D6:  M4 -> M2
  {M1_PIN, M3_PIN, (1 << M1_PIN) | (1 << M3_PIN), (1 << M1_PIN)},   // D7:  M1 -> M3
  {M2_PIN, M3_PIN, (1 << M2_PIN) | (1 << M3_PIN), (1 << M2_PIN)},   // D8:  M2 -> M3
  {M4_PIN, M3_PIN, (1 << M4_PIN) | (1 << M3_PIN), (1 << M4_PIN)},   // D9:  M4 -> M3
  {M1_PIN, M4_PIN, (1 << M1_PIN) | (1 << M4_PIN), (1 << M1_PIN)},   // D10: M1 -> M4
  {M2_PIN, M4_PIN, (1 << M2_PIN) | (1 << M4_PIN), (1 << M2_PIN)},   // D11: M2 -> M4
  {M3_PIN, M4_PIN, (1 << M3_PIN) | (1 << M4_PIN), (1 << M3_PIN)}    // D12: M3 -> M4
};

uint16_t LED_PATTERN = 0;

uint8_t LAST_LED_PATTERN = 0;

uint8_t LED_COUNT = 0;

uint8_t LEDS[12];

inline void set_led_pattern(uint16_t pattern) {
  if (LAST_LED_PATTERN == pattern) return;
  LAST_LED_PATTERN = LED_PATTERN;
  LED_PATTERN = pattern;

  LED_COUNT = 0;
  for (uint8_t led = 0; led < 12; led++) {
    if (pattern & 0x1) {
      LEDS[LED_COUNT++] = led;
    }
    pattern >>= 1;
  }
}

inline void clear_leds() {
  // Assume LEDs are wired to physical pins PB0 -> PB3
  DDRB &= ~0x0f;
  PORTB &= ~0x0f;
}

inline void turn_on_led(uint8_t led) {
  clear_leds();
  DDRB |= LED_CONFIGS[led].pin_mask_out;
  PORTB |= LED_CONFIGS[led].port_mask_high;
}

void commit_leds() {
  // Listen to all active interrupts
  while (!(INTERRUPTS & INTERRUPT_MASK)) {
    for (uint8_t led_idx = 0; led_idx < LED_COUNT; led_idx++) {
      turn_on_led(LEDS[led_idx]);
      _delay_us(10);  // Give LEDs enough time to light up
    }
  }
}

ISR(PCINT0_vect) {
  BTN_DOWN = !BTN_DOWN;
  if (!BTN_DOWN) {
    LED_SHOW_IDX = (LED_SHOW_IDX + 1) % 6;
  }
  INTERRUPTS |= 1 << I_BTN;
}

uint8_t cnt = 0;
ISR(TIM0_COMPA_vect) {
  if (++cnt < 10) return;
  cnt = 0;
  INTERRUPTS |= 1 << I_TIMER;
}

int main() {
  cli();
  DDRB &= ~(1 << BTN_PIN);    // Set pin to input
  PORTB |= (1 << BTN_PIN);    // Set pin to high to enable pull-up resistor
  GIMSK |= (1 << PCIE);       // Enable Pin Change Interrupts
  PCMSK |= (1 << PCINT4);     // Enable Pin Change Interrupts on PB4

  TCCR0A = 1 << WGM01;                // Clear timer when TIMSK reaches the value in OCR0A
  TCCR0B = (1 << CS02) | (1 << CS00); // Set prescaler 1024 => 1 timer finish every 1.024 ms
  TCNT0 = 0;                          // Reset the timer counter
  TIMSK = 1 << OCIE0A;                // Enable timer comparison with OCR0A
  OCR0A = 96;                         // 97 cycles, trigger interrupt every 99.328 ms
  sei();

  // Check if button is pressed on boot
  if (PINB & (1 << PINB4)) {
    // In normal mode, enable both interrupts
    INTERRUPT_MASK = (1 << I_TIMER) | (1 << I_BTN);

    while (true) {
      if (LED_SHOW_IDX == 0) {
        set_led_pattern(3);
      } else if (LED_SHOW_IDX == 1) {
        set_led_pattern(15);
      } else if (LED_SHOW_IDX == 2) {
        set_led_pattern(63);
      } else if (LED_SHOW_IDX == 3) {
        set_led_pattern(255);
      } else if (LED_SHOW_IDX == 4) {
        set_led_pattern(1023);
      } else if (LED_SHOW_IDX == 5) {
        set_led_pattern(4095);
      }
      commit_leds();
      INTERRUPTS = 0;
    }
  } else {
    // In secret mode, disable the timer interrupt
    INTERRUPT_MASK = 1 << I_BTN;

    while (true) {
      // TODO: add cooldown after button press to avoid unwanted clicks
      if (!BTN_DOWN) {
        set_led_pattern(4095);
      } else {
        set_led_pattern(240);
      }
      commit_leds();
      INTERRUPTS = 0;
    }
  }

  return 0;
}
