#include <Wire.h>                // ไลบรารี I2C สำหรับสื่อสารกับเซ็นเซอร์และ PCF8574
#include <AccelStepper.h>         // ไลบรารีควบคุม Stepper Motor แบบ non-blocking
#include <Adafruit_GFX.h>         // ไลบรารีกราฟิกฐานสำหรับ TFT
#include <Adafruit_ST7735.h>      // ไลบรารีควบคุมจอ ST7735 over SPI
#include <VL53L0X.h>              // ไลบรารีเซ็นเซอร์วัดระยะ VL53L0X
#include <Keypad.h>               // ไลบรารีอ่าน Keypad 4x4 (polling)

// ====== กำหนดพิน Stepper Motor (4 ขด, HALF4WIRE) ======
#define IN1 24  // ขด 1
#define IN2 25  // ขด 2
#define IN3 26  // ขด 3
#define IN4 27  // ขด 4
AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);

// ====== กำหนดพิน TFT ST7735 (SPI) ======
#define TFT_CS  53  // Chip Select
#define TFT_DC   9  // Data/Command
#define TFT_RST  8  // Reset
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ====== กำหนดพินปุ่มกด Up/Down ======
const uint8_t SW_UP   = 22; // ปุ่มเพิ่ม (active LOW)
const uint8_t SW_DOWN = 23; // ปุ่มลด (active LOW)

// ====== สร้างอ็อบเจ็กต์ VL53L0X ======
VL53L0X sensor;

// ====== กำหนด Keypad 4x4 (ต่อกับ digital pins) ======
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {{'1','2','3','A'}, {'4','5','6','B'}, {'7','8','9','C'}, {'*','0','#','D'}};
byte rowPins[ROWS] = {32, 33, 34, 35}; // ต่อกับ Row ของ keypad
byte colPins[COLS] = {28, 29, 30, 31}; // ต่อกับ Column ของ keypad
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ====== ตัวแปรควบคุมหลัก ======
int counter = 0;                       // นับค่าจากปุ่ม Up/Down
uint16_t bgColor;                      // สีพื้นหลังข้อความบน TFT
const float RUN_SPEED = 1000.0;        // ความเร็วหมุน stepper (steps/sec)
const unsigned long REFRESH_MS = 200;  // ความถี่อัปเดต counter (ms)
const unsigned long SENSOR_INTERVAL = 300; // ความถี่อ่าน VL53L0X (ms)
unsigned long lastRefresh = 0;         // เวลาอัปเดต counter ล่าสุด
unsigned long lastSensor = 0;          // เวลาอ่าน VL53L0X ล่าสุด
char lastKey = ' ';                    // เก็บปุ่ม keypad ล่าสุด

// ====== เก็บตำแหน่ง x ของแต่ละฟิลด์ ======
uint16_t xValueNum, xDistNum, xKeyNum;

// ====== ฟังก์ชันช่วยลบพื้นที่และพิมพ์ข้อความใหม่ ======
void updateField(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const String &text) {
  tft.fillRect(x, y, w, h, bgColor);     // ลบพื้นที่เดิม
  tft.setCursor(x, y);                   // ตั้ง cursor
  tft.print(text);                       // พิมพ์ข้อความใหม่
  delay(100);
}

void setup() {
  // --- เริ่ม Serial Monitor ---
  Serial.begin(9600);
  while (!Serial);
  // --- เริ่ม I2C bus ---
  Wire.begin();
  // --- ตั้งค่าและตรวจสอบ VL53L0X ---
  sensor.setTimeout(500);
  Serial.print("init VL53L0X... ");
  if (!sensor.init()) { Serial.println("FAILED"); while (1); }
  Serial.println("OK");
  sensor.setSignalRateLimit(0.25);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
  sensor.setMeasurementTimingBudget(200000);
  sensor.startContinuous();
  // --- ตั้งค่า TFT ---
  pinMode(TFT_CS, OUTPUT);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextWrap(false);
  bgColor = tft.color565(200,200,100);
  tft.setTextColor(ST77XX_ORANGE, bgColor);
  // --- ตั้งค่า Stepper ---
  stepper.setMaxSpeed(800.0);
  stepper.setAcceleration(500.0);
  // --- ตั้งค่าสวิตช์ Up/Down ---
  pinMode(SW_UP, INPUT_PULLUP);
  pinMode(SW_DOWN, INPUT_PULLUP);
  // --- พิมพ์ label คงที่และเก็บตำแหน่ง x ---
  tft.setCursor(5, 35); tft.print("Value:"); xValueNum = tft.getCursorX();
  tft.setCursor(5, 80); tft.print("Dist:");  xDistNum = tft.getCursorX();
  tft.setCursor(5,125); tft.print("Key:");   xKeyNum = tft.getCursorX();
}

void loop() {
  unsigned long now = millis();      // เวลาปัจจุบัน (ms)
  // --- ปุ่ม Up/Down ควบคุม counter + motor ---
  bool up = (digitalRead(SW_UP)==LOW), down = (digitalRead(SW_DOWN)==LOW);
  if (up) {
    stepper.setSpeed(RUN_SPEED);
    if (now-lastRefresh>=REFRESH_MS) { counter++; lastRefresh=now; }
  } else if (down) {
    stepper.setSpeed(-RUN_SPEED);
    if (now-lastRefresh>=REFRESH_MS) { counter--; lastRefresh=now; }
  } else {
    stepper.setSpeed(0); lastRefresh=now;
  }
  stepper.runSpeed();
  // --- อ่าน VL53L0X ตาม interval ---
  if (now-lastSensor>=SENSOR_INTERVAL) {
    uint16_t mm = sensor.readRangeContinuousMillimeters();
    bool to = sensor.timeoutOccurred();
    int dcm = (!to && mm!=65535)? mm/10 : -1;
    lastSensor = now;
    // เตรียมข้อความแสดงระยะ พร้อมเช็คกรณีพิเศษ 819
    String distText;
    if (dcm == 819) {
      distText = "Error";
    } else if (dcm >= 0) {
      distText = String(dcm) + " cm";
    } else {
      distText = "--";
    }
    updateField(xDistNum, 80, 60, 16, distText);   // อัปเดตฟิลด์ Dist
    Serial.print("Dist: ");
    if(dcm==819) Serial.println("Error"); else Serial.println(distText);
  }
  // --- อัปเดต counter ---
  updateField(xValueNum, 35, 40, 16, String(counter));
  // --- อ่านและอัปเดต Keypad ---
  char k = keypad.getKey();
  if (k && k!=lastKey) {
    lastKey = k;
    updateField(xKeyNum,125,12,16,String(k));
    Serial.print("Key: "); Serial.println(k);
  }
}
