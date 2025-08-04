#include <Wire.h>                // ไลบรารี I2C
#include <AccelStepper.h>         // ไลบรารีควบคุม Stepper Motor
#include <Adafruit_GFX.h>         // ไลบรารีกราฟิกพื้นฐานสำหรับ TFT
#include <Adafruit_ST7735.h>      // ไลบรารีควบคุมจอ ST7735
#include <VL53L0X.h>              // ไลบรารีเซ็นเซอร์วัดระยะ VL53L0X
#include <Keypad.h>               // ไลบรารีอ่าน Keypad 4x4

// ====== กำหนดพิน Stepper Motor (4 ขดลำดับ HALF4WIRE) ======
#define IN1 24                     // พินขดที่ 1
#define IN2 25                     // พินขดที่ 2
#define IN3 26                     // พินขดที่ 3
#define IN4 27                     // พินขดที่ 4
AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);

// ====== กำหนดพิน TFT ST7735 ======
#define TFT_CS    53              // CS (Chip Select) SPI
#define TFT_DC     9              // D/C (Data/Command)
#define TFT_RST    8              // Reset
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ====== กำหนดพินปุ่ม Up/Down ======
const uint8_t SW_UP   = 22;      // ปุ่มเพิ่มค่า (active low)
const uint8_t SW_DOWN = 23;      // ปุ่มลดค่า (active low)

// ====== สร้างอ็อบเจ็กต์เซ็นเซอร์ VL53L0X ======
VL53L0X sensor;

