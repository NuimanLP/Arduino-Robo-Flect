#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>
#include <VL53L0X.h>
#include <Keypad.h>

// ====== Debug logging ======
#define DEBUG_LOG        1
#define LOG_INTERVAL_MS  200   // log ตำแหน่งระหว่างวิ่งทุก ๆ x ms (0=ปิด)

// ====== Stepping Motor (Driver mode) ======
#define STEP_PIN     14
#define DIR_PIN      15
#define SW_FWD_PIN   16    // JOG เดินหน้า (ไป "ซ้าย" = หา Home) Active LOW
#define SW_BWD_PIN   17    // JOG ถอยหลัง (ไป "ขวา" = หามอเตอร์) Active LOW

// ====== Home switch ======
#define HOME_PIN       18        // ลิมิตสวิตช์ NO (ต่อ GND), Active LOW
const bool HOME_ACTIVE_LOW = true;

// ====== ความเร็ว/เร่ง ======
const float MAX_SPEED      = 1333.0;  // pulses/sec (สำหรับการวิ่งปกติ)
const float ACCELERATION   = 500.0;   // pulses/sec^2
const float HOMING_SPEED_SLOW = 200.0; // ความเร็วเฉพาะตอนเข้า Home ช้า ๆ
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ---- Invert DIR line (flip motor direction in hardware) ----
const bool DIR_INVERT = true;   // ถ้ายังไปผิดทิศ ให้สลับ true/false ตรงนี้

// ====== LCD 20x4 (I2C) ======
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ====== VL53L0X ======
VL53L0X sensor;

// ====== Keypad 4x4 ======
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
// ตามที่คุณสลับพินไว้
byte rowPins[ROWS] = {29, 28, 27, 26};
byte colPins[COLS] = {33, 32, 31, 30};
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ====== LCD columns ======
const uint8_t colTarget = 8; // แถว0: Target cm
const uint8_t colDist   = 6; // แถว1: Dist
const uint8_t colKey    = 5; // แถว2: Key

// ====== นิยามทิศ ======
// คงตรรกะ: FWD = ไปซ้าย (หา Home) , BWD = ไปขวา (หามอเตอร์)
const int DIR_SIGN_FWD = +1;  // setSpeed(+X) = ไปซ้าย
const int DIR_SIGN_BWD = -1;  // setSpeed(-X) = ไปขวา
// *** กำหนดแกนตำแหน่ง *** : 0 = ซ้าย (Home), ไปขวา = ค่าลบ
const int POS_SIGN_RIGHT = -1;  // ระยะทางทางขวาให้เป็น "ค่าลบ"

// ====== Helpers (LCD) ======
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

// ====== Kinematics (Belt + DM542) ======
const float PULSES_PER_REV = 400.0;   // DIP DM542 (ถ้า DIP เป็น 200 ให้เปลี่ยนเป็น 200.0)
const float BELT_PITCH_MM  = 2.0;     // GT2 = 2.0mm
const float PULLEY_TEETH   = 20.0;    // ฟันพูลเลย์จริง
const float CAL            = 0.50;    // จากเคสวิ่งเกิน ~2× → ใช้ 0.50

const float MM_PER_REV     = BELT_PITCH_MM * PULLEY_TEETH;           // mm/รอบ
const float PULSES_PER_MM  = (PULSES_PER_REV / MM_PER_REV) * CAL;    // pulses/mm
const float STEPS_PER_CM   = PULSES_PER_MM * 10.0;                   // pulses/cm

// ====== Travel Soft-limit (ระยะราง 70 cm) ======
// ตำแหน่งขณะใช้งาน: [MIN_NEGATIVE .. 0], โดย 0 = ซ้ายสุด
const float TRAVEL_MAX_CM   = 70.0;     // start → end
const float SAFE_MARGIN_CM  = 0.5;      // กันชนเล็กน้อย
const long  MIN_POS_STEPS   = (long)(POS_SIGN_RIGHT * (TRAVEL_MAX_CM - SAFE_MARGIN_CM) * STEPS_PER_CM); // ค่าลบ
const long  MAX_POS_STEPS   = 0;        // ซ้ายสุด

