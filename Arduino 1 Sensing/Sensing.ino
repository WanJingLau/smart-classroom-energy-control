// Upload to ARDUINO 1: all sensing inputs. Arduino Uno R3 / ATmega328P.
// Standalone sketch: only the built-in Wire library is required.
#include <Wire.h>

const uint8_t SENSOR_ADDRESS = 0x08;
const uint8_t PIR_PIN = 2;
const uint8_t ADD_PERSON_PIN = 3;     // Button between D3 and GND.
const uint8_t REMOVE_PERSON_PIN = 4;  // Button between D4 and GND.
const uint8_t TEMPERATURE_PIN = A0;
const uint8_t LDR_PIN = A1;
const uint16_t MAX_PEOPLE = 999;
const uint32_t DEBOUNCE_MS = 35;
const uint32_t SAMPLE_INTERVAL_MS = 50;
const uint32_t CONTROLLER_TIMEOUT_MS = 2500;
const uint8_t FRAME_SIZE = 8;

uint16_t peopleCount = 0;
uint16_t potentiometerRaw = 0;
uint16_t temperatureCentiC = 0;
uint16_t daylightValue = 0;
bool motionDetected = false;
bool lastReportedMotion = false;
bool temperatureValid = false;
bool temperatureEnabled = false;
bool haveControllerCommand = false;
uint32_t lastControllerCommand = 0;
uint32_t lastSample = 0;
uint32_t lastPrint = 0;

bool buttonRawHigh[2] = {true, true};
bool buttonStableHigh[2] = {true, true};
uint32_t buttonChangedAt[2] = {0, 0};

// Callbacks only copy bytes. Measurements and Serial output stay in loop().
volatile uint8_t sensorFrame[FRAME_SIZE] = {1, 0, 0, 0, 255, 255, 0, 0};
volatile uint8_t pendingSamplingCommand = 0;
volatile bool newSamplingCommand = false;

bool pressed(uint8_t index, bool rawHigh, uint32_t now) {
  if (rawHigh != buttonRawHigh[index]) {
    buttonRawHigh[index] = rawHigh;
    buttonChangedAt[index] = now;
  }
  if (rawHigh != buttonStableHigh[index] &&
      uint32_t(now - buttonChangedAt[index]) >= DEBOUNCE_MS) {
    buttonStableHigh[index] = rawHigh;
    return !rawHigh;  // Only the stable press; holding does not repeat.
  }
  return false;
}

void changeCount(bool addPerson, bool removePerson) {
  if (addPerson == removePerson) return;
  if (addPerson && peopleCount < MAX_PEOPLE) ++peopleCount;
  if (removePerson && peopleCount > 0) --peopleCount;
}

uint16_t convertTemperature(uint16_t adcValue) {
  return uint16_t((uint32_t(adcValue) * 4000UL + 511UL) / 1023UL);
}

uint16_t readAverage(uint8_t pin) {
  analogRead(pin);  // Discard first read after switching ADC channels.
  uint32_t total = 0;
  for (uint8_t i = 0; i < 4; ++i) total += analogRead(pin);
  return uint16_t(total / 4);
}

void publishReadings() {
  // [version, flags, peopleH, peopleL, temperatureH, temperatureL, LDRH, LDRL]
  // Flags: bit 0 = PIR movement; bit 1 = temperature is a valid active sample.
  const uint16_t temperature = temperatureValid ? temperatureCentiC : 0xFFFF;
  uint8_t frame[FRAME_SIZE] = {
    1, uint8_t((motionDetected ? 1 : 0) | (temperatureValid ? 2 : 0)),
    uint8_t(peopleCount >> 8), uint8_t(peopleCount),
    uint8_t(temperature >> 8), uint8_t(temperature),
    uint8_t(daylightValue >> 8), uint8_t(daylightValue)
  };
  noInterrupts();
  for (uint8_t i = 0; i < FRAME_SIZE; ++i) sensorFrame[i] = frame[i];
  interrupts();
}

void sendReadings() {
  uint8_t snapshot[FRAME_SIZE];
  for (uint8_t i = 0; i < FRAME_SIZE; ++i) snapshot[i] = sensorFrame[i];
  Wire.write(snapshot, FRAME_SIZE);
}

void receiveSamplingCommand(int byteCount) {
  if (byteCount == 1 && Wire.available() == 1) {
    const uint8_t command = uint8_t(Wire.read());
    if (command <= 1) {
      pendingSamplingCommand = command;
      newSamplingCommand = true;
    }
  }
  while (Wire.available()) Wire.read();
}

void setup() {
  Serial.begin(9600);
  pinMode(PIR_PIN, INPUT);
  pinMode(ADD_PERSON_PIN, INPUT_PULLUP);
  pinMode(REMOVE_PERSON_PIN, INPUT_PULLUP);
  Wire.begin(SENSOR_ADDRESS);
  Wire.onRequest(sendReadings);
  Wire.onReceive(receiveSamplingCommand);
  Serial.println(F("Arduino 1 sensing: 0x08; +D3 / -D4"));
}

void loop() {
  const uint32_t now = millis();
  changeCount(pressed(0, digitalRead(ADD_PERSON_PIN) == HIGH, now),
              pressed(1, digitalRead(REMOVE_PERSON_PIN) == HIGH, now));
  // PIR movement is independent of the simulated people count.
  motionDetected = digitalRead(PIR_PIN) == HIGH;
  if (motionDetected != lastReportedMotion) {
    lastReportedMotion = motionDetected;
  }

  noInterrupts();
  const bool received = newSamplingCommand;
  const uint8_t command = pendingSamplingCommand;
  newSamplingCommand = false;
  interrupts();
  const bool previouslyEnabled = temperatureEnabled;
  if (received) {
    haveControllerCommand = true;
    lastControllerCommand = now;
    temperatureEnabled = command == 1;
  }
  if (!haveControllerCommand ||
      uint32_t(now - lastControllerCommand) >= CONTROLLER_TIMEOUT_MS) {
    temperatureEnabled = false;
  }
  if (!temperatureEnabled) temperatureValid = false;

  if (uint32_t(now - lastSample) >= SAMPLE_INTERVAL_MS ||
      (temperatureEnabled && !previouslyEnabled)) {
    lastSample = now;
    daylightValue = readAverage(LDR_PIN);
    if (temperatureEnabled) {
      potentiometerRaw = readAverage(TEMPERATURE_PIN);
      temperatureCentiC = convertTemperature(readAverage(TEMPERATURE_PIN));
      temperatureValid = true;
    }
  }
  publishReadings();

  if (uint32_t(now - lastPrint) >= 1000) {
    lastPrint = now;
    Serial.print(F("People=")); Serial.print(peopleCount);
    Serial.print(F(" PIR=")); Serial.print(motionDetected);
    Serial.print(F(" LDR=")); Serial.print(daylightValue);
    Serial.print(F(" PotRaw="));
    if (temperatureValid) {
      Serial.print(potentiometerRaw);
      Serial.print(F(" Temp="));
      Serial.println(temperatureCentiC / 100.0, 2);
    } else {
      Serial.println(F("-- Temp=SLEEP"));
    }
  }
}
