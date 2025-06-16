# Robo-Flect

**IMPORTANT: This project is protected by patent. All rights reserved.**

## Project Overview

Robo-Flect is an educational Arduino-based system for practicing reflexology with interactive ultrasonic distance sensing. It combines hardware sensing, motor movement, and data logging to create an interactive learning environment.

## System Architecture

The project is built around two main controllers that communicate with each other:

1. **Arduino Mega 2560** - The main controller that handles:
   - User interface via LCD display and keypad
   - Stepper motor control
   - VL53L0X distance sensing
   - RFID card reading
   - LED and buzzer feedback

2. **ESP32** - The communication module that provides:
   - WiFi connectivity
   - Firebase integration for cloud storage
   - User profile management
   - Offline data backup via SD card

## Key Components

### Hardware

- Arduino Mega 2560
- ESP32 development board
- VL53L0X time-of-flight distance sensor
- 20x4 I2C LCD display
- 4x4 matrix keypad
- Stepper motor with driver
- RFID-RC522 module
- RGB LED indicators
- Buzzer for audio feedback
- SD card module (optional, for ESP32)

### Software Libraries

- AccelStepper - For smooth stepper motor control
- LiquidCrystal_I2C - For LCD display
- VL53L0X - For the distance sensor
- MFRC522 - For RFID reader
- ArduinoJson - For data processing and communication
- Firebase_ESP_Client - For cloud connectivity
- WiFi - For ESP32 networking

## Getting Started

### Prerequisites

- Arduino IDE (version 1.8.13 or newer)
- Required libraries installed via Library Manager
- Hardware components assembled according to pin definitions

### Setup Instructions

1. **Hardware Assembly**
   - Connect all components according to the pin definitions in the main code files
   - For the Mega, refer to pin definitions in `V1.1-Mega.ino`
   - For the ESP32, refer to pin definitions in `V1.1-ESP32.ino`

2. **Firebase Setup** (for cloud features)
   - Create a Firebase account and project
   - Set up Realtime Database
   - Update the ESP32 code with your Firebase credentials

3. **WiFi Configuration**
   - Update the ESP32 code with your WiFi credentials

4. **Upload the Code**
   - Upload `V1.1-Mega.ino` to the Arduino Mega
   - Upload `V1.1-ESP32.ino` to the ESP32

## Project Structure

The repository is organized into several directories, each containing specific functionality:

### Main System Code

- `/Combine/NEW/` - The latest working version of the project
  - `V1.1-Mega` - Main Arduino Mega controller code
  - `V1.1-ESP32` - ESP32 WiFi module code
  - `V1.1-Section` - Sectioned code for better understanding

### Component Test Code

- `/Distance-sensor-53LOX/` - Test code for the VL53L0X distance sensor
- `/RFID_esp32/` - Test code for RFID module with ESP32
- `/STEP_motor/` - Test code for stepper motor control
- `/i2c_SCAN/` - Utility to scan I2C devices on the bus
- `/UART_/` - Serial communication tests between Mega and ESP32

### Cloud Connectivity

- `/Firebase/` - Firebase connectivity examples
- `/MQTT/` - MQTT protocol examples for IoT connectivity

### Legacy Code

- `/Old_edition/` - Previous versions of project components
- `/Combine/OLD/` - Earlier versions of the combined system
- `/Readable_Combine/` - Commented versions for educational purposes

## How It Works

### Basic Operation

1. **Boot Sequence**
   - System initializes hardware components
   - Performs self-check of sensors and motor
   - Establishes communication between Mega and ESP32

2. **User Authentication**
   - User scans RFID card
   - ESP32 retrieves user profile from Firebase or local storage
   - LCD displays welcome message and user info

3. **Training Mode**
   - User selects training mode and distance setting
   - Stepper motor moves target to the selected distance
   - System measures and logs user's response time
   - Results are stored locally and synced to Firebase when online

4. **Testing Mode**
   - Similar to training mode but with scoring
   - System randomly selects distances and scores user accuracy