// ====== Motion State Machine ======
enum Mode : uint8_t { MODE_IDLE, MODE_MOVING, MODE_HOMING, MODE_JOG };
Mode mode = MODE_IDLE;

// ====== Selection/Targets ======
int   selectedCm   = -1;     // ค่าที่เลือก (ยังไม่ยืนยัน)
long  targetSteps  = 0;      // เป้าหมายสเต็ป (สัมบูรณ์จาก Home)

// ====== Flags / state ======
bool  postReadPending = false;     // อ่าน “หลังจบการเคลื่อนที่หนึ่งครั้ง”
unsigned long lastLogMs = 0;       // throttle log
bool  homed = false;               // โฮมแล้วหรือยัง

// ====== Homing params ======
const float BACKOFF_MM        = 3.0;     // ถอยออกจากสวิตช์ก่อนตั้งศูนย์
const unsigned long HOMING_TIMEOUT_MS = 20000;

enum HomeStage : uint8_t { HS_IDLE, HS_APPROACH, HS_BACKOFF, HS_LATCH, HS_DONE };
HomeStage hs = HS_IDLE;

inline bool homeTriggered() {
  int v = digitalRead(HOME_PIN);
  return HOME_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

// ====== Debug helpers ======
#if DEBUG_LOG
void logPosition(const char* tag) {
  long pos = stepper.currentPosition();
  float cm = (float)pos / STEPS_PER_CM * (POS_SIGN_RIGHT == -1 ? -1 : 1); // แปลงให้ขวาเป็น +cm ใน log
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

// ====== Key → cm (จาก Home ไปทางขวา) ======
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

// ====== Sensor: single-shot only (ไม่มีการอ่านระหว่างเลื่อน) ======
int readDistanceCmOnce() {
  uint16_t mm = sensor.readRangeSingleMillimeters(); // single-shot
  bool to = sensor.timeoutOccurred();
  int dcm = (!to && mm != 65535 && mm/10 != 819) ? mm/10 : -1;
  return dcm;
}
void readAndShowOnce() {
  int dcm = readDistanceCmOnce();
  if (dcm >= 0) {
    char buf[8]; snprintf(buf, sizeof(buf), "%dcm", dcm);
    showDistText(buf);
    Serial.print("[SENSOR] Dist: "); Serial.println(buf);
  } else {
    showDistText("--");
    Serial.println("[SENSOR] Dist: --");
  }
}

// ====== Move: ไปตำแหน่งสัมบูรณ์ (cm จาก Home → ขวาเป็นบวกสำหรับผู้ใช้) ======
void startMoveToCm(float cm_right) {
  if (!homed) { logMsg("[WARN] Not homed. Press 0 to HOME first."); return; }
  if (cm_right < 0) cm_right = 0;

  // clamp ตาม soft-limit
  float limitCm = cm_right;
  if (limitCm > (TRAVEL_MAX_CM - SAFE_MARGIN_CM)) limitCm = (TRAVEL_MAX_CM - SAFE_MARGIN_CM);

  // อ่าน 1 ครั้งก่อนเริ่ม
  readAndShowOnce();

  // ขวา = ค่าลบ (ในแกนสเต็ป)
  long absSteps = (long)(limitCm * STEPS_PER_CM + 0.5f);
  targetSteps   = POS_SIGN_RIGHT * absSteps;

  // Clamp steps ให้อยู่ในช่วง [MIN_NEGATIVE .. 0]
  if (targetSteps < MIN_POS_STEPS) targetSteps = MIN_POS_STEPS;
  if (targetSteps > MAX_POS_STEPS) targetSteps = MAX_POS_STEPS;

  logTargetInfo(cm_right, limitCm, targetSteps);
  logPosition("START_FROM");

  stepper.moveTo(targetSteps);
  mode = MODE_MOVING;
  lastLogMs = 0;
}

// ====== Homing (วิ่งช้าไปซ้าย หา Home) ======
unsigned long homingStartMs = 0;

void startHoming() {
  // แสดงสถานะบนจอ (ใช้ใน Boot flow ด้วย)
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Getting Ready");
  lcd.setCursor(0,1); lcd.print("Calibrating...");
  lcd.setCursor(0,2); lcd.print("Please wait");

  // อ่าน 1 ครั้งก่อนเริ่ม
  readAndShowOnce();

  homed = false;
  hs = HS_APPROACH;
  mode = MODE_HOMING;
  homingStartMs = millis();

  // ตั้งความเร็ว homing ช้า ๆ
  stepper.setMaxSpeed(HOMING_SPEED_SLOW);
  stepper.setAcceleration(0); // homing แบบ runSpeed ไม่พึ่ง accel
  stepper.setSpeed(DIR_SIGN_FWD * HOMING_SPEED_SLOW);
  logMsg("[HOMING] start → APPROACH (toward left)");
}

void finishHomingRestoreMotion() {
  // หลัง calibrate เสร็จ: คืนค่าความเร็ว/เร่งปกติ
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
}

void updateHoming() {
  if (mode != MODE_HOMING) return;

  // timeout กันค้าง
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
    case HS_APPROACH: {
      if (!homeTriggered()) {
        stepper.runSpeed();  // วิ่งไปซ้ายเรื่อย ๆ
      } else {
        hs = HS_BACKOFF;
        logMsg("[HOMING] hit switch → BACKOFF");
      }
    } break;

    case HS_BACKOFF: {
      long backoffSteps = (long)(BACKOFF_MM * PULSES_PER_MM + 0.5f);
      stepper.move(DIR_SIGN_BWD * backoffSteps); // ถอยไปขวาเล็กน้อย
      while (stepper.distanceToGo() != 0) stepper.run();
      stepper.setSpeed(DIR_SIGN_FWD * HOMING_SPEED_SLOW);
      hs = HS_LATCH;
      logMsg("[HOMING] BACKOFF → LATCH (slow approach)");
    } break;

    case HS_LATCH: {
      if (!homeTriggered()) {
        stepper.runSpeed();  // เข้าใกล้ช้า ๆ จนชนอีกครั้ง
      } else {
        // ถอยออกจากสวิตช์แล้วตั้ง pos=0
        long releaseSteps = (long)(BACKOFF_MM * PULSES_PER_MM + 0.5f);
        stepper.move(DIR_SIGN_BWD * releaseSteps); // ถอยไปขวาให้หลุดสวิตช์
        while (stepper.distanceToGo() != 0) stepper.run();

        stepper.setCurrentPosition(0);  // 0 = ซ้าย (Home)
        homed = true;

        hs = HS_DONE;
        logMsg("[HOMING] DONE (pos=0 at left, off-switch)");
      }
    } break;

    case HS_DONE: {
      mode = MODE_IDLE;
      hs = HS_IDLE;
      finishHomingRestoreMotion();   // <<< คืนค่าความเร็ว/เร่งปกติที่นี่
      postReadPending = true;        // อ่านหลังจบ
      // UI flow หลังโฮมเสร็จจะทำต่อใน updateUI()
    } break;

    default: break;
  }
}

// ====== JOG (เฉพาะตอน IDLE) ======
void updateJog() {
  if (mode != MODE_IDLE) return;

  bool fwd = (digitalRead(SW_FWD_PIN) == LOW); // ไปซ้าย
  bool bwd = (digitalRead(SW_BWD_PIN) == LOW); // ไปขวา
  long pos = stepper.currentPosition();

  if (fwd ^ bwd) {
    if (fwd && pos >= MAX_POS_STEPS) { stepper.setSpeed(0); logMsg("[JOG] limit @ LEFT (0)"); return; }
    if (bwd && pos <= MIN_POS_STEPS) { stepper.setSpeed(0); logMsg("[JOG] limit @ RIGHT (min)"); return; }

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

// =========================================
// ========== UI / MENU STATE ==============
// =========================================
enum UIState : uint8_t {
  UI_BOOT,        // แสดง Getting Ready + เรียก Homing
  UI_WAIT_CARD,   // Robo-Flect Ready → Scan Your Card (กด A เข้าหน้าเมนู)
  UI_MENU_MAIN,   // 1.Play 2.Tutorial 3.Setting
  UI_MENU_PLAY,   // 1.Training 2.Exam 3.Developer
  UI_MENU_TUTOR,  // placeholder
  UI_MENU_SETTING,// placeholder
  UI_DEV_MODE     // Developer Mode (โหมดควบคุมเดิม: Target Dist / keypad)
};
UIState ui = UI_BOOT;

// เมนูเลือกได้ (index เริ่ม 1)
int menuIndex = 1;

// กุญแจลับสำหรับ homing
const char *SECRET = "0*99AB";
String secretBuf;

// ---- Rendering ----
void showBoot() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Getting Ready");
  lcd.setCursor(0,1); lcd.print("Calibrating...");
  lcd.setCursor(0,2); lcd.print("Please wait");
}
void showReady() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Robo-Flect Ready");
}
void showScanCard() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Scan Your Card");
  lcd.setCursor(0,1); lcd.print("(Press A to Menu)");
}
void showMenuMain() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Main Menu");
  lcd.setCursor(0,1); lcd.print("1.Play Mode");
  lcd.setCursor(0,2); lcd.print("2.Tutorial");
  lcd.setCursor(0,3); lcd.print("3.Setting");
}
void showMenuPlay() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Play Mode");
  lcd.setCursor(0,1); lcd.print("1.Training");
  lcd.setCursor(0,2); lcd.print("2.Exam");
  lcd.setCursor(0,3); lcd.print("3.Developer");
}
void showMenuTutor() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Tutorial");
  lcd.setCursor(0,2); lcd.print("(press * to back)");
}
void showMenuSetting() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Setting");
  lcd.setCursor(0,2); lcd.print("(press * to back)");
}
void showDeveloperHUD() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Target:");
  lcd.setCursor(0,1); lcd.print("Dist:");
  lcd.setCursor(0,2); lcd.print("Key:");
  showTargetCm(-1);
  showDistText("--");
  clearField(2, colKey, 1); lcd.setCursor(colKey,2); lcd.print(" ");
}

