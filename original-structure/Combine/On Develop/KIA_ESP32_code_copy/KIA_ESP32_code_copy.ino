// === เลือกขา DFPlayer (ESP32) ===
// กำหนดพินสำหรับเชื่อมต่อ ESP32 กับ DFPlayer Mini
// ESP32 RX (รับ) -> DFPlayer TX (ส่ง)
#define DF_RX_PIN 25
// ESP32 TX (ส่ง) -> (ตัวต้านทาน 1kΩ) -> DFPlayer RX (รับ)
#define DF_TX_PIN 26

// === Import Libraries ===
#include <SPI.h> // สำหรับ MFRC522 (ใช้ SPI)
#include <MFRC522.h> // สำหรับโมดูล RFID
#include <DFRobotDFPlayerMini.h> // สำหรับโมดูลเล่นเสียง

// ===== RC522 Pins (SPI) =====
#define SS_PIN 5   // พิน Slave Select (SS) หรือ Chip Select (CS)
#define RST_PIN 22 // พิน Reset

// === สร้าง Object (Instances) ===
// Object สำหรับ DFPlayer
DFRobotDFPlayerMini myDFPlayer;
// Object สำหรับ MFRC522 (ระบุพิน SS และ RST)
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ===== User DB (ฐานข้อมูลผู้ใช้) =====
// สร้างโครงสร้าง (struct) เพื่อเก็บข้อมูลผู้ใช้แต่ละคน
struct User {
  byte uid[7];      // UID ของการ์ด (รองรับสูงสุด 7 bytes)
  byte uidSize;     // ขนาด UID (ปกติ 4 หรือ 7 bytes)
  String name;      // ชื่อผู้ใช้ (สำหรับแสดงผล)
  int folderNumber; // หมายเลขโฟลเดอร์ใน SD card
  int trackNumber;  // หมายเลขไฟล์เสียงในโฟลเดอร์
};

// อาร์เรย์ของ struct User (นี่คือฐานข้อมูลของเรา)
// เมื่อสแกนการ์ดได้ UID, โปรแกรมจะมาค้นหาในนี้
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

// ===== Playback queue (ระบบคิวเล่นเสียง) =====
// ตัวแปรสถานะ: กำลังเล่นเสียงอยู่หรือไม่
bool isPlaying = false;
// struct สำหรับเก็บไฟล์เสียง (โฟลเดอร์/แทร็ก)
struct AudioFile { int folder; int track; };
// อาร์เรย์สำหรับเก็บคิว (รองรับสูงสุด 10 ไฟล์ต่อคิว)
AudioFile audioQueue[10];
// จำนวนไฟล์ในคิวปัจจุบัน
int queueSize = 0;
// ไฟล์ที่กำลังเล่นในคิว (ลำดับที่เท่าไหร่)
int currentQueueIndex = 0;

// ฟังก์ชันล้างคิว (รีเซ็ตค่า)
void clearQueue() {
  queueSize = 0;
  currentQueueIndex = 0;
}

// ฟังก์ชันเล่นไฟล์ถัดไปในคิว
void playNextInQueue() {
  // ถ้าลำดับปัจจุบันยังน้อยกว่าจำนวนไฟล์ในคิว (ยังเล่นไม่หมด)
  if (currentQueueIndex < queueSize) {
    // สั่ง DFPlayer เล่นไฟล์ตามโฟลเดอร์และแทร็ก
    myDFPlayer.playFolder(audioQueue[currentQueueIndex].folder,
                          audioQueue[currentQueueIndex].track);
    // เลื่อนไปไฟล์ถัดไป
    currentQueueIndex++;
    // อัปเดตสถานะเป็น "กำลังเล่น"
    isPlaying = true;
  } else {
    // ถ้าเล่นจบหมดคิวแล้ว
    isPlaying = false; // หยุดเล่น
    clearQueue();      // ล้างคิว
    Serial.println(F("Audio sequence finished."));
  }
}

void setup() {
  // เริ่ม Serial Monitor (สำหรับ Debug)
  Serial.begin(115200);
  while (!Serial); // รอ Serial พร้อม (สำหรับบางบอร์ด)

  // เริ่ม SPI (สำหรับ RFID)
  SPI.begin();
  // เริ่ม MFRC522
  mfrc522.PCD_Init();

  // เริ่ม Serial2 (สำหรับ DFPlayer)
  // ใช้ UART2 ของ ESP32 ที่พิน 25 (RX) และ 26 (TX)
  // 9600 baud, 8N1 (DFPlayer ต้องการ 8N1)
  Serial2.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);

  Serial.println();
  Serial.println(F("Initializing DFPlayer ... (May take 3-5 seconds)"));
  // เริ่ม DFPlayer โดยใช้ Serial2
  if (!myDFPlayer.begin(Serial2)) {
    // ถ้าไม่สำเร็จ (เช่น ต่อสายผิด, ไม่มี SD card)
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the wiring!"));
    Serial.println(F("2.Please make sure you have a SD card and it's FAT32 formatted!"));
    Serial.println(F("Program will not continue. Please fix the issue."));
    // ค้างการทำงานไว้
    while (true);
  }
  Serial.println(F("DFPlayer Mini is online."));

  // ตั้งค่าความดังเสียง (0-30)
  myDFPlayer.volume(20);

  // ===== สร้างคิว "เสียงต้อนรับ" (Welcome queue) =====
  // โค้ดส่วนนี้จะทำงานแค่ครั้งเดียวตอนเปิดเครื่อง
  // /02/001.mp3, /02/002.mp3, /02/003.mp3
  audioQueue[0] = {2, 1}; // โฟลเดอร์ 2, แทร็ก 1
  audioQueue[1] = {2, 2}; // โฟลเดอร์ 2, แทร็ก 2
  audioQueue[2] = {2, 3}; // โฟลเดอร์ 2, แทร็ก 3
  queueSize = 3;          // มี 3 ไฟล์ในคิว
  currentQueueIndex = 0;  // เริ่มที่ไฟล์แรก
  isPlaying = true;       // ตั้งสถานะเป็น "กำลังเล่น"
  playNextInQueue();      // เริ่มเล่นคิว

  Serial.println(F("Ready to scan an RFID card."));
  Serial.println(F("-----------------------------------"));
}