### State Machine Design

The system uses a state machine architecture for reliable operation. Main states include:

- Boot
- Self-Check
- Idle (awaiting RFID)
- Main Menu
- Mode Selection
- Training
- Testing
- Settings
- Calibration

## Troubleshooting

### Common Issues

1. **WiFi Connection Problems**
   - Check WiFi credentials in ESP32 code
   - Verify router settings and signal strength
   - System will operate in offline mode if WiFi is unavailable

2. **Sensor Errors**
   - Ensure proper wiring of the VL53L0X sensor
   - Check I2C connections and address conflicts
   - Run the I2C scanner to verify device detection

3. **Motor Issues**
   - Verify stepper driver connections
   - Check power supply adequate for motor requirements
   - Test with basic stepper code to isolate issues

4. **Communication Problems**
   - Ensure TX/RX connections between Mega and ESP32 are correct
   - Check baud rate settings (9600 baud)
   - Use UART test code to verify communication

## Legal Notice

This project, Robo-Flect, is protected by patent. Unauthorized reproduction, distribution, or commercial use of this system or its design is prohibited and may result in legal action. This codebase is provided for educational and reference purposes only.

---

# Robo-Flect (ภาษาไทย)

**สำคัญ: โครงการนี้ได้รับการคุ้มครองโดยสิทธิบัตร สงวนลิขสิทธิ์ทั้งหมด**

## ภาพรวมโครงการ

Robo-Flect เป็นระบบการเรียนรู้บนฐาน Arduino สำหรับฝึกการสะท้อนกลับโดยใช้เซ็นเซอร์วัดระยะทางอัลตร้าโซนิคแบบมีปฏิสัมพันธ์ โครงการนี้รวมการตรวจจับด้วยฮาร์ดแวร์ การเคลื่อนไหวของมอเตอร์ และการบันทึกข้อมูลเพื่อสร้างสภาพแวดล้อมการเรียนรู้แบบโต้ตอบ

## สถาปัตยกรรมระบบ

โครงการนี้สร้างขึ้นรอบตัวควบคุมหลักสองตัวที่สื่อสารกัน:

1. **Arduino Mega 2560** - ตัวควบคุมหลักที่จัดการ:
   - อินเตอร์เฟซผู้ใช้ผ่านจอ LCD และแป้นกด
   - ควบคุมสเต็ปเปอร์มอเตอร์
   - การตรวจจับระยะทางด้วย VL53L0X
   - การอ่านบัตร RFID
   - ไฟ LED และเสียงเตือน

2. **ESP32** - โมดูลการสื่อสารที่ให้บริการ:
   - การเชื่อมต่อ WiFi
   - การเชื่อมต่อกับ Firebase สำหรับจัดเก็บข้อมูลบนคลาวด์
   - การจัดการโปรไฟล์ผู้ใช้
   - การสำรองข้อมูลแบบออฟไลน์ผ่านการ์ด SD

## องค์ประกอบหลัก

### ฮาร์ดแวร์

- Arduino Mega 2560
- บอร์ดพัฒนา ESP32
- เซ็นเซอร์วัดระยะทาง VL53L0X แบบ time-of-flight
- จอ LCD 20x4 แบบ I2C
- แป้นกดเมทริกซ์ 4x4
- สเต็ปเปอร์มอเตอร์พร้อมไดรเวอร์
- โมดูล RFID-RC522
- ไฟ LED RGB
- บัซเซอร์สำหรับเสียงตอบสนอง
- โมดูลการ์ด SD (ตัวเลือกเพิ่มเติม สำหรับ ESP32)

### ไลบรารีซอฟต์แวร์

- AccelStepper - สำหรับควบคุมสเต็ปเปอร์มอเตอร์แบบราบรื่น
- LiquidCrystal_I2C - สำหรับจอแสดงผล LCD
- VL53L0X - สำหรับเซ็นเซอร์วัดระยะทาง
- MFRC522 - สำหรับเครื่องอ่าน RFID
- ArduinoJson - สำหรับประมวลผลข้อมูลและการสื่อสาร
- Firebase_ESP_Client - สำหรับเชื่อมต่อคลาวด์
- WiFi - สำหรับเครือข่าย ESP32