// ตรวจรหัสลับ (เรียก homing)
void checkSecretAndMaybeHome(char k) {
  if (k == 0) return;
  // เก็บเฉพาะจาก WAIT_CARD และเมนู
  if (ui == UI_WAIT_CARD || ui == UI_MENU_MAIN || ui == UI_MENU_PLAY || ui == UI_MENU_TUTOR || ui == UI_MENU_SETTING) {
    secretBuf += k;
    if (secretBuf.length() > 6) secretBuf.remove(0, secretBuf.length() - 6);
    if (secretBuf == SECRET) {
      secretBuf = "";
      startHoming(); // เริ่ม homing
      ui = UI_BOOT;  // กลับไป flow boot อีกครั้ง
    }
  }
}

// จัดการ UI ตาม state (render และ transitionหลัง homing)
void updateUI() {
  switch (ui) {
    case UI_BOOT:
      // เริ่ม homing ครั้งเดียวเมื่อ boot
      if (!homed && mode != MODE_HOMING) {
        showBoot();
        startHoming();
      }
      // เมื่อ homing เสร็จ (MODE_IDLE && homed) → Ready → Scan Card
      if (mode == MODE_IDLE && homed) {
        showReady();
        delay(2000);
        showScanCard();
        ui = UI_WAIT_CARD;
      }
      break;

    case UI_WAIT_CARD:
      // รอ A เพื่อเข้าเมนู
      break;

    case UI_MENU_MAIN:
      // render ครั้งแรกโดย caller
      break;

    case UI_MENU_PLAY:
      break;

    case UI_MENU_TUTOR:
      break;

    case UI_MENU_SETTING:
      break;

    case UI_DEV_MODE:
      // โหมดควบคุมเดิม — จัดการใน loop key handler
      break;
  }
}

