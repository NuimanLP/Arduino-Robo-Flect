#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Firebase_ESP_Client.h>

// ─── กำหนดขาพินสำหรับ RFID ───────────────────────────
#define RST_PIN     5    // ขา RST ของ MFRC522
#define SS_PIN      21   // ขา SDA(SS) ของ MFRC522

// ─── ข้อมูล Wi-Fi ────────────────────────────────────
#define WIFI_SSID   "Boat"
#define WIFI_PASS   "klahan2546*"

// ─── ข้อมูล Firebase ─────────────────────────────────
#define API_KEY        "cklMfRy3CPUauXquOe2d5BDuvo2Kiaysvs0Ohtj8"
#define DATABASE_URL   "https://test-robo-flect-default-rtdb.asia-southeast1.firebasedatabase.app"

MFRC522 rfid(SS_PIN, RST_PIN);  // สร้างอ็อบเจ็กต์อ่าน RFID
FirebaseConfig config;         // เก็บคอนฟิก Firebase
FirebaseAuth auth;             // ต้องประกาศไว้ แม้จะไม่ใช้ข้อมูลใด ๆ
FirebaseData fbdo;             // สำหรับรับผลลัพธ์จากการเรียกใช้งาน

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  // ─── เริ่ม SPI + RFID ───────────────────────
  SPI.begin();
  rfid.PCD_Init();
  Serial.println(">> RFID พร้อมทดสอบ");

  // ─── เชื่อม Wi-Fi ─────────────────────────
  Serial.print(">> เชื่อม Wi-Fi: ");
  Serial.print(WIFI_SSID);
  Serial.print(" …");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println(" สำเร็จ!");
  Serial.print(">> IP ของบอร์ด: ");
  Serial.println(WiFi.localIP());

  // ─── ตั้งค่า Firebase (Test Mode) ─────────
  config.api_key      = API_KEY;
  config.database_url = DATABASE_URL;
  // บอกไลบรารีว่าให้ข้ามการขอ token และเชื่อมต่อแบบเปิดสาธารณะ
  config.signer.test_mode = true; // [oai_citation:0‡GitHub](https://github.com/rolan37/Firebase-ESP-Client-main?utm_source=chatgpt.com)

  // เรียกฟังก์ชัน begin ต้องส่งทั้ง config และ auth pointer
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println(">> Firebase พร้อมใช้งาน (Test Mode)");
}

void loop() {
  // ถ้าไม่มีบัตรใหม่ ให้ข้าม
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // อ่าน UID เป็น HEX string เช่น AA:BB:CC:DD
  String uid;
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i + 1 < rfid.uid.size) uid += ":";
  }
  uid.toUpperCase();
  Serial.println(">> อ่าน UID: " + uid);

  // สร้าง JSON แล้วพุชขึ้น RTDB
  FirebaseJson json;
  json.set("uid", uid);
  json.set("timestamp", millis());

  if (Firebase.RTDB.pushJSON(&fbdo, "/rfid_logs", &json)) {
    Serial.println(">> ส่งข้อมูลขึ้น Firebase เรียบร้อย");
  } else {
    Serial.println("!! ส่ง Firebase ล้มเหลว: " + fbdo.errorReason());
  }

  // หยุดรอบนี้ รออ่านครั้งถัดไป
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(500);
}