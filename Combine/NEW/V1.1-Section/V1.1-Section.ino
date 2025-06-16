#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>
#include <VL53L0X.h>
#include <Keypad.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

#define STEP_PIN        14
#define DIR_PIN         15
#define SW_FWD_PIN      16
#define SW_BWD_PIN      17
#define BUZZER_PIN      18
#define LED_RED_PIN     19
#define LED_GREEN_PIN   20
#define LED_BLUE_PIN    21
#define ESP32_RX_PIN    22
#define ESP32_TX_PIN    23

const float MAX_SPEED = 1333.0;
const float ACCELERATION = 2000.0;
const unsigned long REFRESH_INTERVAL = 100;
const unsigned long SENSOR_INTERVAL = 200;
const unsigned long IDLE_TIMEOUT = 15000;
const unsigned long BOOT_ANIMATION_DELAY = 500;

const int DISTANCE_POSITIONS[] = {10, 20, 30, 40, 50, 60};
const int NUM_POSITIONS = 6;
const int STEPS_PER_CM = 200;

enum SystemState {
  STATE_BOOT,
  STATE_SELF_CHECK,
  STATE_IDLE_AWAIT_RFID,
  STATE_MAIN_MENU,
  STATE_MODE_SELECTION,
  STATE_TRAINING,
  STATE_TESTING,
  STATE_TUTORIALS,
  STATE_SETTINGS,
  STATE_CALIBRATION,
  STATE_ERROR
};

enum MenuOption {
  MENU_MODE = 1,
  MENU_TUTORIALS = 2,
  MENU_SETTINGS = 3,
  MENU_LOGOUT = 0
};

enum ModeType {
  MODE_TRAINING,
  MODE_TESTING
};

struct UserProfile {
  String uid;
  String nickname;
  int level;
  int progress;
  unsigned long lastLogin;
  bool isValid;
};

struct SystemStatus {
  bool wifiConnected;
  bool offlineMode;
  bool sensorReady;
  bool stepperReady;
  bool keypadReady;
  bool lcdReady;
  bool esp32Ready;
};

LiquidCrystal_I2C lcd(0x27, 20, 4);
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
VL53L0X sensor;
SoftwareSerial esp32Serial(ESP32_RX_PIN, ESP32_TX_PIN);

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {30, 31, 32, 33};
byte colPins[COLS] = {26, 27, 28, 29};
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

SystemState currentState = STATE_BOOT;
SystemStatus systemStatus = {false, false, false, false, false, false, false};
UserProfile currentUser = {"", "", 1, 0, 0, false};
unsigned long lastActivity = 0;
unsigned long stateStartTime = 0;
int currentMenuSelection = 1;
int targetDistance = 30;
bool isTrainingMode = true;

unsigned long lastRefresh = 0;
unsigned long lastSensorRead = 0;
unsigned long lastIdlePrompt = 0;
unsigned long bootAnimationStart = 0;

String sessionData = "";
int trainingAttempts = 0;
int testScore = 0;

void setup() {
  Serial.begin(9600);
  esp32Serial.begin(9600);
  currentState = STATE_BOOT;
  stateStartTime = millis();
  bootAnimationStart = millis();
  initializeHardware();
  displayBootScreen();
}

void initializeHardware() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(SW_FWD_PIN, INPUT_PULLUP);
  pinMode(SW_BWD_PIN, INPUT_PULLUP);
  Wire.begin();
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  systemStatus.lcdReady = true;
  initializeSensor();
  initializeStepper();
  playBootSound();
  Serial.println(F("Hardware initialization complete"));
}

void initializeSensor() {
  Serial.println(F("Initializing VL53L0X..."));
  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println(F("VL53L0X init failed!"));
    systemStatus.sensorReady = false;
    setLEDColor(255, 0, 0);
    return;
  }
  sensor.setSignalRateLimit(0.25);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
  sensor.setMeasurementTimingBudget(200000);
  sensor.startContinuous();
  systemStatus.sensorReady = true;
  Serial.println(F("VL53L0X OK"));
}

