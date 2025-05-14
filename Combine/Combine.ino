#include <Wire.h>
#include <AccelStepper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <VL53L0X.h>

// —————— พิน Stepper ——————
#define IN1 24
#define IN2 25
#define IN3 26
#define IN4 27
AccelStepper stepper(AccelStepper::HALF4WIRE, IN1, IN3, IN2, IN4);

// —————— พิน TFT ——————
#define TFT_CS    53
#define TFT_DC     9
#define TFT_RST    8
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// —————— พินสวิตช์ ——————
const uint8_t SW_UP   = 22;
const uint8_t SW_DOWN = 23;

// —————— อ็อบเจ็กต์เซ็นเซอร์ VL53L0X (Pololu) ——————
VL53L0X sensor;

int      counter         = 0;                    // ค่าตัวนับบนจอ
uint16_t bgColor;                                // สีพื้นหลังตัวอักษร
const   float    RUN_SPEED       = 1000.0;        // ความเร็ว step/s
const   unsigned long REFRESH_MS      = 200;      // อัปเดต counter ทุก 200 ms
const   unsigned long SENSOR_INTERVAL = 300;      // อ่านเซ็นเซอร์ทุก 200 ms
unsigned long lastRefresh   = 0;
unsigned long lastSensor    = 0;

void setup() {
  // 1) Serial monitor
  Serial.begin(9600);
  while (!Serial) {}
  Serial.println(F(">> setup start"));

  // 2) I2C และตั้งค่า VL53L0X
  Wire.begin();  // Mega: SDA=20, SCL=21
  sensor.setTimeout(500);

  Serial.print(F("init VL53L0X... "));
  if (!sensor.init()) {               // init() แทน begin()
    Serial.println(F("FAILED"));
    while (1) {}
  }
  Serial.println(F("OK"));

  // 🔧 ปรับโหมด Long Range + High Accuracy
  sensor.setSignalRateLimit(0.25);     // Mcps (default 0.25)  [oai_citation:0‡GitHub](https://github.com/pololu/vl53l0x-arduino/blob/master/VL53L0X.cpp?utm_source=chatgpt.com)
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
  sensor.setMeasurementTimingBudget(200000); // μs (200 ms)  [oai_citation:1‡GitHub](https://github.com/pololu/vl53l0x-arduino/blob/master/VL53L0X.cpp?utm_source=chatgpt.com)

  // เริ่ม Continuous Ranging (เร็วสุดไม่หน่วง)  [oai_citation:2‡FritzenLab electronics](https://fritzenlab.net/2024/08/25/laser-distance-sensor-vl53l0x-time-of-flight/?utm_source=chatgpt.com)
  sensor.startContinuous();

  // 3) ตั้งค่า TFT
  Serial.println(F("init TFT"));
  pinMode(TFT_CS, OUTPUT);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextWrap(false);
  bgColor = tft.color565(200,200,100);
  tft.setTextColor(ST77XX_ORANGE, bgColor);

  // 4) ตั้งค่า Stepper
  Serial.println(F("init stepper"));
  stepper.setMaxSpeed(800.0);
  stepper.setAcceleration(500.0);

  // 5) ตั้งค่าสวิตช์ (active-low)
  Serial.println(F("init switches"));
  pinMode(SW_UP,   INPUT_PULLUP);
  pinMode(SW_DOWN, INPUT_PULLUP);

  // วาดค่าเริ่มต้นบนหน้าจอ
  showCounter();
  showDistance(-1);

  Serial.println(F(">> setup done"));
}

void loop() {
  unsigned long now = millis();

  // —– ปุ่ม counter + มอเตอร์ –—
  bool up   = (digitalRead(SW_UP)   == LOW);
  bool down = (digitalRead(SW_DOWN) == LOW);
  if (up) {
    stepper.setSpeed(RUN_SPEED);
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
  stepper.runSpeed();

  // —– อ่าน distance ทุก SENSOR_INTERVAL –—
  if (now - lastSensor >= SENSOR_INTERVAL) {
    uint16_t mm = sensor.readRangeContinuousMillimeters();
    bool to = sensor.timeoutOccurred();
    int dcm = (!to && mm != 65535) ? mm/10 : -1;

    showDistance(dcm);
    Serial.print(F("Dist: "));
    if (dcm >= 0) Serial.print(dcm);
    else          Serial.print(F("Out of range"));
    Serial.println(F(" cm"));

    lastSensor = now;
  }
}

// —— ฟังก์ชันแสดง counter ——
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

// —— ฟังก์ชันแสดงระยะ ——
void showDistance(int cm) {
  tft.fillRect(5, 80, 130, 20, bgColor);
  tft.setCursor(5, 80);
  if (cm >= 0) {
    tft.print(F("Dist:"));
    tft.print(cm);
    tft.print(F(" cm"));
  } else {
    tft.print(F("Out of range"));
  }
}