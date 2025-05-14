#include <Wire.h>
#include <Adafruit_VL53L0X.h>

#define PIN_XSHUT 7    // ถ้าไม่ใช้ ให้ comment หรือต่อ VIN ตรง ๆ

Adafruit_VL53L0X lox = Adafruit_VL53L0X();

void setup() {
  Serial.begin(9600);
  while (!Serial) delay(10);

  // ตั้ง PIN_XSHUT เป็น OUTPUT แล้วปลุกเซ็นเซอร์
  pinMode(PIN_XSHUT, OUTPUT);
  digitalWrite(PIN_XSHUT, LOW);
  delay(10);
  digitalWrite(PIN_XSHUT, HIGH);
  delay(10);

  Wire.begin();  // SDA=20, SCL=21

  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while (1) delay(10);
  }
  Serial.println(F("VL53L0X ready!"));
}

void loop() {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);

  if (measure.RangeStatus != 4) {
    Serial.print(F("Distance: "));
    Serial.print((measure.RangeMilliMeter)/10);
    Serial.println(F(" cm"));
  } else {
    Serial.println(F("Out of range"));
  }
  delay(200);
}