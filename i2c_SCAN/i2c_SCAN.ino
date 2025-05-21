#include <Wire.h>

void setup() {
  Serial.begin(9600);
  while (!Serial);
  Wire.begin();               // Mega: SDA=20, SCL=21
  Serial.println(F("I²C Multi-Device Scanner"));
}

void loop() {
  byte count = 0;

  Serial.println(F("Scanning I²C bus..."));
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      // Device ack’d at this address
      Serial.print(F(" • Found device at 0x"));
      if (address < 16) Serial.print('0');
      Serial.print(address, HEX);
      Serial.println();
      count++;
    }
    else if (error == 4) {
      Serial.print(F(" • Unknown error at 0x"));
      if (address < 16) Serial.print('0');
      Serial.print(address, HEX);
      Serial.println();
    }
  }

  if (count == 0) {
    Serial.println(F("No I²C devices found."));
  } else {
    Serial.print(count);
    Serial.println(F(" device(s) found."));
  }

  Serial.println();
  delay(2000);  // repeat every 5 seconds
}