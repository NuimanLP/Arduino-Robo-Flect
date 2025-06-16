// โค้ดนี้ใช้สำหรับตรวจสอบอุปกรณ์ที่เชื่อมต่อผ่าน USB Host Shield
#include <SPI.h>           // ไลบรารี SPI (จำเป็นสำหรับ USB Host Shield)
#include <Wire.h>          // ไลบรารี I2C (ใช้กับ LCD ถ้าใช้ต่อร่วมกัน)
#include <Usb.h>           // ไลบรารีหลักของ USB Host Shield Library 2.0
#include <usbhub.h>        // ไลบรารีสำหรับ USB Hub (ถ้าไม่ได้ต่อ Hub ก็สร้างว่างไว้แล้วไม่เรียกใช้ก็ได้)
#include <hiduniversal.h>  // ไลบรารีควบคุมอุปกรณ์ HID บน USB

// ====== สร้างอ็อบเจ็กต์สำหรับ USB Host Shield ======
USB      Usb;             // สร้างอ็อบเจ็กต์หลักของ USB Host
USBHub   Hub(&Usb);       // ถ้ามีการต่อผ่าน USB Hub ให้สร้างอ็อบเจ็กต์นี้ (ถ้าไม่มี HUB ก็ไม่ต้องใช้ก็ได้ แต่สร้างทิ้งไว้ก็ไม่เป็นไร)
HIDUniversal Hid(&Usb);   // สร้างอ็อบเจ็กต์สำหรับอุปกรณ์ชนิด HID (คีย์บอร์ด / คีย์แพด USB)

void setup() {
  Serial.begin(115200);        // เริ่ม Serial Monitor ที่ความเร็ว 115200 bps
  while (!Serial);             // รอจนกว่า Serial Monitor จะพร้อมใช้งาน

  // ====== เริ่มต้น USB Host Shield ======
  if (Usb.Init() == -1) {
    // ถ้าไม่สามารถเริ่ม USB Host ได้ ให้ส่งข้อความแจ้ง แล้วหยุดอยู่ตรงนี้
    Serial.println("USB Host Shield Init Error");
    while (1);
  }
  Serial.println("HIDList Example Initialized");
}

void loop() {
  Usb.Task();  // เรียกทุกครั้งใน loop เพื่อให้ USB Host Shield ทำงานตรวจจับอุปกรณ์

  // ====== ตรวจสอบว่า HID ถูก enumerate เรียบร้อยหรือยัง ======
  if (Hid.isReady()) {
    // ถ้า HID พร้อมแล้ว ให้ดึง address ของอุปกรณ์มา
    uint8_t addr = Hid.GetAddress();

    // สร้างตัวแปรสำหรับเก็บ Device Descriptor (ขนาด 18 ไบต์)
    USB_DEVICE_DESCRIPTOR desc;

    // เรียกอ่าน Device Descriptor เต็มรูปแบบ (sizeof(desc)=18) จากอุปกรณ์ address นั้น
    if (Usb.getDevDescr(addr, 0, sizeof(desc), (uint8_t*)&desc) == 0) {
      // ถ้าอ่านสำเร็จ (ฟังก์ชันคืนค่า 0) ให้พิมพ์ VID/PID ออกมา
      Serial.print("Device Found! VID = 0x");
      if (desc.idVendor < 0x10) Serial.print("0");  // เติม 0 นำหน้าให้ครบ 2 หลัก
      Serial.print(desc.idVendor, HEX);
      Serial.print(", PID = 0x");
      if (desc.idProduct < 0x10) Serial.print("0");
      Serial.println(desc.idProduct, HEX);
      delay(1000); // หน่วงสักครู่ไม่ให้พิมพ์ซ้ำเร็วเกินไป
    }
  }
}