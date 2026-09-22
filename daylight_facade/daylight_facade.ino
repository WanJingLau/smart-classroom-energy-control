#include <Wire.h>
#include <Servo.h>

constexpr byte NODE_ADDRESS = 0x09;
constexpr byte LDR_PIN = A0;
constexpr byte LIGHT_LED_PIN = 6;
constexpr byte CURTAIN_SERVO_PIN = 9;
constexpr int CURTAIN_OPEN_ANGLE = 0;
constexpr int CURTAIN_CLOSED_ANGLE = 90;

struct __attribute__((packed)) FacadeData {
  uint16_t lightLevel;
};

volatile FacadeData latestData{};
volatile bool requestedLightsOn = false;
volatile bool requestedCurtainsClosed = false;
Servo curtainServo;

void sendLightLevel() {
  Wire.write(reinterpret_cast<const byte *>(&latestData), sizeof(latestData));
}

void receiveCommand(int byteCount) {
  if (byteCount < 2) return;
  requestedLightsOn = Wire.read() == 1;
  requestedCurtainsClosed = Wire.read() == 1;
  while (Wire.available()) Wire.read();
}

void setup() {
  pinMode(LIGHT_LED_PIN, OUTPUT);
  curtainServo.attach(CURTAIN_SERVO_PIN);
  curtainServo.write(CURTAIN_OPEN_ANGLE);

  Wire.begin(NODE_ADDRESS);
  Wire.onRequest(sendLightLevel);
  Wire.onReceive(receiveCommand);
}

void loop() {
  latestData.lightLevel = analogRead(LDR_PIN);
  digitalWrite(LIGHT_LED_PIN, requestedLightsOn ? HIGH : LOW);
  curtainServo.write(requestedCurtainsClosed ? CURTAIN_CLOSED_ANGLE : CURTAIN_OPEN_ANGLE);
  delay(100);
}
