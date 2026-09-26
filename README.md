# Three-Arduino Smart Building: Occupancy and Temperature Control

This package implements all three Arduino roles.

## Roles

- Arduino 1 (`0x08`): potentiometer temperature simulation and red/green LEDs.
- Arduino 2: the only I2C main controller; it collects information and makes decisions.
- Arduino 3 (`0x09`): PIR motion-sensor node.

## PIR wiring on Arduino 3

| PIR pin | Arduino 3 |
| --- | --- |
| VCC | 5V |
| OUT | D2 |
| GND | GND |

Check the labels on the specific PIR module because physical pin order varies.
For an HC-SR501, use retriggering/H mode when available.

## Shared I2C wiring

| Signal | Arduino 1 | Arduino 2 | Arduino 3 |
| --- | --- | --- | --- |
| SDA | A4 | A4 | A4 |
| SCL | A5 | A5 | A5 |
| Ground | GND | GND | GND |

Use one pair of I2C pull-ups for the whole bus: SDA to 5V and SCL to 5V,
normally 4.7 kOhm each. For short breadboard wiring, 10 kOhm can work.
If every Uno is powered by its own USB cable, do not join their 5V pins.

## Arduino 1 components

- Potentiometer centre pin to A0; outer pins to 5V and GND.
- Green LED anode to D8 through 220-330 ohm; cathode to GND.
- Red LED anode to D9 through 220-330 ohm; cathode to GND.

## I2C commands

| Value | Meaning |
| ---: | --- |
| 0 | Sleep/unoccupied |
| 1 | Occupied/comfortable |
| 2 | Heating; red LED on |
| 3 | Cooling; green LED on |

Arduino 1 stops reading the potentiometer and turns both LEDs off while asleep,
but continues listening to I2C. Arduino 2 keeps occupancy active for 10 seconds
after the most recent motion signal received from Arduino 3.

## Data flow

1. Arduino 2 requests one PIR status byte from Arduino 3.
2. Arduino 3 returns `0` for no motion or `1` for motion.
3. Arduino 2 maintains the 10-second occupancy timeout.
4. When occupied, Arduino 2 wakes Arduino 1 and requests its temperature.
5. Arduino 2 makes the heating, comfortable, or cooling decision.
6. Arduino 2 sends that command back to Arduino 1.
7. When the occupancy timeout expires, Arduino 2 sends the sleep command.

## Upload order

1. Open `Arduino1_Temperature_Node/Arduino1_Temperature_Node.ino` and upload it to Arduino 1.
2. Open `Arduino3_PIR_Node/Arduino3_PIR_Node.ino` and upload it to Arduino 3.
3. Open `Arduino2_Main_Controller/Arduino2_Main_Controller.ino` and upload it to Arduino 2.
4. Connect the shared I2C wiring with power removed, then power all boards.
5. Allow the PIR to stabilise after power-up.
6. Open the Serial Monitors at 9600 baud.

## Expected demonstration

1. With no occupancy, Arduino 1 reports sleep, stops sampling, and both LEDs remain off.
2. Move in front of the PIR. Arduino 3 reports motion to Arduino 2.
3. Arduino 2 wakes Arduino 1 and requests its temperature.
4. Potentiometer at 0-15 C: red LED on.
5. Potentiometer above 15 C and below 25 C: both LEDs off.
6. Potentiometer at 25-40 C: green LED on.
7. Stop moving. Ten seconds after the final motion signal, Arduino 2 sends sleep and both LEDs turn off.

## Host logic tests

From this package directory, run:

```sh
g++ -std=c++17 -Wall -Wextra -Werror tests/controller_logic_test.cpp -o tests/controller_logic_test
./tests/controller_logic_test
g++ -std=c++17 -Wall -Wextra -Werror tests/node_logic_test.cpp -o tests/node_logic_test
./tests/node_logic_test
g++ -std=c++17 -Wall -Wextra -Werror tests/pir_logic_test.cpp -o tests/pir_logic_test
./tests/pir_logic_test
```
