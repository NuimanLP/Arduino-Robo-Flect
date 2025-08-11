#include <Wire.h>               // ไลบรารีสำหรับใช้งาน I2C (สื่อสารกับอุปกรณ์บนบัส I2C)
#include <LiquidCrystal_I2C.h>  // ไลบรารีสำหรับจอ LCD 20x4 ผ่าน I2C
#include <AccelStepper.h>       // ไลบรารีสำหรับควบคุมมอเตอร์แบบสเต็ปเปอร์
#include <VL53L0X.h>            // ไลบรารีสำหรับเซ็นเซอร์วัดระยะ VL53L0X
#include <Keypad.h>             // ไลบรารีสำหรับอ่าน Keypad 4x4

// ====== Stepping Motor (Driver mode) ======
#define STEP_PIN     14    // ขา STEP ของไดร์เวอร์ (ส่งพัลส์ให้หมุน)
#define DIR_PIN      15    // ขา DIR ของไดร์เวอร์ (กำหนดทิศทางหมุน)
#define SW_FWD_PIN   16    // สวิตช์เดินหน้า (active LOW) ต่อกับขา 16
#define SW_BWD_PIN   17    // สวิตช์ถอยหลัง (active LOW) ต่อกับขา 17

const float MAX_SPEED    = 1333.0;   // กำหนดความเร็วสูงสุดของสเต็ปเปอร์
const float ACCELERATION = 2000.0;   // กำหนดอัตราเร่งของสเต็ปเปอร์
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);  // สร้างออบเจ็กต์ควบคุมสเต็ปเปอร์โหมดไดรเวอร์

// ====== LCD 20x4 (I2C) ======
LiquidCrystal_I2C lcd(0x27, 20, 4);// สร้างออบเจ็กต์จอ LCD ขนาด 20 คอลัมน์ x 4 แถว, ที่อยู่ I2C = 0x27

// ====== Counter Buttons ======
const uint8_t SW_UP   = 16;  // ขาอ่านปุ่มเพิ่มค่า (active LOW) อยู่ขา 16
const uint8_t SW_DOWN = 17;  // ขาอ่านปุ่มลดค่า (active LOW) อยู่ขา 17

// ====== VL53L0X Distance Sensor ======
VL53L0X sensor;  // สร้างออบเจ็กต์เซ็นเซอร์ VL53L0X

// ====== Keypad 4x4 (Direct GPIO) ======
const byte ROWS = 4;  // จำนวนแถวของ Keypad
const byte COLS = 4;  // จำนวนคอลัมน์ของ Keypad

// กำหนดข้อมูลตัวอักษรบนปุ่ม Keypad แต่ละแถว-คอลัมน์
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// กำหนดขาที่เชื่อมกับแต่ละแถวและคอลัมน์ของ Keypad
byte rowPins[ROWS] = {30, 31, 32, 33};  // ขา 30-33 ต่อกับแถว 1-4
byte colPins[COLS] = {26, 27, 28, 29};  // ขา 26-29 ต่อกับคอลัมน์ 1-4

// สร้างออบเจ็กต์ Keypad โดยส่ง makeKeymap(keys) และชุดขา rowPins, colPins
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ====== Global Variables ======
int counter = 0;         // ตัวนับ (Counter) เริ่มจาก 0
char lastKey = ' ';      // เก็บคีย์ล่าสุดที่กดบน Keypad (เริ่มเป็นช่องว่าง)
const unsigned long REFRESH_MS      = 200;  // ระยะเวลาหน่วง (ms) สำหรับอัปเดต Counter
const unsigned long SENSOR_INTERVAL = 300;  // ระยะเวลาหน่วง (ms) สำหรับอ่านเซ็นเซอร์ VL53L0X
unsigned long lastRefresh = 0;  // เก็บเวลาที่อัปเดต Counter ล่าสุด (ms)
unsigned long lastSensor  = 0;  // เก็บเวลาที่อ่านเซ็นเซอร์ล่าสุด (ms)

// กำหนดคอลัมน์ (column) บน LCD เพื่อแสดงค่าแต่ละตัว (นับจาก 0)
const uint8_t colValue = 7;  // ค่า Counter จะแสดงที่คอลัมน์ 7 ของแถว 0
const uint8_t colDist  = 6;  // ค่าระยะ (Distance) จะแสดงที่คอลัมน์ 6 ของแถว 1
const uint8_t colKey   = 5;  // คีย์ที่กดจะแสดงที่คอลัมน์ 5 ของแถว 2

// === ฟังก์ชันช่วย: ล้างพื้นที่บน LCD (พิมพ์ช่องว่างทับ) ===
void clearField(uint8_t row, uint8_t startCol, uint8_t width) {
  // เลื่อนเคอร์เซอร์ไปยังแถว row, คอลัมน์ startCol
  lcd.setCursor(startCol, row);
  // พิมพ์ ' ' (space) จำนวน width ตัว เพื่อทับข้อความเก่า
  for (uint8_t i = 0; i < width; i++) {
    lcd.print(' ');
  }
}

