// Upload to ARDUINO 3: LEDs and blind actuator only.
#include <Wire.h>
#include <Servo.h>

const uint8_t SERVICE_ADDRESS = 0x09;
const uint8_t LIGHT_LED_PIN = 10;
const uint8_t COOLING_LED_PIN = 9;
const uint8_t HEATING_LED_PIN = 8;
const uint8_t SERVO_PIN = 6;
const uint8_t START_ANGLE = 20;
const uint32_t COMMAND_TIMEOUT_MS = 2500;

Servo blindServo;
volatile uint8_t pendingLight = 0;
volatile uint8_t pendingHvac = 0;
volatile uint8_t pendingAngle = START_ANGLE;
volatile bool commandAvailable = false;
uint8_t lightCommand = 0;
uint8_t hvacCommand = 0;
uint8_t blindAngle = START_ANGLE;
bool haveCommand = false;
uint32_t lastCommandTime = 0;
uint32_t lastPrint = 0;

void receiveCommands(int byteCount) {
  // [version=1, light 0/1, HVAC 0=sleep 1=idle 2=heat 3=cool, angle 0..180]
  if (byteCount == 4 && Wire.available() == 4) {
    const uint8_t version = uint8_t(Wire.read());
    const uint8_t light = uint8_t(Wire.read());
    const uint8_t hvac = uint8_t(Wire.read());
    const uint8_t angle = uint8_t(Wire.read());
    if (version == 1 && light <= 1 && hvac <= 3 && angle <= 180) {
      pendingLight = light;
      pendingHvac = hvac;
      pendingAngle = angle;
      commandAvailable = true;
    }
  }
  while (Wire.available()) Wire.read();
}

void setup() {
  Serial.begin(9600);
  pinMode(LIGHT_LED_PIN, OUTPUT);
  pinMode(COOLING_LED_PIN, OUTPUT);
  pinMode(HEATING_LED_PIN, OUTPUT);
  digitalWrite(LIGHT_LED_PIN, LOW);
  digitalWrite(COOLING_LED_PIN, LOW);
  digitalWrite(HEATING_LED_PIN, LOW);
  blindServo.attach(SERVO_PIN);
  blindServo.write(START_ANGLE);
  Wire.begin(SERVICE_ADDRESS);
  Wire.onReceive(receiveCommands);
  Serial.println(F("Arduino 3 services: 0x09"));
}

void loop() {
  const uint32_t now = millis();
  noInterrupts();
  const bool received = commandAvailable;
  const uint8_t light = pendingLight;
  const uint8_t hvac = pendingHvac;
  const uint8_t angle = pendingAngle;
  commandAvailable = false;
  interrupts();

  if (received) {
    haveCommand = true;
    lastCommandTime = now;
    lightCommand = light;
    hvacCommand = hvac;
    if (angle != blindAngle) {
      blindAngle = angle;
      blindServo.write(blindAngle);
    }
  }
  const bool timedOut = !haveCommand ||
                        uint32_t(now - lastCommandTime) >= COMMAND_TIMEOUT_MS;
  digitalWrite(LIGHT_LED_PIN, !timedOut && lightCommand ? HIGH : LOW);
  digitalWrite(HEATING_LED_PIN, !timedOut && hvacCommand == 2 ? HIGH : LOW);
  digitalWrite(COOLING_LED_PIN, !timedOut && hvacCommand == 3 ? HIGH : LOW);
  // Communication loss switches all LEDs off and holds the blind position.

  if (uint32_t(now - lastPrint) >= 1000) {
    lastPrint = now;
    Serial.print(F("Light=")); Serial.print(timedOut ? 0 : lightCommand);
    Serial.print(F(" HVAC=")); Serial.print(timedOut ? 0 : hvacCommand);
    Serial.print(F(" Blind=")); Serial.print(blindAngle);
    if (timedOut) Serial.print(F(" COMMAND TIMEOUT"));
    Serial.println();
  }
}
