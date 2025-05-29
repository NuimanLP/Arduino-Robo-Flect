#include <AccelStepper.h>

#define STEP_PIN    7
#define DIR_PIN     6
#define SW_FWD_PIN 22
#define SW_BWD_PIN 23

// ใส่ค่าที่คำนวณจากสูตรข้างบน
const float MAX_SPEED    = 1333.0;   // ตัวอย่าง ~200 RPM
const float ACCELERATION = 2000.0;   // ลองเอาไว้ก่อน ปรับลดลงถ้าเวียนหัว

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

void setup() {
  Serial.begin(9600);
  pinMode(SW_FWD_PIN, INPUT_PULLUP);
  pinMode(SW_BWD_PIN, INPUT_PULLUP);

  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCELERATION);
  Serial.println("Setup done");
}

void loop() {
  bool fwd = digitalRead(SW_FWD_PIN)==LOW;
  bool bwd = digitalRead(SW_BWD_PIN)==LOW;

  if (fwd && !bwd) {
    stepper.setSpeed( MAX_SPEED );
  }
  else if (bwd && !fwd) {
    stepper.setSpeed( -MAX_SPEED );
  }
  else {
    stepper.setSpeed( 0 );
  }

  stepper.runSpeed();
}
