# Smart Classroom — Three Arduino Uno R3 Boards

An I²C classroom demonstrator using a potentiometer to simulate temperature, an LDR for daylight, buttons to simulate people entering and leaving, and a PIR for motion monitoring. LEDs represent lighting, heating and cooling; a servo represents a blind.

The potentiometer does not measure room temperature. LED states are requested operating modes, not evidence of actual heating, cooling or energy savings.

## 1. Board roles and sketch folders

| Board | Recommended sketch path | Role | I²C role/address |
|---|---|---|---|
| Arduino 1 | `Arduino1_Sensing/Arduino1_Sensing.ino` | PIR, potentiometer, LDR and count buttons | Peripheral `0x08` |
| Arduino 2 | `Arduino2_Controller/Arduino2_Controller.ino` | Control decisions and parallel 1602 LCD | Sole controller; `Wire.begin()` |
| Arduino 3 | `Arduino3_Services/Arduino3_Services.ino` | Three LEDs and blind servo | Peripheral `0x09` |

Rename each sketch and its enclosing folder together if the package uses different names. These paths are the recommended layout, not a claim that the supplied sketches have already been renamed. Upload each sketch separately to its assigned board. Do not combine the three sketches.

## 2. Final operating rules

### Occupancy and five-second HVAC shutdown

- The + button increases the people count; the − button decreases it. The allowed range is 0–999.
- A count above zero means occupied. A count of zero means unoccupied.
- PIR reports motion separately; it does not change the count or independently wake the services in this configuration.
- On startup with zero people, HVAC is in SLEEP, the classroom LED is off, and the blind is open.
- When the count changes from positive to zero, the classroom LED switches off and the blind opens. HVAC remains active for five seconds, continuing to follow the temperature rule, then enters SLEEP with both HVAC LEDs off.
- Adding a person during the five-second countdown cancels shutdown.
- Potentiometer and LDR sampling continue during the countdown and stop when HVAC enters SLEEP. PIR, buttons, I²C communication and the main loops continue running. All boards remain powered; this is software sampling suspension, not hardware power-down.

### Temperature and HVAC

Temperature uses the averaged potentiometer ADC count:

`temperatureCentiC = (ADC × 4000 + 511) / 1023`

The calculation uses unsigned integer arithmetic with a 32-bit intermediate. Divide the result by 100 for °C. This corresponds to rounding the simulated temperature to the nearest hundredth of a degree; it is not a statement of physical measurement accuracy.

| Condition while HVAC is active | Mode | Red LED | Green LED |
|---|---|---|---|
| Temperature <20.00 °C | HEAT | On | Off |
| 20.00–26.00 °C inclusive | IDLE | Off | Off |
| Temperature >26.00 °C | COOL | Off | On |
| Temperature unavailable | IDLE/waiting | Off | Off |
| HVAC asleep | SLEEP | Off | Off |

Arduino 2 must use strict comparisons `<2000` and `>2600`, and `HVAC_SLEEP_DELAY_MS = 5000`. An earlier sketch using `<=2000` and `>=2600` does not implement this table.

| ADC count | Calculated temperature | Expected active mode |
|---:|---:|---|
| 0 | 0.00 °C | HEAT |
| 511 | 19.98 °C | HEAT |
| 512 | 20.02 °C | IDLE |
| 665 | 26.00 °C | IDLE |
| 666 | 26.04 °C | COOL |
| 1023 | 40.00 °C | COOL |

These rows are calculated checks; they are not six claimed hardware observations. Fixed HVAC thresholds do not implement state-dependent hysteresis or a minimum equipment run time. The five-second departure delay is separate from threshold switching.

### Lighting and blind

Lower LDR counts mean darker conditions with the divider described below.

| Condition | Command |
|---|---|
| Occupied and LDR ≤300 | Classroom LED on |
| Occupied and LDR ≥400 | Classroom LED off |
| Occupied and LDR 301–399 | Retain previous classroom LED state |
| Occupied and LDR >500 | Blind closed, 90° |
| Occupied and LDR ≤500 | Blind open, 0° |
| Unoccupied | Classroom LED off; blind open |

Lighting starts off and resets to off when unoccupied or on a sensor communication fault. For example, while occupied: 650 → off, 380 → off, 300 → on, 350 → on, 400 → off. Entering occupancy at 380 after a reset leaves the LED off.