void initializeStepper() {
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  systemStatus.stepperReady = true;
  Serial.println(F("Stepper initialized"));
}

void displayBootScreen() {
  lcd.setCursor(0, 0);
  lcd.print(F("   Robo-Flect   "));
  lcd.setCursor(0, 1);
  lcd.print(F("  Getting Ready..."));
  lcd.setCursor(0, 2);
  lcd.print(F("                    "));
  lcd.setCursor(0, 3);
  lcd.print(F("   Please Wait     "));
}

void loop() {
  unsigned long currentTime = millis();
  stepper.run();
  char key = keypad.getKey();
  if (key) {
    lastActivity = currentTime;
    handleKeyInput(key);
  }
  switch (currentState) {
    case STATE_BOOT:
      handleBootState(currentTime);
      break;
    case STATE_SELF_CHECK:
      handleSelfCheckState(currentTime);
      break;
    case STATE_IDLE_AWAIT_RFID:
      handleIdleState(currentTime);
      break;
    case STATE_MAIN_MENU:
      handleMainMenuState(currentTime);
      break;
    case STATE_MODE_SELECTION:
      handleModeSelectionState(currentTime);
      break;
    case STATE_TRAINING:
      handleTrainingState(currentTime);
      break;
    case STATE_TESTING:
      handleTestingState(currentTime);
      break;
    case STATE_TUTORIALS:
      handleTutorialsState(currentTime);
      break;
    case STATE_SETTINGS:
      handleSettingsState(currentTime);
      break;
    case STATE_CALIBRATION:
      handleCalibrationState(currentTime);
      break;
    case STATE_ERROR:
      handleErrorState(currentTime);
      break;
  }
  updateSensorReading(currentTime);
  checkIdleTimeout(currentTime);
}

void handleBootState(unsigned long currentTime) {
  if (currentTime - bootAnimationStart > BOOT_ANIMATION_DELAY) {
    static int dots = 0;
    lcd.setCursor(17 + (dots % 4), 1);
    lcd.print(dots < 4 ? '.' : ' ');
    dots = (dots + 1) % 8;
    bootAnimationStart = currentTime;
  }
  if (currentTime - stateStartTime > 3000) {
    changeState(STATE_SELF_CHECK);
  }
}

void handleSelfCheckState(unsigned long currentTime) {
  static int checkStep = 0;
  static unsigned long stepTime = 0;
  if (currentTime - stepTime > 1000) {
    switch (checkStep) {
      case 0:
        displaySelfCheck("Checking Sensor...");
        checkStep++;
        break;
      case 1:
        displaySelfCheck("Checking Stepper...");
        performStepperTest();
        checkStep++;
        break;
      case 2:
        displaySelfCheck("Checking ESP32...");
        checkESP32Connection();
        checkStep++;
        break;
      case 3:
        displaySelfCheck("System Ready!");
        setLEDColor(0, 255, 0);
        playReadySound();
        checkStep++;
        break;
      default:
        changeState(STATE_IDLE_AWAIT_RFID);
        return;
    }
    stepTime = currentTime;
  }
}

void handleIdleState(unsigned long currentTime) {
  if (currentTime - stateStartTime < 1000) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("   Robo-Flect   "));
    lcd.setCursor(0, 1);
    lcd.print(F("     Ready!      "));
    lcd.setCursor(0, 2);
    lcd.print(F("   Scan Card...  "));
    lcd.setCursor(0, 3);
    lcd.print(systemStatus.wifiConnected ? F("    Wi-Fi: OK    ") : F("   Offline Mode  "));
  }
  if (currentTime - lastIdlePrompt > 15000) {
    playPromptSound();
    lastIdlePrompt = currentTime;
  }
}

void handleMainMenuState(unsigned long currentTime) {
  displayMainMenu();
}

void handleModeSelectionState(unsigned long currentTime) {
  displayModeSelection();
}

