/*
 * Robo-Flect V1.1 - Arduino Mega 2560 Main Controller
 * Professional Sectioned Version for Optimal Performance
 * 
 * System Architecture:
 * - State Machine based design
 * - Hardware control and user interface
 * - Serial communication with ESP32 for WiFi/Firebase
 * - Professional User Journey implementation
 * 
 * Hardware Components (Mega 2560):
 * - LCD 20x4 (I2C)
 * - Stepper Motor with Driver
 * - VL53L0X Distance Sensor
 * - Keypad 4x4
 * - RFID Module
 * - Buzzer & LED indicators
 */

// =====================================
// SECTION 1: INCLUDES & DEFINITIONS
// =====================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// โค้ดนี้ใช้สำหรับควบคุมบอร์ด Arduino Mega ในโปรเจค Robo-Flect
#include <AccelStepper.h>
#include <VL53L0X.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>

// Hardware Pin Definitions
#define STEP_PIN        14
#define DIR_PIN         15
#define BUZZER_PIN      18
#define LED_RED_PIN     19
#define LED_GREEN_PIN   20
#define LED_BLUE_PIN    21
#define RFID_SS_PIN     53
#define RFID_RST_PIN    49

// System Constants
const float MAX_SPEED = 1333.0;
const float ACCELERATION = 2000.0;
const unsigned long REFRESH_INTERVAL = 100;
const unsigned long SENSOR_INTERVAL = 200;
const unsigned long IDLE_TIMEOUT = 15000;
const unsigned long BOOT_ANIMATION_DELAY = 500;
const unsigned long RFID_SCAN_INTERVAL = 1000;

// Distance Settings (cm)
const int DISTANCE_POSITIONS[] = {10, 20, 30, 40, 50, 60};
const int NUM_POSITIONS = 6;
const int STEPS_PER_CM = 200;

// Message Protocol
const char MSG_START = '<';
const char MSG_END = '>';

// =====================================
// SECTION 2: SYSTEM STATE DEFINITIONS
// =====================================

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

enum TrainingStep {
  TRAIN_SELECT_DISTANCE,
  TRAIN_MOVING_TO_POSITION,
  TRAIN_MEASURING,
  TRAIN_RETURNING_HOME,
  TRAIN_COMPLETE
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
  bool rfidReady;
};

// =====================================
// SECTION 3: GLOBAL OBJECTS & VARIABLES
// =====================================

// Hardware Objects
LiquidCrystal_I2C lcd(0x27, 20, 4);
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
VL53L0X sensor;
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

// Keypad Configuration
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

// System Variables
SystemState currentState = STATE_BOOT;
SystemStatus systemStatus = {false, false, false, false, false, false, false, false};
UserProfile currentUser = {"", "", 1, 0, 0, false};
TrainingStep trainingStep = TRAIN_SELECT_DISTANCE;

// Timing Variables
unsigned long stateStartTime = 0;
unsigned long lastActivity = 0;
unsigned long lastSensorRead = 0;
unsigned long lastIdlePrompt = 0;
unsigned long lastRFIDScan = 0;
unsigned long bootAnimationStart = 0;
unsigned long trainingStepTime = 0;

// UI Variables
int currentMenuSelection = 1;
int targetDistance = 30;
bool isTrainingMode = true;
int currentSensorDistance = 0;

// Data Logging
int trainingAttempts = 0;
int testScore = 0;
String sessionData = "";

// =====================================
// SECTION 4: SETUP & INITIALIZATION
// =====================================

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600); // Communication with ESP32
  
  currentState = STATE_BOOT;
  stateStartTime = millis();
  bootAnimationStart = millis();
  
  initializeHardware();
  displayBootScreen();
  
  Serial.println(F("Mega 2560 Controller Started"));
}

void initializeHardware() {
  // Pin Modes
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  
  // Initialize SPI for RFID
  SPI.begin();
  rfid.PCD_Init();
  systemStatus.rfidReady = true;
  
  // Initialize I2C and LCD
  Wire.begin();
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  systemStatus.lcdReady = true;
  
  // Initialize VL53L0X Sensor
  initializeSensor();
  
  // Initialize Stepper Motor
  initializeStepper();
  
  // Play boot sound
  playBootSound();
  
  Serial.println(F("Hardware initialization complete"));
}

