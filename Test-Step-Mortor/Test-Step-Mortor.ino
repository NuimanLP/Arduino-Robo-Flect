#include <AccelStepper.h>

// —————— กำหนดพิน และค่าก้าวต่อรอบ ——————
#define IN1 24
#define IN2 25
#define IN3 26
#define IN4 27
const int STEPS_PER_REV = 2048;        // ก้าวที่ต้องเดินครบ 360°

/*
  สร้างอ็อบเจ็กต์ AccelStepper
  - HALF4WIRE: ใช้ half‐step กับมอเตอร์ unipolar 4 ขด
  - พินเรียงตามลำดับ: pin1, pin2, pin3, pin4
  (ใช้ลำดับเดียวกับ Stepper library: IN1,IN3,IN2,IN4)
*/
AccelStepper stepper(AccelStepper::HALF4WIRE,
                     IN1, IN3, IN2, IN4);

void setup() {
  Serial.begin(9600);
  
  // ตั้งค่าสูงสุดของความเร็ว (steps ต่อวินาที)
  stepper.setMaxSpeed(600.0);
  // ตั้งค่าอัตราเร่ง (steps ต่อวินาทีกำลังสอง)
  stepper.setAcceleration(100.0);

  Serial.println("AccelStepper ready");
}

void loop() {
  // ——— หมุน CW 1 รอบ ———
  Serial.println("CW 1 rev");
  stepper.moveTo(+STEPS_PER_REV);     // ตั้งเป้าก้าวไปข้างหน้า
  stepper.runToPosition();            // บล็อกจนถึงตำแหน่ง
  delay(1000);

  // ——— หมุน CCW 1 รอบ ———
  Serial.println("CCW 1 rev");
  stepper.moveTo(-STEPS_PER_REV);     // ตั้งเป้าก้าวถอยหลัง
  stepper.runToPosition();            // บล็อกจนถึงตำแหน่ง
  delay(1000);
}