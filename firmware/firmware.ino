#include <avr/delay.h>
#include <avr/interrupt.h>

#define F_CPU 8000000UL   // 1 MHz CPU

#define M1_PIN    2
#define M2_PIN    1
#define M3_PIN    0
#define M4_PIN    3
#define BTN_PIN   4


bool INTERRUPT = false;
bool BTN_DOWN = false;
uint16_t LIGHTS_STATE = 0;  // The 4 left-most bits are 0. The 12th represents the state of D12, whereas the 1st, the state of D1.
uint8_t ROW = 0;
uint8_t ROW_X_INDICES[4][4] = {
  {1, 2, 3, 4},
  {2, 1, 3, 4},
  {3, 1, 2, 4},
  {4, 1, 2, 3}
};

// --- Utils ---

bool interrupted_delay(uint32_t millis) {
  while (millis-- > 0) {
    _delay_ms(1);
    if (INTERRUPT) {
      return true;
    }
  }
  return false;
}

inline void pin_mode(uint8_t pin, uint8_t mode, bool batch_mode = false) {
  if (!batch_mode) cli();
  switch (mode) {
    case INPUT:
      DDRB &= ~(1 << pin);
      break;
    case OUTPUT:
      DDRB |= (1 << pin);
      break;
  }
  if (!batch_mode) sei();
}

inline void pin_write(uint8_t pin, uint8_t val, bool batch_mode = false) {
  if (!batch_mode) cli();
  if (val == LOW) {
    PORTB &= ~(1 << pin);
  } else if (val == HIGH) {
    PORTB |= (1 << pin);
  }
  if (!batch_mode) sei();
}

// --- Utils ---


inline uint8_t get_pin(uint8_t mx) {
  switch (mx) {
    case 1:
      return M1_PIN;
    case 2:
      return M2_PIN;
    case 3:
      return M3_PIN;
    case 4:
      return M4_PIN;
    default:
      return M1_PIN;
  }
}

void iterate_row(uint8_t row_idx) {
  auto high_pin = get_pin(row_idx + 1);
  pin_mode(high_pin, OUTPUT);
  pin_write(high_pin, HIGH);

  for (uint8_t i = 0; i < 4; i++) {
    if (get_pin(ROW_X_INDICES[row_idx][i]) == high_pin) {
      continue;
    }
    pin_mode(get_pin(ROW_X_INDICES[row_idx][i]), OUTPUT);
    pin_write(get_pin(ROW_X_INDICES[row_idx][i]), LOW);

    for (uint8_t j = 0; j < 4; j++) {
      if (get_pin(ROW_X_INDICES[row_idx][j]) == get_pin(ROW_X_INDICES[row_idx][i]) || get_pin(ROW_X_INDICES[row_idx][j]) == high_pin) {
        continue;
      }
      pin_mode(get_pin(ROW_X_INDICES[row_idx][j]), INPUT);
    }
    if (interrupted_delay(100)) return;
  }
}

void turn_on_row(uint8_t row_idx) {
  cli();
  pin_write(M1_PIN, LOW, true);
  pin_write(M2_PIN, LOW, true);
  pin_write(M3_PIN, LOW, true);
  pin_write(M4_PIN, LOW, true);
  pin_write(get_pin(row_idx), HIGH, true);
  sei();
}

void clear_all() {
  for (uint8_t i = 1; i <= 4; i++) {
    pin_mode(get_pin(i), INPUT);
    pin_write(get_pin(i), LOW);
  }
}

// This method is to be used only if the total number
// of ON LEDs is at most 3
void commit_three(uint16_t* state_ptr) {
  uint16_t lights_state_tmp = *state_ptr;
  // TODO
}

// This multiplexing approach should only be used in case 
// we wish to light up more than 3 LEDs at a time
void commit_all(uint16_t* state_ptr) {
  uint16_t lights_state_tmp = *state_ptr;
  uint8_t column_on = 0;
  while (true) {
    for (uint8_t high_pin = 1; high_pin <= 4; high_pin++) {
      clear_all();
      for (uint8_t low_pin = 1; low_pin <= 4; low_pin++) {
        if (high_pin == low_pin) continue;
        column_on |= (lights_state_tmp & 0x1);
        pin_mode(get_pin(low_pin), lights_state_tmp & 0x1);
        pin_write(get_pin(low_pin), ~(lights_state_tmp & 0x1));
        lights_state_tmp >>= 1;
      }
      pin_mode(get_pin(high_pin), column_on);
      pin_write(get_pin(high_pin), column_on);
      column_on = 0;
    }
    if (INTERRUPT) return;
    lights_state_tmp = *state_ptr;
  }
}

// ISR(PCINT0_vect) {
//   btn_down = !btn_down;
//   interrupt = true;
// }

uint8_t cnt = 0;
ISR(TIM0_COMPA_vect) {
  if (++cnt < 10) return;
  cnt = 0;
  BTN_DOWN = !BTN_DOWN;
  INTERRUPT = true;
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

  // TODO: add game mode when button is down before boot, change timer interrupt to trigger every 1 ms

  LIGHTS_STATE = 2735;

  while (true) {
    if (!BTN_DOWN) {
      commit_all(&LIGHTS_STATE);
    } else {
      clear_all();
    }

    INTERRUPT = false;  // Always reset interrupt flag
  }
  return 0;
}

