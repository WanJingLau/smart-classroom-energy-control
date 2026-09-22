# Smart Classroom Energy Control

Starter firmware for a three-node I2C prototype that models energy-aware classroom HVAC, lighting, and curtains.

## Nodes

| Node | I2C address | Purpose |
| --- | ---: | --- |
| Main controller | I2C master | Reads the other nodes, applies control rules, updates the LCD and status LED. |
| Indoor conditions | `0x08` | Reads the LM35/potentiometer temperature input and PIR occupancy sensor. |
| Daylight and facade | `0x09` | Reads an LDR and drives the classroom-light LED and curtain servo. |

## Wiring assumptions

All Arduino boards share **GND**, and their I2C SDA/SCL pins are connected together (Uno/Nano: SDA=A4, SCL=A5). Keep the I2C wires short for this prototype.

- Indoor node: LM35 or potentiometer signal -> A0; PIR output -> D2.
- Daylight node: LDR divider output -> A0; light LED -> D6 (via resistor); servo signal -> D9.
- Main controller: status LED -> D13; 1602 I2C LCD -> same I2C bus (default address `0x27`).

## Control rules in this starter

- Occupied and hot: HVAC request ON and lights ON.
- Occupied and dark: lights ON.
- Occupied, hot, and bright: HVAC request ON, curtains close, and lights remain OFF unless the room is otherwise dark.
- Unoccupied: HVAC request OFF, lights OFF, curtains open.