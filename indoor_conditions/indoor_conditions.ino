#include <Wire.h>

constexpr byte NODE_ADDRESS = 0x08;
constexpr byte TEMPERATURE_PIN = A0;
constexpr byte PIR_PIN = 2;
constexpr bool USE_LM35 = true;  // Set false when testing with a potentiometer.

struct __attribute__((packed)) IndoorData {
  int16_t temperatureTenthsC;
  uint8_t occupied;
};

volatile IndoorData latestData{};

int16_t readTemperatureTenthsC() {
  const int raw = analogRead(TEMPERATURE_PIN);
  if (USE_LM35) {
    // Arduino Uno uses a 5 V ADC reference by default. LM35 outputs 10 mV per deg C.
    const float voltage = raw * (5.0 / 1023.0);
    return static_cast<int16_t>(voltage * 1000.0);
  }
  // Potentiometer test range: 15.0 to 35.0 deg C.
  return map(raw, 0, 1023, 150, 350);
}

void sendSensorData() {
  Wire.write(reinterpret_cast<const byte *>(&latestData), sizeof(latestData));
}

void setup() {
  pinMode(PIR_PIN, INPUT);
  Wire.begin(NODE_ADDRESS);
  Wire.onRequest(sendSensorData);
}

void loop() {
  latestData.temperatureTenthsC = readTemperatureTenthsC();
  latestData.occupied = digitalRead(PIR_PIN) == HIGH ? 1 : 0;
  delay(100);
}