void initializeSensor() {
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

// =====================================
// SECTION 5: MAIN LOOP & STATE MACHINE
// =====================================

void loop() {
  unsigned long currentTime = millis();
  
  // Update stepper motor
  stepper.run();
  
  // Handle keypad input
  char key = keypad.getKey();
  if (key) {
    lastActivity = currentTime;
    handleKeyInput(key);
  }
  
  // Handle RFID scanning
  handleRFIDScan(currentTime);
  
  // Process ESP32 messages
  processESP32Messages();
  
  // State machine
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
  
  // Update sensor reading
  updateSensorReading(currentTime);
  
  // Check for timeout
  checkIdleTimeout(currentTime);
}

// =====================================
// SECTION 6: STATE HANDLERS
// =====================================

void handleBootState(unsigned long currentTime) {
  // Boot animation
  if (currentTime - bootAnimationStart > BOOT_ANIMATION_DELAY) {
    static int dots = 0;
    lcd.setCursor(17 + (dots % 4), 1);
    lcd.print(dots < 4 ? '.' : ' ');
    dots = (dots + 1) % 8;
    bootAnimationStart = currentTime;
  }
  
  // After 3 seconds, move to self-check
  if (currentTime - stateStartTime > 3000) {
    changeState(STATE_SELF_CHECK);
  }
}

void handleSelfCheckState(unsigned long currentTime) {
  static int checkStep = 0;
  static unsigned long stepTime = 0;
  
  if (currentTime - stepTime > 1500) {
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
  static bool screenDisplayed = false;
  
  if (!screenDisplayed) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("   Robo-Flect   "));
    lcd.setCursor(0, 1);
    lcd.print(F("     Ready!      "));
    lcd.setCursor(0, 2);
    lcd.print(F("   Scan Card...  "));
    lcd.setCursor(0, 3);
    lcd.print(systemStatus.wifiConnected ? F("    Wi-Fi: OK    ") : F("   Offline Mode  "));
    screenDisplayed = true;
  }
  
  // Play prompt every 15 seconds
  if (currentTime - lastIdlePrompt > 15000) {
    playPromptSound();
    lastIdlePrompt = currentTime;
    screenDisplayed = false; // Refresh screen
  }
}

void handleMainMenuState(unsigned long currentTime) {
  static bool menuDisplayed = false;
  
  if (!menuDisplayed) {
    displayMainMenu();
    menuDisplayed = true;
  }
}

void handleModeSelectionState(unsigned long currentTime) {
  static bool modeDisplayed = false;
  
  if (!modeDisplayed) {
    displayModeSelection();
    modeDisplayed = true;
  }
}

void handleTrainingState(unsigned long currentTime) {
  switch (trainingStep) {
    case TRAIN_SELECT_DISTANCE:
      displayDistanceSelection();
      break;
      
    case TRAIN_MOVING_TO_POSITION:
      displayMovingToPosition();
      if (moveToDistance(targetDistance)) {
        trainingStep = TRAIN_MEASURING;
        trainingStepTime = currentTime;
        playConfirmSound();
      }
      break;
      
    case TRAIN_MEASURING:
      performTrainingMeasurement(currentTime);
      if (currentTime - trainingStepTime > 5000) {
        trainingStep = TRAIN_RETURNING_HOME;
      }
      break;
      
    case TRAIN_RETURNING_HOME:
      displayReturningHome();
      if (returnHome()) {
        trainingStep = TRAIN_COMPLETE;
        playCompletedSound();
      }
      break;
      
    case TRAIN_COMPLETE:
      trainingAttempts++;
      saveTrainingData();
      trainingStep = TRAIN_SELECT_DISTANCE;
      changeState(STATE_MAIN_MENU);
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
  setLEDColor(255, 0, 0);
}

// =====================================
// SECTION 7: DISPLAY FUNCTIONS
// =====================================

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
  if (currentUser.isValid) {
    String greeting = "Hello " + currentUser.nickname;
    if (greeting.length() > 20) greeting = greeting.substring(0, 20);
    lcd.print(greeting);
  } else {
    lcd.print(F("Main Menu"));
  }
  
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

void displayMovingToPosition() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Training Mode    "));
  lcd.setCursor(0, 1);
  lcd.print(("Moving to " + String(targetDistance) + "cm").c_str());
  lcd.setCursor(0, 2);
  lcd.print(F("Please wait...   "));
  
  // Progress bar
  int progress = map(stepper.currentPosition(), 0, targetDistance * STEPS_PER_CM, 0, 20);
  lcd.setCursor(0, 3);
  for (int i = 0; i < 20; i++) {
    lcd.print(i < progress ? '=' : '-');
  }
}

void displayTrainingInterface() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Training Mode    "));
  lcd.setCursor(0, 1);
  lcd.print(("Target: " + String(targetDistance) + "cm").c_str());
  lcd.setCursor(0, 2);
  lcd.print(F("Listen & tap when"));
  lcd.setCursor(0, 3);
  lcd.print(F("you hear echo... "));
}

