[README.md](https://github.com/user-attachments/files/32687339/README.md)
# Smart classroom — three Arduino Uno R3 boards

This package implements the latest sensing/controller/services arrangement.
Use all three sketches from this package together. They replace the earlier
temperature-node/PIR-node arrangement and use a different message format.

## Which sketch goes on which board?

| Board | Open this sketch | Role | I2C address |
|---|---|---|---|
| Arduino 1 | `Arduino 1 Sensing/Sensing.ino` | PIR, temperature potentiometer, LDR and people-count buttons | `0x08` |
| Arduino 2 | `Arduino 2 Main Controller/Main Controller.ino` | All decisions and the 1602 LCD | Main controller; `Wire.begin()` |
| Arduino 3 | `Arduino 3 Services/services.ino` | Classroom-light LED, heating/cooling LEDs and blind servo | `0x09` |

Each sketch is a **single `.ino` file**. No `NodeLogic.h`, `ControlLogic.h`
or `PirLogic.h` is needed. The `tests` folder is only for optional computer-side
checks; do not copy it into a sketch folder or upload it to an Arduino.

## Occupancy and control rules

The room is occupied when the people count is above zero OR PIR is HIGH.
A positive count keeps lighting, HVAC and blind control active even when PIR
is LOW. It becomes unoccupied only when the count is zero AND PIR is LOW,
on the next controller poll (every 100 ms), with no software departure delay.
When unoccupied, all LEDs turn off, temperature sampling stops, and the blind
opens. The count buttons update N (0-999); O/U shows this combined occupancy
condition. The PIR module may hold HIGH for its own configured duration.

| Condition | Output |
|---|---|
| Occupied, temperature <=20 C | Heating on |
| Occupied, temperature >=26 C | Cooling on |
| Occupied, temperature strictly between 20 and 26 C | HVAC indicators off |
| Unoccupied or temperature unavailable | HVAC indicators off |
| Occupied, LDR <=300 | Classroom LED on |
| Occupied, LDR >=400 | Classroom LED off |
| Occupied, LDR 301-399 | Keep previous classroom LED state |
| Unoccupied | Classroom LED off and blind open (0 degrees) |
| Occupied and LDR >500 | Blind closed (90 degrees) |
| Otherwise | Blind open |

These use the final 20/26 C HVAC rules, rather than the alternative thresholds.
Temperature is simulated with a potentiometer. LDR thresholds are raw ADC counts
and need calibration. Sleep stops temperature sampling only; all boards remain
powered and PIR, buttons and daylight continue to be read.

## Lighting hysteresis and calibration

Arduino 2 stores the LED state in the global `selectedLight` variable, initially
zero (off). Each control cycle updates it with `chooseLight()` before sending
it to Arduino 3 and displaying the status. At or below `LIGHT_ON_THRESHOLD`
(300), the occupied-room LED turns on. At or above `LIGHT_OFF_THRESHOLD` (400),
it turns off. Between these thresholds it retains its previous state.
Unoccupied status or a sensor communication fault resets the state to off.
Occupancy uses people count >0 OR PIR HIGH, without an added software delay.

For example, while occupied: 650 -> off, 380 -> off, 300 -> on, 350 -> on,
399 -> on, 400 -> off. Re-entering occupancy at 380 after the room was
unoccupied leaves the LED off until the reading reaches 300 or below.

Record about ten readings each with the LDR uncovered, partially covered and
fully covered, using Serial Monitor at 9600 baud. Choose separated ON/OFF
thresholds from those measurements; 300/400 are starting values, not calibrated
results. The current wiring makes lower ADC readings darker. Slowly cover and
uncover the LDR and record the actual switching readings for the Results section.
Test both directions through 301-399 to confirm state retention, and check
that the LED turns off when the count is zero and PIR is LOW, regardless of daylight.
The blind closes only when occupied and LDR >500 ADC; at 500 it stays open.

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

The LCD uses one fixed page, refreshed every 250 ms:

```text
T:28.00C N:002
O COOL L0 B0
```

T is temperature; N is people count. O/U means occupied/unoccupied using the
combined count/PIR rule. HEAT, COOL, IDLE or SLEEP shows HVAC status. L1/L0
means classroom LED on/off; B1/B0 means blind closed/open (90/0 degrees).
Temperature shows --.--C when unavailable. Communication faults replace row 2
with SENSOR I2C ERROR, SENSOR CMD ERROR or SERVICE I2C ERR. During a sensor
fault, N is the last received count.

PIR, raw LDR, blind angle, and link diagnostics remain in Arduino 2's
Serial Monitor at 9600 baud. Arduino 3's local servo-change and command-timeout
messages remain in that board's Serial Monitor. Output states are commands,
not physical feedback.

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
0 degrees open and 90 degrees closed.

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
6. With zero people and no motion, verify that all LEDs are off and the blind is open.
7. Trigger PIR motion or add a person to wake temperature sampling.
8. Test temperature <20 C, between 20 and 26 C, and >26 C for HVAC states.
9. Test LDR <=300, at 301-399, and >=400 for lighting hysteresis.
10. While occupied, verify the blind opens at LDR <=500 and closes at LDR >500.
11. With the count at zero and PIR LOW, verify on the next cycle that all LEDs turn
    off, temperature sampling stops and the blind opens.
12. Set N above zero and let PIR go LOW: all controls must remain active,
    following their temperature and daylight rules. Remove the last person
    while PIR is LOW to verify immediate unoccupied behavior.

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

Arduino 2's monitorSystemComponents() summarizes valid sensing data and
acknowledged sampling/service commands using the existing Sensor/Service field.
It does not verify individual physical components.

If sensor data cannot be read, Arduino 2 commands the service LEDs off, opens the blind, and asks
Arduino 1 to stop temperature sampling. If Arduino 1 or 3 stops receiving valid
commands for 2.5 seconds, Arduino 1 stops temperature sampling and Arduino 3
turns all LEDs off, respectively. The servo holds its previous commanded angle.
Arduino 2 explicitly enables a 25 ms AVR Wire timeout so a stalled transaction
can return an error. Sensor communication failure also clears occupancy: on recovery, a positive count or PIR motion wakes the system; otherwise it stays asleep.

To demonstrate a lost-controller timeout, leave all boards powered and hold
Arduino 2's RESET button for more than 2.5 seconds. Observe Arduino 1's Serial
output change to SLEEP and Arduino 3's LEDs turn off. Releasing RESET allows
control to resume from Arduino 1's current count and PIR reading.

## Verification

The updated rules require Arduino compilation and physical circuit verification.
No hardware test results are claimed here.

## Official references

- [Arduino Uno R3 pinout](https://docs.arduino.cc/resources/pinouts/A000066-full-pinout.pdf)
- [Arduino LiquidCrystal HelloWorld wiring and constructor](https://github.com/arduino-libraries/LiquidCrystal/blob/master/examples/HelloWorld/HelloWorld.ino)
- [Arduino AVR Wire API](https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/Wire/src/Wire.h)
- [Arduino servo motor guidance](https://docs.arduino.cc/learn/electronics/servo-motors/)
- [SunFounder HC-SR501 controls and trigger modes](https://docs.sunfounder.com/projects/umsk/en/latest/01_components_basic/12-component_pir_motion.html)

## Checking a stationary blind servo

Lighting hysteresis (300/400 ADC) is independent of blind control (LDR >500
while occupied). Temperature does not control the blind. At occupied LDR 400 and temperature 28 C, for example,
the LED is off but the blind stays open at 0 degrees.

Upload the current controller and services sketches to Arduino 2 and Arduino 3.
At 9600 baud, Arduino 2 reports Blind, Angle and Sensor/Service.
Arduino 3 reports Blind and Angle and prints Servo command D6 whenever the target changes.
Blind is the indicator (0=open, 1=closed); Angle is the commanded servo angle,
not a measured position.

For a controlled check, set people count above zero. At LDR <=500, expect
Blind=0 Angle=0. Raise LDR above 500: expect Blind=1 Angle=90. Lower it to 500 or below:
expect Blind=0 Angle=0. With zero people and PIR LOW, expect Blind=0 Angle=0 regardless
of daylight. Compare both boards.
If Arduino 2 changes but Arduino 3 does not, check for SERVICE I2C ERR or
COMMAND TIMEOUT and verify the uploaded sketches and I2C connections.
If both report the expected angles but there is no movement, inspect the servo
signal connection to Arduino 3 D6, its supply and common ground using the wiring
section above. Software does not confirm physical movement.
