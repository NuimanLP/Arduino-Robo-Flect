// ======================================================
// ESP32  — RFID (MFRC522) + DFPlayer + UART link to MEGA
// - DFPlayer  : Serial2 @9600   (RX=25, TX=26)
// - MEGA LINK : UART1  @115200  (TX=17 -> MEGA RX2=D17,  RX=16 (ไม่ใช้))
// - SPI (RC522): SS=5, RST=22, (SCK=18, MOSI=23, MISO=19 - ขา default SPI)
// ======================================================

// === เลือกขา DFPlayer (ESP32) ===
// ESP32 RX (รับ) -> DFPlayer TX (ส่ง)
#define DF_RX_PIN 25
// ESP32 TX (ส่ง) -> (ตัวต้านทาน ~1kΩ) -> DFPlayer RX (รับ)
#define DF_TX_PIN 26

// === เลือกขา UART Link ไป MEGA ===
#define LINK_RX_PIN 16   // ไม่ใช้ (ส่งทางเดียวก็ได้ แต่ต้องกำหนดให้ begin)
#define LINK_TX_PIN 17   // ต่อไป MEGA RX2 (D17)
#define LINK_BAUD   115200

// === Import Libraries ===
#include <SPI.h>                     // สำหรับ MFRC522 (SPI)
#include <MFRC522.h>                 // โมดูล RFID
#include <DFRobotDFPlayerMini.h>     // โมดูลเล่นเสียง

// ===== RC522 Pins (SPI) =====
#define SS_PIN   5    // CS/SS ของ MFRC522
#define RST_PIN 22    // RST ของ MFRC522

// === Objects ===
DFRobotDFPlayerMini myDFPlayer;
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ===== UARTs =====
HardwareSerial LINK(1);  // UART1 สำหรับส่งไป MEGA (TX=17, RX=16)

// ===== User DB =====
struct User {
  byte   uid[7];
  byte   uidSize;
  String name;
  int    folderNumber;
  int    trackNumber;
};

User users[] = {
  {{0x8A, 0xA7, 0x3F, 0x02},                  4, "User 1", 1, 1},
  {{0x93, 0xD5, 0x2D, 0x28},                  4, "User 2", 1, 2},
  {{0x04, 0x53, 0x75, 0x01, 0x83, 0x45, 0x03}, 7, "User 3", 1, 3},
  {{0x04, 0xB2, 0x53, 0x01, 0x54, 0x47, 0x03}, 7, "User 4", 1, 4},
  {{0x04, 0x09, 0xA7, 0x77, 0x70, 0x00, 0x00}, 7, "User 5", 1, 5},
  {{0x04, 0xA2, 0x81, 0x01, 0x95, 0x47, 0x03}, 7, "User 6", 1, 6},
  {{0x04, 0x53, 0xA0, 0x01, 0x98, 0x45, 0x03}, 7, "User 7", 1, 7},
  {{0x04, 0xB2, 0x0B, 0x56, 0x70, 0x00, 0x00}, 7, "User 8", 1, 8}
};

// ===== Playback queue =====
struct AudioFile { int folder; int track; };
AudioFile audioQueue[10];
int  queueSize         = 0;
int  currentQueueIndex = 0;
bool isPlaying         = false;

// ====== Prototypes ======
void clearQueue();
void playNextInQueue();
String getUserName(byte* uid, byte uidSize);
void sendUidToMega(const byte* uid, byte uidSize);
void sendNameToMega(const String& name);

// ------------------------------------------------------
// Queue helpers
// ------------------------------------------------------
void clearQueue() {
  queueSize = 0;
  currentQueueIndex = 0;
}

void playNextInQueue() {
  if (currentQueueIndex < queueSize) {
    myDFPlayer.playFolder(audioQueue[currentQueueIndex].folder,
                          audioQueue[currentQueueIndex].track);
    currentQueueIndex++;
    isPlaying = true;
  } else {
    isPlaying = false;
    clearQueue();
    Serial.println(F("Audio sequence finished."));
  }
}

// ------------------------------------------------------
// UID/Name → MEGA (UART1)
// ------------------------------------------------------
void sendUidToMega(const byte* uid, byte uidSize) {
  // ส่งแบบไม่มีช่องว่าง: RFID:04AABBCCDDEE\n
  // (Mega จะ parse แล้วเอาไปขึ้นจอ)
  for (byte i = 0; i < uidSize; i++) {
    // ตรงนี้เพื่อกันบัฟเฟอร์: สร้างเป็นสตริงครั้งเดียว
  }
  // ทำสตริง HEX
  char hexBuf[2*16+1];
  int p = 0;
  for (byte i = 0; i < uidSize; i++) {
    sprintf(&hexBuf[p], "%02X", uid[i]);
    p += 2;
  }
  hexBuf[p] = 0;

  LINK.print("RFID:");
  LINK.println(hexBuf);

  Serial.print(F("[LINK→MEGA] RFID:"));
  Serial.println(hexBuf);
}