// =========================================
// ========== SETUP / LOOP =================
// =========================================
void setup() {
  Serial.begin(115200);
  while (!Serial);

  // I2C + VL53L0X (single-shot setup)
  Wire.begin();
  sensor.setTimeout(500);
  Serial.println("Checking VL53L0X...");
  if (!sensor.init()) {
    Serial.println("VL53L0X init failed!");
    while (1);
  }
  sensor.setSignalRateLimit(0.25);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
  sensor.setMeasurementTimingBudget(200000);
  Serial.println("VL53L0X OK (single-shot)");

  // LCD
  lcd.begin(20,4); lcd.backlight(); lcd.clear();

  // Stepper & inputs
  stepper.setPinsInverted(false, DIR_INVERT, false);
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  pinMode(SW_FWD_PIN,  INPUT_PULLUP);
  pinMode(SW_BWD_PIN,  INPUT_PULLUP);
  pinMode(HOME_PIN,    INPUT_PULLUP);  // NO to GND ⇒ Active LOW

  // แสดงพารามิเตอร์หลัก
  Serial.println("=== CONFIG ===");
  Serial.print("PULSES_PER_REV="); Serial.println(PULSES_PER_REV);
  Serial.print("BELT_PITCH_MM=");  Serial.println(BELT_PITCH_MM);
  Serial.print("PULLEY_TEETH=");   Serial.println(PULLEY_TEETH);
  Serial.print("CAL=");            Serial.println(CAL, 6);
  Serial.print("PULSES_PER_MM=");  Serial.println(PULSES_PER_MM, 6);
  Serial.print("STEPS_PER_CM=");   Serial.println(STEPS_PER_CM, 6);
  Serial.print("SoftLimit: ");     Serial.print(MIN_POS_STEPS); Serial.print(" .. "); Serial.println(MAX_POS_STEPS);
  Serial.print("DIR_INVERT=");     Serial.println(DIR_INVERT ? "true" : "false");
  Serial.println("Coord: Home(0) left, right is NEGATIVE steps");
  Serial.println("==============");

  // เข้าสู่ BOOT UI (จะเรียก homing อัตโนมัติ)
  ui = UI_BOOT;
  showBoot();
}

