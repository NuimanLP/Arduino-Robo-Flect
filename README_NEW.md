# 🤖 Robo-Flect

**IMPORTANT: This project is protected by patent. All rights reserved.**

## 📋 Table of Contents
- [Overview](#overview)
- [Project Structure](#project-structure)
- [Quick Start](#quick-start)
- [Hardware Requirements](#hardware-requirements)
- [Software Setup](#software-setup)
- [Documentation](#documentation)
- [Legal Notice](#legal-notice)

## 🎯 Overview

Robo-Flect is an educational Arduino-based system for practicing reflexology with interactive ultrasonic distance sensing. It combines hardware sensing, motor movement, and data logging to create an interactive learning environment.

### Key Features
- 🎮 Interactive training and testing modes
- 📊 Real-time distance measurement with VL53L0X sensor
- 💳 RFID-based user authentication
- ☁️ Cloud data storage via Firebase
- 📱 WiFi connectivity with ESP32
- 🖥️ LCD display interface with keypad control
- 🔄 Smooth stepper motor control
- 💾 Offline data backup capability

## 📁 Project Structure

```
Robo-Flect/
├── 📁 src/                    # Source code
│   ├── 📁 main/              # Production code
│   │   ├── arduino-mega/     # Main controller
│   │   ├── esp32/           # WiFi module
│   │   └── shared/          # Shared libraries
│   └── 📁 components/        # Component tests
│       ├── sensors/         # Sensor code
│       ├── actuators/       # Motor/display code
│       ├── communication/   # UART/I2C code
│       └── connectivity/    # Network code
├── 📁 legacy/                # Archived versions
├── 📁 docs/                  # Documentation
├── 📁 tests/                 # Test suites
├── 📁 examples/              # Example code
├── 📁 resources/             # Additional files
└── 📁 tools/                 # Utilities
```

## 🚀 Quick Start

### 1. Clone the repository
```bash
git clone https://github.com/yourusername/Robo-Flect.git
cd Robo-Flect
```

### 2. Hardware Setup
Connect components according to pin definitions in:
- `/src/main/arduino-mega/V1.1-Mega.ino`
- `/src/main/esp32/V1.1-ESP32.ino`

### 3. Install Required Libraries
Open Arduino IDE and install:
- AccelStepper
- LiquidCrystal_I2C
- VL53L0X
- MFRC522
- ArduinoJson
- Firebase_ESP_Client

### 4. Configure WiFi and Firebase
Edit `/src/main/esp32/V1.1-ESP32.ino`:
```cpp
#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define FIREBASE_HOST "your-project.firebaseio.com"
#define FIREBASE_AUTH "your_firebase_auth_token"
```

### 5. Upload Code
- Upload `/src/main/arduino-mega/V1.1-Mega.ino` to Arduino Mega
- Upload `/src/main/esp32/V1.1-ESP32.ino` to ESP32

## 🔧 Hardware Requirements

### Main Controllers
- Arduino Mega 2560
- ESP32 Development Board

### Sensors & Actuators
- VL53L0X Distance Sensor
- MFRC522 RFID Reader
- Stepper Motor with Driver
- 20x4 I2C LCD Display
- 4x4 Matrix Keypad

### Additional Components
- RGB LED
- Buzzer
- SD Card Module (optional)
- Power Supply (12V for motor)

## 💻 Software Setup

### Development Environment
- Arduino IDE 1.8.13 or newer
- ESP32 Board Package
- Required libraries (see Quick Start)

### Firebase Setup
1. Create Firebase project
2. Enable Realtime Database
3. Get authentication credentials
4. Update ESP32 code with credentials

## 📚 Documentation

- **User Guide**: `/docs/guides/user-manual.md`
- **Setup Guide**: `/docs/guides/setup-guide.md`
- **API Reference**: `/docs/api/`
- **Hardware Datasheets**: `/docs/datasheets/`
- **Testing Guide**: `/tests/Testing_Roadmap.md`

## 🧪 Testing Components

To test individual components:
```bash
# Test distance sensor
/src/components/sensors/distance-vl53l0x/

# Test RFID reader
/src/components/sensors/rfid-rc522/

# Test stepper motor
/src/components/actuators/stepper-motor/
```

## 🔍 Troubleshooting

### Common Issues
1. **WiFi Connection Failed**
   - Check credentials in ESP32 code
   - Verify router compatibility (2.4GHz)

2. **Sensor Not Detected**
   - Run I2C scanner: `/src/components/communication/i2c-scanner/`
   - Check wiring connections

3. **Motor Not Moving**
   - Verify power supply voltage
   - Check driver connections
   - Test with basic motor code

## 📜 Legal Notice

This project, Robo-Flect, is protected by patent. Unauthorized reproduction, distribution, or commercial use of this system or its design is prohibited and may result in legal action. This codebase is provided for educational and reference purposes only.

## 🤝 Contributing

This is a proprietary project. Contributions are currently not accepted. For questions or licensing inquiries, please contact the project maintainer.

## 📞 Support

For technical support or questions:
- Create an issue in the repository
- Contact: [your-email@example.com]

---

© 2024 Robo-Flect. All rights reserved. Patent pending.