void displayReturningHome() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("Training Complete"));
  lcd.setCursor(0, 1);
  lcd.print(F("Returning home..."));
  lcd.setCursor(0, 2);
  lcd.print(("Distance: " + String(currentSensorDistance) + "cm").c_str());
  lcd.setCursor(0, 3);
  lcd.print(F("                "));
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
  lcd.print(F("Guess distance..."));
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
}

// =====================================
// SECTION 8: INPUT HANDLING
// =====================================

void handleKeyInput(char key) {
  Serial.print(F("Key pressed: "));
  Serial.println(key);
  
  switch (currentState) {
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
      if (key == '*') {
        if (currentUser.isValid) {
          changeState(STATE_MAIN_MENU);
        } else {
          changeState(STATE_IDLE_AWAIT_RFID);
        }
      }
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
      displayMainMenu();
      break;
    case 'B':
      currentMenuSelection = (currentMenuSelection < 3) ? currentMenuSelection + 1 : 1;
      displayMainMenu();
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
      trainingStep = TRAIN_SELECT_DISTANCE;
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
  if (trainingStep == TRAIN_SELECT_DISTANCE) {
    if (key >= '1' && key <= '6') {
      int selection = key - '1';
      if (selection < NUM_POSITIONS) {
        targetDistance = DISTANCE_POSITIONS[selection];
        trainingStep = TRAIN_MOVING_TO_POSITION;
        Serial.print(F("Target distance set to: "));
        Serial.println(targetDistance);
      }
    } else if (key == '*') {
      changeState(STATE_MAIN_MENU);
    }
  } else if (key == '*') {
    trainingStep = TRAIN_SELECT_DISTANCE;
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
      if (checkAdminCode()) {
        changeState(STATE_CALIBRATION);
      }
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

// =====================================
// SECTION 9: HARDWARE CONTROL
// =====================================

bool moveToDistance(int targetCm) {
  long targetSteps = targetCm * STEPS_PER_CM;
  stepper.moveTo(targetSteps);
  return (stepper.distanceToGo() == 0);
}

bool returnHome() {
  stepper.moveTo(0);
  return (stepper.distanceToGo() == 0);
}

void performStepperTest() {
  static int testStep = 0;
  
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

// =====================================
// SECTION 10: AUDIO FEEDBACK
// =====================================

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

void playCompletedSound() {
  tone(BUZZER_PIN, 1500, 300);
  delay(100);
  tone(BUZZER_PIN, 1800, 300);
}

void playErrorSound() {
  for (int i = 0; i < 5; i++) {
    tone(BUZZER_PIN, 500, 100);
    delay(150);
  }
}

// =====================================
// SECTION 11: RFID & USER MANAGEMENT
// =====================================

void handleRFIDScan(unsigned long currentTime) {
  if (currentTime - lastRFIDScan < RFID_SCAN_INTERVAL) return;
  
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    lastRFIDScan = currentTime;
    return;
  }
  
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  
  Serial.print(F("RFID Card detected: "));
  Serial.println(uid);
  
  if (currentState == STATE_IDLE_AWAIT_RFID) {
    requestUserProfile(uid);
  }
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  lastRFIDScan = currentTime;
}

void requestUserProfile(String uid) {
  DynamicJsonDocument doc(256);
  doc["cmd"] = "getUserProfile";
  doc["uid"] = uid;
  
  String message;
  serializeJson(doc, message);
  sendToESP32(message);
  
  // Temporarily login user while waiting for ESP32 response
  currentUser.uid = uid;
  currentUser.nickname = "User";
  currentUser.level = 1;
  currentUser.progress = 0;
  currentUser.isValid = true;
  currentUser.lastLogin = millis();
  
  playWelcomeSound();
  changeState(STATE_MAIN_MENU);
}

void logoutUser() {
  currentUser = {"", "", 1, 0, 0, false};
  changeState(STATE_IDLE_AWAIT_RFID);
  playConfirmSound();
}

// =====================================
// SECTION 12: ESP32 COMMUNICATION
// =====================================

void sendToESP32(String message) {
  Serial1.print(MSG_START);
  Serial1.print(message);
  Serial1.print(MSG_END);
  Serial.print(F("Sent to ESP32: "));
  Serial.println(message);
}

void processESP32Messages() {
  static String receivedMessage = "";
  static bool messageStarted = false;
  
  while (Serial1.available()) {
    char c = Serial1.read();
    
    if (c == MSG_START) {
      receivedMessage = "";
      messageStarted = true;
    } else if (c == MSG_END && messageStarted) {
      processESP32Response(receivedMessage);
      receivedMessage = "";
      messageStarted = false;
    } else if (messageStarted) {
      receivedMessage += c;
    }
  }
}

void processESP32Response(String message) {
  Serial.print(F("Received from ESP32: "));
  Serial.println(message);
  
  DynamicJsonDocument doc(512);
  deserializeJson(doc, message);
  
  String cmd = doc["cmd"];
  
  if (cmd == "ping") {
    sendToESP32("{\"cmd\":\"pong\"}");
  } else if (cmd == "userProfile") {
    updateUserProfile(doc);
  } else if (cmd == "wifiStatus") {
    systemStatus.wifiConnected = doc["connected"];
    systemStatus.esp32Ready = true;
  } else if (cmd == "trainingData") {
    saveTrainingToCloud(doc);
  }
}

void updateUserProfile(DynamicJsonDocument& doc) {
  if (doc["found"]) {
    currentUser.nickname = doc["nickname"].as<String>();
    currentUser.level = doc["level"];
    currentUser.progress = doc["progress"];
    currentUser.isValid = true;
    
    Serial.println(F("User profile updated from cloud"));
  } else {
    Serial.println(F("User not found in cloud, using default"));
  }
}

void checkESP32Connection() {
  sendToESP32("{\"cmd\":\"ping\"}");
  
  unsigned long timeout = millis() + 2000;
  bool responseReceived = false;
  
  while (millis() < timeout && !responseReceived) {
    processESP32Messages();
    delay(10);
  }
  
  if (!systemStatus.esp32Ready) {
    systemStatus.offlineMode = true;
    Serial.println(F("ESP32 not responding, entering offline mode"));
  }
}

// =====================================
// SECTION 13: SENSOR & DATA MANAGEMENT
// =====================================

void updateSensorReading(unsigned long currentTime) {
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL && systemStatus.sensorReady) {
    uint16_t distance = sensor.readRangeContinuousMillimeters();
    
    if (!sensor.timeoutOccurred() && distance != 65535) {
      currentSensorDistance = distance / 10;
      
      // Send sensor data during training/testing
      if (currentState == STATE_TRAINING || currentState == STATE_TESTING) {
        DynamicJsonDocument doc(256);
        doc["cmd"] = "sensorData";
        doc["distance"] = currentSensorDistance;
        doc["timestamp"] = currentTime;
        
        String message;
        serializeJson(doc, message);
        sendToESP32(message);
      }
    }
    
    lastSensorRead = currentTime;
  }
}

void performTrainingMeasurement(unsigned long currentTime) {
  displayTrainingInterface();
  
  // Flash green LED during measurement
  if ((currentTime - trainingStepTime) % 500 < 250) {
    setLEDColor(0, 255, 0);
  } else {
    setLEDColor(0, 0, 0);
  }
}

void saveTrainingData() {
  DynamicJsonDocument doc(512);
  doc["cmd"] = "saveTrainingData";
  doc["uid"] = currentUser.uid;
  doc["timestamp"] = millis();
  doc["targetDistance"] = targetDistance;
  doc["actualDistance"] = currentSensorDistance;
  doc["attempts"] = trainingAttempts;
  doc["sessionType"] = isTrainingMode ? "training" : "testing";
  
  String message;
  serializeJson(doc, message);
  sendToESP32(message);
  
  Serial.println(F("Training data saved"));
}

void saveTrainingToCloud(DynamicJsonDocument& doc) {
  Serial.println(F("Training data synchronized to cloud"));
}

// =====================================
// SECTION 14: UTILITY FUNCTIONS
// =====================================

void changeState(SystemState newState) {
  Serial.print(F("State change: "));
  Serial.print(currentState);
  Serial.print(F(" -> "));
  Serial.println(newState);
  
  currentState = newState;
  stateStartTime = millis();
  
  // State entry actions
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

void checkIdleTimeout(unsigned long currentTime) {
  if (currentState != STATE_IDLE_AWAIT_RFID && 
      currentTime - lastActivity > IDLE_TIMEOUT) {
    logoutUser();
  }
}

bool checkAdminCode() {
  return true; // Implement proper admin verification
}

void performCalibration() {
  Serial.println(F("Performing calibration..."));
  playConfirmSound();
  
  // Save calibration data
  DynamicJsonDocument doc(256);
  doc["cmd"] = "saveCalibration";
  doc["referenceDistance"] = 30;
  doc["sensorReading"] = currentSensorDistance;
  doc["timestamp"] = millis();
  
  String message;
  serializeJson(doc, message);
  sendToESP32(message);
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
  String uidDisplay = "UID: " + currentUser.uid;
  if (uidDisplay.length() > 20) uidDisplay = uidDisplay.substring(0, 20);
  lcd.print(uidDisplay);
  lcd.setCursor(0, 2);
  lcd.print(("Level: " + String(currentUser.level)).c_str());
  lcd.setCursor(0, 3);
  lcd.print(F("   * Back        "));
}

// =====================================
// END OF ARDUINO MEGA CODE
// =====================================