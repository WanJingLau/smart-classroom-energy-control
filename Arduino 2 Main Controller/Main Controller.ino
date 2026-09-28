// Upload to ARDUINO 2: the only I2C main controller, with parallel 1602 LCD.
#include <Wire.h>
#include <LiquidCrystal.h>
#include <stdio.h>

const uint8_t SENSOR_ADDRESS = 0x08;
const uint8_t SERVICE_ADDRESS = 0x09;
const uint8_t SENSOR_FRAME_SIZE = 8;
const uint32_t CONTROL_INTERVAL_MS = 100;
const uint32_t EMPTY_ROOM_DELAY_MS = 10000;
const uint16_t HEAT_AT_OR_BELOW = 1500; // 15.00 C, in hundredths of a degree.
const uint16_t COOL_AT_OR_ABOVE = 2500; // 25.00 C.
const uint16_t LIGHT_THRESHOLD = 300;  // Observed LDR range ~200 dark to ~450 bright.
const uint16_t HIGH_DAYLIGHT_THRESHOLD = 400; // Calibrate against your own light.
const uint8_t BLIND_OPEN_ANGLE = 20;   // Calibrate for the model's linkage.
const uint8_t BLIND_CLOSED_ANGLE = 90;

// LCD RS, E, D4, D5, D6, D7. LCD R/W goes to GND.
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

uint16_t sensorPeople = 0;
uint16_t sensorTemp = 0;
uint16_t sensorDaylight = 0;
bool sensorMotion = false;
bool lastReportedMotion = false;
bool sensorTempValid = false;
bool previousOccupied = false;
bool gracePending = false;
bool hvacActive = false;
uint32_t emptySince = 0;
uint32_t lastControl = 0;
uint32_t lastDisplay = 0;
uint32_t lastPrint = 0;
bool sensorLinkOk = false;
bool samplingLinkOk = false;
bool serviceLinkOk = false;
uint8_t selectedHvac = 0;
uint8_t selectedLight = 0;
uint8_t selectedBlind = BLIND_OPEN_ANGLE;

bool updateHvacActive(bool occupied, uint32_t now) {
  if (occupied) {
    hvacActive = true;
    gracePending = false;
  } else {
    if (previousOccupied) {
      emptySince = now;
      gracePending = true;
    }
    hvacActive = gracePending &&
                 uint32_t(now - emptySince) < EMPTY_ROOM_DELAY_MS;
    if (!hvacActive) gracePending = false;
  }
  previousOccupied = occupied;
  return hvacActive;
}

uint8_t chooseHvac(bool active, bool validTemperature, uint16_t centiC) {
  if (!active) return 0;             // SLEEP: LEDs off.
  if (!validTemperature) return 1;   // Wait for a fresh sample; LEDs off.
  if (centiC <= HEAT_AT_OR_BELOW) return 2;
  if (centiC >= COOL_AT_OR_ABOVE) return 3;
  return 1;                         // IDLE: comfortable; LEDs off.
}

bool chooseLight(bool occupied, uint16_t lightReading) {
  return occupied && lightReading < LIGHT_THRESHOLD;
}

uint8_t chooseBlind(uint16_t lightReading) {
  return lightReading >= HIGH_DAYLIGHT_THRESHOLD ?
         BLIND_CLOSED_ANGLE : BLIND_OPEN_ANGLE;
}

bool decodeSensors(const uint8_t* p, uint8_t length) {
  if (length != SENSOR_FRAME_SIZE || p[0] != 1 || (p[1] & 0xFC) != 0) return false;
  const uint16_t count = (uint16_t(p[2]) << 8) | p[3];
  const uint16_t temperature = (uint16_t(p[4]) << 8) | p[5];
  const uint16_t light = (uint16_t(p[6]) << 8) | p[7];
  const bool valid = (p[1] & 2) != 0;
  if (count > 999 || light > 1023 || (valid && temperature > 4000) ||
      (!valid && temperature != 0xFFFF)) return false;
  sensorPeople = count;
  sensorMotion = (p[1] & 1) != 0;
  sensorTemp = temperature;
  sensorTempValid = valid;
  sensorDaylight = light;
  return true;
}

bool requestSensors() {
  const uint8_t received = Wire.requestFrom(SENSOR_ADDRESS, SENSOR_FRAME_SIZE);
  uint8_t p[SENSOR_FRAME_SIZE];
  uint8_t n = 0;
  while (Wire.available()) {
    const uint8_t value = uint8_t(Wire.read());
    if (n < SENSOR_FRAME_SIZE) p[n++] = value;
  }
  return received == SENSOR_FRAME_SIZE && decodeSensors(p, n);
}

bool sendSamplingCommand(bool enabled) {
  Wire.beginTransmission(SENSOR_ADDRESS);
  Wire.write(uint8_t(enabled ? 1 : 0));
  return Wire.endTransmission() == 0;
}

bool sendServiceCommands(uint8_t light, uint8_t hvac, uint8_t blind) {
  const uint8_t command[] = {1, light, hvac, blind};
  Wire.beginTransmission(SERVICE_ADDRESS);
  Wire.write(command, sizeof(command));
  return Wire.endTransmission() == 0;
}

