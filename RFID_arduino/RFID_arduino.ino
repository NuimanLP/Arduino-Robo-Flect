#include <SPI.h>
#include <MFRC522.h>

// กำหนดพิน SS และ RST ตามที่ต่อสายไว้ด้านบน
#define SS_PIN  7
#define RST_PIN  6

// สร้างอ็อบเจ็กต์ RFID
MFRC522 rfid(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  while (!Serial);           // รอ Serial พร้อมใช้งาน

  SPI.begin();               // เริ่ม SPI bus (MOSI=51, MISO=50, SCK=52)
  rfid.PCD_Init();           // เริ่มโมดูล MFRC522
  Serial.println("RFID reader initialized");
}

void loop() {
  // ถ้าไม่มีบัตรใหม่เข้ามา ให้จบ loop ทันที
  if (!rfid.PICC_IsNewCardPresent()) return;
  // ถ้าอ่าน UID ไม่สำเร็จ ให้จบ loop
  if (!rfid.PICC_ReadCardSerial()) return;

  // ถ้าอ่าน UID สำเร็จ ให้พิมพ์ออก Serial
  Serial.print("UID: ");
  for (byte i = 0; i < rfid.uid.size; i++) {
    // พิมพ์เป็นเลขฐาน 16 (HEX)
    Serial.print(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) Serial.print(":");
  }
  Serial.println();

  // หยุดการอ่านปัจจุบัน (prepare for next)
  rfid.PICC_HaltA();
}