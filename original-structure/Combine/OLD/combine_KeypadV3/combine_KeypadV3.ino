#include <Wire.h>
#include <AccelStepper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <VL53L0X.h>
#include <Keypad.h>

// ====== Stepping Motor (Driver mode) ======
#define STEP_PIN     14       // STEP signal .7
#define DIR_PIN      15      // DIR signal ,6
#define SW_FWD_PIN  16       // forward switch (active LOW) , 22
#define SW_BWD_PIN  17       // backward switch (active LOW),23

const float MAX_SPEED    = 1333.0;
const float ACCELERATION = 2000.0;
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ====== TFT Display ======
#define TFT_CS   53
#define TFT_DC    9
#define TFT_RST   8
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ====== Counter Buttons ======
const uint8_t SW_UP   = 16; //,22
const uint8_t SW_DOWN = 17; //,23

// ====== VL53L0X Distance Sensor ======
VL53L0X sensor;

// ====== Keypad 4x4 (Direct GPIO) ======
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
byte rowPins[ROWS] = {30,31,32,33};
byte colPins[COLS] = {26,27,28,29};
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ====== Global Variables ======
int counter = 0;
char lastKey = ' ';
uint16_t bgColor;
uint16_t xValueNum, xDistNum, xKeyNum;
const unsigned long REFRESH_MS      = 200;
const unsigned long SENSOR_INTERVAL = 300;
unsigned long lastRefresh = 0, lastSensor = 0;

// ====== Helper: clear and print field ======
void updateField(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const String &text) {
  tft.fillRect(x, y, w, h, bgColor);
  tft.setCursor(x, y);
  tft.print(text);
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // Check VL53L0X Sensor
  Serial.println("Checking VL53L0X...");
  Wire.begin();
  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("VL53L0X init failed!");
    while (1);
  }
  sensor.setSignalRateLimit(0.25);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
  sensor.setMeasurementTimingBudget(200000);
  sensor.startContinuous();
  Serial.println("VL53L0X OK");

  // Check TFT Display
  Serial.println("Checking TFT...");
  pinMode(TFT_CS, OUTPUT);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  tft.setTextWrap(false);
  bgColor = tft.color565(200,200,100);
  tft.setTextColor(ST77XX_ORANGE, bgColor);
  Serial.println("TFT OK");

  // Check Stepper Motor
  Serial.println("Checking Stepper...");
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  pinMode(SW_FWD_PIN, INPUT_PULLUP);
  pinMode(SW_BWD_PIN, INPUT_PULLUP);
  Serial.println("Stepper OK");

  // Check Counter Buttons
  Serial.println("Checking counter buttons...");
  pinMode(SW_UP, INPUT_PULLUP);
  pinMode(SW_DOWN, INPUT_PULLUP);
  Serial.println("Counter buttons OK");

  // Check Keypad
  Serial.println("Checking Keypad...");
  // No init needed for direct GPIO Keypad
  Serial.println("Keypad OK");

  // Print static labels and record cursor X
  tft.setCursor(5, 35);
  tft.print("Value:"); xValueNum = tft.getCursorX();
  tft.setCursor(5, 80);
  tft.print("Dist:");  xDistNum  = tft.getCursorX();
  tft.setCursor(5,125);
  tft.print("Key:");   xKeyNum   = tft.getCursorX();

  // Initialize display fields
  updateField(xValueNum,35,40,16,String(counter));
  updateField(xDistNum, 80,60,16,"--");
  updateField(xKeyNum,125,12,16,String(lastKey));
}

void loop() {
  unsigned long now = millis();

  // Stepping Motor control (Driver)
  bool fwd = digitalRead(SW_FWD_PIN) == LOW;
  bool bwd = digitalRead(SW_BWD_PIN) == LOW;
  if (fwd && !bwd) stepper.setSpeed(MAX_SPEED);
  else if (bwd && !fwd) stepper.setSpeed(-MAX_SPEED);
  else stepper.setSpeed(0);
  stepper.runSpeed();

  // Counter update from buttons
  bool up   = (digitalRead(SW_UP)   == LOW);
  bool down = (digitalRead(SW_DOWN) == LOW);
  if (up && !down && now - lastRefresh >= REFRESH_MS) {
    counter++;
    updateField(xValueNum,35,40,16,String(counter));
    lastRefresh = now;
  } else if (down && !up && now - lastRefresh >= REFRESH_MS) {
    counter--;
    updateField(xValueNum,35,40,16,String(counter));
    lastRefresh = now;
  }

  // Read VL53L0X sensor
  if (now - lastSensor >= SENSOR_INTERVAL) {
    uint16_t mm = sensor.readRangeContinuousMillimeters();
    bool to = sensor.timeoutOccurred();
    int dcm = (!to && mm != 65535 && mm/10 != 819) ? mm/10 : -1;
    lastSensor = now;
    String distText = (dcm >= 0 ? String(dcm)+" cm" : "--");
    updateField(xDistNum,80,60,16,distText);
    Serial.print("Dist: "); Serial.println(distText);
  }

  // Read Keypad
  char k = keypad.getKey();
  if (k && k != lastKey) {
    lastKey = k;
    updateField(xKeyNum,125,12,16,String(k));
    Serial.print("Key: "); Serial.println(k);
  }
}