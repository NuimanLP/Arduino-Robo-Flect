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
#define SW_FWD_PIN   16    // JOG เดินหน้า (Active LOW)
#define SW_BWD_PIN   17    // JOG ถอยหลัง (Active LOW) — ไม่ใช่ลิมิตแล้ว

const float MAX_SPEED    = 1333.0;    // pulses/sec
const float ACCELERATION = 2000.0;    // pulses/sec^2
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

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
byte rowPins[ROWS] = {30, 31, 32, 33};
byte colPins[COLS] = {26, 27, 28, 29};
Keypad keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ====== LCD columns ======
const uint8_t colTarget = 8; // แถว0: Target cm
const uint8_t colDist   = 6; // แถว1: Dist
const uint8_t colKey    = 5; // แถว2: Key

// ====== Helpers ======
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
// ตั้งค่าให้ตรงฮาร์ดแวร์จริง
const float PULSES_PER_REV = 400.0;   // DIP DM542 (ถ้าเป็น 200 ให้เปลี่ยนเป็น 200.0)
const float BELT_PITCH_MM  = 2.0;     // GT2 = 2.0mm
const float PULLEY_TEETH   = 20.0;    // ฟันพูลเลย์จริง
const float CAL            = 0.50;

const float MM_PER_REV     = BELT_PITCH_MM * PULLEY_TEETH;           // mm/รอบ
const float PULSES_PER_MM  = (PULSES_PER_REV / MM_PER_REV) * CAL;    // pulses/mm
const float STEPS_PER_CM   = PULSES_PER_MM * 10.0;                   // pulses/cm

// ====== Travel Soft-limit (ระยะราง 70 cm) ======
const float TRAVEL_MAX_CM   = 70.0;     // start → end
const float SAFE_MARGIN_CM  = 0.5;      // กันชนเล็กน้อย
const long  MIN_POS_STEPS   = 0;        // Home = 0
const long  MAX_POS_STEPS   = (long)((TRAVEL_MAX_CM - SAFE_MARGIN_CM) * STEPS_PER_CM);

// ====== Motion State Machine ======
enum Mode : uint8_t { MODE_IDLE, MODE_MOVING, MODE_JOG };
Mode mode = MODE_IDLE;

// ====== Selection/Targets ======
int   selectedCm   = -1;     // ค่าที่เลือก (ยังไม่ยืนยัน)
long  targetSteps  = 0;      // เป้าหมายสเต็ป (สัมบูรณ์จาก Home)

// ====== Flags / state ======
bool  postReadPending = false;     // อ่าน “หลังจบการเคลื่อนที่หนึ่งครั้ง”
unsigned long lastLogMs = 0;       // throttle log
bool  homeSet = false;             // ตั้ง Home แล้วหรือยัง (ไม่มีลิมิตสวิตช์ → soft home)

// ====== Key memory ======
char lastKey = ' ';

// ====== Debug helpers ======
#if DEBUG_LOG
void logPosition(const char* tag) {
  long pos = stepper.currentPosition();
  float cm = pos / STEPS_PER_CM;
  Serial.print("["); Serial.print(tag); Serial.print("] pos=");
  Serial.print(pos); Serial.print(" pulses (");
  Serial.print(cm, 2); Serial.println(" cm)");
}
void logTargetInfo(float requestedCm, float limitedCm, long tgt) {
  Serial.print("[MOVE] req="); Serial.print(requestedCm, 2); Serial.print(" cm");
  Serial.print("  clamped=");  Serial.print(limitedCm, 2);  Serial.print(" cm");
  Serial.print("  target=");   Serial.print(tgt);           Serial.println(" pulses");
}
void logMsg(const char* m) { Serial.println(m); }
#else
void logPosition(const char*) {}
void logTargetInfo(float,float,long) {}
void logMsg(const char*) {}
#endif

// ปุ่มตัวเลข → cm
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

// ====== Move ======
void startMoveToCm(float cm) {
  if (cm < 0) return;

  // clamp ตาม soft-limit
  float limitCm = cm;
  if (limitCm > (TRAVEL_MAX_CM - SAFE_MARGIN_CM)) limitCm = (TRAVEL_MAX_CM - SAFE_MARGIN_CM);
  if (limitCm < 0) limitCm = 0;

  // อ่าน 1 ครั้งก่อนเริ่ม
  readAndShowOnce();

  targetSteps = (long)(limitCm * STEPS_PER_CM + 0.5f);
  if (targetSteps > MAX_POS_STEPS) targetSteps = MAX_POS_STEPS;
  if (targetSteps < MIN_POS_STEPS) targetSteps = MIN_POS_STEPS;

  logTargetInfo(cm, limitCm, targetSteps);
  logPosition("START_FROM");

  stepper.moveTo(targetSteps);
  mode = MODE_MOVING;
  lastLogMs = 0;
}

// ====== Soft Home (ไม่มีลิมิตสวิตช์) ======
void goHomeOrSetZero() {
  if (!homeSet) {
    // กด 0 ครั้งแรก → ตั้งตำแหน่งปัจจุบันเป็น Home
    stepper.setCurrentPosition(0);
    homeSet = true;
    logMsg("[HOME] Set current position as 0 cm (soft home)");
    lcd.setCursor(0,0); lcd.print("Target:"); showTargetCm(0);
    postReadPending = true; // อ่าน 1 ครั้งเพื่ออัปเดตจอ
  } else {
    // ตั้งแล้ว → วิ่งกลับ Home (0 cm)
    startMoveToCm(0);
  }
}

