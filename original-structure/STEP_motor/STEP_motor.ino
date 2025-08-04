// โค้ดนี้ใช้สำหรับควบคุมสเต็ปเปอร์มอเตอร์
#include <Stepper.h>
#include <AccelStepper.h>

// กำหนดขาเชื่อมต่อกับไดร์เวอร์
#define STEP_PIN 7   // ขาส่งพัลส์ (STEP หรือ PUL+) ON mega use PIN 7, UNO 9
#define DIR_PIN  6   // ขาส่งทิศทาง (DIR+) ON mega use  PIN6 , UNO 8

// ตั้งค่าก้าวทั้งหมดที่ต้องการให้เป็นระยะ 70 cm
const long STEPS_PER_RUN = 3366;  //400 ~17 cm 1.)3366 ~70cm [2.37A] 2.)

// ค่าความเร็วสูงสุดและการเร่ง (ปรับได้ตามมอเตอร์และโหลด)
const float MAX_SPEED     = 1000.0;   // หน่วย: ก้าว/วินาที ,1000 [2200 MAX]
const float ACCELERATION  = 500.0;    // หน่วย: ก้าว/วินาที²,500 [2200 MAX]

// สร้างอ็อบเจ็กต์ AccelStepper แบบใช้ driver 2 ขา
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

void setup() {
  // ตั้งความเร็วสูงสุดและอัตราเร่ง
  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
}

void loop() {
  // —── เดินหน้า 70 cm ────────────────────────
  stepper.moveTo(STEPS_PER_RUN);    // ตั้งเป้าหมายตำแหน่ง (ก้าว)
  while (stepper.distanceToGo() != 0) {
    stepper.run();                  // เรียกให้ขยับทีละนิดจนถึงเป้า
  }
  delay(1000);                      // หยุดพัก 1 วินาที

  // —── ถอยหลังกลับจุดเริ่มต้น ─────────────
  stepper.moveTo(0);               // กลับไปตำแหน่งก้าวศูนย์
  while (stepper.distanceToGo() != 0) {
    stepper.run();
  }
  delay(1000);                     // หยุดพัก 1 วินาที
}