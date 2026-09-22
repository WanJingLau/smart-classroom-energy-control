#include <Wire.h>
#include <LiquidCrystal_I2C.h>

constexpr byte INDOOR_NODE_ADDRESS = 0x08;
constexpr byte FACADE_NODE_ADDRESS = 0x09;
constexpr byte STATUS_LED_PIN = LED_BUILTIN;

// Tune after testing
constexpr float HOT_TEMPERATURE_C = 26.0;
constexpr int DARK_LIGHT_LEVEL = 350;  // 0-1023; lower is darker for the assumed LDR divider.

LiquidCrystal_I2C lcd(0x27, 16, 2);

struct __attribute__((packed)) IndoorData {
  int16_t temperatureTenthsC;
  uint8_t occupied;
};

struct __attribute__((packed)) FacadeData {
  uint16_t lightLevel;
};

bool readIndoorData(IndoorData &data) {
  const byte expectedBytes = sizeof(IndoorData);
  if (Wire.requestFrom(INDOOR_NODE_ADDRESS, expectedBytes) != expectedBytes) return false;
  byte *target = reinterpret_cast<byte *>(&data);
  for (byte i = 0; i < expectedBytes; ++i) target[i] = Wire.read();
  return true;
}

bool readFacadeData(FacadeData &data) {
  const byte expectedBytes = sizeof(FacadeData);
  if (Wire.requestFrom(FACADE_NODE_ADDRESS, expectedBytes) != expectedBytes) return false;
  byte *target = reinterpret_cast<byte *>(&data);
  for (byte i = 0; i < expectedBytes; ++i) target[i] = Wire.read();
  return true;
}

void sendFacadeCommand(bool lightsOn, bool curtainsClosed) {
  Wire.beginTransmission(FACADE_NODE_ADDRESS);
  Wire.write(lightsOn ? 1 : 0);
  Wire.write(curtainsClosed ? 1 : 0);
  Wire.endTransmission();
}

void displayStatus(float temperatureC, bool occupied, bool hvacOn, bool lightsOn) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperatureC, 1);
  lcd.print("C ");
  lcd.print(occupied ? "IN" : "OUT");
  lcd.setCursor(0, 1);
  lcd.print("AC:");
  lcd.print(hvacOn ? "ON " : "OFF");
  lcd.print(" L:");
  lcd.print(lightsOn ? "ON" : "OFF");
}

void setup() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.print("Classroom Ctrl");
  delay(1200);
}

void loop() {
  IndoorData indoor{};
  FacadeData facade{};
  const bool indoorAvailable = readIndoorData(indoor);
  const bool facadeAvailable = readFacadeData(facade);

  if (!indoorAvailable || !facadeAvailable) {
    digitalWrite(STATUS_LED_PIN, LOW);
    lcd.clear();
    lcd.print("I2C node error");
    delay(1000);
    return;
  }

  const float temperatureC = indoor.temperatureTenthsC / 10.0;
  const bool occupied = indoor.occupied == 1;
  const bool hot = temperatureC >= HOT_TEMPERATURE_C;
  const bool dark = facade.lightLevel < DARK_LIGHT_LEVEL;

  const bool hvacOn = occupied && hot;
  const bool lightsOn = occupied && (dark || hot && !dark); // visible output during the hot-and-occupied case
  const bool curtainsClosed = occupied && hot && !dark;

  sendFacadeCommand(lightsOn, curtainsClosed);
  digitalWrite(STATUS_LED_PIN, hvacOn ? HIGH : LOW);
  displayStatus(temperatureC, occupied, hvacOn, lightsOn);
  delay(1000);
}