const char* hvacName(uint8_t code) {
  if (code == 2) return "HEAT";
  if (code == 3) return "COOL";
  if (code == 1) return "IDLE";
  return "SLEEP";
}

void printLcdRow(uint8_t row, const char* text) {
  lcd.setCursor(0, row);
  uint8_t i = 0;
  while (text[i] && i < 16) lcd.write(uint8_t(text[i++]));
  while (i++ < 16) lcd.write(uint8_t(' '));
}

void refreshLcd(uint32_t now) {
  // Format with room for every uint16_t value; printLcdRow limits output to 16.
  char line[24];
  if (sensorLinkOk && sensorTempValid) {
    snprintf(line, sizeof(line), "T:%2u.%02uC N:%03u", unsigned(sensorTemp / 100),
             unsigned(sensorTemp % 100), unsigned(sensorPeople));
  } else {
    snprintf(line, sizeof(line), "T:--.--C N:%03u", unsigned(sensorPeople));
  }
  printLcdRow(0, line);
  if (!sensorLinkOk) {
    printLcdRow(1, "SENSOR I2C ERROR");
  } else if (!samplingLinkOk) {
    printLcdRow(1, "SENSOR CMD ERROR");
  } else if (!serviceLinkOk) {
    printLcdRow(1, "SERVICE I2C ERR");
  } else if (gracePending && hvacActive) {
    const uint32_t elapsed = uint32_t(now - emptySince);
    const uint16_t seconds = elapsed >= EMPTY_ROOM_DELAY_MS ? 0 :
                            uint16_t((EMPTY_ROOM_DELAY_MS - elapsed + 999) / 1000);
    snprintf(line, sizeof(line), "WAIT:%2us L%u B%u", unsigned(seconds),
             unsigned(selectedLight), unsigned(selectedBlind == BLIND_CLOSED_ANGLE));
    printLcdRow(1, line);
  } else {
    snprintf(line, sizeof(line), "%c %s L%u B%u", sensorPeople > 0 ? 'O' : 'U',
             hvacName(selectedHvac), unsigned(selectedLight),
             unsigned(selectedBlind == BLIND_CLOSED_ANGLE));
    printLcdRow(1, line);
  }
}

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  printLcdRow(0, "Smart classroom");
  printLcdRow(1, "Starting...");
  Wire.begin();  // Sole I2C controller: no peripheral address here.
  Wire.setClock(100000);
  // Uno R3: use the current Arduino AVR Boards package (tested with 1.8.8).
  Wire.setWireTimeout(25000, true); // 25 ms timeout, reset the I2C hardware.
  lastControl = uint32_t(millis()) - CONTROL_INTERVAL_MS;
  Serial.println(F("Arduino 2: sensing=0x08 services=0x09"));
}

void loop() {
  const uint32_t now = millis();
  if (uint32_t(now - lastControl) >= CONTROL_INTERVAL_MS) {
    lastControl = now;
    sensorLinkOk = requestSensors();
    if (sensorLinkOk) {
      if (sensorMotion != lastReportedMotion) {
        lastReportedMotion = sensorMotion;
      }
      const bool occupied = sensorPeople > 0;
      updateHvacActive(occupied, now);
      samplingLinkOk = sendSamplingCommand(hvacActive);
      selectedHvac = chooseHvac(hvacActive, sensorTempValid && samplingLinkOk, sensorTemp);
      selectedLight = chooseLight(occupied, sensorDaylight) ? 1 : 0;
      // High daylight controls the blind even in an empty room.
      selectedBlind = chooseBlind(sensorDaylight);
    } else {
      // A sensor fault ends the grace period. Recovery with zero people must
      // stay asleep; a confirmed positive count can wake the system again.
      previousOccupied = false;
      gracePending = false;
      hvacActive = false;
      samplingLinkOk = sendSamplingCommand(false);
      selectedLight = 0;
      selectedHvac = 0;
      // No valid daylight reading: hold the last blind command.
    }
    serviceLinkOk = sendServiceCommands(selectedLight, selectedHvac, selectedBlind);
  }
  if (uint32_t(now - lastDisplay) >= 250) {
    lastDisplay = now;
    refreshLcd(now);
  }
  if (uint32_t(now - lastPrint) >= 1000) {
    lastPrint = now;
    Serial.print(F("People=")); Serial.print(sensorPeople);
    Serial.print(F(" PIR=")); Serial.print(sensorMotion);
    Serial.print(F(" LDR=")); Serial.print(sensorDaylight);
    Serial.print(F(" Temp="));
    if (sensorLinkOk && sensorTempValid) Serial.print(sensorTemp / 100.0, 2);
    else Serial.print(F("--"));
    Serial.print(F(" HVAC=")); Serial.print(hvacName(selectedHvac));
    Serial.print(F(" Light=")); Serial.print(selectedLight);
    Serial.print(F(" Blind=")); Serial.print(selectedBlind);
    Serial.print(F(" Sensor/Service=")); Serial.print(sensorLinkOk);
    Serial.print('/'); Serial.println(serviceLinkOk);
  }
}
