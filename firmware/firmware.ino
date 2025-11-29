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

struct led_config {
  uint8_t vcc_pin;
  uint8_t gnd_pin;
  uint8_t pin_mask_out;
  uint8_t port_mask_high;
};

struct led_show {
  uint16_t led_patterns[10];
  uint8_t i_breaks[10];
  uint8_t length;
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

uint8_t INTERRUPTS = 0;       // No interrupt is triggered by default

uint8_t INTERRUPT_MASK = 0;   // No interrupt is enabled by default

uint16_t TIMER_INTERRUPT_CNT = 0;

uint8_t LED_SHOW_IDX = 0;

bool BTN_DOWN = false;

uint16_t LED_PATTERN = 0;

uint8_t LAST_LED_PATTERN = 0;

uint8_t LED_COUNT = 0;

uint8_t LEDS[12];

inline void turn_on_led(uint8_t led) {
  clear_leds();
  DDRB |= LED_CONFIGS[led].pin_mask_out;
  PORTB |= LED_CONFIGS[led].port_mask_high;
}

inline void clear_leds() {
  // Assume LEDs are wired to physical pins PB0 -> PB3
  DDRB &= ~0x0f;
  PORTB &= ~0x0f;
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

void set_led_pattern(uint16_t pattern) {
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

void set_led_show(struct led_show* show) {
  uint16_t curr_int_cnt = TIMER_INTERRUPT_CNT % show->i_breaks[show->length - 1];
  for (uint8_t i = 0; i < show->length; i++) {
    if (curr_int_cnt < show->i_breaks[i]) {
      set_led_pattern(show->led_patterns[i]);
      return;
    }
  }
  // If LED shows are correctly set, we should never end up here
  set_led_pattern(show->led_patterns[show->length - 1]);
}

ISR(PCINT0_vect) {
  BTN_DOWN = !BTN_DOWN;
  if (!BTN_DOWN) {
    LED_SHOW_IDX = (LED_SHOW_IDX + 1) % 1;
  }
  INTERRUPTS |= 1 << I_BTN;
}

ISR(TIM0_COMPA_vect) {
  TIMER_INTERRUPT_CNT = (TIMER_INTERRUPT_CNT + 1) % 0xffff;
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

  const struct led_show led_shows[1] = {
    {{0x555, 0xaaa, 0x555, 0xaaa, 0x555, 0xaaa}, {2, 4, 6, 8, 10, 12}, 6},
    // {{0x555, 0xaaa, 0x555, 0xaaa, 0x555, 0xaaa}, {4, 8, 12, 16, 18, 20}, 6}
  };

  // Check if button is pressed on boot
  if (PINB & (1 << PINB4)) {
    // In normal mode, enable both interrupts
    INTERRUPT_MASK = (1 << I_TIMER) | (1 << I_BTN);

    while (true) {
      if (LED_SHOW_IDX == 0) {
        set_led_show(&led_shows[0]);
      }
      // else if (LED_SHOW_IDX == 1) {
      //   set_led_show(&led_shows[1]);
      // }
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
