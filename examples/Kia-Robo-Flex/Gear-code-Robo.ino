/*
  Refactored Robo-Flect Firmware
  - RFID + Passcode Access Control
  - Training Mode: move stepper to fixed distances
  - Testing Mode: random distances + scoring
  - Gear selection affects stepper speed
  - Clean state machine structure
*/

#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Keypad_I2C.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>

// ----- Pin & Address Definitions -----
constexpr uint8_t SS_PIN        = 10;
constexpr uint8_t RST_PIN       = 9;
constexpr uint8_t BUZZER_PIN    = 3;
constexpr uint8_t KEYPAD_ADDR   = 0x20;
constexpr uint8_t LCD_ADDR      = 0x27;
constexpr uint8_t STEPPER_IN1   = 4;
constexpr uint8_t STEPPER_IN2   = 6;
constexpr uint8_t STEPPER_IN3   = 5;
constexpr uint8_t STEPPER_IN4   = 7;

// ----- Hardware Objects -----
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
AccelStepper stepper(AccelStepper::FULL4WIRE, STEPPER_IN1, STEPPER_IN3, STEPPER_IN2, STEPPER_IN4);

// ----- Keypad Setup -----
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {4,5,6,7};   // I2C expander pins
byte colPins[COLS] = {0,1,2,3};   // I2C expander pins
Keypad_I2C keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, KEYPAD_ADDR);

// ----- Constants -----
constexpr float DEFAULT_MAX_SPEED   = 500.0;
constexpr float DEFAULT_ACCEL       = 200.0;
constexpr char  DEFAULT_PASSCODE[]  = "1234";
constexpr uint8_t TEST_ROUNDS       = 10;

// List of authorized RFID UIDs (big-endian concatenation)
const uint32_t authorizedUids[] = {
  0xDEADBEEF,  // example UID 1
  0xCAFEBABE   // example UID 2
};

// ----- System States -----
enum class SystemState {
  Idle,
  AwaitRFID,
  AwaitPasscode,
  MenuSelection,
  TrainingMode,
  TestingMode
};
SystemState state = SystemState::Idle;

bool accessGranted = false;
bool trainingMode = true;
float gearFactor = 1.0;
uint16_t currentDistance = 200;
uint8_t testScore = 0;

// ----- Helper Functions -----
void showMessage(const String &line1, const String &line2 = "") {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(line1);
  if (line2.length()) {
    lcd.setCursor(0,1);
    lcd.print(line2);
  }
}

bool validateRFID() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return false;
  uint32_t uid = 0;
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid = (uid << 8) | rfid.uid.uidByte[i];
  }
  for (uint32_t auth : authorizedUids) {
    if (auth == uid) return true;
  }
  return false;
}

bool validatePasscode() {
  String entry;
  showMessage("Enter Passcode:", "****");
  while (entry.length() < 4) {
    char key = keypad.getKey();
    if (key >= '0' && key <= '9') {
      entry += key;
      lcd.setCursor(entry.length()-1,1);
      lcd.print("*");
    }
  }
  return (entry == DEFAULT_PASSCODE);
}

char waitKey() {
  char k;
  do { k = keypad.getKey(); } while (!k);
  return k;
}

// ----- Mode Handlers -----
void enterTrainingMode() {
  state = SystemState::TrainingMode;
  showMessage("Training Mode", "(1-5)=10-50cm");
}

void processTraining() {
  char key = waitKey();
  if (key >= '1' && key <= '5') {
    currentDistance = (key - '0') * 200;
    showMessage("Moving to", String((key - '0')*10)+"cm");
    stepper.moveTo(currentDistance);
    while (stepper.distanceToGo()) stepper.run();
    showMessage("Position at", String((key - '0')*10)+"cm");
    delay(1000);
  } else if (key == '*') {
    state = SystemState::MenuSelection;
  }
}

void enterTestingMode() {
  state = SystemState::TestingMode;
  testScore = 0;
  showMessage("Testing Mode", "Ready...");
  delay(1000);
}

void processTesting() {
  for (uint8_t round = 0; round < TEST_ROUNDS; round++) {
    uint16_t target = (random(1,6) * 200);
    stepper.moveTo(target);
    while (stepper.distanceToGo()) stepper.run();
    showMessage("Enter Position", "1-5");
    char key = waitKey();
    uint16_t guess = (key - '0') * 200;
    if (guess == target) { testScore++; showMessage("Correct!", ""); }
    else showMessage("Wrong!", "");
    delay(800);
  }
  showMessage("Score:", String(testScore) + "/" + TEST_ROUNDS);
  delay(2000);
  state = SystemState::MenuSelection;
}

// ----- Setup & Loop -----
void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  Wire.begin();
  keypad.begin();
  lcd.begin();   // ใช้ begin() แบบไม่มีพารามิเตอร์
  lcd.backlight();

  pinMode(BUZZER_PIN, OUTPUT);

  stepper.setMaxSpeed(DEFAULT_MAX_SPEED);
  stepper.setAcceleration(DEFAULT_ACCEL);

  randomSeed(analogRead(A0));
  showMessage("Robo-Flect Ready");
  delay(1000);
  state = SystemState::AwaitRFID;
}

void loop() {
  switch (state) {
    case SystemState::AwaitRFID:
      showMessage("Scan RFID Tag");
      if (validateRFID()) {
        showMessage("RFID OK", "Enter Passcode");
        state = SystemState::AwaitPasscode;
      }
      break;

    case SystemState::AwaitPasscode:
      if (validatePasscode()) {
        accessGranted = true;
        showMessage("Access Granted");
        delay(1000);
        state = SystemState::MenuSelection;
      } else {
        showMessage("Access Denied");
        delay(1000);
        state = SystemState::AwaitRFID;
      }
      break;

    case SystemState::MenuSelection:
      showMessage("Select Mode", "C:Train D:Test");
      {
        char m = waitKey();
        if (m == 'C') enterTrainingMode();
        else if (m == 'D') enterTestingMode();
        else if (m == '#') {
          accessGranted = false;
          state = SystemState::AwaitRFID;
        }
      }
      break;

    case SystemState::TrainingMode:
      processTraining();
      break;

    case SystemState::TestingMode:
      processTesting();
      break;

    default:
      state = SystemState::AwaitRFID;
      break;
  }
}
