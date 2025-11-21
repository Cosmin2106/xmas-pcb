#include <avr/delay.h>
#include <avr/interrupt.h>

// Mx follow the silkscreen notation of the board
#define M1_PIN    2
#define M2_PIN    1
#define M3_PIN    0
#define M4_PIN    3
#define BTN_PIN   4


bool interrupt = false;
bool btn_down = false;
uint8_t row = 0;
uint8_t row_x_indices[4][4] = {
  {1, 2, 3, 4},
  {2, 1, 3, 4},
  {3, 1, 2, 4},
  {4, 1, 2, 3}
};

// --- Utils ---

bool interrupted_delay(uint32_t millis) {
  while (millis-- > 0) {
    _delay_ms(1);
    if (interrupt) {
      return true;
    }
  }
  return false;
}

void pin_mode(uint8_t pin, uint8_t mode) {
  cli();
  switch (mode) {
    case INPUT:
      DDRB &= ~(1 << pin);
      break;
    case OUTPUT:
      DDRB |= (1 << pin);
      break;
  }
  sei();
}

void pin_write(uint8_t pin, uint8_t val) {
  cli();
  if (val == LOW) {
    PORTB &= ~(1 << pin);
  } else if (val == HIGH) {
    PORTB |= (1 << pin);
  }
  sei();
}

// --- Utils ---


uint8_t get_pin(uint8_t mx) {
  switch (mx) {
    case 1:
      return M1_PIN;
    case 2:
      return M2_PIN;
    case 3:
      return M3_PIN;
    case 4:
      return M4_PIN;
  }
}

void iterate_row(uint8_t row_idx) {
  auto high_pin = get_pin(row_idx + 1);
  pin_mode(high_pin, OUTPUT);
  pin_write(high_pin, HIGH);

  for (uint8_t i = 0; i < 4; i++) {
    if (get_pin(row_x_indices[row_idx][i]) == high_pin) {
      continue;
    }
    pin_mode(get_pin(row_x_indices[row_idx][i]), OUTPUT);
    pin_write(get_pin(row_x_indices[row_idx][i]), LOW);

    for (uint8_t j = 0; j < 4; j++) {
      if (get_pin(row_x_indices[row_idx][j]) == get_pin(row_x_indices[row_idx][i]) || get_pin(row_x_indices[row_idx][j]) == high_pin) {
        continue;
      }
      pin_mode(get_pin(row_x_indices[row_idx][j]), INPUT);
    }
    if (interrupted_delay(100)) return;
  }
}

void turn_on_all() {
  pin_write(M1_PIN, HIGH);
  pin_write(M2_PIN, LOW);
  pin_write(M3_PIN, LOW);
  pin_write(M4_PIN, LOW);

  pin_write(M1_PIN, LOW);
  pin_write(M2_PIN, HIGH);
  pin_write(M3_PIN, LOW);
  pin_write(M4_PIN, LOW);

  pin_write(M1_PIN, LOW);
  pin_write(M2_PIN, LOW);
  pin_write(M3_PIN, HIGH);
  pin_write(M4_PIN, LOW);

  pin_write(M1_PIN, LOW);
  pin_write(M2_PIN, LOW);
  pin_write(M3_PIN, LOW);
  pin_write(M4_PIN, HIGH);
  if (interrupt) return;
}

ISR(PCINT0_vect) {
  btn_down = !btn_down;
  interrupt = true;
}

int main() {
  cli();
  DDRB &= ~(1 << BTN_PIN);    // Set pin to input
  PORTB |= (1 << BTN_PIN);    // Set pin to high to enable pull-up resistor
  GIMSK |= (1 << PCIE);       // Enable Pin Change Interrupts
  PCMSK |= (1 << PCINT4);     // Enable Pin Change Interrupts on PB4
  sei();

  while (true) {
    if (btn_down) {
      for (uint8_t i = 1; i <= 4; i++) {
        pin_mode(get_pin(i), OUTPUT);
      }
      turn_on_all();
    } else {
      iterate_row(row);
      row = (row + 1) & 3;
    }

    interrupt = false;  // Always reset interrupt flag
  }
  return 0;
}