Calibrate 300/400 and 500 for the actual circuit and lighting. LDR counts are not lux. Blind control is independent of temperature and lighting hysteresis. At occupied LDR 400 and 28 °C, the classroom LED is off, the blind is open, and HVAC is COOL.

## 3. Shared I²C wiring and power

For 5 V Arduino Uno R3 / ATmega328P boards:

| Signal | Arduino 1 | Arduino 2 | Arduino 3 |
|---|---|---|---|
| SDA | A4 | A4 | A4 |
| SCL | A5 | A5 | A5 |
| Common ground | GND | GND | GND |

Connect all SDA pins together, all SCL pins together, and all grounds together. Arduino 2 initiates every transaction. Use short wires and a 100 kHz bus.

For a bare-board bus without suitable existing pull-ups, use one external pair: SDA through 4.7 kΩ to Arduino 2 5 V, and SCL through 4.7 kΩ to the same supply. Account for any pull-ups already fitted to connected modules.

Power all three boards during operation. With separate USB supplies, leave the boards’ 5 V pins unconnected to each other; share ground and I²C signals. Do not leave a board unpowered while other boards drive the shared bus. A USB connection is also used to upload a sketch and view that board’s Serial Monitor.

## 4. Arduino 1 — sensing wiring

| Component | Connection |
|---|---|
| Compatible 5 V PIR VCC | Arduino 1 5 V |
| PIR GND | Arduino 1 GND |
| PIR OUT | D2 |
| + button | Between D3 and GND |
| − button | Between D4 and GND |
| Temperature potentiometer wiper | A0 |
| Temperature potentiometer outer terminals | 5 V and GND |
| LDR first leg | 5 V |
| LDR second leg | A1 |
| 10 kΩ fixed resistor | A1 junction to GND |

Use approximately a 10 kΩ potentiometer. The LDR has no polarity. Buttons use `INPUT_PULLUP`: unpressed is HIGH, pressed is LOW. Debounce is 35 ms; holding a button should not repeatedly change the count. A four-leg tactile switch must use contacts that connect only when pressed.

Follow the PIR’s printed pin labels; physical order varies. Its output is assumed active HIGH. Allow its startup stabilisation period before evaluating motion. A PIR can hold HIGH after movement stops; repeated HIGH print lines are repeated samples of one pulse, not necessarily separate detections. Adjust its TIME control and H/L trigger mode according to the module documentation. Software cannot infer additional rising edges while OUT remains HIGH.

Active analogue sampling occurs approximately every 50 ms. The sketch discards the first ADC read after changing channels and averages four readings. On sleep, the previous stored readings may remain visible unless Serial output explicitly replaces them with `--` or `SLEEP`; stale stored values are not fresh samples. In this configuration, the sampling command gates both potentiometer and LDR acquisition.

## 5. Arduino 2 — parallel 1602 LCD

For a 16-pin HD44780-compatible LCD without an I²C backpack:

| Physical pin | LCD label | Connection |
|---:|---|---|
| 1 | VSS | GND |
| 2 | VDD | 5 V |
| 3 | VO | Wiper of a separate 10 kΩ contrast potentiometer |
| 4 | RS | Arduino 2 D12 |
| 5 | R/W | GND |
| 6 | E | Arduino 2 D11 |
| 7–10 | D0–D3 | Unconnected, 4-bit mode |
| 11 | D4 | Arduino 2 D5 |
| 12 | D5 | Arduino 2 D4 |
| 13 | D6 | Arduino 2 D3 |
| 14 | D7 | Arduino 2 D2 |
| 15 | A / LED+ | 5 V through a suitable backlight resistor; check the module rating |
| 16 | K / LED− | GND |

Connect the contrast potentiometer’s outer terminals to Arduino 2 5 V and GND. It is separate from Arduino 1’s temperature potentiometer. The constructor is:

```cpp
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
```

Digital pin numbers on different boards do not conflict. A4/A5 are reserved for I²C on each Uno.

The LCD refreshes approximately every 250 ms. Example:

```text
T:28.00C N:002
O COOL L0 B0
```

- T: simulated temperature; N: people count.
- O/U: occupied/unoccupied according to the count.
- HEAT/COOL/IDLE/SLEEP: HVAC command.
- L1/L0: classroom LED on/off.
- B1/B0: blind closed/open.

