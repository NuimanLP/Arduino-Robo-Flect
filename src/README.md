# Source Code Directory

This directory contains all the source code for the Robo-Flect project.

## Structure

### 📁 main/
Production-ready code for the Robo-Flect system
- **arduino-mega/** - Main controller code for Arduino Mega 2560
- **esp32/** - WiFi module code for ESP32
- **shared/** - Shared libraries and headers used by both controllers

### 📁 components/
Individual component test and development code organized by function:

#### 📁 sensors/
- **distance-vl53l0x/** - VL53L0X time-of-flight distance sensor code
- **rfid-rc522/** - RFID card reader module code

#### 📁 actuators/
- **stepper-motor/** - Stepper motor control and testing
- **lcd-display/** - LCD display graphics and text tests

#### 📁 communication/
- **uart/** - Serial communication between Mega and ESP32
- **i2c-scanner/** - I2C device detection utility
- **usb-interface/** - USB interface scanning tools

#### 📁 connectivity/
- **firebase/** - Firebase cloud database integration
- **mqtt/** - MQTT protocol implementation for IoT

## Usage

1. For production deployment, use code from `main/`
2. For testing individual components, refer to `components/`
3. Always check pin definitions match your hardware setup
4. Ensure required libraries are installed before compiling