void setup() {
  Serial.begin(9600);       // เริ่มต้น Serial Monitor ความเร็ว 9600 bps
  while (!Serial);          // รอจนกว่า Serial จะเริ่มทำงาน

  // ====== I2C และเซ็นเซอร์ VL53L0X init ======
  Wire.begin();             // เริ่มต้นบัส I2C
  Serial.println("Checking VL53L0X...");
  sensor.setTimeout(500);   // ตั้งเวลา timeout ให้เซ็นเซอร์ 500 ms
  if (!sensor.init()) {     // พยายามเรียกใช้งาน VL53L0X
    Serial.println("VL53L0X init failed!");  
    while (1) {
      // ถ้าไม่สามารถ init ได้ ให้หยุดรออยู่ตรงนี้ตลอด
    }
  }
  // กำหนดค่าเพิ่มเติมให้เซ็นเซอร์เพื่อปรับความแม่นยำ
  sensor.setSignalRateLimit(0.25);  
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
  sensor.setMeasurementTimingBudget(200000);
  sensor.startContinuous();  // เริ่มการวัดแบบต่อเนื่อง
  Serial.println("VL53L0X OK");

  // ====== LCD init ======
  Serial.println("Initializing LCD...");
  lcd.begin();       // เริ่มต้นจอ LCD 20x4
  lcd.backlight();   // เปิดไฟแบ็คไลท์ให้ LCD อ่านง่าย
  lcd.clear();       // ล้างหน้าจอทั้งหมด

  // พิมพ์ Label คงที่บน LCD (แก้ไขสเกลเป็นแถว/คอลัมน์ ไม่ใช่พิกเซล)
  lcd.setCursor(0, 0);  // แถว 0 คอลัมน์ 0
  lcd.print("Value:");  // แสดงคำว่า "Value:" ที่บรรทัดแรก
  lcd.setCursor(0, 1);  // แถว 1 คอลัมน์ 0
  lcd.print("Dist:");   // แสดงคำว่า "Dist:" ที่บรรทัดที่สอง
  lcd.setCursor(0, 2);  // แถว 2 คอลัมน์ 0
  lcd.print("Key:");    // แสดงคำว่า "Key:" ที่บรรทัดที่สาม

  // แสดงค่าเริ่มต้นของแต่ละช่อง (Counter=0, ระยะ="--", Key=" ")
  clearField(0, colValue, 4);   // ล้างพื้นที่ช่อง Counter (4 ตัวอักษร) บนแถว 0
  lcd.setCursor(colValue, 0);   // เคอร์เซอร์ไปที่คอลัมน์ 7 แถว 0
  lcd.print("0");               // พิมพ์ตัวเลข "0"

  clearField(1, colDist, 4);    // ล้างพื้นที่ช่อง Distance (4 ตัวอักษร) บนแถว 1
  lcd.setCursor(colDist, 1);    // เคอร์เซอร์ไปที่คอลัมน์ 6 แถว 1
  lcd.print("--");              // พิมพ์ "--" แทนค่าระยะยังไม่มี

  clearField(2, colKey, 1);     // ล้างพื้นที่ช่อง Key (1 ตัวอักษร) บนแถว 2
  lcd.setCursor(colKey, 2);     // เคอร์เซอร์ไปที่คอลัมน์ 5 แถว 2
  lcd.print(" ");               // พิมพ์ช่องว่าง 1 ตัว

  Serial.println("LCD OK");

  // ====== Stepper init ======
  Serial.println("Checking Stepper...");
  stepper.setMaxSpeed(MAX_SPEED);       // ตั้งค่าความเร็วสูงสุดของสเต็ปเปอร์
  stepper.setAcceleration(ACCELERATION);// ตั้งค่าอัตราเร่งของสเต็ปเปอร์
  pinMode(SW_FWD_PIN, INPUT_PULLUP);    // ตั้งขา SW_FWD_PIN เป็น INPUT_PULLUP
  pinMode(SW_BWD_PIN, INPUT_PULLUP);    // ตั้งขา SW_BWD_PIN เป็น INPUT_PULLUP
  Serial.println("Stepper OK");

  // ====== Counter Buttons init ======
  Serial.println("Checking counter buttons...");
  pinMode(SW_UP, INPUT_PULLUP);    // ตั้งขา SW_UP เป็น INPUT_PULLUP
  pinMode(SW_DOWN, INPUT_PULLUP);  // ตั้งขา SW_DOWN เป็น INPUT_PULLUP
  Serial.println("Counter buttons OK");

  // ====== Keypad init ======
  Serial.println("Checking Keypad...");
  // ไลบรารี Keypad ไม่ต้องเรียก init เพิ่มเติม หลังจากสร้างออบเจ็กต์แล้ว
  Serial.println("Keypad OK");
}

