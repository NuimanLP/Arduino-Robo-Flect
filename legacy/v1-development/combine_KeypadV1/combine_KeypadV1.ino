#include <Wire.h>
#include <AccelStepper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <VL53L0X.h>
#include <Keypad.h>

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

// —————— พินสวิตช์ Up/Down ——————
const uint8_t SW_UP   = 22;
const uint8_t SW_DOWN = 23;

// —————— เซ็นเซอร์ VL53L0X (Pololu) ——————
VL53L0X sensor;

// —————— พิน Keypad 4×4 (digital) ——————
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {32, 33, 34, 35};
byte colPins[COLS] = {28, 29, 30, 31};
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// —————— ตัวแปรควบคุม ——————
int      counter            = 0;
uint16_t bgColor;
const   float    RUN_SPEED       = 1000.0;
const   unsigned long REFRESH_MS      = 200;
const   unsigned long SENSOR_INTERVAL = 300;
unsigned long lastRefresh      = 0;
unsigned long lastSensor       = 0;
char     lastKey             = ' ';

// —————— เก็บตำแหน่ง X ของตัวเลข ——————
uint16_t xValueNum, xDistNum, xKeyNum;

// —————— ฟังก์ชันประกาศ ——————
void showCounter();
void showDistance(int cm);
void showKey(char k);

void setup() {
  // Serial และ I2C
  Serial.begin(9600);
  while (!Serial) {}
  Wire.begin();  // SDA=20, SCL=21

  // ตั้งค่า VL53L0X
  sensor.setTimeout(500);
  Serial.print(F("init VL53L0X... "));
  if (!sensor.init()) {
    Serial.println(F("FAILED"));
    while (1) {}
  }
  Serial.println(F("OK"));
  sensor.setSignalRateLimit(0.25);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
  sensor.setMeasurementTimingBudget(200000);
  sensor.startContinuous();

  // ตั้งค่า TFT
  pinMode(TFT_CS, OUTPUT);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextWrap(false);
  bgColor = tft.color565(200,200,100);
  tft.setTextColor(ST77XX_ORANGE, bgColor);

  // ตั้งค่า Stepper
  stepper.setMaxSpeed(800.0);
  stepper.setAcceleration(500.0);

  // ตั้งค่าสวิตช์ Up/Down
  pinMode(SW_UP,   INPUT_PULLUP);
  pinMode(SW_DOWN, INPUT_PULLUP);

  // วาดป้ายคงที่ แล้วเก็บตำแหน่ง Cursor หลังป้าย
  tft.setCursor(5, 35);
  tft.print(F("Value:"));
  xValueNum = tft.getCursorX();

  tft.setCursor(5, 80);
  tft.print(F("Dist:"));
  xDistNum = tft.getCursorX();

  tft.setCursor(5,125);
  tft.print(F("Key:"));
  xKeyNum = tft.getCursorX();

  // แสดงค่าเริ่มต้น
  showCounter();
  showDistance(-1);
  showKey(' ');
}

void loop() {
  unsigned long now = millis();

  // ควบคุม counter + มอเตอร์
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

  // อ่าน VL53L0X ทุก SENSOR_INTERVAL
  if (now - lastSensor >= SENSOR_INTERVAL) {
    uint16_t mm = sensor.readRangeContinuousMillimeters();
    bool to = sensor.timeoutOccurred();
    int dcm = (!to && mm != 65535) ? mm/10 : -1;
    showDistance(dcm);
    Serial.print(F("Dist: "));
    if (dcm == 819) Serial.print(F("Error"));
    else if (dcm >= 0) Serial.print(dcm);
    else               Serial.print(F("Out of range"));
    Serial.println(F(" cm"));
    lastSensor = now;
  }

  // อ่าน Keypad
  char k = keypad.getKey();
  if (k && k != lastKey) {
    lastKey = k;
    showKey(k);
    Serial.print(F("Key: "));
    Serial.println(k);
  }
}

// แสดง counter (ล้างเฉพาะตัวเลข)
void showCounter() {
  const int y = 35, w = 40, h = 16;
  tft.fillRect(xValueNum, y, w, h, bgColor);
  tft.setCursor(xValueNum, y);
  tft.print(counter);
}

// แสดงระยะ (ล้างเฉพาะตัวเลข)
void showDistance(int cm) {
  const int y = 80, w = 60, h = 16;
  tft.fillRect(xDistNum, y, w, h, bgColor);
  tft.setCursor(xDistNum, y);
  if (cm == 819) {
    tft.print(F("Error"));
  } else if (cm >= 0) {
    tft.print(cm);
    tft.print(F(" cm"));
  } else {
    tft.print(F("--"));
  }
}

// แสดงปุ่มที่กด (ล้างเฉพาะตัวอักษรเดียว)
void showKey(char k) {
  const int y = 125, w = 12, h = 16;
  tft.fillRect(xKeyNum, y, w, h, bgColor);
  tft.setCursor(xKeyNum, y);
  tft.print(k);
}