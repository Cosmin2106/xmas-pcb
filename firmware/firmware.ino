#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#define F_CPU             1000000UL   // 1 MHz CPU

#define EEPROM_FLAG_ADDR  0
#define EEPROM_RAND_ADDR  126

#define M1_PIN            0
#define M2_PIN            1
#define M3_PIN            2
#define M4_PIN            3
#define BTN_PIN           4

#define I_TIMER           0
#define I_BTN             1


// LED shows are constructed as follows:
//  - Array of 16 bit values, where the highest 12 represent the LED pattern and the lowest 4, 
//    the number of timer interrupts that have to pass until the next sequence value is read.
//  - Total number of interrupts of the sequence, used to determine which LED pattern to select.
//  - Total number of individual LED pattern in the sequence.
struct led_show {
  uint16_t patterns_and_breaks[24];
  uint16_t breaks_total;
  uint8_t pattern_length;
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

volatile uint8_t INTERRUPTS = 0;       // No interrupt is triggered by default

volatile uint8_t INTERRUPT_MASK = 0;   // No interrupt is enabled by default

volatile uint16_t TIMER_INTERRUPT_CNT = 0;

volatile uint8_t LED_SHOW_IDX = 0;

volatile bool BTN_DOWN = false;

volatile uint8_t LED_COUNT = 0;

uint8_t LEDS[12];

const uint8_t LED_SHOW_COUNT = 8;

const struct led_show LED_SHOWS[LED_SHOW_COUNT] PROGMEM = {
  {{0x7df1}, 1, 1},   // Add a secret mode here if you want to reward the player for getting at least 10 points
  {{0x0401, 0x0c01, 0x1801, 0x3001, 0x6001, 0x4001, 0x0002, 0x0011, 0x0031, 0x0061, 0x00c1, 0x0181, 0x0101, 0x0004, 0x8001, 0x0201, 0x0004}, 24, 17},
  {{0x4101, 0x6181, 0x71c1, 0x79e1, 0x7df8, 0x7df1, 0x3cf1, 0x1c71, 0x0c31, 0x0411, 0x0006, 0x8201, 0x0001, 0x8201, 0x0006}, 32, 15},
  {{0xd55f, 0x2aaf, 0xd55f, 0x2aaf, 0xd552, 0x2aa2, 0xd552, 0x2aa2, 0xd552, 0x2aa2, 0xd552, 0x2aa2}, 76, 12},
  {{0xd551, 0x0001, 0xd551, 0x0001, 0xd551, 0x0001, 0xd551, 0x0004, 0x2aa1, 0x0001, 0x2aa1, 0x0001, 0x2aa1, 0x0001, 0x2aa1, 0x0004}, 22, 16},
  {{0x4001, 0x0801, 0x0081, 0x0101, 0x1001, 0x0041, 0x0201, 0x0011, 0x2001, 0x0021, 0x8001, 0x0401}, 12, 12},
  {{0x4001, 0x4801, 0x4881, 0x4981, 0x5981, 0x59c1, 0x5bc1, 0x5bd1, 0x7bd1, 0x7bf1, 0xfbf1, 0xfff8, 0xf7f1, 0xf7d1, 0xf751, 0xb751, 0xb351, 0x3351, 0x3341, 0x2341, 0x0341, 0x0141, 0x0101, 0x0008}, 38, 24},
  {{0xfff1}, 1, 1}
};

void clear_leds() {
  // Assume LEDs are wired to physical pins PB0 -> PB3
  DDRB &= ~0x0f;
  PORTB &= ~0x0f;
}

void turn_on_led(uint8_t led) {
  clear_leds();
  DDRB |= LED_CONFIGS[led].pin_mask_out;
  PORTB |= LED_CONFIGS[led].port_mask_high;
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

void set_led_show(uint8_t show_idx) {
  uint16_t breaks_total = pgm_read_word(&LED_SHOWS[show_idx].breaks_total);
  uint8_t len = pgm_read_byte(&LED_SHOWS[show_idx].pattern_length);
  uint16_t curr_int_cnt = TIMER_INTERRUPT_CNT % breaks_total;
  
  for (uint8_t i = 0; i < len; i++) {
    uint16_t val = pgm_read_word(&LED_SHOWS[show_idx].patterns_and_breaks[i]);
    if (curr_int_cnt < (val & 0xf)) {
      set_led_pattern(val >> 4);
      return;
    }
    curr_int_cnt -= val & 0xf;
  }

  uint16_t last_val = pgm_read_word(&LED_SHOWS[show_idx].patterns_and_breaks[len - 1]);
  set_led_pattern(last_val >> 4);
}

void game_blink_pattern(uint16_t pattern, uint8_t cycles) {
  TIMER_INTERRUPT_CNT = 0;
  while (TIMER_INTERRUPT_CNT < cycles) {
    if (!((TIMER_INTERRUPT_CNT >> 1) & 1)) { 
      set_led_pattern(pattern);
    } else {
      set_led_pattern(0x000);
    }
    commit_leds();
    INTERRUPTS = 0;
  }
}

uint16_t next_rand_led() {
  uint16_t state = eeprom_read_word((uint16_t*) EEPROM_RAND_ADDR);
  // Linear congruential generator as PRNG
	state = (state * 28165 + 1133) & 0xffff;
  eeprom_write_word((uint16_t*) EEPROM_RAND_ADDR, state);
  uint16_t res = state % 12;
  return res != 5 && res != 11 ? res % 12 : res % 12 - 1;
}

ISR(PCINT0_vect) {
  if (!(PINB & (1 << BTN_PIN))) {
    BTN_DOWN = true;
  } else {
    if (BTN_DOWN) {
      LED_SHOW_IDX = (LED_SHOW_IDX + 1) % LED_SHOW_COUNT;
    }
    BTN_DOWN = false;
    INTERRUPTS |= 1 << I_BTN;
  }
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

  // Enable both interrupts
  INTERRUPT_MASK = (1 << I_TIMER) | (1 << I_BTN);

  // Check if button is pressed on boot
  if (PINB & (1 << PINB4)) {
    bool ten_points_flag = eeprom_read_byte((uint8_t*) EEPROM_FLAG_ADDR) == 0x1;
    uint8_t last_led_show_idx = 0xf;

    while (true) {
      if (last_led_show_idx != LED_SHOW_IDX) {
        if (!ten_points_flag && LED_SHOW_IDX == 0) {
          LED_SHOW_IDX = 1;
        }
        last_led_show_idx = LED_SHOW_IDX;
        // Reset timer interrupt counter to always start show from the beginning
        TIMER_INTERRUPT_CNT = 0;
      }
      set_led_show(LED_SHOW_IDX);

      commit_leds();
      INTERRUPTS = 0;
    }
  } else {
    uint16_t target_pos;
    uint16_t points = 0;
    bool lvl_won = false;
    bool game_lost = true;

    while (true) {
      // Beginning animation
      if (game_lost) {
        // Disable button interrupt while anomations running
        INTERRUPT_MASK &= ~(1 << I_BTN);

        for (uint8_t i = 0; i < 2; i++) {
          TIMER_INTERRUPT_CNT = 0;
          uint16_t end_pattern = 0;
          while (TIMER_INTERRUPT_CNT < 10) {
            if (TIMER_INTERRUPT_CNT < 5) {
              end_pattern |= (1 << TIMER_INTERRUPT_CNT) | (1 << (TIMER_INTERRUPT_CNT + 6));
            } else {
              end_pattern &= ~((1 << (TIMER_INTERRUPT_CNT - 5)) | (1 << (TIMER_INTERRUPT_CNT + 1)));
            }

            set_led_pattern(end_pattern);

            commit_leds();
            INTERRUPTS = 0;
          }
          _delay_ms(100);
        }
        _delay_ms(200);

        game_lost = false;
        INTERRUPT_MASK |= 1 << I_BTN;
      }

      target_pos = next_rand_led();

      TIMER_INTERRUPT_CNT = 0;

      // Game loop
      while (!lvl_won && !game_lost) {
        // If the player has more than 5 points, increase game speed
        uint16_t player_pos_raw = (TIMER_INTERRUPT_CNT / (points < 5 ? 3 : 2)) % 22;
        uint16_t player_pos;

        if (player_pos_raw <= 4) {
          player_pos = 4 - player_pos_raw;
        } else if (player_pos_raw == 5) {
          TIMER_INTERRUPT_CNT += 2;
          continue;
        } else if (player_pos_raw <= 10) {
          player_pos = 16 - player_pos_raw;
        } else if (player_pos_raw == 11) {
          TIMER_INTERRUPT_CNT += 4;
          continue;
        } 
        else if (player_pos_raw <= 16) {
          player_pos = player_pos_raw - 6;
        } 
        else if (player_pos_raw == 17) {
          TIMER_INTERRUPT_CNT += 2;
          continue;
        } else {
          player_pos = player_pos_raw - 18;
        }

        set_led_pattern((1 << player_pos) | (1 << target_pos));

        if (BTN_DOWN) {
          if (player_pos == target_pos) {
            lvl_won = true;
            points += 1;
          } else {
            game_lost = true;
          }
        }

        commit_leds();
        INTERRUPTS = 0;
      }

      // Blink target when hit
      TIMER_INTERRUPT_CNT = 0;
      if (lvl_won) {
        game_blink_pattern(1 << target_pos, 6);
      } else {
        // Disable button interrupt while anomations running
        INTERRUPT_MASK &= ~(1 << I_BTN);

        // Run end anomation before displaying points count
        game_blink_pattern(0x7df, 12);
        _delay_ms(500);

        // Set flag if user reached at least 10 points
        if (points >= 10) {
          eeprom_write_byte((uint8_t*) EEPROM_FLAG_ADDR, 0x1);
        }

        // Display points count using D6 and D12
        while (points) {
          if (points / 10) {
            game_blink_pattern(0x020, 4);
            points -= 10;
          } else {
            game_blink_pattern(0x800, 4);
            points -= 1;
          }
        }
        _delay_ms(1000);
        points = 0;

        INTERRUPT_MASK |= 1 << I_BTN;
      }

      lvl_won = false;
    }
  }

  return 0;
}