void loop() {
  unsigned long now = millis();

  // ====== Keypad ======
  char k = keypad.getKey();

  // ระหว่างเคลื่อนที่/โฮม/จ๊อก: รับเฉพาะ '*' เพื่อ abort (กัน stutter)
  if (mode != MODE_IDLE) {
    if (k == '*') {
      stepper.stop();  // ค่อย ๆ ชะลอหยุด
      if (DEBUG_LOG) Serial.println("[CMD] STOP requested (moving)");
    }
    // ยังอ่าน UI เพื่อ transition หลังหยุด
    if (mode == MODE_MOVING) {
      stepper.run();
      if (LOG_INTERVAL_MS && now - lastLogMs >= LOG_INTERVAL_MS) { logPosition("MOVING"); lastLogMs = now; }
      if (stepper.distanceToGo() == 0) { mode = MODE_IDLE; logPosition("ARRIVED"); postReadPending = true; }
    } else if (mode == MODE_HOMING) {
      updateHoming();
    } else if (mode == MODE_JOG) {
      // (ไม่มีโหมดจ๊อกระหว่าง move ในดีไซน์นี้)
    }
    // อ่านหลังจบ
    if (postReadPending && mode == MODE_IDLE) { readAndShowOnce(); postReadPending = false; }
    updateUI();
    return; // <<< สำคัญ: ตัดการประมวลผลคีย์อื่นทั้งหมด ขณะกำลังเคลื่อน
  }

  // ====== เมื่อ IDLE เท่านั้น จึงประมวลผลคีย์เต็มรูปแบบ ======

  // เช็ครหัสลับในหน้า ScanCard/เมนู
  if (k) checkSecretAndMaybeHome(k);

  // UI state machine
  switch (ui) {
    case UI_BOOT:
      updateUI(); // จะเรียก homing และเปลี่ยน state เอง
      break;

    case UI_WAIT_CARD:
      // กด A เพื่อเข้าเมนูหลัก
      if (k == 'A') { showMenuMain(); ui = UI_MENU_MAIN; }
      break;

    case UI_MENU_MAIN:
      if (k == '*') { showScanCard(); ui = UI_WAIT_CARD; break; }
      if (k == '1') { showMenuPlay();    ui = UI_MENU_PLAY; }
      else if (k == '2') { showMenuTutor();   ui = UI_MENU_TUTOR; }
      else if (k == '3') { showMenuSetting(); ui = UI_MENU_SETTING; }
      break;

    case UI_MENU_PLAY:
      if (k == '*') { showMenuMain(); ui = UI_MENU_MAIN; break; }
      if (k == '1') { /* Training Mode - demo only */ lcd.clear(); lcd.print("Training Mode"); }
      else if (k == '2') { /* Exam Mode - demo only */ lcd.clear(); lcd.print("Exam Mode"); }
      else if (k == '3' || (k == 'A' && menuIndex==3)) {
        // เข้า Developer Mode
        showDeveloperHUD();
        ui = UI_DEV_MODE;
      }
      break;

    case UI_MENU_TUTOR:
      if (k == '*') { showMenuMain(); ui = UI_MENU_MAIN; }
      break;

    case UI_MENU_SETTING:
      if (k == '*') { showMenuMain(); ui = UI_MENU_MAIN; }
      break;

    case UI_DEV_MODE:
      // ========== Developer Mode: พฤติกรรมเดิม ==========
      if (k) {
        clearField(2, colKey, 1);
        lcd.setCursor(colKey, 2);
        lcd.print(k);
        if (DEBUG_LOG) { Serial.print("[KEY] "); Serial.println(k); }

        if (k >= '1' && k <= '6') {
          selectedCm = mapKeyToCm(k);          // 10..60 cm (ไปขวา)
          showTargetCm(selectedCm);
          if (DEBUG_LOG) { Serial.print("[SELECT] "); Serial.print(selectedCm); Serial.println(" cm (right)"); }
        }
        else if (k == 'A') {
          if (selectedCm >= 0) startMoveToCm(selectedCm);
        }
        else if (k == '*') {
          // ใน Dev Mode: * = Back ไป Main Menu (ถ้าไม่ได้กำลังวิ่ง)
          showMenuMain(); ui = UI_MENU_MAIN;
          selectedCm = -1; showTargetCm(-1);
        }
        else if (k == '0') {
          startHoming(); ui = UI_BOOT; // บังคับกลับ flow boot
        }
      }
      break;
  }

  // ====== อัปเดตมอเตอร์เมื่อ IDLE ======
  if (mode == MODE_MOVING) {
    stepper.run();
    if (LOG_INTERVAL_MS && now - lastLogMs >= LOG_INTERVAL_MS) { logPosition("MOVING"); lastLogMs = now; }
    if (stepper.distanceToGo() == 0) { mode = MODE_IDLE; logPosition("ARRIVED"); postReadPending = true; }
  } else if (mode == MODE_HOMING) {
    updateHoming();
  } else {
    updateJog(); // jog เฉพาะ IDLE
  }

  // อ่าน “หลังจบ” ครั้งเดียว
  if (postReadPending && mode == MODE_IDLE) { readAndShowOnce(); postReadPending = false; }

  // อัปเดต UI (เช่น flow หลัง homing)
  updateUI();
}