void loop() {
  unsigned long now = millis();  // อ่านเวลาปัจจุบัน (ms)

  // ====== Stepping Motor control (Driver mode) ======
  bool fwd = digitalRead(SW_FWD_PIN) == LOW;  // สวิตช์เดินหน้า ถูกกด (LOW) หรือไม่
  bool bwd = digitalRead(SW_BWD_PIN) == LOW;  // สวิตช์ถอยหลัง ถูกกด (LOW) หรือไม่
  if (fwd && !bwd) {
    stepper.setSpeed(MAX_SPEED);    // ถ้าเดินหน้าแต่ไม่ถอยหลัง ให้หมุนไปข้างหน้า
  } 
  else if (bwd && !fwd) {
    stepper.setSpeed(-MAX_SPEED);   // ถ้าถอยหลังแต่ไม่เดินหน้า ให้หมุนกลับทางลบ
  } 
  else {
    stepper.setSpeed(0);            // ถ้าไม่มีการกด หรือกดทั้งสองพร้อมกัน หยุดมอเตอร์
  }
  stepper.runSpeed();  // สั่งให้สเต็ปเปอร์หมุนด้วยความเร็วที่ตั้งไว้

  // ====== Counter update จากปุ่ม Up/Down ======
  bool up   = (digitalRead(SW_UP)   == LOW);   // ปุ่มเพิ่ม นับถือตอนกด (LOW)
  bool down = (digitalRead(SW_DOWN) == LOW);   // ปุ่มลด นับถือตอนกด (LOW)
  // ถ้ากดปุ่มเพิ่ม และผ่านมาอย่างน้อย REFRESH_MS เพื่อหน่วงเวลา
  if (up && !down && now - lastRefresh >= REFRESH_MS) {
    counter++;  // เพิ่มตัวนับขึ้น 1
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", counter);  // แปลงตัวเลขเป็นสตริง
    clearField(0, colValue, 4);     // ล้างพื้นที่ช่อง Counter เดิม
    lcd.setCursor(colValue, 0);     // เคอร์เซอร์ไปที่คอลัมน์ 7 บรรทัด 0
    lcd.print(buf);                 // พิมพ์ค่าตัวนับใหม่
    lastRefresh = now;              // บันทึกเวลาที่อัปเดตล่าสุด
  }
  // ถ้ากดปุ่มลด และผ่านมาอย่างน้อย REFRESH_MS เพื่อหน่วงเวลา
  else if (down && !up && now - lastRefresh >= REFRESH_MS) {
    counter--;  // ลดตัวนับลง 1
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", counter);  // แปลงตัวเลขเป็นสตริง
    clearField(0, colValue, 4);     // ล้างพื้นที่ช่อง Counter เดิม
    lcd.setCursor(colValue, 0);     // เคอร์เซอร์ไปที่คอลัมน์ 7 บรรทัด 0
    lcd.print(buf);                 // พิมพ์ค่าตัวนับใหม่
    lastRefresh = now;              // บันทึกเวลาที่อัปเดตล่าสุด
  }

  // ====== อ่านค่าจากเซ็นเซอร์ VL53L0X ทุก SENSOR_INTERVAL ms ======
  if (now - lastSensor >= SENSOR_INTERVAL) {
    uint16_t mm = sensor.readRangeContinuousMillimeters();  // อ่านค่าเป็นมิลลิเมตร
    bool to = sensor.timeoutOccurred();  // ตรวจสอบว่ามี timeout หรือไม่
    // กำหนดค่าเป็นครึ่งเซนติเมตร (dcm) ถ้าอ่านผิดพลาดให้ -1
    int dcm = (!to && mm != 65535 && mm/10 != 819) ? mm/10 : -1;
    lastSensor = now;  // บันทึกเวลาที่อ่านเซ็นเซอร์ล่าสุด
    char buf[8];
    if (dcm >= 0) {
      snprintf(buf, sizeof(buf), "%dcm", dcm);  // ถ้าอ่านได้ ให้แสดงหน่วยเซนติเมตร
    } 
    else {
      snprintf(buf, sizeof(buf), "--");        // ถ้าอ่านไม่ได้ ให้แสดง "--"
    }
    clearField(1, colDist, 4);   // ล้างพื้นที่ช่อง Dist เดิม
    lcd.setCursor(colDist, 1);   // เคอร์เซอร์ไปที่คอลัมน์ 6 แถว 1
    lcd.print(buf);              // พิมพ์ค่าระยะใหม่
    Serial.print("Dist: ");
    Serial.println(buf);         // ส่งค่าไปที่ Serial Monitor เพื่อดีบัก
  }

  // ====== อ่าน Keypad ======
  char k = keypad.getKey();  // อ่านคีย์ล่าสุด ถ้าไม่มีจะได้ '\0'
  if (k && k != lastKey) {   // ถ้ามีการกดและต่างจากครั้งก่อน
    lastKey = k;             // เก็บคีย์ล่าสุด
    clearField(2, colKey, 1);  // ล้างพื้นที่ช่อง Key เดิม
    lcd.setCursor(colKey, 2);  // เคอร์เซอร์ไปที่คอลัมน์ 5 แถว 2
    lcd.print(k);              // พิมพ์ตัวอักษรของคีย์ที่กด
    Serial.print("Key: ");
    Serial.println(k);         // ส่งค่าคีย์ไปที่ Serial Monitor
  }
}