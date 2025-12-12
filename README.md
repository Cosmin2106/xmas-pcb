# ATtiny-powered Christmas Tree

The goal of this project was to create a Christmas decoration that houses a secret puzzle.
The implementation is based around the Microchip ATtiny25V at 1 MHz, which offers 5 GPIO pins with pin change interrupts.
The board is driven by a single CR2032 battery and features a button for user input.

The MCU can be either programmed using the exposed connectors of the "tree trunk", or with an SOIC-8 clip.
In my case, I opted for the latter.
Details regarding the programmer can be found in [this repository](https://github.com/Cosmin2106/arduinoisp-attiny-8-pin-pcb).

## Modes of Operation

When we turn on the board, we can perform a quick check to see if the button is already being held down or not.
If we encounter a normal power cycle, we enter **christmas decoration mode**.
The button is now dedicated to cycling through different prebuilt light shows.
If the button is being pressed at boot, we enter **game mode** instead.
The game idea is inspired by [Pop The Lock](https://apps.apple.com/us/app/pop-the-lock/id979100082) and [Stop Me](https://github.com/SolderedElectronics/-Stop-me-game-solder-kit).
To optimize flash usage, we *emulate randomness* by using a linear congruential generator as PRNG.
At the end of each game, the board displays the total number of points using the "branch" LEDs: one left blink means +10, one right blink, +1.
Using EEPROM flags, we can remember if the user surpassed a hard-coded points threshold in order to unlock secret light shows in the other mode.

## BOM

Designator | Footprint | Quantity | Details
---------- | --------- | -------- | -------
BT1 | Renata_HU2032LF | 1 | Renata 701106 holder for CR2032
C1 | 402 | 1 | Ceramic 100 nF
C2 | 402 | 1 | Ceramic 10 uF
D1, D11, D3, D5, D7, D9 | 603 | 6 | Osram LO L29K-J2L1-24-0-2-R18-Z
D10, D12, D2, D4, D6, D8 | 603 | 6 | Osram LY L29K-H1K2-26-0-2-R18-Z
R1, R2, R3, R4 | 402 | 4 | 150 Ω
SW1 | SW_Push_1P1T_NO_CK_KMR2 | 1 | C&K KMR421NGLFS
SW3 | SW_DPDT_CK_JS202011JCQN | 1 | C&K JS202011JCQN
U1 | SOIC-8_5.3x5.3mm_P1.27mm | 1 | ATtiny25V-10SU

## Photos

<img src="docs/front.jpg" alt="Front Side of Assembled PCB">

<img src="docs/back.jpg" alt="Back Side of Assembled PCB">

#### Image Licenses:

- <a href="https://www.flaticon.com/free-icons/ball" title="ball icons">Ball icons created by Good Ware - Flaticon</a>
- <a href="https://www.flaticon.com/free-icons/snowflake" title="snowflake icons">Snowflake icons created by Good Ware - Flaticon</a>
- <a href="https://www.flaticon.com/free-icons/snowflake" title="snowflake icons">Snowflake icons created by kmg design - Flaticon</a>

#### Improvements?

If you have ideas for improvements, please don't hesitate to reach out!
Since this is my first custom PCB project, advice would be highly appreciated! :D
