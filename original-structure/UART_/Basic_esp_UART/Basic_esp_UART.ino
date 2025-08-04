// ESP32_UART_from_Mega.ino

#define RX2_PIN 16   // receive from Mega TX1 (through level shifter)
#define TX2_PIN 17   // send to Mega RX1

void setup() {
  // USB-Serial to PC (for debug)
  Serial.begin(115200);
  while(!Serial);

  // UART2 on GPIO16/17 ↔ Mega’s Serial1
  Serial2.begin(9600, SERIAL_8N1, RX2_PIN, TX2_PIN);
}

void loop() {
  // If Mega sent us anything, read + reply
  if (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    Serial.println("Received: " + msg);
    Serial2.println("Pong from ESP32");
  }

  // (Optional) forward anything typed in Serial Monitor down to Mega
  if (Serial.available()) {
    String in = Serial.readString();
    Serial2.print(in);
  }
}