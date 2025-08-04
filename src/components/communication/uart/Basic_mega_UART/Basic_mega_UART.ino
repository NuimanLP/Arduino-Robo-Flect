// Mega2560_UART_to_ESP32.ino

// โค้ดนี้ใช้สำหรับการสื่อสาร UART บนบอร์ด Arduino Mega

void setup() {
  // USB-Serial to PC (for debug)
  Serial.begin(115200);
  while(!Serial);

  // UART1 to ESP32
  // • TX1 = pin 18  → (level-shifter or resistor divider) → ESP32 RX2
  // • RX1 = pin 19  ← ESP32 TX2 (3.3 V is safe for Mega)
  Serial1.begin(9600);
}

void loop() {
  // Send a ping string every second
  Serial1.println("Ping from Mega");

  // Forward any response back to the USB-Serial monitor
  while (Serial1.available()) {
    char c = Serial1.read();
    Serial.print(c);
  }

  delay(1000);
}