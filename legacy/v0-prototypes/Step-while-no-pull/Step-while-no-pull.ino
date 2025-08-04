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
int counter       = 0;
bool lastUp       = HIGH;
bool lastDown     = HIGH;
uint16_t bgColor  = tft.color565(200,200,100);
const float RUN_SPEED = 500.0;  // สเต็ป/วินาทีขณะหมุนต่อเนื่อง

void setup() {
  // —– Stepper —–
  stepper.setMaxSpeed(800.0);
  stepper.setAcceleration(500.0);

  // —– TFT —–
  pinMode(TFT_CS, OUTPUT);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextWrap(false);
  tft.setTextColor(ST77XX_ORANGE, bgColor);

  // —– สวิตช์ —–
  pinMode(SW_UP,   INPUT_PULLUP);
  pinMode(SW_DOWN, INPUT_PULLUP);

  showCounter();
}

void loop() {
  bool upState   = digitalRead(SW_UP);
  bool downState = digitalRead(SW_DOWN);

  if (upState == LOW) {
    if (lastUp == HIGH) {
      counter++;
      showCounter();
    }
    stepper.setSpeed( RUN_SPEED );
  }
  else if (downState == LOW) {
    if (lastDown == HIGH) {
      counter--;
      showCounter();
    }
    stepper.setSpeed( -RUN_SPEED );
  }
  else {
    stepper.setSpeed( 0 );
  }

  stepper.runSpeed();

  lastUp   = upState;
  lastDown = downState;
}

// —————— ฟังก์ชันแสดงผลตัวนับ ——————
void showCounter() {
  // ลบพื้นหลังเฉพาะพื้นที่แสดงผล (130x40 พอดีกับขนาดข้อความ)
  tft.fillRect(5, 35, 130, 40, bgColor);

  tft.setCursor(5, 35);
  tft.print("Value:");
  tft.print(counter);
  tft.print("   ");  // เว้นให้ลบเลขเก่าที่อาจเหลือ

  // (ถ้าต้องการพิมพ์ HELLO-ROBO-FLECT ด้วย ให้เพิ่มบรรทัดนี้)
  // tft.setCursor(5, 15);
  // tft.print("HELLO-ROBO-FLECT");
}