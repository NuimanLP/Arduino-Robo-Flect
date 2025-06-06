#include <Wire.h>                    // ไลบรารีสำหรับสื่อสาร I2C
#include <LiquidCrystal_I2C.h>       // ไลบรารีสำหรับจอ LCD 20x4 ผ่าน I2C
#include <SPI.h>                     // ไลบรารี SPI (ใช้กับ USB Host Shield)
#include <hidboot.h>                 // HID keyboard driver ของ USB Host Shield
#include <usbhub.h>                  // ไลบรารี USB hub ของ USB Host Shield

// ====== กำหนดจอ LCD 20×4 (I2C) ======
// ที่อยู่ I2C ของโมดูล LCD 20×4 บอร์ดส่วนใหญ่จะเป็น 0x27 หรือ 0x3F (ตรวจสอบจริงก่อนใช้งาน)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ====== สร้างอ็อบเจ็กต์ USB Host Shield ======
USB     Usb;              // สร้างอ็อบเจ็กต์ USB Host
USBHub  Hub(&Usb);        // สร้างอ็อบเจ็กต์ USB Hub (ถ้ามี HUB ใช้ร่วมได้ ถ้าไม่มี ก็ไม่ต้องใส่ไว้ก็ได้)
HIDBoot<USB_HID_PROTOCOL_KEYBOARD> Keyboard(&Usb);
// ^ กำหนดว่าอุปกรณ์ที่ต่อเข้าจะเป็น HID keyboard (Keypad USB ก็ถือเป็นคีย์บอร์ดชนิดหนึ่ง)


// ====== คลาสสำหรับแปลงและแสดงผลเมื่อกดปุ่ม ======
class KbdRptParser : public KeyboardReportParser {
  public:
    void OnKeyDown(uint8_t mod, uint8_t key);
    void OnKeyUp(uint8_t mod, uint8_t key);
};

// สร้างอ็อบเจ็กต์ตัวจัดการเหตุการณ์ key report
KbdRptParser Prs;


/*
  ฟังก์ชัน keycode2ASCII:
  - แปลง HID keycode ให้เป็นอักขระ ASCII
  - รองรับเฉพาะตัวเลข 0–9, เครื่องหมายบนคีย์แพด (+, -, .), Enter, Backspace
  - ถ้า keycode ไม่อยู่ในตารางที่กำหนด คืนค่าเป็น 0
*/
static char keycode2ASCII(uint8_t keycode, bool shift) {
  // ตาราง HID keycode สำหรับตัวเลข 0–9 (นับจาก ‘1’ ไปถึง ‘0’)
  // ค่าอ้างอิงจาก HID Usage Tables
  static const uint8_t hidNumbers[] = {
    0x27,  // '0'
    0x1E,  // '1'
    0x1F,  // '2'
    0x20,  // '3'
    0x21,  // '4'
    0x22,  // '5'
    0x23,  // '6'
    0x24,  // '7'
    0x25,  // '8'
    0x26   // '9'
  };

  // ตรวจสอบตัวเลข 0–9
  for (int i = 1; i <= 9; i++) {
    if (keycode == hidNumbers[i]) {
      // ถ้ามี Shift ให้แปลงเป็นตัวอักษรที่ shift แล้ว (อันนี้ไม่จำเป็นสำหรับ Numeric Keypad)
      return '0' + i;
    }
  }
  // ตรวจว่า keycode เป็น '0' หรือไม่ (hidNumbers[0] คือ 0x27)
  if (keycode == hidNumbers[0]) {
    return '0';
  }

  // กรณี Enter (HID keycode 0x28) → ส่ง '\r'
  if (keycode == 0x28) return '\r';

  // กรณี Backspace (HID keycode 0x2A) → ส่ง '\b'
  if (keycode == 0x2A) return '\b';

  // กรณีเครื่องหมายบน Keypad (HID keycode 0x2D = '‐' หรือ minus)
  if (keycode == 0x2D) return '-';

  // กรณี Keypad ‘.’ (HID keycode 0x55)
  if (keycode == 0x55) return '.';

  // กรณี Keypad ฝั่งขวา (Enter อีกตำแหน่ง หน้าตารางตัวเลข = 0x58)
  if (keycode == 0x58) return '\r';

  // กรณี Keypad '+' (HID keycode 0x57)
  if (keycode == 0x57) return '+';

  // ไม่เจอในตาราง → คืนค่า 0
  return 0;
}