void handleTrainingState(unsigned long currentTime) {
  static int trainingStep = 0;
  static unsigned long stepTime = 0;
  switch (trainingStep) {
    case 0:
      displayDistanceSelection();
      break;
    case 1:
      if (moveToDistance(targetDistance)) {
        trainingStep++;
        stepTime = currentTime;
      }
      break;
    case 2:
      performTrainingMeasurement(currentTime, stepTime);
      if (currentTime - stepTime > 5000) {
        trainingStep++;
      }
      break;
    case 3:
      if (returnHome()) {
        trainingAttempts++;
        saveTrainingData();
        trainingStep = 0;
        changeState(STATE_MAIN_MENU);
      }
      break;
  }
}

void handleTestingState(unsigned long currentTime) {
  displayTestingInterface();
}

void handleTutorialsState(unsigned long currentTime) {
  displayTutorials();
}

void handleSettingsState(unsigned long currentTime) {
  displaySettings();
}

void handleCalibrationState(unsigned long currentTime) {
  displayCalibration();
}

void handleErrorState(unsigned long currentTime) {
  displayError();
}

void displaySelfCheck(const char* message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("  System Check  "));
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 2);
  lcd.print(message);
  lcd.setCursor(0, 3);
  lcd.print(F("                "));
}

void displayMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(currentUser.isValid ? ("Hello " + currentUser.nickname).c_str() : F("Main Menu"));
  lcd.setCursor(0, 1);
  lcd.print(currentMenuSelection == 1 ? F("> 1.Mode         ") : F("  1.Mode         "));
  lcd.setCursor(0, 2);
  lcd.print(currentMenuSelection == 2 ? F("> 2.Tutorials    ") : F("  2.Tutorials    "));
  lcd.setCursor(0, 3);
  lcd.print(currentMenuSelection == 3 ? F("> 3.Settings     ") : F("  3.Settings     "));
}

void displayModeSelection() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("   Select Mode   "));
  lcd.setCursor(0, 1);
  lcd.print(F("                 "));
  lcd.setCursor(0, 2);
  lcd.print(F("1.Training 2.Test"));
  lcd.setCursor(0, 3);
  lcd.print(F("  * Back         "));
}

void displayDistanceSelection() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Select Distance: "));
  lcd.setCursor(0, 1);
  lcd.print(F("1=10cm 2=20cm    "));
  lcd.setCursor(0, 2);
  lcd.print(F("3=30cm 4=40cm    "));
  lcd.setCursor(0, 3);
  lcd.print(F("5=50cm 6=60cm    "));
}

void displayTrainingInterface() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Training Mode    "));
  lcd.setCursor(0, 1);
  lcd.print(("Target: " + String(targetDistance) + "cm").c_str());
  lcd.setCursor(0, 2);
  lcd.print(F("Tap tongue when  "));
  lcd.setCursor(0, 3);
  lcd.print(F("you hear echo... "));
}

void displayTestingInterface() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Testing Mode     "));
  lcd.setCursor(0, 1);
  lcd.print(("Score: " + String(testScore) + "/5").c_str());
  lcd.setCursor(0, 2);
  lcd.print(F("Listen carefully "));
  lcd.setCursor(0, 3);
  lcd.print(F("and guess...     "));
}

void displayTutorials() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("   Tutorials     "));
  lcd.setCursor(0, 1);
  lcd.print(F("Coming Soon...   "));
  lcd.setCursor(0, 2);
  lcd.print(F("                 "));
  lcd.setCursor(0, 3);
  lcd.print(F("   * Back        "));
}

void displaySettings() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("    Settings     "));
  lcd.setCursor(0, 1);
  lcd.print(F("1.Profile        "));
  lcd.setCursor(0, 2);
  lcd.print(F("2.Calibration    "));
  lcd.setCursor(0, 3);
  lcd.print(F("   * Back        "));
}

void displayCalibration() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("  Calibration    "));
  lcd.setCursor(0, 1);
  lcd.print(F("Place target at  "));
  lcd.setCursor(0, 2);
  lcd.print(F("30cm and press # "));
  lcd.setCursor(0, 3);
  lcd.print(F("   * Cancel      "));
}