During the five-second departure countdown, U can appear together with HEAT, COOL or IDLE: the room is unoccupied but HVAC shutdown is pending. Temperature displays `--.--C` when unavailable. Row 2 can show `SENSOR I2C ERROR`, `SENSOR CMD ERROR` or `SERVICE I2C ERR`. On a sensor fault, the displayed count can be the last successfully received count.

Use the Serial Monitor at 9600 baud for PIR, LDR, commanded angle and communication status when those fields are included in the uploaded sketch. Displayed output states are commands, not physical feedback.

## 6. Arduino 3 — outputs

| Component | Connection |
|---|---|
| Classroom LED | D10 through its own 220–330 Ω resistor to anode; cathode to GND |
| Green cooling LED | D9 through its own 220–330 Ω resistor to anode; cathode to GND |
| Red heating LED | D8 through its own 220–330 Ω resistor to anode; cathode to GND |
| Servo signal | D6 |
| Servo positive supply | Regulated supply suitable for its voltage and stall current |
| Servo ground | Supply ground and Arduino 3/common GND |

With Arduino 3 on USB, do not connect a separate servo supply’s positive terminal to the Arduino 5 V pin. Share ground. LEDs use `digitalWrite`; they do not require PWM operation on D9/D10.

The documented blind positions are 0° open and 90° closed. Match Arduino 2’s angle constants and Arduino 3’s `START_ANGLE` to the actual mechanism. Set `START_ANGLE = 0` for these defaults; earlier versions used 20°. Check usable servo travel before attaching the linkage.

## 7. Upload and demonstration

1. Extract the package and ensure each sketch folder matches its `.ino` filename.
2. Select Arduino Uno and the correct port for each board in Arduino IDE.
3. Install the Arduino AVR Boards package and the standard LiquidCrystal and Servo libraries if needed. Wire comes with the board package. The controller requires a Wire version supporting `setWireTimeout()`; update the board package if that API is unavailable.
4. Confirm the uploaded sketches implement the rules in Section 2, including strict HVAC boundaries, count-based occupancy, a 5000 ms delay and gating both analogue inputs.
5. Upload Arduino 1 and Arduino 3, then Arduino 2. Power all boards and open the relevant Serial Monitor at 9600 baud.
6. At startup with N=0, verify SLEEP, all LEDs off, and blind open. PIR alone must not enable services.
7. Press + once. Verify active sensing and occupancy, including when PIR is LOW.
8. Test temperatures below 20 °C, inside 20–26 °C, and above 26 °C. Verify IDLE at 26.00 °C and use actual ADC values for boundary checks. An integer ADC count may not map to exactly 20.00 °C; distinguish display rounding from the centidegree value used for decisions.
9. Test LDR ≤300, 301–399 and ≥400 in both directions. Verify retained lighting state in the intermediate band.
10. While occupied, verify the blind opens at LDR ≤500 and closes at >500.
11. Reduce N from 1 to 0 while HEAT or COOL is active. Verify immediate classroom-light off/blind open, then HVAC SLEEP after approximately five seconds. Both analogue channels should then stop updating.
12. Repeat departure and press + before five seconds expire. Verify shutdown is cancelled.
13. Reset with N=0 and verify there is no startup departure countdown.

The five-second setting is a software target. Measure elapsed time if reporting shutdown timing accuracy; controller polling and output updates affect the observed timing.

## 8. Communication and fault behaviour

Arduino 2 runs a control cycle approximately every 100 ms. It reads Arduino 1, sends a sampling command, then sends Arduino 3 an output command.

| Message | Format |
|---|---|
| Sensor reply, 8 bytes | Version 1; flags; count high/low; temperature high/low; LDR high/low |
| Flags | Bit 0: PIR HIGH; bit 1: valid active temperature sample |
| Temperature | Unsigned centidegrees, 0–4000; `0xFFFF` when unavailable |
| Sampling command, 1 byte | 0: suspend both analogue channels; 1: enable both |
| Service command, 4 bytes | Version 1; classroom LED 0/1; HVAC code; angle 0–180 |
| HVAC code | 0 SLEEP; 1 IDLE/waiting; 2 HEAT; 3 COOL |