// ====== กำหนด Keypad 4x4 (แบบ digital pins) ======
const byte ROWS = 4, COLS = 4;    // กำหนดขนาด matrix
char keys[ROWS][COLS] = {         // แผนที่ตัวอักษรปุ่ม
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
// กำหนดพินที่ต่อสายเข้าปุ่มแถวและคอลัมน์จริงบน Mega
byte rowPins[ROWS] = {32, 33, 34, 35};
byte colPins[COLS] = {28, 29, 30, 31};
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ====== ตัวแปรควบคุมและค่าสำหรับ loop ======
int      counter            = 0;              // ตัวแปรนับค่า
uint16_t bgColor;                              // เก็บค่าสีพื้นหลังข้อความบน TFT
const   float    RUN_SPEED       = 1000.0;     // ความเร็วเคลื่อนที่ของมอเตอร์ (step/sec)
const   unsigned long REFRESH_MS      = 200;   // หน่วงเวลา refresh ค่า counter (ms)
const   unsigned long SENSOR_INTERVAL = 300;   // หน่วงเวลาอ่านเซ็นเซอร์ (ms)
unsigned long lastRefresh      = 0;            // เวลาล่าสุดที่ refresh counter
unsigned long lastSensor       = 0;            // เวลาล่าสุดที่อ่านเซ็นเซอร์
char     lastKey             = ' ';            // เก็บค่าปุ่มล่าสุด เพื่อป้องกันการทำงานซ้ำ

// ====== ตำแหน่ง x ของการพิมพ์ตัวเลขบน TFT หลังข้อความคงที่ ======
uint16_t xValueNum, xDistNum, xKeyNum;

// ====== ประกาศฟังก์ชันล่วงหน้า ======
void showCounter();                            // ฟังก์ชันแสดง counter บนจอ
void showDistance(int cm);                     // ฟังก์ชันแสดงระยะ cm บนจอ
void showKey(char k);                          // ฟังก์ชันแสดงปุ่มที่กดบนจอ

void setup() {
  // ----- เริ่ม Serial Monitor สำหรับ debug -----
  Serial.begin(9600);
  while (!Serial) {}   // รอจน Serial พร้อมใช้งาน

  // ----- เริ่ม I2C bus (Mega: SDA=20, SCL=21) -----
  Wire.begin();

  // ----- ตั้งค่าและทดสอบ VL53L0X -----
  sensor.setTimeout(500);                     // กำหนด timeout ในการอ่าน
  Serial.print(F("init VL53L0X... "));
  if (!sensor.init()) {                       // ถ้า init ไม่ผ่าน
    Serial.println(F("FAILED"));
    while (1) {}                              // หยุดที่นี่เลย
  }
  Serial.println(F("OK"));                  // init ผ่านแล้ว
  // ปรับค่าสัญญาณสะท้อนและพัลส์เพื่อ long-range + accuracy
  sensor.setSignalRateLimit(0.25);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
  sensor.setMeasurementTimingBudget(200000);
  sensor.startContinuous();                   // เริ่ม continuous reading mode

  // ----- ตั้งค่า TFT -----
  pinMode(TFT_CS, OUTPUT);
  tft.initR(INITR_BLACKTAB);                  // ใช้ init สำหรับ black tab
  tft.setRotation(0);                         // หมุนจอ (0-3)
  tft.fillScreen(ST77XX_BLACK);               // ล้างจอสีดำทั้งจอ
  tft.setTextSize(2);                         // ขนาดข้อความ 2x
  tft.setTextWrap(false);                     // ปิด wrap เพื่อจัดตำแหน่งเอง
  bgColor = tft.color565(200,200,100);        // สร้างสีพื้นหลังสำหรับข้อความ
  tft.setTextColor(ST77XX_ORANGE, bgColor);  // กำหนดสีข้อความและ bg

  // ----- ตั้งค่า Stepper Motor -----
  stepper.setMaxSpeed(800.0);                 // ความเร็วสูงสุด (step/sec)
  stepper.setAcceleration(500.0);             // อัตราเร่ง (step/sec^2)

  // ----- ตั้งค่าสวิตช์เพิ่ม/ลด -----
  pinMode(SW_UP,   INPUT_PULLUP);             // เปิด internal pull-up
  pinMode(SW_DOWN, INPUT_PULLUP);

  // ----- วาดข้อความคงที่บน TFT และเก็บตำแหน่ง Cursor -----
  tft.setCursor(5, 35);                       // กำหนดจุดเริ่มพิมพ์
  tft.print(F("Value:"));                    // พิมพ์คำว่า Value:
  xValueNum = tft.getCursorX();               // เก็บตำแหน่ง X ถัดจากคำว่า Value:

  tft.setCursor(5, 80);
  tft.print(F("Dist:"));                     // พิมพ์ Dist:
  xDistNum = tft.getCursorX();                // เก็บตำแหน่ง X ถัดจาก Dist:

  tft.setCursor(5,125);
  tft.print(F("Key:"));                      // พิมพ์ Key:
  xKeyNum = tft.getCursorX();                 // เก็บตำแหน่ง X ถัดจาก Key:

  // ----- แสดงค่าตั้งต้นบนจอ -----
  showCounter();                              // แสดง counter = 0
  showDistance(-1);                           // แสดง -- (out of range)
  showKey(' ');                               // แสดงช่องว่าง
}

void loop() {
  unsigned long now = millis();               // อ่านเวลาปัจจุบัน (ms)

  // ----- ตรวจปุ่ม Up/Down เพื่อควบคุม counter และมอเตอร์ -----
  bool up   = (digitalRead(SW_UP)   == LOW);  // ปุ่มกด = LOW
  bool down = (digitalRead(SW_DOWN) == LOW);
  if (up) {
    stepper.setSpeed(RUN_SPEED);              // ถ้ากด up ให้หมุนต่อเนื่องทางบวก
    if (now - lastRefresh >= REFRESH_MS) {
      counter++;                              // และนับ counter ขึ้นทีละ 1
      showCounter();                          // รีเฟรชตัวเลขบนจอ
      lastRefresh = now;                      // บันทึกเวลาที่รีเฟรช
    }
  }
  else if (down) {
    stepper.setSpeed(-RUN_SPEED);             // ถ้ากด down ให้หมุนทางลบ
    if (now - lastRefresh >= REFRESH_MS) {
      counter--;                              // ลด counter ทีละ 1
      showCounter();                          // รีเฟรชตัวเลข
      lastRefresh = now;
    }
  }
  else {
    stepper.setSpeed(0);                      // ไม่กดปุ่ม หยุดมอเตอร์
    lastRefresh = now;                        // รีเซ็ตเวลานับใหม่
  }
  stepper.runSpeed();                         // สั่งมอเตอร์ก้าวตาม speed ปัจจุบัน

  // ----- อ่านค่าจาก VL53L0X ทุกช่วง SENSOR_INTERVAL -----
  if (now - lastSensor >= SENSOR_INTERVAL) {
    uint16_t mm = sensor.readRangeContinuousMillimeters();  // อ่าน mm
    bool to = sensor.timeoutOccurred();                    // ตรวจ timeout
    int dcm = (!to && mm != 65535) ? mm/10 : -1;           // แปลงเป็น cm หรือ out of range
    showDistance(dcm);                                     // รีเฟรชตัวเลขระยะบนจอ
    // แสดงผลทาง Serial Monitor
    Serial.print(F("Dist: "));
    if (dcm == 819) Serial.print("Error");
    else if (dcm >= 0) Serial.print(dcm);
    else          Serial.print(F("Out of range"));
    Serial.println(F(" cm"));
    lastSensor = now;                                      // บันทึกเวลาที่อ่าน
  }

  // ----- อ่านค่าปุ่มจาก Keypad -----
  char k = keypad.getKey();                     // อ่านปุ่มล่าสุด
  if (k && k != lastKey) {                      // ถ้ามีการกดใหม่
    lastKey = k;                                // เก็บปุ่มล่าสุด
    showKey(k);                                 // รีเฟรชตัวอักษรบนจอ
    // แสดงผลทาง Serial
    Serial.print(F("Key: "));
    Serial.println(k);
  }
}

// ====== ฟังก์ชันแสดงค่า counter ======
void showCounter() {
  const int y = 35, w = 40, h = 16;            // กำหนดขอบเขตพื้นที่ล้าง
  tft.fillRect(xValueNum, y, w, h, bgColor);   // ลบพื้นที่เดิมเฉพาะตัวเลข
  tft.setCursor(xValueNum, y);                 // เคอร์เซอร์ตำแหน่งเริ่มพิมพ์
  tft.print(counter);                          // พิมพ์ค่า counter ปัจจุบัน
}

// ====== ฟังก์ชันแสดงระยะ ======
void showDistance(int cm) {
  const int y = 80, w = 60, h = 16;            // กำหนดขอบเขตพื้นที่ล้าง
  tft.fillRect(xDistNum, y, w, h, bgColor);    // ล้างตัวเลขระยะเดิม
  tft.setCursor(xDistNum, y);
  if (cm == 819) tft.print("Error");             // ตั้งเคอร์เซอร์ใหม่
  else if (cm >= 0) {                               // ถ้าอ่านได้
    tft.print(cm);                             // พิมพ์เลข cm
    tft.print(F(" cm"));                     // พิมพ์หน่วย cm
  } else {                                     // ถ้า out of range
    tft.print(F("--"));                      // พิมพ์ "--"
  }
}

// ====== ฟังก์ชันแสดงปุ่มที่กด ======
void showKey(char k) {
  const int y = 125, w = 12, h = 16;           // กำหนดขอบเขตพื้นที่ล้าง
  tft.fillRect(xKeyNum, y, w, h, bgColor);     // ลบตัวอักษรเก่า
  tft.setCursor(xKeyNum, y);                   // ตั้งเคอร์เซอร์ใหม่
  tft.print(k);                                // พิมพ์ตัวอักษรปุ่มที่กด
}