## เริ่มต้นใช้งาน

### สิ่งที่ต้องมีก่อน

- Arduino IDE (เวอร์ชัน 1.8.13 หรือใหม่กว่า)
- ติดตั้งไลบรารีที่จำเป็นผ่าน Library Manager
- ประกอบอุปกรณ์ฮาร์ดแวร์ตามการกำหนดขาใช้งาน

### ขั้นตอนการติดตั้ง

1. **การประกอบฮาร์ดแวร์**
   - เชื่อมต่ออุปกรณ์ทั้งหมดตามการกำหนดขาในไฟล์โค้ดหลัก
   - สำหรับ Mega ดูการกำหนดขาใน `V1.1-Mega.ino`
   - สำหรับ ESP32 ดูการกำหนดขาใน `V1.1-ESP32.ino`

2. **การตั้งค่า Firebase** (สำหรับคุณสมบัติคลาวด์)
   - สร้างบัญชีและโปรเจคใน Firebase
   - ตั้งค่า Realtime Database
   - อัปเดตโค้ด ESP32 ด้วยข้อมูลประจำตัว Firebase ของคุณ

3. **การกำหนดค่า WiFi**
   - อัปเดตโค้ด ESP32 ด้วยข้อมูลประจำตัว WiFi ของคุณ

4. **อัปโหลดโค้ด**
   - อัปโหลด `V1.1-Mega.ino` ไปยัง Arduino Mega
   - อัปโหลด `V1.1-ESP32.ino` ไปยัง ESP32

## โครงสร้างโปรเจค

ที่เก็บโค้ดถูกจัดเรียงเป็นหลายไดเรกทอรี แต่ละอันมีฟังก์ชันการทำงานเฉพาะ:

### โค้ดระบบหลัก

- `/Combine/NEW/` - เวอร์ชันล่าสุดที่ทำงานได้ของโปรเจค
  - `V1.1-Mega` - โค้ดควบคุมหลักสำหรับ Arduino Mega
  - `V1.1-ESP32` - โค้ดโมดูล WiFi สำหรับ ESP32
  - `V1.1-Section` - โค้ดที่แบ่งเป็นส่วนเพื่อทำความเข้าใจได้ง่ายขึ้น

### โค้ดทดสอบองค์ประกอบ

- `/Distance-sensor-53LOX/` - โค้ดทดสอบเซ็นเซอร์วัดระยะทาง VL53L0X
- `/RFID_esp32/` - โค้ดทดสอบโมดูล RFID กับ ESP32
- `/STEP_motor/` - โค้ดทดสอบการควบคุมสเต็ปเปอร์มอเตอร์
- `/i2c_SCAN/` - ยูทิลิตี้สำหรับสแกนอุปกรณ์ I2C บนบัส
- `/UART_/` - การทดสอบการสื่อสารแบบอนุกรมระหว่าง Mega และ ESP32

### การเชื่อมต่อคลาวด์

- `/Firebase/` - ตัวอย่างการเชื่อมต่อ Firebase
- `/MQTT/` - ตัวอย่างโปรโตคอล MQTT สำหรับการเชื่อมต่อ IoT

### โค้ดเก่า

- `/Old_edition/` - เวอร์ชันก่อนหน้าของส่วนประกอบโปรเจค
- `/Combine/OLD/` - เวอร์ชันก่อนหน้าของระบบรวม
- `/Readable_Combine/` - เวอร์ชันที่มีคำอธิบายสำหรับวัตถุประสงค์ทางการศึกษา

## วิธีการทำงาน

### การทำงานพื้นฐาน

1. **ลำดับการบูต**
   - ระบบเริ่มต้นส่วนประกอบฮาร์ดแวร์
   - ทำการตรวจสอบตัวเองของเซ็นเซอร์และมอเตอร์
   - สร้างการสื่อสารระหว่าง Mega และ ESP32