The packet layout is unchanged when the sampling command gates both analogue channels. There is no separate LDR-valid flag; ignore the retained LDR value when sampling is suspended. A newly enabled cycle can initially contain a previous LDR value until the next acquisition. Temperature validity indicates whether an active temperature sample is available.

Lengths, version and permitted values are checked. There is no application-level checksum. I²C acknowledgements and `monitorSystemComponents()` status do not verify physical LED or servo operation.

On sensor communication failure, Arduino 2 commands LEDs off, opens the blind, disables sampling and cancels the departure countdown. Communication faults bypass the normal five-second delay. On recovery, a positive people count enables operation; PIR alone does not.

After 2.5 seconds without valid commands, Arduino 1 disables both analogue channels and Arduino 3 turns all LEDs off. Arduino 3 holds its last commanded servo position on this timeout. This differs from a functioning Arduino 2 explicitly commanding the blind open after a sensor fault.

Arduino 2 uses `Wire.setWireTimeout(25000, true)` for a 25 ms transaction timeout. This bounds individual stalled transactions; it does not guarantee a fixed end-to-end response time under faults.

To check lost-controller behaviour, keep the boards powered and hold Arduino 2 RESET for more than 2.5 seconds. Verify sampling suspension and LEDs off; release RESET and verify recovery according to the current count.

## 9. Troubleshooting

### PIR remains HIGH

Check the module’s output delay, trigger mode, stable power and surroundings. Place it still, facing away from nearby people. With PIR OUT disconnected, connecting Arduino 1 D2 to GND should give PIR=0. Do not ground the connected sensor output. An unconnected INPUT can float, so an unplugged D2 reading is not a reliable sensor test.

### Temperature or daylight appears unchanged

Check whether sampling is asleep. Press + to set N above zero, verify communication and wait for a fresh sample. Check the potentiometer wiper on A0 and the LDR divider junction on A1. Different reachable ADC endpoints alone do not identify an electrical cause.

### Servo does not move

Set N above zero and cross LDR 500 in both directions. Arduino 2 should command 0° at ≤500 and 90° at >500. Check service communication, the D6 signal, servo power and common ground. Compare Arduino 3’s reported command if available. Correct reported angles do not confirm mechanical motion. Do not assume that a particular diagnostic message exists unless present in the uploaded sketch.

## 10. Reported observations and verification scope

The following are user-reported prototype observations from the project record, not tests independently repeated when preparing this README:

| Check | Reported observation |
|---|---|
| PotRaw, only Arduino 1 on USB | 0–1018 |
| PotRaw, only Arduino 2 on USB | 0–1022 |
| PotRaw, both Arduino 1 and Arduino 2 on USB | 0–1023 |
| Valid temperature samples | 5/5 valid |
| LCD comparison | Five readings matched the calculated value to displayed precision |
| HVAC decisions | 10/10 correct |
| Boundary holds | Two holds reported IDLE at 20 °C and 26 °C, with no unexpected changes |
| Heating and cooling LEDs together | 0 occurrences in five checks |
| I²C/missing-sample events | 0 events in five checks |

The reported 20 °C hold should be documented with the underlying centidegree/ADC value if an exact boundary claim is required; an integer ADC input does not map to exactly 20.00 °C with this conversion. The USB-range observations do not establish their cause, and USB connection descriptions alone do not specify how every board was powered.

The five-second shutdown is the selected final behaviour. No measured timing error or trial count is supplied for it. Earlier observations do not establish that every later firmware revision was retested. The final three sketches were not supplied with the README review, so their compilation and implementation of these rules still require verification. Keep the report, README and uploaded code consistent before demonstration or submission.

## 11. Official documentation

- [Arduino Uno R3 pinout](https://docs.arduino.cc/resources/pinouts/A000066-full-pinout.pdf)
- [LiquidCrystal HelloWorld example](https://github.com/arduino-libraries/LiquidCrystal/blob/master/examples/HelloWorld/HelloWorld.ino)
- [Arduino AVR Wire API](https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/Wire/src/Wire.h)
- [Arduino servo guidance](https://docs.arduino.cc/learn/electronics/servo-motors/)
- [SunFounder HC-SR501 controls](https://docs.sunfounder.com/projects/umsk/en/latest/01_components_basic/12-component_pir_motion.html)