void loop() {
  // === ส่วนที่ 1: ตรวจสอบ Event จาก DFPlayer ===
  // (ต้องเรียกบ่อยๆ)
  if (myDFPlayer.available()) {
    uint8_t type = myDFPlayer.readType();
    // ถ้า Event คือ "เล่นไฟล์จบ" (DFPlayerPlayFinished)
    if (type == DFPlayerPlayFinished) {
      Serial.println(F("Audio playback finished. Playing next in queue..."));
      // ถ้าในคิวยังมีไฟล์
      if (queueSize > 0)
        playNextInQueue(); // เล่นไฟล์ถัดไป
      else
        isPlaying = false; // ถ้าหมดคิวแล้ว ก็หยุดเล่น
    }
  }

  // === ส่วนที่ 2: ตรวจสอบการ์ด RFID ===
  // จะตรวจสอบก็ต่อเมื่อ *ไม่ได้* กำลังเล่นเสียง (isPlaying == false)
  // นี่คือตรรกะสำคัญที่ป้องกันการอ่านการ์ดขณะเล่นเสียง
  if (!isPlaying) {
    // ถ้าตรวจพบการ์ดใหม่ *และ* อ่าน UID ได้สำเร็จ
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      // แสดง UID ที่อ่านได้ (สำหรับ Debug)
      Serial.print(F("Found card with UID:"));
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(mfrc522.uid.uidByte[i], HEX);
      }
      Serial.println();

      // ค้นหาชื่อผู้ใช้จาก UID ที่อ่านได้ (เรียกใช้ฟังก์ชันด้านล่าง)
      String userName = getUserName(mfrc522.uid.uidByte, mfrc522.uid.size);
      Serial.print(F("User: ")); Serial.println(userName);
      Serial.println(F("-----------------------------------"));

      // ค้นหาไฟล์เสียงที่ mapping กับผู้ใช้
      int folderToPlay = 1; // สมมติว่าไฟล์ของผู้ใช้อยู่ในโฟลเดอร์ 1
      int trackToPlay = -1; // -1 หมายถึง ยังไม่เจอ
      // วนลูปฐานข้อมูล user
      for (int i = 0; i < (int)(sizeof(users)/sizeof(users[0])); i++) {
        // ถ้าชื่อ user ตรงกัน
        if (users[i].name == userName) {
          trackToPlay = users[i].trackNumber; // เอาเลขแทร็กมาเก็บ
          break; // ออกจากลูป
        }
      }

      // === ส่วนที่ 3: สั่งเล่นเสียงตามผลลัพธ์ ===
      if (trackToPlay != -1) {
        // --- 3.1) ถ้าเจอการ์ดในระบบ (User ที่รู้จัก) ---
        // สร้างคิวใหม่ (มีไฟล์เดียว)
        audioQueue[0] = {folderToPlay, trackToPlay};
        queueSize = 1;
        currentQueueIndex = 0;
        isPlaying = true; // ตั้งสถานะ "กำลังเล่น"
        playNextInQueue(); // สั่งเล่น
      } else {
        // --- 3.2) ถ้าไม่เจอการ์ดในระบบ (Unknown User) ---
        clearQueue(); // ล้างคิว (เผื่อมี)
        // เล่นไฟล์ "ไม่พบข้อมูล" (สมมติว่าคือ /01/099.mp3)
        myDFPlayer.playFolder(1, 99);
        isPlaying = true; // ตั้งสถานะ "กำลังเล่น"
      }

      // หน่วงเวลาเล็กน้อย
      // (ช่วยป้องกันการอ่านการ์ดใบเดิมซ้ำๆ ทันทีที่เล่นเสียงจบ)
      delay(2000);

      // หยุดการสื่อสารกับการ์ดใบนี้ (เพื่อรอรับใบใหม่)
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
  }
}

// ===== Helper: ฟังก์ชันค้นหาชื่อผู้ใช้จาก UID =====
// รับ UID (แบบ byte array) และขนาด
String getUserName(byte* uid, byte uidSize) {
  // วนลูปฐานข้อมูล 'users'
  // (int)(sizeof(users) / sizeof(users[0])) คือการคำนวณจำนวน user ในอาร์เรย์
  for (int i = 0; i < (int)(sizeof(users) / sizeof(users[0])); i++) {
    // 1. ตรวจสอบว่าขนาด UID (4 หรือ 7 bytes) ตรงกันหรือไม่
    if (uidSize == users[i].uidSize) {
      // 2. ถ้าขนาดเท่ากัน ให้เปรียบเทียบข้อมูล UID (ทีละ byte)
      // memcmp คือ 'memory compare' ถ้าเหมือนกันเป๊ะ จะคืนค่า 0
      if (memcmp(uid, users[i].uid, uidSize) == 0)
        return users[i].name; // คืนชื่อผู้ใช้ ถ้าเจอ
    }
  }
  // ถ้าวนจนจบแล้วไม่เจอ
  return "Unknown User";
}
s