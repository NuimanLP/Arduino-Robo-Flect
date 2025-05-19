#include <Wire.h>
#include <AccelStepper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_VL53L0X.h>

// —————— พิน Stepper ——————
#define IN1 24
#define IN2 25
#define IN3 26
#define IN4 27
AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);

// —————— พิน TFT ——————
#define TFT_CS   53
#define TFT_DC    9
#define TFT_RST   8
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// —————— พินสวิตช์ ——————
const uint8_t SW_UP   = 22;
const uint8_t SW_DOWN = 23;

// —————— พินเซ็นเซอร์ VL53L0X ——————
#define PIN_XSHUT 7    // เชื่อม VIN ถ้าไม่ใช้ XSHUT
Adafruit_VL53L0X lox;

// —————— ตัวแปรควบคุม ——————
int      counter        = 0;                    // ค่าตัวนับบนจอ
uint16_t bgColor;                                 // พื้นหลังตัวอักษร
const   float RUN_SPEED     = 600.0;              // ความเร็ว step/s
const   unsigned long REFRESH_MS    = 180;        // อัปเดตรอบ counter (ms)
const   unsigned long SENSOR_INTERVAL = 200;      // อัปเดตเซ็นเซอร์ (ms)
unsigned long lastRefresh   = 0;
unsigned long lastSensor    = 0;

void setup() {
  // —– ตั้งค่า Stepper —–
  stepper.setMaxSpeed(800.0);
  stepper.setAcceleration(1000.0);

  // —– ตั้งค่า TFT —–
  pinMode(TFT_CS, OUTPUT);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextWrap(false);
  bgColor = tft.color565(200, 200, 100);
  tft.setTextColor(ST77XX_ORANGE, bgColor);

  // —– ตั้งค่าสวิตช์ (active-low) —–
  pinMode(SW_UP,   INPUT_PULLUP);
  pinMode(SW_DOWN, INPUT_PULLUP);

  // —– ตั้งค่าเซ็นเซอร์ VL53L0X —–
  pinMode(PIN_XSHUT, OUTPUT);
  digitalWrite(PIN_XSHUT, LOW);
  delay(10);
  digitalWrite(PIN_XSHUT, HIGH);
  delay(10);

  Wire.begin();  // SDA=20, SCL=21 on Mega
  if (!lox.begin()) {
    Serial.begin(9600);
    Serial.println(F("Failed to boot VL53L0X"));
    while (1) delay(10);
  }
  Serial.begin(9600);
  Serial.println(F("VL53L0X ready!"));

  // แสดงค่าเริ่มต้น
  showCounter();
  showDistance(0);
}

void loop() {
  unsigned long now = millis();

  // —– อ่านปุ่มและอัปเดต counter —–
  bool up   = (digitalRead(SW_UP)   == LOW);
  bool down = (digitalRead(SW_DOWN) == LOW);
  if (up) {
    stepper.setSpeed( RUN_SPEED );
    if (now - lastRefresh >= REFRESH_MS) {
      counter++;
      showCounter();
      lastRefresh = now;
    }
  }
  else if (down) {
    stepper.setSpeed(-RUN_SPEED);
    if (now - lastRefresh >= REFRESH_MS) {
      counter--;
      showCounter();
      lastRefresh = now;
    }
  }
  else {
    stepper.setSpeed(0);
    lastRefresh = now;
  }

  // —– ก้าวสเต็ปตามความเร็ว —–
  stepper.runSpeed();

  // —– อ่านเซ็นเซอร์ทุก SENSOR_INTERVAL —–
  if (now - lastSensor >= SENSOR_INTERVAL) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);
    int dist_cm = -1;
    if (measure.RangeStatus != 4) {
      dist_cm = measure.RangeMilliMeter / 10;
    }
    showDistance(dist_cm);
    Serial.print(F("Distance: "));
    if (dist_cm >= 0) Serial.print(dist_cm);
    else            Serial.print(F("Out of range"));
    Serial.println(F(" cm"));
    lastSensor = now;
  }
}

// —————— ฟังก์ชันแสดงผล counter ——————
void showCounter() {
  tft.fillRect(5, 35, 130, 40, bgColor);
  tft.setCursor(5, 35);
  char buf[16];
  sprintf(buf, "Value:%d   ", counter);
  for (uint8_t i = 0; buf[i]; i++) {
    tft.print(buf[i]);
    stepper.runSpeed();
  }
}

// —————— ฟังก์ชันแสดงผล distance ——————
void showDistance(int cm) {
  tft.fillRect(5, 80, 130, 20, bgColor);
  tft.setCursor(5, 80);
  if (cm >= 0) {
    tft.print(F("Dist: "));
    tft.print(cm);
    tft.print(F(" cm"));
  } else {
    tft.print(F("Out of range"));
  }
}
