// Upload to ARDUINO 2: the only I2C main controller, with parallel 1602 LCD.
#include <Wire.h>
#include <LiquidCrystal.h>
#include <stdio.h>

const uint8_t SENSOR_ADDRESS = 0x08;
const uint8_t SERVICE_ADDRESS = 0x09;
const uint8_t SENSOR_FRAME_SIZE = 8;
const uint32_t CONTROL_INTERVAL_MS = 100;
const uint16_t HEAT_AT_OR_BELOW = 2000; // 20.00 C, in hundredths of a degree.
const uint16_t COOL_AT_OR_ABOVE = 2600; // 26.00 C.
// Lower ADC means darker. Calibrate these thresholds using measured LDR values.
constexpr uint16_t LIGHT_ON_THRESHOLD = 300;
constexpr uint16_t LIGHT_OFF_THRESHOLD = 400;
static_assert(LIGHT_ON_THRESHOLD < LIGHT_OFF_THRESHOLD,
              "Lighting hysteresis requires ON < OFF");
const uint16_t HIGH_DAYLIGHT_THRESHOLD = 500;
const uint8_t BLIND_OPEN_ANGLE = 0;   // Calibrate for the model's linkage.
const uint8_t BLIND_CLOSED_ANGLE = 90;

// LCD RS, E, D4, D5, D6, D7. LCD R/W goes to GND.
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

uint16_t sensorPeople = 0;
uint16_t sensorTemp = 0;
uint16_t sensorDaylight = 0;
bool sensorMotion = false;
bool lastReportedMotion = false;
bool sensorTempValid = false;
bool roomOccupied = false;
bool hvacActive = false;
uint32_t lastControl = 0;
uint32_t lastDisplay = 0;
uint32_t lastPrint = 0;
bool sensorLinkOk = false;
bool samplingLinkOk = false;
bool serviceLinkOk = false;
uint8_t selectedHvac = 0;
uint8_t selectedLight = 0; // Persistent LED state; starts off and survives loop().
uint8_t selectedBlind = BLIND_OPEN_ANGLE;

bool sensingAvailable = false;
bool servicesAvailable = false;

// Monitor existing communication results without extra traffic or output.
// This checks board communication, not physical component connections.
void monitorSystemComponents() {
  sensingAvailable = sensorLinkOk && samplingLinkOk;
  servicesAvailable = serviceLinkOk;
}
uint8_t chooseHvac(bool active, bool validTemperature, uint16_t centiC) {
  if (!active) return 0;             // SLEEP: LEDs off.
  if (!validTemperature) return 1;   // Wait for a fresh sample; LEDs off.
  if (centiC <= HEAT_AT_OR_BELOW) return 2;
  if (centiC >= COOL_AT_OR_ABOVE) return 3;
  return 1;                         // IDLE: comfortable; LEDs off.
}

bool chooseLight(bool occupied, uint16_t lightReading) {
  if (!occupied) return false;
  if (lightReading <= LIGHT_ON_THRESHOLD) return true;
  if (lightReading >= LIGHT_OFF_THRESHOLD) return false;
  return selectedLight != 0; // Preserve state in the 301..399 ADC band.
}

uint8_t chooseBlind(bool occupied, uint16_t lightReading) {
  return occupied && lightReading > HIGH_DAYLIGHT_THRESHOLD ?
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

void refreshLcd() {
  char line[24];
  if (sensorLinkOk && sensorTempValid) {
    snprintf(line, sizeof(line), "T:%2u.%02uC N:%03u", unsigned(sensorTemp / 100),
             unsigned(sensorTemp % 100), unsigned(sensorPeople));
  } else {
    snprintf(line, sizeof(line), "T:--.--C N:%03u", unsigned(sensorPeople));
  }
  printLcdRow(0, line);
  // Show communication faults; detailed readings remain in Serial Monitor.
  if (!sensorLinkOk) printLcdRow(1, "SENSOR I2C ERROR");
  else if (!samplingLinkOk) printLcdRow(1, "SENSOR CMD ERROR");
  else if (!serviceLinkOk) printLcdRow(1, "SERVICE I2C ERR");
  else {
    snprintf(line, sizeof(line), "%c %s L%u B%u", roomOccupied ? 'O' : 'U',
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
  Serial.println();
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
      // A positive count keeps the room active even without PIR motion.
      roomOccupied = (sensorPeople > 0) || sensorMotion;
      hvacActive = roomOccupied;
      const bool occupied = roomOccupied;
      samplingLinkOk = sendSamplingCommand(hvacActive);
      selectedHvac = chooseHvac(hvacActive, sensorTempValid && samplingLinkOk, sensorTemp);
      selectedLight = chooseLight(occupied, sensorDaylight) ? 1 : 0;
      // Close only when occupied and daylight is strictly above 500 ADC.
      selectedBlind = chooseBlind(occupied, sensorDaylight);
    } else {
      // A sensor fault clears occupancy; valid count or motion can wake it again.
      roomOccupied = false;
      hvacActive = false;
      samplingLinkOk = sendSamplingCommand(false);
      selectedLight = 0;
      selectedHvac = 0;
      selectedBlind = BLIND_OPEN_ANGLE;
    }
    serviceLinkOk = sendServiceCommands(selectedLight, selectedHvac, selectedBlind);
    monitorSystemComponents();
  }
  if (uint32_t(now - lastDisplay) >= 250) {
    lastDisplay = now;
    refreshLcd();
  }
  if (uint32_t(now - lastPrint) >= 1000) {
    lastPrint = now;
    Serial.print(F("People=")); Serial.print(sensorPeople);
    Serial.print(F(" PIR=")); Serial.print(sensorMotion);
    Serial.print(F(" LDR=")); Serial.print(sensorDaylight);
    Serial.print(F(" Light=")); Serial.print(selectedLight ? F("ON") : F("OFF"));
    Serial.print(F(" Temp="));
    if (sensorLinkOk && sensorTempValid) Serial.print(sensorTemp / 100.0, 2);
    else Serial.print(F("--"));
    Serial.print(F(" HVAC=")); Serial.print(hvacName(selectedHvac));
    Serial.print(F(" Blind=")); Serial.print(selectedBlind == BLIND_CLOSED_ANGLE ? 1 : 0);
    Serial.print(F(" Angle=")); Serial.print(selectedBlind);
    Serial.print(F(" Sensor/Service=")); Serial.print(sensingAvailable);
    Serial.print('/'); Serial.println(servicesAvailable);
  }
}
