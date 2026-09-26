#include <Wire.h>

#include "ControlLogic.h"

const uint8_t TEMPERATURE_NODE_ADDRESS = 0x08;
const uint8_t PIR_NODE_ADDRESS = 0x09;

const unsigned long PIR_POLL_INTERVAL_MS = 100;
const unsigned long CONTROL_INTERVAL_MS = 500;
const unsigned long OCCUPANCY_TIMEOUT_MS = 10000;
const unsigned long ERROR_PRINT_INTERVAL_MS = 1000;

MotionState motionState = {false, 0};
bool occupied = false;

unsigned long lastPirPollTime = 0;
unsigned long lastControlTime = 0;
unsigned long lastPirErrorPrintTime = 0;

bool requestPirMotion(bool& motionDetected) {
  const uint8_t receivedBytes =
      Wire.requestFrom(PIR_NODE_ADDRESS, static_cast<uint8_t>(1));

  if (receivedBytes != 1 || Wire.available() < 1) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  const uint8_t statusByte = Wire.read();

  while (Wire.available()) {
    Wire.read();
  }

  return decodePirSignal(statusByte, motionDetected);
}

bool requestTemperature(uint16_t& temperatureCentiC) {
  const uint8_t receivedBytes =
      Wire.requestFrom(TEMPERATURE_NODE_ADDRESS, static_cast<uint8_t>(2));

  if (receivedBytes != 2 || Wire.available() < 2) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  const uint8_t highValue = Wire.read();
  const uint8_t lowValue = Wire.read();

  temperatureCentiC =
      (static_cast<uint16_t>(highValue) << 8) | lowValue;

  while (Wire.available()) {
    Wire.read();
  }

  return temperatureCentiC <= 4000;
}

bool sendCommandToArduino1(uint8_t command) {
  Wire.beginTransmission(TEMPERATURE_NODE_ADDRESS);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

const char* commandName(uint8_t command) {
  switch (command) {
    case COMMAND_COMFORTABLE:
      return "OCCUPIED / COMFORTABLE";
    case COMMAND_HEATING:
      return "HEATING";
    case COMMAND_COOLING:
      return "COOLING";
    default:
      return "SLEEP / UNOCCUPIED";
  }
}

void setup() {
  Serial.begin(9600);

  // Arduino 2 is the only I2C controller.
  Wire.begin();
  Wire.setClock(100000);

  sendCommandToArduino1(COMMAND_SLEEP);

  Serial.println("Arduino 2 main controller ready");
  Serial.println("Arduino 1 temperature address: 0x08");
  Serial.println("Arduino 3 PIR address: 0x09");
  Serial.println("Occupancy timeout: 10 seconds");
}

void loop() {
  const unsigned long now = millis();

  if (now - lastPirPollTime >= PIR_POLL_INTERVAL_MS) {
    lastPirPollTime = now;

    bool motionDetected = false;
    if (requestPirMotion(motionDetected)) {
      motionState =
          recordMotionSample(motionState, motionDetected, now);

      if (motionDetected) {
        Serial.println("Arduino 3: MOTION");
      }
    } else if (now - lastPirErrorPrintTime >= ERROR_PRINT_INTERVAL_MS) {
      lastPirErrorPrintTime = now;
      Serial.println("Arduino 3 PIR request failed");
    }
  }

  const bool newOccupiedState =
      occupancyIsActive(motionState, now, OCCUPANCY_TIMEOUT_MS);

  if (newOccupiedState != occupied) {
    occupied = newOccupiedState;

    if (occupied) {
      Serial.println("OCCUPIED - waking Arduino 1");
      sendCommandToArduino1(COMMAND_COMFORTABLE);

      // Allow Arduino 1 to take its first potentiometer sample.
      delay(150);

      // Force an immediate temperature-control cycle.
      lastControlTime = now - CONTROL_INTERVAL_MS;
    } else {
      Serial.println("UNOCCUPIED - sending Arduino 1 to sleep");
      sendCommandToArduino1(COMMAND_SLEEP);
    }
  }

  if (!occupied || now - lastControlTime < CONTROL_INTERVAL_MS) {
    return;
  }

  lastControlTime = now;

  uint16_t temperatureCentiC = 0;
  if (!requestTemperature(temperatureCentiC)) {
    Serial.println("Arduino 1 temperature request failed");

    // Keep Arduino 1 active with both LEDs off so the request can retry.
    sendCommandToArduino1(COMMAND_COMFORTABLE);
    return;
  }

  const uint8_t command = determineCommand(true, temperatureCentiC);
  const bool commandSent = sendCommandToArduino1(command);

  Serial.print("Temperature: ");
  Serial.print(temperatureCentiC / 100.0, 1);
  Serial.print(" C | Decision: ");
  Serial.print(commandName(command));
  Serial.print(" | Command sent: ");
  Serial.println(commandSent ? "YES" : "FAILED");
}