void displayError() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("    ERROR!       "));
  lcd.setCursor(0, 1);
  lcd.print(F("System fault     "));
  lcd.setCursor(0, 2);
  lcd.print(F("Please restart   "));
  lcd.setCursor(0, 3);
  lcd.print(F("                 "));
  setLEDColor(255, 0, 0);
}

void handleKeyInput(char key) {
  Serial.print(F("Key pressed: "));
  Serial.println(key);
  switch (currentState) {
    case STATE_IDLE_AWAIT_RFID:
      simulateRFIDScan("USER001");
      break;
    case STATE_MAIN_MENU:
      handleMainMenuInput(key);
      break;
    case STATE_MODE_SELECTION:
      handleModeSelectionInput(key);
      break;
    case STATE_TRAINING:
      handleTrainingInput(key);
      break;
    case STATE_TESTING:
      handleTestingInput(key);
      break;
    case STATE_SETTINGS:
      handleSettingsInput(key);
      break;
    case STATE_CALIBRATION:
      handleCalibrationInput(key);
      break;
    default:
      if (key == '*')
        changeState(STATE_MAIN_MENU);
      break;
  }
}

void handleMainMenuInput(char key) {
  switch (key) {
    case '1':
      changeState(STATE_MODE_SELECTION);
      break;
    case '2':
      changeState(STATE_TUTORIALS);
      break;
    case '3':
      changeState(STATE_SETTINGS);
      break;
    case 'A':
      currentMenuSelection = (currentMenuSelection > 1) ? currentMenuSelection - 1 : 3;
      break;
    case 'B':
      currentMenuSelection = (currentMenuSelection < 3) ? currentMenuSelection + 1 : 1;
      break;
    case '#':
      handleMenuConfirm();
      break;
    case '*':
      logoutUser();
      break;
  }
}

void handleModeSelectionInput(char key) {
  switch (key) {
    case '1':
      isTrainingMode = true;
      changeState(STATE_TRAINING);
      break;
    case '2':
      isTrainingMode = false;
      changeState(STATE_TESTING);
      break;
    case '*':
      changeState(STATE_MAIN_MENU);
      break;
  }
}

void handleTrainingInput(char key) {
  if (key >= '1' && key <= '6') {
    int selection = key - '1';
    if (selection < NUM_POSITIONS) {
      targetDistance = DISTANCE_POSITIONS[selection];
      Serial.print(F("Target distance set to: "));
      Serial.println(targetDistance);
    }
  } else if (key == '*') {
    changeState(STATE_MAIN_MENU);
  }
}

void handleTestingInput(char key) {
  handleTrainingInput(key);
}

void handleSettingsInput(char key) {
  switch (key) {
    case '1':
      displayProfileSettings();
      break;
    case '2':
      if (checkAdminCode())
        changeState(STATE_CALIBRATION);
      break;
    case '*':
      changeState(STATE_MAIN_MENU);
      break;
  }
}

void handleCalibrationInput(char key) {
  switch (key) {
    case '#':
      performCalibration();
      break;
    case '*':
      changeState(STATE_SETTINGS);
      break;
  }
}

void changeState(SystemState newState) {
  Serial.print(F("State change: "));
  Serial.print(currentState);
  Serial.print(F(" -> "));
  Serial.println(newState);
  currentState = newState;
  stateStartTime = millis();
  switch (newState) {
    case STATE_MAIN_MENU:
      currentMenuSelection = 1;
      break;
    case STATE_TRAINING:
    case STATE_TESTING:
      targetDistance = 30;
      break;
  }
}

void updateSensorReading(unsigned long currentTime) {
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL && systemStatus.sensorReady) {
    uint16_t distance = sensor.readRangeContinuousMillimeters();
    if (!sensor.timeoutOccurred() && distance != 65535) {
      int distanceCm = distance / 10;
    }
    lastSensorRead = currentTime;
  }
}

void checkIdleTimeout(unsigned long currentTime) {
  if (currentState != STATE_IDLE_AWAIT_RFID && currentTime - lastActivity > IDLE_TIMEOUT) {
    logoutUser();
  }
}

