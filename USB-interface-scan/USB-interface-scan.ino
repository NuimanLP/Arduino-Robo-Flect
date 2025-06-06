#include <SPI.h>
#include <Wire.h>
#include <usbhub.h>
#include <hid.h>
#include <hiduniversal.h>
#include <hidboot.h>
#include <Usb.h>

USB     Usb;
USBHub  Hub(&Usb);
HIDUniversal Hid(&Usb);

void setup() {
  Serial.begin(115200);
  while (!Serial);
  if (Usb.Init() == -1) {
    Serial.println("USB Host Shield Init Error");
    while (1); // หยุดโปรแกรม
  }
  Serial.println("HIDList Example Initialized");
}

void loop() {
  Usb.Task();
  
  USB_DEVICE_DESCRIPTOR desc;
  uint8_t buf[128];
  if (Hid.GetAddress() && Hid.Enumerated()) {
    Hid.GetDeviceDescriptor(&desc);
    Serial.print("Device Found! VID=0x");
    Serial.print(desc.idVendor, HEX);
    Serial.print(", PID=0x");
    Serial.println(desc.idProduct, HEX);
    delay(1000);
  }
}