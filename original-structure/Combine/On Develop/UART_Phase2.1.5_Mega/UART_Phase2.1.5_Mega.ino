/*
  Mega2560 - Robo-Flect (Menu + Homing FAST in Dev Mode + UART from ESP32 + No-stutter)

  Wiring highlights:
  - Step driver: STEP=14, DIR=15
  - JOG FWD/BWD: D22 (ไปซ้าย/หา Home), D23 (ไปขวา/หามอเตอร์), Active LOW
  - Home switch: D18, NO-to-GND, Active LOW
  - LCD 20x4 I2C: addr 0x27
  - VL53L0X: I2C
  - Serial2 (UART link from ESP32): RX2=D17, TX2=D16 (ใช้รับจาก ESP32)
    ESP32 TX(GPIO17) -> MEGA RX2(D17), GND shared
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>
#include <VL53L0X.h>
#include <Keypad.h>

// ===== Debug =====
#define DEBUG_LOG        1
#define LOG_INTERVAL_MS  200   // log pos while moving (0=off)

// ===== Stepper (driver mode) =====
#define STEP_PIN     14
#define DIR_PIN      15
#define SW_FWD_PIN   22    // JOG เดินหน้า (ไปซ้าย/หา Home) Active LOW
#define SW_BWD_PIN   23    // JOG ถอยหลัง (ไปขวา/หามอเตอร์) Active LOW
#define HOME_PIN     18    // Limit switch (NO to GND) Active LOW

const bool HOME_ACTIVE_LOW = true;
const bool DIR_INVERT = true;   // flip DIR line in hardware if needed

const float MAX_SPEED        = 1333.0; // pulses/s (normal move)
const float ACCELERATION     = 500.0;  // pulses/s^2
const float HOMING_SPEED_BASE= 200.0;  // pulses/s (boot/secret homing speed)

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ===== LCD 20x4 =====
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ===== VL53L0X =====
VL53L0X sensor;

// ===== Keypad 4x4 =====
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
// ตามสายของคุณ
byte rowPins[ROWS] = {29, 28, 27, 26};
byte colPins[COLS] = {33, 32, 31, 30};
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== LCD columns =====
const uint8_t colTarget = 8; // row0
const uint8_t colDist   = 6; // row1
const uint8_t colKey    = 5; // row2

// ===== Axis direction conventions =====
// FWD = left (home), BWD = right (motor side)
const int DIR_SIGN_FWD    = +1;   // setSpeed(+X) = left
const int DIR_SIGN_BWD    = -1;   // setSpeed(-X) = right
// Coordinate system: 0 at left/home, going right is NEGATIVE steps
const int POS_SIGN_RIGHT  = -1;

// ===== Helpers (LCD) =====
void clearField(uint8_t row, uint8_t startCol, uint8_t width) {
  lcd.setCursor(startCol, row);
  for (uint8_t i = 0; i < width; i++) lcd.print(' ');
}
void showTargetCm(int cm) {
  clearField(0, colTarget, 6);
  lcd.setCursor(colTarget, 0);
  if (cm >= 0) { lcd.print(cm); lcd.print("cm"); }
  else         { lcd.print("--"); }
}
void showDistText(const char* s) {
  clearField(1, colDist, 6);
  lcd.setCursor(colDist, 1);
  lcd.print(s);
}

// ===== Kinematics (Belt + DM542) =====
const float PULSES_PER_REV = 400.0; // DM542 DIP
const float BELT_PITCH_MM  = 2.0;   // GT2
const float PULLEY_TEETH   = 20.0;
const float CAL            = 0.50;  // ปรับตามคาลิเบรตจริง

const float MM_PER_REV     = BELT_PITCH_MM * PULLEY_TEETH;
const float PULSES_PER_MM  = (PULSES_PER_REV / MM_PER_REV) * CAL;
const float STEPS_PER_CM   = PULSES_PER_MM * 10.0;

// ===== Soft-limit (travel ~70 cm) =====
// Position range: [MIN_NEGATIVE .. 0], 0 = left/home
const float TRAVEL_MAX_CM   = 70.0;
const float SAFE_MARGIN_CM  = 0.5;
const long  MIN_POS_STEPS   = (long)(POS_SIGN_RIGHT * (TRAVEL_MAX_CM - SAFE_MARGIN_CM) * STEPS_PER_CM); // negative
const long  MAX_POS_STEPS   = 0; // left

// ===== Motion state =====
enum Mode : uint8_t { MODE_IDLE, MODE_MOVING, MODE_HOMING, MODE_JOG };
Mode mode = MODE_IDLE;

int   selectedCm   = -1;
long  targetSteps  = 0;

bool  postReadPending = false;
unsigned long lastLogMs = 0;
bool  homed = false;

// ===== Homing =====
const float BACKOFF_MM           = 3.0;
const unsigned long HOMING_TIMEOUT_MS = 20000;
enum HomeStage : uint8_t { HS_IDLE, HS_APPROACH, HS_BACKOFF, HS_LATCH, HS_DONE };
HomeStage hs = HS_IDLE;

inline bool homeTriggered() {
  int v = digitalRead(HOME_PIN);
  return HOME_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

// ===== Debug helpers =====
#if DEBUG_LOG
void logPosition(const char* tag) {
  long pos = stepper.currentPosition();
  float cm = (float)pos / STEPS_PER_CM * (POS_SIGN_RIGHT == -1 ? -1 : 1);
  Serial.print("["); Serial.print(tag); Serial.print("] pos=");
  Serial.print(pos); Serial.print(" pulses (");
  Serial.print(cm, 2); Serial.println(" cm from Home → right is +)");
}
void logTargetInfo(float requestedCm, float limitedCm, long tgt) {
  Serial.print("[MOVE] req="); Serial.print(requestedCm, 2); Serial.print(" cm (right)");
  Serial.print("  clamped=");  Serial.print(limitedCm, 2);  Serial.print(" cm");
  Serial.print("  targetSteps=");   Serial.println(tgt);
}
void logMsg(const char* m) { Serial.println(m); }
#else
void logPosition(const char*) {}
void logTargetInfo(float,float,long) {}
void logMsg(const char*) {}
#endif

// ===== Key -> cm =====
int mapKeyToCm(char k) {
  switch (k) {
    case '1': return 10;
    case '2': return 20;
    case '3': return 30;
    case '4': return 40;
    case '5': return 50;
    case '6': return 60;
    default:  return -1;
  }
}

// ===== Sensor (single-shot only; no read while moving) =====
int readDistanceCmOnce() {
  uint16_t mm = sensor.readRangeSingleMillimeters();
  bool to = sensor.timeoutOccurred();
  int dcm = (!to && mm != 65535 && mm/10 != 819) ? mm/10 : -1;
  return dcm;
}
void readAndShowOnce() {
  int dcm = readDistanceCmOnce();
  if (dcm >= 0) {
    char buf[8]; snprintf(buf, sizeof(buf), "%dcm", dcm);
    showDistText(buf);
    if (DEBUG_LOG) { Serial.print("[SENSOR] Dist: "); Serial.println(buf); }
  } else {
    showDistText("--");
    if (DEBUG_LOG) Serial.println("[SENSOR] Dist: --");
  }
}

// ===== Move absolute (cm from home → right) =====
void startMoveToCm(float cm_right) {
  if (!homed) { logMsg("[WARN] Not homed. Press 0 to HOME first."); return; }
  if (cm_right < 0) cm_right = 0;
  float limitCm = cm_right;
  if (limitCm > (TRAVEL_MAX_CM - SAFE_MARGIN_CM)) limitCm = (TRAVEL_MAX_CM - SAFE_MARGIN_CM);

  readAndShowOnce();

  long absSteps = (long)(limitCm * STEPS_PER_CM + 0.5f);
  targetSteps   = POS_SIGN_RIGHT * absSteps; // right = negative

  if (targetSteps < MIN_POS_STEPS) targetSteps = MIN_POS_STEPS;
  if (targetSteps > MAX_POS_STEPS) targetSteps = MAX_POS_STEPS;

  logTargetInfo(cm_right, limitCm, targetSteps);
  logPosition("START_FROM");

  stepper.moveTo(targetSteps);
  mode = MODE_MOVING;
  lastLogMs = 0;
}

// ===== Homing (boot/secret: slow, DevMode: fast=ACCELERATION) =====
unsigned long homingStartMs = 0;

void finishHomingRestoreMotion() {
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
}

void startHoming(bool fast = false) {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Getting Ready");
  lcd.setCursor(0,1); lcd.print("Calibrating...");
  lcd.setCursor(0,2); lcd.print("Please wait");

  readAndShowOnce();

  homed = false;
  hs = HS_APPROACH;
  mode = MODE_HOMING;
  homingStartMs = millis();

  float homingSpeed = fast ? ACCELERATION : HOMING_SPEED_BASE;

  stepper.setMaxSpeed(homingSpeed);
  stepper.setAcceleration(0); // ใช้ runSpeed
  stepper.setSpeed(DIR_SIGN_FWD * homingSpeed); // left
  logMsg(fast ? "[HOMING] start FAST (speed=ACCELERATION)"
              : "[HOMING] start SLOW (speed=BASE)");
}

void updateHoming() {
  if (mode != MODE_HOMING) return;

  if (millis() - homingStartMs > HOMING_TIMEOUT_MS) {
    logMsg("[HOMING] TIMEOUT! Abort.");
    stepper.setSpeed(0);
    mode = MODE_IDLE;
    hs = HS_IDLE;
    postReadPending = true;
    finishHomingRestoreMotion();
    return;
  }

  switch (hs) {
    case HS_APPROACH:
      if (!homeTriggered()) stepper.runSpeed();
      else { hs = HS_BACKOFF; logMsg("[HOMING] hit switch → BACKOFF"); }
      break;

    case HS_BACKOFF: {
      long backoffSteps = (long)(BACKOFF_MM * PULSES_PER_MM + 0.5f);
      stepper.move(DIR_SIGN_BWD * backoffSteps); // right a bit
      while (stepper.distanceToGo() != 0) stepper.run();
      stepper.setSpeed(DIR_SIGN_FWD * (float)stepper.maxSpeed()); // keep same homing speed
      hs = HS_LATCH; logMsg("[HOMING] BACKOFF → LATCH");
    } break;

    case HS_LATCH:
      if (!homeTriggered()) stepper.runSpeed();
      else {
        long releaseSteps = (long)(BACKOFF_MM * PULSES_PER_MM + 0.5f);
        stepper.move(DIR_SIGN_BWD * releaseSteps); // right to clear
        while (stepper.distanceToGo() != 0) stepper.run();
        stepper.setCurrentPosition(0); homed = true;
        hs = HS_DONE; logMsg("[HOMING] DONE (pos=0)");
      }
      break;

    case HS_DONE:
      mode = MODE_IDLE; hs = HS_IDLE;
      finishHomingRestoreMotion();   // restore normal speed/accel
      postReadPending = true;
      break;
  }
}

// ===== JOG (IDLE only) =====
void updateJog() {
  if (mode != MODE_IDLE) return;

  bool fwd = (digitalRead(SW_FWD_PIN) == LOW); // left
  bool bwd = (digitalRead(SW_BWD_PIN) == LOW); // right
  long pos = stepper.currentPosition();

  if (fwd ^ bwd) {
    if (fwd && pos >= MAX_POS_STEPS) { stepper.setSpeed(0); logMsg("[JOG] limit @ LEFT(0)"); return; }
    if (bwd && pos <= MIN_POS_STEPS) { stepper.setSpeed(0); logMsg("[JOG] limit @ RIGHT(min)"); return; }

    static bool jogging = false;
    if (!jogging) { readAndShowOnce(); logMsg("[JOG] start"); logPosition("JOG_FROM"); jogging = true; }
    stepper.setSpeed((fwd ? DIR_SIGN_FWD : DIR_SIGN_BWD) * MAX_SPEED);
    stepper.runSpeed();

    if (!(fwd ^ bwd)) jogging = false;
  } else {
    static bool wasJogging = false;
    if (stepper.speed() != 0) wasJogging = true;
    stepper.setSpeed(0);
    if (wasJogging) { logMsg("[JOG] stop"); logPosition("JOG_TO"); postReadPending = true; wasJogging = false; }
  }
}

// ===== UI / Menu =====
enum UIState : uint8_t {
  UI_BOOT, UI_WAIT_CARD, UI_MENU_MAIN, UI_MENU_PLAY, UI_MENU_TUTOR, UI_MENU_SETTING, UI_DEV_MODE
};
UIState ui = UI_BOOT;

const char *SECRET = "0*99AB";
String secretBuf;

void showReady()        { lcd.clear(); lcd.setCursor(0,0); lcd.print("Robo-Flect Ready"); }
void showScanCard()     { lcd.clear(); lcd.setCursor(0,0); lcd.print("Scan Your Card"); lcd.setCursor(0,1); lcd.print("(Press A to Menu)"); }
void showMenuMain()     { lcd.clear(); lcd.setCursor(0,0); lcd.print("Main Menu"); lcd.setCursor(0,1); lcd.print("1.Play Mode"); lcd.setCursor(0,2); lcd.print("2.Tutorial"); lcd.setCursor(0,3); lcd.print("3.Setting"); }
void showMenuPlay()     { lcd.clear(); lcd.setCursor(0,0); lcd.print("Play Mode"); lcd.setCursor(0,1); lcd.print("1.Training"); lcd.setCursor(0,2); lcd.print("2.Exam"); lcd.setCursor(0,3); lcd.print("3.Developer"); }
void showMenuTutor()    { lcd.clear(); lcd.setCursor(0,0); lcd.print("Tutorial"); lcd.setCursor(0,2); lcd.print("(press * to back)"); }
void showMenuSetting()  { lcd.clear(); lcd.setCursor(0,0); lcd.print("Setting");  lcd.setCursor(0,2); lcd.print("(press * to back)"); }
void showDeveloperHUD() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Target:");
  lcd.setCursor(0,1); lcd.print("Dist:");
  lcd.setCursor(0,2); lcd.print("Key:");
  showTargetCm(-1);
  showDistText("--");
  clearField(2, colKey, 1); lcd.setCursor(colKey,2); lcd.print(" ");
}

void checkSecretAndMaybeHome(char k) {
  if (k == 0) return;
  if (ui == UI_WAIT_CARD || ui == UI_MENU_MAIN || ui == UI_MENU_PLAY || ui == UI_MENU_TUTOR || ui == UI_MENU_SETTING) {
    secretBuf += k;
    if (secretBuf.length() > 6) secretBuf.remove(0, secretBuf.length() - 6);
    if (secretBuf == SECRET) {
      secretBuf = "";
      startHoming(false);   // ช้า เหมือนช่วงบูต
      ui = UI_BOOT;
    }
  }
}

void updateUI() {
  switch (ui) {
    case UI_BOOT:
      if (!homed && mode != MODE_HOMING) { startHoming(false); } // boot homing = ช้า
      if (mode == MODE_IDLE && homed) { showReady(); delay(2000); showScanCard(); ui = UI_WAIT_CARD; }
      break;
    default: break;
  }
}

// ===== UART link (MEGA RX2) =====
#define LINK_BAUD 115200
String linkLine;
String lastUid = "";
bool   uidAvailable = false;

void pollLink() {
  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (linkLine.startsWith("RFID:")) {
        lastUid = linkLine.substring(5);
        uidAvailable = true;
        if (DEBUG_LOG) { Serial.print("[LINK] UID="); Serial.println(lastUid); }
      }
      linkLine = "";
    } else {
      if (linkLine.length() < 64) linkLine += c;
    }
  }
}

void maybeShowUidOnScanPage() {
  if (ui == UI_WAIT_CARD && uidAvailable) {
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Card ID:");
    lcd.setCursor(0,1); lcd.print(lastUid);
    lcd.setCursor(0,3); lcd.print("Press A to Menu");
    uidAvailable = false;
  }
}

// ===== Setup / Loop =====
void setup() {
  Serial.begin(115200);
  while (!Serial);

  // UART from ESP32
  Serial2.begin(LINK_BAUD); // RX2=D17, TX2=D16

  // I2C + VL53L0X
  Wire.begin();
  sensor.setTimeout(500);
  if (!sensor.init()) { while (1) { Serial.println("VL53L0X init failed!"); delay(500); } }
  sensor.setSignalRateLimit(0.25);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
  sensor.setMeasurementTimingBudget(200000);

  // LCD
  lcd.init();      // ใช้ init() เพื่อความเข้ากันได้กว้าง
  lcd.backlight();
  lcd.clear();

  // Stepper & inputs
  stepper.setPinsInverted(false, DIR_INVERT, false);
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  pinMode(SW_FWD_PIN,  INPUT_PULLUP);
  pinMode(SW_BWD_PIN,  INPUT_PULLUP);
  pinMode(HOME_PIN,    INPUT_PULLUP);

  // Boot UI
  ui = UI_BOOT;
  lcd.setCursor(0,0); lcd.print("Getting Ready");
  lcd.setCursor(0,1); lcd.print("Calibrating...");
  lcd.setCursor(0,2); lcd.print("Please wait");

  // Config log
  Serial.println("=== CONFIG ===");
  Serial.println("UART LINK: ESP32 TX17 -> MEGA RX2(D17) @115200");
  Serial.print("DIR_INVERT="); Serial.println(DIR_INVERT ? "true" : "false");
  Serial.print("SoftLimit: "); Serial.print(MIN_POS_STEPS); Serial.print(" .. "); Serial.println(MAX_POS_STEPS);
  Serial.println("==============");
}

void loop() {
  unsigned long now = millis();

  // Always poll UART (non-blocking)
  pollLink();

  // Read keypad
  char k = keypad.getKey();

  // While moving/homing/jog: accept only '*' to abort (anti-stutter)
  if (mode != MODE_IDLE) {
    if (k == '*') { stepper.stop(); if (DEBUG_LOG) Serial.println("[CMD] STOP requested (moving)"); }

    if (mode == MODE_MOVING) {
      stepper.run();
      if (LOG_INTERVAL_MS && now - lastLogMs >= LOG_INTERVAL_MS) { logPosition("MOVING"); lastLogMs = now; }
      if (stepper.distanceToGo() == 0) { mode = MODE_IDLE; logPosition("ARRIVED"); postReadPending = true; }
    } else if (mode == MODE_HOMING) {
      updateHoming();
    }

    if (postReadPending && mode == MODE_IDLE) { readAndShowOnce(); postReadPending = false; }
    updateUI();
    maybeShowUidOnScanPage();
    return; // IMPORTANT: ignore other keys while moving/homing
  }

  // ===== IDLE-only key handling =====
  if (k) checkSecretAndMaybeHome(k);

  switch (ui) {
    case UI_BOOT:
      updateUI();
      break;

    case UI_WAIT_CARD:
      if (k == 'A') { showMenuMain(); ui = UI_MENU_MAIN; }
      break;

    case UI_MENU_MAIN:
      if (k == '*') { showScanCard(); ui = UI_WAIT_CARD; break; }
      if      (k == '1') { showMenuPlay();    ui = UI_MENU_PLAY; }
      else if (k == '2') { showMenuTutor();   ui = UI_MENU_TUTOR; }
      else if (k == '3') { showMenuSetting(); ui = UI_MENU_SETTING; }
      break;

    case UI_MENU_PLAY:
      if (k == '*') { showMenuMain(); ui = UI_MENU_MAIN; break; }
      if      (k == '1') { lcd.clear(); lcd.print("Training Mode"); }
      else if (k == '2') { lcd.clear(); lcd.print("Exam Mode"); }
      else if (k == '3') { showDeveloperHUD(); ui = UI_DEV_MODE; }
      break;

    case UI_MENU_TUTOR:
      if (k == '*') { showMenuMain(); ui = UI_MENU_MAIN; }
      break;

    case UI_MENU_SETTING:
      if (k == '*') { showMenuMain(); ui = UI_MENU_MAIN; }
      break;

    case UI_DEV_MODE:
      if (k) {
        clearField(2, colKey, 1); lcd.setCursor(colKey, 2); lcd.print(k);
        if      (k >= '1' && k <= '6') { selectedCm = mapKeyToCm(k); showTargetCm(selectedCm); }
        else if (k == 'A')             { if (selectedCm >= 0) startMoveToCm(selectedCm); }
        else if (k == '*')             { showMenuMain(); ui = UI_MENU_MAIN; selectedCm = -1; showTargetCm(-1); }
        else if (k == '0')             { startHoming(true); ui = UI_BOOT; } // FAST homing = ACCELERATION
      }
      break;
  }

  // Motor updates (IDLE branch)
  if (mode == MODE_MOVING) {
    stepper.run();
    if (LOG_INTERVAL_MS && now - lastLogMs >= LOG_INTERVAL_MS) { logPosition("MOVING"); lastLogMs = now; }
    if (stepper.distanceToGo() == 0) { mode = MODE_IDLE; logPosition("ARRIVED"); postReadPending = true; }
  } else if (mode == MODE_HOMING) {
    updateHoming();
  } else {
    updateJog();
  }

  if (postReadPending && mode == MODE_IDLE) { readAndShowOnce(); postReadPending = false; }

  updateUI();
  maybeShowUidOnScanPage(); // Show Card ID at Scan page if received
}