void simulateRFIDScan(String uid) {
  currentUser.uid = uid;
  currentUser.nickname = "User";
  currentUser.level = 2;
  currentUser.progress = 3;
  currentUser.isValid = true;
  currentUser.lastLogin = millis();
  playWelcomeSound();
  changeState(STATE_MAIN_MENU);
}

void logoutUser() {
  currentUser = {"", "", 1, 0, 0, false};
  changeState(STATE_IDLE_AWAIT_RFID);
}

bool moveToDistance(int targetCm) {
  long targetSteps = targetCm * STEPS_PER_CM;
  stepper.moveTo(targetSteps);
  if (stepper.distanceToGo() == 0) return true;
  return false;
}

bool returnHome() {
  stepper.moveTo(0);
  return (stepper.distanceToGo() == 0);
}

void performStepperTest() {
  static int testStep = 0;
  static unsigned long stepTime = 0;
  switch (testStep) {
    case 0:
      stepper.moveTo(10 * STEPS_PER_CM);
      if (stepper.distanceToGo() == 0) testStep++;
      break;
    case 1:
      stepper.moveTo(60 * STEPS_PER_CM);
      if (stepper.distanceToGo() == 0) testStep++;
      break;
    case 2:
      stepper.moveTo(10 * STEPS_PER_CM);
      if (stepper.distanceToGo() == 0) {
        systemStatus.stepperReady = true;
        testStep = 0;
      }
      break;
  }
}

void setLEDColor(int red, int green, int blue) {
  analogWrite(LED_RED_PIN, red);
  analogWrite(LED_GREEN_PIN, green);
  analogWrite(LED_BLUE_PIN, blue);
}

void playBootSound() {
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 1000, 100);
    delay(150);
  }
}

void playWelcomeSound() {
  tone(BUZZER_PIN, 1500, 200);
  delay(250);
  tone(BUZZER_PIN, 2000, 200);
}

void playReadySound() {
  tone(BUZZER_PIN, 2000, 500);
}

void playPromptSound() {
  tone(BUZZER_PIN, 800, 100);
}

void playConfirmSound() {
  tone(BUZZER_PIN, 1200, 150);
}

void playErrorSound() {
  for (int i = 0; i < 5; i++) {
    tone(BUZZER_PIN, 500, 100);
    delay(150);
  }
}

void checkESP32Connection() {
  esp32Serial.println(F("{\"cmd\":\"ping\"}"));
  unsigned long timeout = millis() + 2000;
  while (millis() < timeout) {
    if (esp32Serial.available()) {
      String response = esp32Serial.readString();
      if (response.indexOf("pong") >= 0) {
        systemStatus.esp32Ready = true;
        systemStatus.wifiConnected = true;
        return;
      }
    }
  }
  systemStatus.esp32Ready = false;
  systemStatus.offlineMode = true;
}

bool checkAdminCode() {
  return true;
}

void performCalibration() {
  Serial.println(F("Performing calibration..."));
  playConfirmSound();
}

void performTrainingMeasurement(unsigned long currentTime, unsigned long stepTime) {
  displayTrainingInterface();
  if ((currentTime - stepTime) % 500 < 250) {
    setLEDColor(0, 255, 0);
  } else {
    setLEDColor(0, 0, 0);
  }
}

void saveTrainingData() {
  String data = "{\"timestamp\":" + String(millis()) +
                ",\"distance\":" + String(targetDistance) +
                ",\"attempts\":" + String(trainingAttempts) + "}";
  Serial.println(F("Training data: ") + data);
}

void handleMenuConfirm() {
  switch (currentMenuSelection) {
    case 1:
      changeState(STATE_MODE_SELECTION);
      break;
    case 2:
      changeState(STATE_TUTORIALS);
      break;
    case 3:
      changeState(STATE_SETTINGS);
      break;
  }
}

void displayProfileSettings() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("   Profile       "));
  lcd.setCursor(0, 1);
  lcd.print(("UID: " + currentUser.uid).c_str());
  lcd.setCursor(0, 2);
  lcd.print(("Level: " + String(currentUser.level)).c_str());
  lcd.setCursor(0, 3);
  lcd.print(F("   * Back        "));
}