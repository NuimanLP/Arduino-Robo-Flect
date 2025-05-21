#include <SPI.h>
#include <MFRC522.h>

// Pin definitions
#define RST_PIN    5   // Reset pin for MFRC522
#define SS_PIN     21   // Slave select pin for MFRC522 (SDA)

MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  while (!Serial) { /* wait for Serial to be ready */ }

  SPI.begin();               // Initialize VSPI: SCK=18, MISO=19, MOSI=23
  rfid.PCD_Init();           // Init MFRC522
  Serial.println("RFID test for ESP32 ready. Present a card/tag...");
}

void loop() {
  // Look for new cards
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  // Print UID in HEX
  Serial.print("Card UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.printf("%02X", rfid.uid.uidByte[i]);
    if (i + 1 < rfid.uid.size) Serial.print(":");
  }
  Serial.println();

  // Halt PICC & stop encryption on PCD
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(500);  // Debounce
}