// ====== JOG (ระหว่าง IDLE เท่านั้น) ======
void updateJog() {
  if (mode != MODE_IDLE) return; // กันแทรกระหว่าง MOVE

  bool fwd = (digitalRead(SW_FWD_PIN) == LOW);
  bool bwd = (digitalRead(SW_BWD_PIN) == LOW);

  long pos = stepper.currentPosition();

  if (fwd ^ bwd) {
    // กันชนขอบซอฟต์
    if (fwd && pos >= MAX_POS_STEPS) { stepper.setSpeed(0); logMsg("[JOG] limit @ MAX"); return; }
    if (bwd && pos <= MIN_POS_STEPS) { stepper.setSpeed(0); logMsg("[JOG] limit @ MIN"); return; }

    // เริ่ม JOG (อ่านก่อน 1 ครั้ง)
    static bool jogging = false;
    if (!jogging) {
      readAndShowOnce();
      logMsg("[JOG] start");
      logPosition("JOG_FROM");
      jogging = true;
    }
    stepper.setSpeed(fwd ? MAX_SPEED : -MAX_SPEED);
    stepper.runSpeed();       // ระหว่าง JOG “ไม่อ่านเซ็นเซอร์”

    if (!(fwd ^ bwd)) jogging = false; // ป้องกันค้าง state
  } else {
    // ปล่อยปุ่ม → หยุด & อ่านหลังจบ
    static bool wasJogging = false;
    if (stepper.speed() != 0) wasJogging = true;
    stepper.setSpeed(0);
    if (wasJogging) {
      logMsg("[JOG] stop");
      logPosition("JOG_TO");
      postReadPending = true;
      wasJogging = false;
    }
  }
}

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
  lcd.begin(); lcd.backlight(); lcd.clear();
  lcd.setCursor(0,0); lcd.print("Target:");
  lcd.setCursor(0,1); lcd.print("Dist:");
  lcd.setCursor(0,2); lcd.print("Key:");
  showTargetCm(-1);
  showDistText("--");
  clearField(2, colKey, 1); lcd.setCursor(colKey,2); lcd.print(" ");

  // Stepper
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  pinMode(SW_FWD_PIN, INPUT_PULLUP);
  pinMode(SW_BWD_PIN, INPUT_PULLUP);

  // แสดงพารามิเตอร์หลัก
  Serial.println("=== CONFIG ===");
  Serial.print("PULSES_PER_REV="); Serial.println(PULSES_PER_REV);
  Serial.print("BELT_PITCH_MM=");  Serial.println(BELT_PITCH_MM);
  Serial.print("PULLEY_TEETH=");   Serial.println(PULLEY_TEETH);
  Serial.print("CAL=");            Serial.println(CAL, 6);
  Serial.print("PULSES_PER_MM=");  Serial.println(PULSES_PER_MM, 6);
  Serial.print("STEPS_PER_CM=");   Serial.println(STEPS_PER_CM, 6);
  Serial.print("SoftLimit: 0..");  Serial.print(MAX_POS_STEPS); Serial.print(" pulses (");
  Serial.print((float)MAX_POS_STEPS / STEPS_PER_CM, 2); Serial.println(" cm)");
  Serial.println("HOME: not set (press 0 to set here)");
  Serial.println("==============");
  Serial.println("Ready.");
}

void loop() {
  unsigned long now = millis();

  // ====== Keypad ======
  char k = keypad.getKey();
  if (k && k != lastKey) {
    lastKey = k;
    clearField(2, colKey, 1);
    lcd.setCursor(colKey, 2);
    lcd.print(k);
    Serial.print("[KEY] "); Serial.println(k);

    if (k >= '1' && k <= '6') {
      int cm = mapKeyToCm(k);
      selectedCm = cm;
      showTargetCm(selectedCm); // แสดง “ที่เลือก” รอ A
      Serial.print("[SELECT] "); Serial.print(selectedCm); Serial.println(" cm");
    }
    else if (k == 'A') {
      if (selectedCm >= 0) startMoveToCm(selectedCm);
    }
    else if (k == '*') {
      stepper.stop(); // ชะลอจนหยุด
      Serial.println("[CMD] STOP requested");
      selectedCm = -1;
      showTargetCm(-1);
      // postReadPending จะถูก set เมื่อหยุดจริง
    }
    else if (k == '0') {
      Serial.println("[CMD] HOME (soft) requested");
      goHomeOrSetZero();
    }
    else if (k == 'B') {
      // TODO: playNarration(); // ยังไม่ใช้ตามสั่ง
      Serial.println("[CMD] Narration (B) is commented out.");
    }
  }

  // ====== อัปเดตการเคลื่อนที่ ======
  if (mode == MODE_MOVING) {
    stepper.run(); // ไปยัง moveTo()

    if (DEBUG_LOG && LOG_INTERVAL_MS > 0 && (now - lastLogMs >= LOG_INTERVAL_MS)) {
      logPosition("MOVING");
      lastLogMs = now;
    }

    if (stepper.distanceToGo() == 0) {
      mode = MODE_IDLE;
      logPosition("ARRIVED");
      postReadPending = true; // จะไปอ่าน “หลังจบ”
    }
  }
  else {
    // ว่าง → อนุญาต JOG (อ่านเฉพาะตอนเริ่ม/จบ JOG) + กันชนขอบ
    updateJog();
  }

  // ====== อ่าน “หลังจบ” ครั้งเดียว (เมื่อหยุดจริงและอยู่ IDLE) ======
  if (postReadPending && mode == MODE_IDLE) {
    readAndShowOnce();
    postReadPending = false;
  }
}