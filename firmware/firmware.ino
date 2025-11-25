#include <avr/delay.h>
#include <avr/interrupt.h>

#define F_CPU 1000000UL   // 1 MHz CPU

#define M1_PIN    2
#define M2_PIN    1
#define M3_PIN    0
#define M4_PIN    3
#define BTN_PIN   4

#define I_TIMER   0
#define I_BTN     1

uint8_t INTERRUPTS = 0;       // No interrupt is triggered by default

uint8_t INTERRUPT_MASK = 0;   // No interrupt is enabled by default

bool LED_SHOW_TOGGLE = false;

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

void commit_leds(uint16_t* state_ptr) {
  uint16_t lights_state = *state_ptr;
  
  // Listen to all active interrupts
  while (!(INTERRUPTS & INTERRUPT_MASK)) {
    for (uint8_t led = 0; led < 12; led++) {
      if (lights_state & (1 << led)) {
        turn_on_led(led);
        _delay_us(10);  // Give LEDs enough time to light up
      }
    }
  }
}

ISR(PCINT0_vect) {
  BTN_DOWN = !BTN_DOWN;
  INTERRUPTS |= 1 << I_BTN;
}

uint8_t cnt = 0;
ISR(TIM0_COMPA_vect) {
  if (++cnt < 10) return;
  cnt = 0;
  LED_SHOW_TOGGLE = !LED_SHOW_TOGGLE;
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
      if (!LED_SHOW_TOGGLE) {
        LED_PATTERN = 1885;
      } else {
        LED_PATTERN = 2210;
      }
      commit_leds(&LED_PATTERN);
      INTERRUPTS = 0;
    }
  } else {
    // In secret mode, disable the timer interrupt
    INTERRUPT_MASK = 1 << I_BTN;

    while (true) {
      // TODO: add cooldown after button press to avoid unwanted clicks
      if (!BTN_DOWN) {
        LED_PATTERN = 4095;
      } else {
        LED_PATTERN = 240;
      }
      commit_leds(&LED_PATTERN);
      INTERRUPTS = 0;
    }
  }

  return 0;
}
