#include <Wire.h>
#include <AccelStepper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <VL53L0X.h>
#include <Keypad.h>

#define IN1 24
#define IN2 25
#define IN3 26
#define IN4 27
#define TFT_CS 53
#define TFT_DC 9
#define TFT_RST 8

const uint8_t SW_UP = 22;
const uint8_t SW_DOWN = 23;

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
byte rowPins[ROWS] = {32,33,34,35};
byte colPins[COLS] = {28,29,30,31};

AccelStepper stepper(AccelStepper::HALF4WIRE, IN1,IN3,IN2,IN4);
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
VL53L0X sensor;
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

int counter = 0;
char lastKey = ' ';
uint16_t bgColor, xValueNum, xDistNum, xKeyNum;
const float RUN_SPEED = 1000.0;
const unsigned long REFRESH_MS = 200, SENSOR_INTERVAL = 300;
unsigned long lastRefresh = 0, lastSensor = 0;

void updateField(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const String &text) {
  tft.fillRect(x, y, w, h, bgColor);
  tft.setCursor(x, y);
  tft.print(text);
}

void setup() {
  Serial.begin(9600);
  while (!Serial);
  Wire.begin();

  sensor.setTimeout(500);
  if (!sensor.init()) while (1);
  sensor.setSignalRateLimit(0.25);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
  sensor.setMeasurementTimingBudget(200000);
  sensor.startContinuous();

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextWrap(false);
  bgColor = tft.color565(200,200,100);
  tft.setTextColor(ST77XX_ORANGE, bgColor);

  stepper.setMaxSpeed(800.0);
  stepper.setAcceleration(500.0);

  pinMode(SW_UP, INPUT_PULLUP);
  pinMode(SW_DOWN, INPUT_PULLUP);

  tft.setCursor(5,35); tft.print("Value:"); xValueNum = tft.getCursorX();
  tft.setCursor(5,80); tft.print("Dist:"); xDistNum = tft.getCursorX();
  tft.setCursor(5,125); tft.print("Key:"); xKeyNum = tft.getCursorX();

  updateField(xValueNum,35,40,16,String(counter));
  updateField(xDistNum,80,60,16,"--");
  updateField(xKeyNum,125,12,16,String(lastKey));
}

void loop() {
  unsigned long now = millis();
  bool up = (digitalRead(SW_UP)==LOW), down = (digitalRead(SW_DOWN)==LOW);
  if (up) {
    stepper.setSpeed(RUN_SPEED);
    if (now - lastRefresh >= REFRESH_MS) { counter++; lastRefresh = now; }
  } else if (down) {
    stepper.setSpeed(-RUN_SPEED);
    if (now - lastRefresh >= REFRESH_MS) { counter--; lastRefresh = now; }
  } else {
    stepper.setSpeed(0); lastRefresh = now;
  }
  stepper.runSpeed();

  if (now - lastSensor >= SENSOR_INTERVAL) {
    uint16_t mm = sensor.readRangeContinuousMillimeters();
    bool to = sensor.timeoutOccurred();
    int dcm = (!to && mm != 65535) ? mm/10 : -1;
    lastSensor = now;
    String distText = (dcm == 819) ? "Error" : (dcm >= 0 ? String(dcm)+" cm" : "--");
    updateField(xDistNum,80,60,16,distText);
    Serial.print("Dist: "); Serial.println(distText);
  }

  updateField(xValueNum,35,40,16,String(counter));

  char k = keypad.getKey();
  if (k && k != lastKey) {
    lastKey = k;
    updateField(xKeyNum,125,12,16,String(k));
    Serial.print("Key: "); Serial.println(k);
  }
}
