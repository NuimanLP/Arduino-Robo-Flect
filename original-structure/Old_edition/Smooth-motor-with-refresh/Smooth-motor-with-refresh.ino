#include <AccelStepper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

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

// —————— ตัวแปร ——————
int counter = 0;                       // ค่าตัวนับที่แสดงบนจอ
uint16_t bgColor;                      // สีพื้นหลังตัวเลข
const float RUN_SPEED     = 600.0;     // ความเร็วขณะหมุน (step/s)
const unsigned long REFRESH_MS = 180;  // หน่วงการอัปเดตตัวนับ (ms)
unsigned long lastRefresh = 0;

void setup() {
  // —– ตั้งค่า Stepper —–
  stepper.setMaxSpeed(800.0);       // ความเร็วสูงสุด (step/s)
  stepper.setAcceleration(1000.0); // เร่งขึ้นทันที (ramp-up เกิดเร็ว)

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

  showCounter();  // วาดค่าเริ่มต้น
}

void loop() {
  bool up   = (digitalRead(SW_UP)   == LOW);
  bool down = (digitalRead(SW_DOWN) == LOW);
  unsigned long now = millis();

  if (up) {
    stepper.setSpeed( RUN_SPEED );
    if (now - lastRefresh >= REFRESH_MS) {
      counter++;
      showCounter();
      lastRefresh = now;
    }
  }
  else if (down) {
    stepper.setSpeed( -RUN_SPEED );
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

  // ส่ง pulse ตาม speed ที่ตั้งไว้
  stepper.runSpeed();
}

// —————— ฟังก์ชันแสดงผลตัวนับ (ปรับตามวิธี 1) ——————
void showCounter() {
  // ลบพื้นที่ตัวเลขเก่า
  tft.fillRect(5, 35, 130, 40, bgColor);
  tft.setCursor(5, 35);

  // เตรียมข้อความลงบัฟเฟอร์
  char buf[16];
  sprintf(buf, "Value:%d   ", counter);

  // พิมพ์ทีละตัว แล้วให้มอเตอร์ก้าวสเต็ประหว่างพิมพ์
  for (uint8_t i = 0; buf[i] != '\0'; i++) {
    tft.print(buf[i]);
    stepper.runSpeed();
  }
}
