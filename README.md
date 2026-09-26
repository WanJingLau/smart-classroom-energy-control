[README.md](https://github.com/user-attachments/files/32687339/README.md)
# Smart classroom — three Arduino Uno R3 boards

This package implements the latest sensing/controller/services arrangement.
Use all three sketches from this package together. They replace the earlier
temperature-node/PIR-node arrangement and use a different message format.

## Which sketch goes on which board?

| Board | Open this sketch | Role | I2C address |
|---|---|---|---|
| Arduino 1 | `Arduino1_Sensing/Arduino1_Sensing.ino` | PIR, temperature potentiometer, LDR and people-count buttons | `0x08` |
| Arduino 2 | `Arduino2_Main_Controller/Arduino2_Main_Controller.ino` | All decisions, the 10-second timer and the 1602 LCD | Main controller; `Wire.begin()` |
| Arduino 3 | `Arduino3_Services/Arduino3_Services.ino` | Classroom-light LED, heating/cooling LEDs and blind servo | `0x09` |

Each sketch is a **single `.ino` file**. No `NodeLogic.h`, `ControlLogic.h`
or `PirLogic.h` is needed. The `tests` folder is only for optional computer-side
checks; do not copy it into a sketch folder or upload it to an Arduino.

## People count, motion and sleep

The `+` and `−` buttons simulate entry and exit. One stable press changes the
count once; holding a button does not keep counting. Counts stay between 0 and
999. Restarting Arduino 1 resets the count to zero.

The PIR reports movement separately. It does not increment the people count or
infer whether someone entered or left. A single PIR cannot reliably distinguish
entry, exit and someone moving inside the classroom. To demonstrate arrival,
press `+`. PIR activity alone does not wake temperature control in this version.

| Situation | Temperature sensing / HVAC | Classroom-light LED |
|---|---|---|
| Startup with zero people | Sleep | Off |
| Count becomes greater than zero | Wake, then control using a fresh temperature sample | On if dark |
| Count remains greater than zero, with or without PIR motion | Stays active | Follows daylight |
| Last person leaves: count changes to zero | Continue temperature control for 10 seconds | Off immediately |
| Someone arrives during those 10 seconds | Cancel the countdown and remain active | Follows daylight |
| Count stays zero for 10 seconds | Stop temperature sampling; heating/cooling LEDs off | Off |

The timer starts when Arduino 2 receives the transition to zero. Its 100 ms
polling cycle and button debounce add a small delay to physical button events.
Repeated zero readings do not restart the timer.

“Sleep” is a software state, **not hardware power-down**. Arduino 1 continues
reading the buttons, PIR and LDR and answering I2C requests. Only temperature
sampling stops. The controller and service board remain powered.

## Control rules

| Input condition | Output |
|---|---|
| HVAC active and temperature at or below 15.00 C | Red LED on; green LED off |
| HVAC active and temperature above 15.00 C but below 25.00 C | Both HVAC LEDs off: IDLE |
| HVAC active and temperature at or above 25.00 C | Green LED on; red LED off |
| HVAC asleep, or waiting for a valid temperature | Both HVAC LEDs off |
| People > 0 and LDR < 300 | Classroom-light LED on |
| People = 0 or LDR >= 300 | Classroom-light LED off |
| LDR >= 400 | Blind commanded to 90 degrees: closed |
| LDR < 400 | Blind commanded to 20 degrees: open |

The 10-second departure period is the exception to the usual occupied-only HVAC
rule: temperature control continues during that period. Blind control responds
to daylight even when the room is empty. On a sensor communication failure, the
controller keeps the previous blind command.

The LDR thresholds 300 and 400 are provisional values based on observed
readings of about 200–450; they are not calibrated illuminance. With the wiring
below, higher readings mean more light. For example, an occupied room with an
LDR reading of 200 turns on the classroom light and leaves the blind open;
a reading of 450 turns that light off and closes the blind. Adjust the two
constants in Arduino 2 after measuring covered and brightly lit readings on
your assembled circuit. The servo angles are demonstration starting values,
not universal mechanical positions. The LEDs
indicate commands, not confirmation that real HVAC equipment is working.

## Shared I2C wiring

For **5 V Arduino Uno R3 / ATmega328P boards**:

| Signal | Arduino 1 | Arduino 2 | Arduino 3 |
|---|---|---|---|
| SDA | A4 | A4 | A4 |
| SCL | A5 | A5 | A5 |
| Common ground | GND | GND | GND |

All A4 pins connect together. All A5 pins connect together. All grounds connect
together. Arduino 2 initiates every I2C transaction.

Use one pair of external pull-ups for this bare-board bus: SDA through 4.7 kohm
to Arduino 2's 5 V, and SCL through 4.7 kohm to the same 5 V. Keep breadboard wires
short and power all boards during operation. If using separate USB power for
the boards, leave their 5 V pins unconnected to one another. Only the grounds
and I2C signals are shared between the boards.

## Arduino 1: all sensing inputs

| Component connection | Arduino 1 connection |
|---|---|
| Typical 5 V PIR module VCC | 5 V |
| PIR module GND | GND |
| PIR OUT | D2 |
| `+1` button | Between D3 and GND |
| `−1` button | Between D4 and GND |
| Temperature potentiometer middle terminal | A0 |
| Temperature potentiometer outer terminals | 5 V and GND |
| LDR first leg | 5 V |
| LDR second leg | A1 |
| 10 kohm fixed resistor | Between the same A1 junction and GND |

Read the PIR's printed pin labels; physical pin order varies. The sketch assumes
a module whose OUT becomes HIGH on motion. Arduino 1 now reads that pin
immediately after startup, and Arduino 2 reports changes within its 100 ms I2C
poll interval. The module itself may still need around 30–60 seconds after
power-up to stabilise, during which motion readings may be unreliable.

If your **HC-SR501** stays HIGH for about 20 seconds after motion, turn its
**TIME/delay** trimmer counterclockwise in small steps toward its minimum; this
shortens the module's output pulse, typically to around 5 seconds on that
variant. Set its jumper to **L** to make the pulse end after the selected time;
**H** can extend HIGH while more motion retriggers the sensor. Adjust the
**sensitivity/range** trimmer separately if movement at your demonstration
distance is not picked up. These are module settings: software can report a
HIGH immediately but cannot detect a new rising edge while the module's OUT
pin remains HIGH. Other PIR module models may use different controls.

The buttons use `INPUT_PULLUP`, so an unpressed input is HIGH and a pressed input
is LOW. No external button resistors are needed. For a four-leg tactile button,
use a pair of contacts that connect only when pressed, not two permanently
connected legs.

Use approximately a 10 kohm potentiometer for temperature. It provides an input
voltage, which the code maps from ADC 0–1023 to simulated 0–40 C. It does not
measure actual room temperature. With the LDR divider connected as above,
brighter light produces a larger reading; covering the LDR produces a smaller
reading. The LDR's legs have no polarity.

## Arduino 2: parallel 1602 LCD

This wiring is for the **16-pin HD44780-compatible LCD without an I2C backpack**.
LCD pin names D4–D7 are display data pins; the table distinguishes them from
Arduino digital pins.

| LCD physical pin | LCD label | Connection |
|---:|---|---|
| 1 | VSS | Arduino 2 GND |
| 2 | VDD / VCC | Arduino 2 5 V |
| 3 | VO | Middle terminal of a separate 10 kohm contrast potentiometer |
| 4 | RS | Arduino 2 D12 |
| 5 | R/W | Arduino 2 GND |
| 6 | E / Enable | Arduino 2 D11 |
| 7–10 | D0–D3 | Leave unconnected; this sketch uses 4-bit mode |
| 11 | D4 | Arduino 2 D5 |
| 12 | D5 | Arduino 2 D4 |
| 13 | D6 | Arduino 2 D3 |
| 14 | D7 | Arduino 2 D2 |
| 15 | A / LED+ | Arduino 2 5 V through 220 ohm for a typical bare backlight; check module rating |
| 16 | K / LED− | Arduino 2 GND |

Connect the contrast potentiometer's two outer terminals to Arduino 2 5 V and
GND. This is separate from the temperature potentiometer on Arduino 1.
Turn the contrast control slowly if the backlight is on but no characters are
visible. If your existing LCD already works with VO connected to GND, that can
remain for this prototype; an adjustable contrast control is more flexible.

The constructor is `LiquidCrystal lcd(12, 11, 5, 4, 3, 2);`.
Using D2–D5 here does not conflict with inputs on Arduino 1: each board has its
own pins. A4 and A5 remain available for I2C.

LCD row 1 shows temperature and people count, for example `T:28.00C N:002`.
`--.--C` means the temperature is unavailable or sampling is asleep.
Row 2 shows status, for example `O COOL L1 B0`:

| Display | Meaning |
|---|---|
| O / U | Occupied / unoccupied, based on the people count |
| HEAT / COOL / IDLE / SLEEP | Requested HVAC state |
| L1 / L0 | Classroom-light command on / off |
| B1 / B0 | Blind command closed / open |
| WAIT:10s ... WAIT:1s | Empty-room countdown before temperature sleep |
| SENSOR I2C ERROR | A valid sensor packet could not be read |
| SENSOR CMD ERROR | Sampling command could not be delivered |
| SERVICE I2C ERR | Output command could not be delivered |

PIR and raw LDR readings are available in Serial Monitor at 9600 baud.
`PIR changed: 1` and `PIR changed: 0` print immediately when the level changes
on Arduino 1 or when Arduino 2 next polls Arduino 1. The one-second status
lines remain available for checking the current state.

## Arduino 3: output components

| Component | Connection |
|---|---|
| Classroom-light LED | D10 through 220–330 ohm to LED anode; cathode to GND |
| Green cooling LED | D9 through 220–330 ohm to LED anode; cathode to GND |
| Red heating LED | D8 through 220–330 ohm to LED anode; cathode to GND |
| Servo signal | D6 |
| Servo positive power | Suitable regulated servo supply, typically 5 V for a small hobby servo |
| Servo ground | Supply ground and Arduino 3 GND |

Use a separate resistor for each LED. The LEDs are switched with `digitalWrite`,
so the Servo library's use of a timer does not require PWM on D9 or D10.
Power the servo from a supply rated for its current, including stall current;
check its voltage rating. Keep the servo supply ground connected to the common
Arduino ground. With Arduino 3 powered by USB, do not connect the separate
servo supply positive to the Arduino 5 V pin.

Set `BLIND_OPEN_ANGLE` and `BLIND_CLOSED_ANGLE` on Arduino 2 for your mechanism.
Also match Arduino 3's `START_ANGLE` to the chosen open angle. Establish the
usable travel before attaching the blind linkage. The initial defaults are
20 degrees open and 90 degrees closed.

## Upload and demonstrate

1. Extract the whole ZIP to a normal folder first.
2. In Arduino IDE, select **Arduino Uno** and the correct port for each board.
   Use a current **Arduino AVR Boards** package (this version was compiled with
   1.8.8); the controller calls the AVR Wire timeout API directly.
3. Open the `.ino` inside its matching sketch folder. Upload Arduino 1 and
   Arduino 3, then upload Arduino 2. Keep the folder names matching the `.ino`
   names. Do not combine the three sketches into one Arduino IDE project.
4. `Wire` comes with Arduino AVR Boards. If needed, install **LiquidCrystal by
   Arduino** and **Servo by Arduino** through Library Manager. These are the
   standard libraries; custom header files are not required.
5. Connect and power the circuit. Open a board's Serial Monitor at **9600 baud**.
6. Initially N is zero and temperature sampling is asleep. Press `+` once on
   Arduino 1; temperature sampling wakes and the display becomes valid after
   the next sampling/poll cycle.
7. Turn the temperature potentiometer: low temperature lights red, middle
   temperature switches both HVAC LEDs off, high temperature lights green.
8. Cover/uncover the LDR. With people present, dark conditions switch on the
   classroom LED. Strong daylight closes the blind. Watch raw LDR values to
   choose thresholds suitable for your room.
9. Stop moving while N is above zero: temperature control must remain active.
   Press `+` again, then `−`; N remains above zero and no countdown starts.
10. Press `−` until N reaches zero. The classroom LED turns off immediately,
    the LCD counts down 10 seconds, then temperature sampling and HVAC LEDs
    sleep. Repeat and press `+` during the countdown to cancel it.

The ADC's finite steps may jump across exactly 15.00 or 25.00 C; the code uses
the unrounded hundredths-of-a-degree value for its decisions. The temperature
threshold tests exercise the exact boundaries separately.

## Communication and fault behavior

Arduino 2 polls Arduino 1 every 100 ms. It reads an eight-byte sensor packet,
sends Arduino 1 a one-byte sampling command, then sends Arduino 3 a four-byte
output command. Arduino 1 samples approximately every 50 ms when enabled.

| Packet | Contents |
|---|---|
| Arduino 1 reply, 8 bytes | Version 1; flags; count high/low; temperature high/low; LDR high/low |
| Flags | Bit 0: PIR movement; bit 1: valid active temperature sample |
| Temperature | Unsigned hundredths of a degree, 0–4000; `0xFFFF` when unavailable |
| Command to Arduino 1, 1 byte | 0 = stop temperature sampling; 1 = enable it |
| Command to Arduino 3, 4 bytes | Version 1; classroom light 0/1; HVAC 0/1/2/3; servo angle 0–180 |
| HVAC codes | 0 sleep; 1 idle/waiting; 2 heating; 3 cooling |

Lengths, version and valid value ranges are checked. The messages do not have
an application checksum. Successful I2C delivery does not verify that a physical
LED, servo or HVAC device acted on the command.

If sensor data cannot be read, Arduino 2 commands the service LEDs off and asks
Arduino 1 to stop temperature sampling. If Arduino 1 or 3 stops receiving valid
commands for 2.5 seconds, Arduino 1 stops temperature sampling and Arduino 3
turns all LEDs off, respectively. The servo holds its previous commanded angle.
Arduino 2 explicitly enables a 25 ms AVR Wire timeout so a stalled transaction
can return an error. Sensor communication failure also cancels any departure
countdown: on recovery, zero people stays asleep and a positive count wakes it.

To demonstrate a lost-controller timeout, leave all boards powered and hold
Arduino 2's RESET button for more than 2.5 seconds. Observe Arduino 1's Serial
output change to SLEEP and Arduino 3's LEDs turn off. Releasing RESET allows
control to resume from Arduino 1's current people count.

## Verification

See `VALIDATION.md` for the checks actually completed and their limits.
The optional computer-side checks run with Python 3 and g++:

```bash
python3 tests/run_tests.py
```

They exercise the actual sketches with hardware API substitutes, not physical
boards. No test headers are needed to upload the three `.ino` files.

## Official references

- [Arduino Uno R3 pinout](https://docs.arduino.cc/resources/pinouts/A000066-full-pinout.pdf)
- [Arduino LiquidCrystal HelloWorld wiring and constructor](https://github.com/arduino-libraries/LiquidCrystal/blob/master/examples/HelloWorld/HelloWorld.ino)
- [Arduino AVR Wire API](https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/Wire/src/Wire.h)
- [Arduino servo motor guidance](https://docs.arduino.cc/learn/electronics/servo-motors/)
- [SunFounder HC-SR501 controls and trigger modes](https://docs.sunfounder.com/projects/umsk/en/latest/01_components_basic/12-component_pir_motion.html)
