#include <avr/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define F_CPU 1000000UL   // 1 MHz CPU

#define M1_PIN    0
#define M2_PIN    1
#define M3_PIN    2
#define M4_PIN    3
#define BTN_PIN   4

#define I_TIMER   0
#define I_BTN     1

struct led_show {
  uint16_t patterns_and_breaks[17];
  uint16_t breaks_total;
  uint8_t length;
};

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

uint8_t INTERRUPTS = 0;       // No interrupt is triggered by default

uint8_t INTERRUPT_MASK = 0;   // No interrupt is enabled by default

uint16_t TIMER_INTERRUPT_CNT = 0;

uint8_t LED_SHOW_IDX = 0;

bool BTN_DOWN = false;

uint16_t LED_PATTERN = 0;

uint8_t LED_COUNT = 0;

uint8_t LEDS[12];

const uint8_t LED_SHOW_COUNT = 4;

// LED shows are constructed as follows:
//  - Array of 16 bit values, where the highest 12 represent the LED pattern and the lowest 4, 
//    the number of timer interrupts that have to pass until the next sequence value is read.
//  - Total number of interrupts of the sequence, used to determine which LED pattern to select.
//  - Total number of individual LED pattern in the sequence.
const struct led_show LED_SHOWS[LED_SHOW_COUNT] PROGMEM = {
  {{0x0401, 0x0c01, 0x1801, 0x3001, 0x6001, 0x4001, 0x0002, 0x0011, 0x0031, 0x0061, 0x00c1, 0x0181, 0x0101, 0x0004, 0x8001, 0x0201, 0x0004}, 24, 17},
  {{0x4101, 0x6181, 0x71c1, 0x79e1, 0x7df8, 0x7df1, 0x3cf1, 0x1c71, 0x0c31, 0x0411, 0x0006, 0x8201, 0x0001, 0x8201, 0x0006}, 32, 16},
  {{0x555f, 0xaaaf, 0x555f, 0xaaaf, 0x5552, 0xaaa2, 0x5552, 0xaaa2, 0x5552, 0xaaa2, 0x5552, 0xaaa2}, 76, 12},
  {{0xfff1}, 1, 1}
};


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
    clear_leds();
    for (uint8_t led_idx = 0; led_idx < LED_COUNT; led_idx++) {
      turn_on_led(LEDS[led_idx]);
      _delay_us(10);  // Give LEDs enough time to light up
    }
  }
}

void set_led_pattern(uint16_t pattern) {
  LED_COUNT = 0;
  for (uint8_t led = 0; led < 12; led++) {
    if (pattern & 0x1) {
      LEDS[LED_COUNT++] = led;
    }
    pattern >>= 1;
  }
}

void set_led_show(struct led_show* show) {
  uint16_t curr_int_cnt = TIMER_INTERRUPT_CNT % show->breaks_total;
  for (uint8_t i = 0; i < show->length; i++) {
    if (curr_int_cnt < (show->patterns_and_breaks[i] & 0xf)) {
      set_led_pattern(show->patterns_and_breaks[i] >> 4);
      return;
    }
    curr_int_cnt -= show->patterns_and_breaks[i] & 0xf;
  }
  // If LED shows are correctly set, we should never end up here
  set_led_pattern(show->patterns_and_breaks[show->length - 1] >> 4);
}

ISR(PCINT0_vect) {
  BTN_DOWN = !BTN_DOWN;
  if (!BTN_DOWN) {
    LED_SHOW_IDX = (LED_SHOW_IDX + 1) % LED_SHOW_COUNT;
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

  // Check if button is pressed on boot
  if (PINB & (1 << PINB4)) {
    // In normal mode, enable both interrupts
    INTERRUPT_MASK = (1 << I_TIMER) | (1 << I_BTN);

    struct led_show current_show;
    uint8_t last_led_show_idx = 1;

    while (true) {
      if (last_led_show_idx != LED_SHOW_IDX) {
        memcpy_P(&current_show, &LED_SHOWS[LED_SHOW_IDX], sizeof(struct led_show));
        last_led_show_idx = LED_SHOW_IDX;
        // Reset timer interrupt counter to always start show from the beginning
        TIMER_INTERRUPT_CNT = 0;
      }
      set_led_show(&current_show);
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