// ฟังก์ชันเรียกเมื่อมีการกดปุ่ม (Key Down)
void KbdRptParser::OnKeyDown(uint8_t mod, uint8_t key) {
  // ตรวจว่ากด Shift หรือไม่ (ใน Numeric Keypad ส่วนใหญ่ไม่กด Shift)
  bool isShift = (mod & 0x02) || (mod & 0x20);
  char c = keycode2ASCII(key, isShift);  // แปลง keycode → ASCII

  if (c) {
    static int col = 0;  // เก็บตำแหน่งคอลัมน์ปัจจุบันบน LCD row=1

    // ถ้าเป็น Backspace (‘\b’) ให้ลบตัวอักษรเดิมบน LCD
    if (c == '\b') {
      if (col > 0) {
        col--;
        lcd.setCursor(col, 1);
        lcd.print(' ');       // พิมพ์ช่องว่างทับตำแหน่งเดิม
        lcd.setCursor(col, 1); 
      }
      return;
    }

    // ถ้าเป็น Enter (‘\r’) ให้เคลียร์บรรทัด row=1 แล้วเคอร์เซอร์กลับไป col=0
    if (c == '\r') {
      lcd.setCursor(0, 1);
      lcd.print("                    "); // ล้าง row=1 (20 ตัวอักษร)
      lcd.setCursor(0, 1);
      col = 0;
      return;
    }

    // ถ้าเป็นตัวเลขหรือเครื่องหมาย ให้พิมพ์ลง LCD บรรทัดที่ 1
    lcd.setCursor(col, 1); 
    lcd.print(c);
    col++;
    if (col >= 20) { 
      // ถ้าวางถึงขอบขวาสุด (คอลัมน์ 19) ให้เลื่อนไปบรรทัดใหม่หรือวนกลับ 0
      col = 0;
    }
  }
}

// ฟังก์ชันเรียกเมื่อปล่อยปุ่ม (Key Up) ในตัวอย่างนี้ไม่ต้องใช้
void KbdRptParser::OnKeyUp(uint8_t mod, uint8_t key) {
  // ไม่ได้ใช้งานในตัวอย่างนี้
}


void setup() {
  // เริ่ม Serial Monitor สำหรับดีบัก (ถ้าไม่ต้องการดีบัก สามารถตัดส่วน Serial ออกได้)
  Serial.begin(9600);
  while (!Serial);

  // ====== เริ่มต้นจอ LCD ======
  lcd.begin();       // เริ่มต้นจอ LCD 20×4
  lcd.backlight();   // เปิดไฟแบ็คไลท์
  lcd.clear();       // ล้างหน้าจอ

  // พิมพ์ข้อความคงที่บนบรรทัด 0
  lcd.setCursor(0, 0);  
  lcd.print("USB Keypad Demo");   // พิมพ์หัวข้อโปรแกรม
  lcd.setCursor(0, 1);
  lcd.print("Press Keypad:");     // พิมพ์บรรทัดแนะนำบน row=1

  // ====== เริ่มต้น USB Host Shield ======
  if (Usb.Init() == -1) {
    // ถ้าไม่สามารถเริ่ม USB Host ได้ ให้ส่งข้อความแจ้งผ่าน Serial แล้วหยุดตรงนี้
    Serial.println("USB Host Shield Init failed");
    while (1);
  }
  delay(200);  // รอให้ USB Host เชื่อมต่ออุปกรณ์ได้เสถียรประมาณ 200 ms

  // ตั้ง callback ให้ KeyboardReportParser รับข้อมูลเมื่อกด/ปล่อยปุ่ม
  Keyboard.SetReportParser(0, &Prs);

  Serial.println("USB Host Shield Initialized");
}


void loop() {
  // เรียก Usb.Task() ในทุก ๆ รอบ เพื่อให้ USB Host Shield ทำงานตรวจจับ HID event
  Usb.Task();
}