2. **การตรวจสอบตัวตนผู้ใช้**
   - ผู้ใช้สแกนบัตร RFID
   - ESP32 ดึงโปรไฟล์ผู้ใช้จาก Firebase หรือที่เก็บข้อมูลท้องถิ่น
   - จอ LCD แสดงข้อความต้อนรับและข้อมูลผู้ใช้

3. **โหมดฝึกอบรม**
   - ผู้ใช้เลือกโหมดฝึกอบรมและการตั้งค่าระยะทาง
   - สเต็ปเปอร์มอเตอร์เคลื่อนเป้าหมายไปยังระยะทางที่เลือก
   - ระบบวัดและบันทึกเวลาตอบสนองของผู้ใช้
   - ผลลัพธ์ถูกเก็บไว้ในท้องถิ่นและซิงค์ไปยัง Firebase เมื่อออนไลน์

4. **โหมดทดสอบ**
   - คล้ายกับโหมดฝึกอบรมแต่มีการให้คะแนน
   - ระบบสุ่มเลือกระยะทางและให้คะแนนความแม่นยำของผู้ใช้

### การออกแบบสถานะของระบบ

ระบบใช้สถาปัตยกรรมแบบ state machine เพื่อการทำงานที่เชื่อถือได้ สถานะหลักรวมถึง:

- Boot (เริ่มต้น)
- Self-Check (ตรวจสอบตัวเอง)
- Idle (รอ RFID)
- Main Menu (เมนูหลัก)
- Mode Selection (เลือกโหมด)
- Training (ฝึกอบรม)
- Testing (ทดสอบ)
- Settings (การตั้งค่า)
- Calibration (การปรับเทียบ)

## การแก้ไขปัญหา

### ปัญหาทั่วไป

1. **ปัญหาการเชื่อมต่อ WiFi**
   - ตรวจสอบข้อมูลประจำตัว WiFi ในโค้ด ESP32
   - ตรวจสอบการตั้งค่าเราเตอร์และความแรงของสัญญาณ
   - ระบบจะทำงานในโหมดออฟไลน์หากไม่มี WiFi

2. **ข้อผิดพลาดของเซ็นเซอร์**
   - ตรวจสอบการเดินสายของเซ็นเซอร์ VL53L0X ให้ถูกต้อง
   - ตรวจสอบการเชื่อมต่อ I2C และความขัดแย้งของที่อยู่
   - รันตัวสแกน I2C เพื่อตรวจสอบการตรวจจับอุปกรณ์

3. **ปัญหาเกี่ยวกับมอเตอร์**
   - ตรวจสอบการเชื่อมต่อไดรเวอร์สเต็ปเปอร์
   - ตรวจสอบแหล่งจ่ายไฟให้เพียงพอสำหรับความต้องการของมอเตอร์
   - ทดสอบด้วยโค้ดสเต็ปเปอร์พื้นฐานเพื่อแยกปัญหา

4. **ปัญหาการสื่อสาร**
   - ตรวจสอบการเชื่อมต่อ TX/RX ระหว่าง Mega และ ESP32 ให้ถูกต้อง
   - ตรวจสอบการตั้งค่าอัตราบอด (9600 บอด)
   - ใช้โค้ดทดสอบ UART เพื่อตรวจสอบการสื่อสาร

## ประกาศทางกฎหมาย

โครงการนี้ Robo-Flect ได้รับการคุ้มครองโดยสิทธิบัตร การทำซ้ำโดยไม่ได้รับอนุญาต การแจกจ่าย หรือการใช้เชิงพาณิชย์ของระบบนี้หรือการออกแบบของมันเป็นสิ่งต้องห้ามและอาจนำไปสู่การดำเนินการทางกฎหมาย ฐานโค้ดนี้มีไว้เพื่อวัตถุประสงค์ทางการศึกษาและการอ้างอิงเท่านั้น