void sendNameToMega(const String& name) {
  LINK.print("NAME:");
  LINK.println(name);
  Serial.print(F("[LINK→MEGA] NAME:"));
  Serial.println(name);
}

// ------------------------------------------------------
// Setup
// ------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  // SPI + RC522
  SPI.begin();               // SCK=18, MISO=19, MOSI=23 (ESP32 default)
  mfrc522.PCD_Init(SS_PIN, RST_PIN);
  delay(50);
  Serial.println();
  Serial.println(F("MFRC522 ready."));

  // UART สำหรับ DFPlayer (Serial2) @9600
  Serial2.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);
  Serial.println(F("Init DFPlayer..."));
  if (!myDFPlayer.begin(Serial2)) {
    Serial.println(F("DFPlayer init failed! Check wiring / SD (FAT32). Halt."));
    while (true) { delay(500); }
  }
  myDFPlayer.volume(20);  // 0..30
  Serial.println(F("DFPlayer online."));

  // UART1 สำหรับส่งไป MEGA
  LINK.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
  Serial.println(F("UART1 link -> MEGA @115200 ready. (TX=17 -> RX2(D17))"));

  // ===== Welcome sequence (one-time) =====
  audioQueue[0] = {2, 1}; // /02/001.mp3
  audioQueue[1] = {2, 2}; // /02/002.mp3
  audioQueue[2] = {2, 3}; // /02/003.mp3
  queueSize = 3;
  currentQueueIndex = 0;
  isPlaying = true;
  playNextInQueue();

  Serial.println(F("Ready to scan RFID card."));
  Serial.println(F("-----------------------------------"));
}

// ------------------------------------------------------
// Loop
// ------------------------------------------------------
void loop() {
  // ===== DFPlayer event (เช็คถี่ ๆ) =====
  if (myDFPlayer.available()) {
    uint8_t type = myDFPlayer.readType();
    if (type == DFPlayerPlayFinished) {
      Serial.println(F("Audio finished → play next (if any)"));
      if (queueSize > 0) playNextInQueue();
      else isPlaying = false;
    }
    // (สามารถอ่าน myDFPlayer.read() เพื่อเหตุผลอื่น ๆ ได้ถ้าต้องการ)
  }

  // ===== RFID scan เฉพาะตอน "ไม่ได้กำลังเล่นเสียง" =====
  if (!isPlaying) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {

      // Debug: print UID
      Serial.print(F("Card UID:"));
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(mfrc522.uid.uidByte[i], HEX);
      }
      Serial.println();

      // 1) ส่ง UID ไป MEGA
      sendUidToMega(mfrc522.uid.uidByte, mfrc522.uid.size);

      // 2) หา user name และส่งชื่อไป MEGA (เผื่อขึ้นชื่อด้วย)
      String userName = getUserName(mfrc522.uid.uidByte, mfrc522.uid.size);
      sendNameToMega(userName);

      // 3) จัดคิวเสียงตาม user mapping
      int folderToPlay = 1; // โฟลเดอร์หลักของผู้ใช้
      int trackToPlay  = -1;
      for (int i = 0; i < (int)(sizeof(users)/sizeof(users[0])); i++) {
        if (users[i].name == userName) {
          folderToPlay = users[i].folderNumber; // ใช้โฟลเดอร์ตาม DB
          trackToPlay  = users[i].trackNumber;
          break;
        }
      }

      if (trackToPlay != -1) {
        // known user → เล่นไฟล์เฉพาะของ user
        clearQueue();
        audioQueue[0] = {folderToPlay, trackToPlay};
        queueSize = 1;
        currentQueueIndex = 0;
        isPlaying = true;
        playNextInQueue();
      } else {
        // unknown user → เล่นไฟล์แจ้งเตือน
        clearQueue();
        myDFPlayer.playFolder(1, 99); // /01/099.mp3 สมมติไฟล์ "ไม่พบข้อมูล"
        isPlaying = true;
      }

      // กันอ่านซ้ำใบเดิมทันที
      delay(200); // สั้นพอ, Mega ยังได้ข้อความครบ
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
  }

  // (หากต้องการรับคำสั่งกลับจาก MEGA ผ่าน LINK.read() ก็เพิ่มได้ที่นี่)
}

// ------------------------------------------------------
// Helpers
// ------------------------------------------------------
String getUserName(byte* uid, byte uidSize) {
  for (int i = 0; i < (int)(sizeof(users) / sizeof(users[0])); i++) {
    if (uidSize == users[i].uidSize) {
      if (memcmp(uid, users[i].uid, uidSize) == 0) return users[i].name;
    }
  }
  return "Unknown User";
}
