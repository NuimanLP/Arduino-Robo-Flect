/*
 * Robo-Flect V1.1 - ESP32 Communication Module
 * Professional Sectioned Version for Optimal Performance
 * 
 * System Architecture:
 * - WiFi connectivity and Firebase integration
 * - Serial communication with Arduino Mega 2560
 * - Data synchronization and cloud storage
 * - Offline mode support with SD card backup
 * 
 * Hardware Components (ESP32):
 * - ESP32 Development Board
 * - SD Card Module (optional for offline backup)
 * - Status LED indicators
 */

// =====================================
// SECTION 1: INCLUDES & DEFINITIONS
// =====================================

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <time.h>
#include <HTTPClient.h>

// Provide the token generation process info
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions
#include "addons/RTDBHelper.h"

// Pin Definitions
#define LED_STATUS_PIN    2
#define LED_WIFI_PIN      4
#define LED_FIREBASE_PIN  5
#define SD_CS_PIN         15

// Firebase Configuration (Replace with your credentials)
#define WIFI_SSID         "YOUR_WIFI_SSID"
#define WIFI_PASSWORD     "YOUR_WIFI_PASSWORD"
#define API_KEY           "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL      "YOUR_FIREBASE_DATABASE_URL"
#define USER_EMAIL        "YOUR_FIREBASE_USER_EMAIL"
#define USER_PASSWORD     "YOUR_FIREBASE_USER_PASSWORD"

// System Constants
const unsigned long WIFI_RETRY_INTERVAL = 30000;  // 30 seconds
const unsigned long FIREBASE_RETRY_INTERVAL = 60000;  // 1 minute
const unsigned long SYNC_INTERVAL = 300000;  // 5 minutes
const unsigned long HEARTBEAT_INTERVAL = 10000;  // 10 seconds

// Message Protocol
const char MSG_START = '<';
const char MSG_END = '>';

// =====================================
// SECTION 2: SYSTEM STATE DEFINITIONS
// =====================================

enum ConnectionState {
  STATE_DISCONNECTED,
  STATE_CONNECTING_WIFI,
  STATE_CONNECTED_WIFI,
  STATE_CONNECTING_FIREBASE,
  STATE_CONNECTED_FIREBASE,
  STATE_ERROR
};

struct SystemStatus {
  bool wifiConnected;
  bool firebaseConnected;
  bool sdCardReady;
  bool offlineMode;
  int signalStrength;
  unsigned long lastSyncTime;
  unsigned long dataQueueSize;
};

struct UserData {
  String uid;
  String nickname;
  int level;
  int progress;
  unsigned long lastLogin;
  bool profileLoaded;
};

// =====================================
// SECTION 3: GLOBAL OBJECTS & VARIABLES
// =====================================

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// System variables
ConnectionState currentState = STATE_DISCONNECTED;
SystemStatus systemStatus = {false, false, false, false, 0, 0, 0};
UserData currentUser = {"", "", 1, 0, 0, false};

// Timing variables
unsigned long lastWiFiRetry = 0;
unsigned long lastFirebaseRetry = 0;
unsigned long lastSyncTime = 0;
unsigned long lastHeartbeat = 0;
unsigned long stateStartTime = 0;

// Data queues for offline mode
QueueHandle_t dataQueue;
QueueHandle_t userQueue;

// Communication buffers
String receivedMessage = "";
bool messageStarted = false;

// =====================================
// SECTION 4: SETUP & INITIALIZATION
// =====================================

void setup() {
  Serial.begin(9600);
  
  // Initialize pins
  pinMode(LED_STATUS_PIN, OUTPUT);
  pinMode(LED_WIFI_PIN, OUTPUT);
  pinMode(LED_FIREBASE_PIN, OUTPUT);
  
  // Initialize status LEDs
  setStatusLED(LOW);
  setWiFiLED(LOW);
  setFirebaseLED(LOW);
  
  // Initialize SD card
  initializeSDCard();
  
  // Create data queues
  dataQueue = xQueueCreate(50, sizeof(String));
  userQueue = xQueueCreate(10, sizeof(String));
  
  // Initialize WiFi and Firebase
  initializeWiFi();
  initializeFirebase();
  
  // Set time server
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  
  Serial.println(F("ESP32 Communication Module Started"));
  sendToMega("{\"cmd\":\"wifiStatus\",\"connected\":false}");
}

void initializeSDCard() {
  if (SD.begin(SD_CS_PIN)) {
    systemStatus.sdCardReady = true;
    Serial.println(F("SD Card initialized"));
    
    // Create directory structure if not exists
    if (!SD.exists("/users")) {
      SD.mkdir("/users");
    }
    if (!SD.exists("/data")) {
      SD.mkdir("/data");
    }
    if (!SD.exists("/logs")) {
      SD.mkdir("/logs");
    }
  } else {
    systemStatus.sdCardReady = false;
    Serial.println(F("SD Card initialization failed"));
  }
}

void initializeWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  currentState = STATE_CONNECTING_WIFI;
  stateStartTime = millis();
  
  Serial.print(F("Connecting to WiFi"));
}

void initializeFirebase() {
  // Assign the API key
  config.api_key = API_KEY;
  
  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  
  // Assign the RTDB URL
  config.database_url = DATABASE_URL;
  
  // Assign the callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback;
  
  // Initialize the library with the Firebase config and auth
  Firebase.begin(&config, &auth);
  
  // Enable automatic token refresh
  Firebase.reconnectWiFi(true);
}

// =====================================
// SECTION 5: MAIN LOOP & STATE MACHINE
// =====================================

void loop() {
  unsigned long currentTime = millis();
  
  // Handle WiFi and Firebase state machine
  handleConnectionStates(currentTime);
  
  // Process messages from Mega
  processMegaMessages();
  
  // Handle periodic tasks
  handlePeriodicTasks(currentTime);
  
  // Update status LEDs
  updateStatusLEDs();
  
  // Small delay for stability
  delay(10);
}

void handleConnectionStates(unsigned long currentTime) {
  switch (currentState) {
    case STATE_DISCONNECTED:
      handleDisconnectedState(currentTime);
      break;
      
    case STATE_CONNECTING_WIFI:
      handleConnectingWiFiState(currentTime);
      break;
      
    case STATE_CONNECTED_WIFI:
      handleConnectedWiFiState(currentTime);
      break;
      
    case STATE_CONNECTING_FIREBASE:
      handleConnectingFirebaseState(currentTime);
      break;
      
    case STATE_CONNECTED_FIREBASE:
      handleConnectedFirebaseState(currentTime);
      break;
      
    case STATE_ERROR:
      handleErrorState(currentTime);
      break;
  }
}

void handleDisconnectedState(unsigned long currentTime) {
  if (currentTime - lastWiFiRetry > WIFI_RETRY_INTERVAL) {
    Serial.println(F("Retrying WiFi connection..."));
    initializeWiFi();
    lastWiFiRetry = currentTime;
  }
}

void handleConnectingWiFiState(unsigned long currentTime) {
  if (WiFi.status() == WL_CONNECTED) {
    systemStatus.wifiConnected = true;
    systemStatus.signalStrength = WiFi.RSSI();
    currentState = STATE_CONNECTED_WIFI;
    stateStartTime = currentTime;
    
    Serial.println();
    Serial.println(F("WiFi connected!"));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
    
    sendToMega("{\"cmd\":\"wifiStatus\",\"connected\":true}");
    
  } else if (currentTime - stateStartTime > 30000) {
    // Timeout after 30 seconds
    currentState = STATE_ERROR;
    Serial.println(F("WiFi connection timeout"));
  }
}

void handleConnectedWiFiState(unsigned long currentTime) {
  if (WiFi.status() != WL_CONNECTED) {
    systemStatus.wifiConnected = false;
    systemStatus.firebaseConnected = false;
    currentState = STATE_DISCONNECTED;
    sendToMega("{\"cmd\":\"wifiStatus\",\"connected\":false}");
    return;
  }
  
  // Try to connect to Firebase
  currentState = STATE_CONNECTING_FIREBASE;
  stateStartTime = currentTime;
}

void handleConnectingFirebaseState(unsigned long currentTime) {
  if (Firebase.ready()) {
    systemStatus.firebaseConnected = true;
    currentState = STATE_CONNECTED_FIREBASE;
    
    Serial.println(F("Firebase connected!"));
    
    // Sync offline data if available
    syncOfflineData();
    
  } else if (currentTime - stateStartTime > 30000) {
    // Timeout after 30 seconds, continue with offline mode
    systemStatus.offlineMode = true;
    currentState = STATE_CONNECTED_WIFI;
    Serial.println(F("Firebase connection timeout, entering offline mode"));
  }
}

void handleConnectedFirebaseState(unsigned long currentTime) {
  if (WiFi.status() != WL_CONNECTED) {
    systemStatus.wifiConnected = false;
    systemStatus.firebaseConnected = false;
    currentState = STATE_DISCONNECTED;
    sendToMega("{\"cmd\":\"wifiStatus\",\"connected\":false}");
    return;
  }
  
  if (!Firebase.ready()) {
    systemStatus.firebaseConnected = false;
    currentState = STATE_CONNECTING_FIREBASE;
    stateStartTime = currentTime;
    return;
  }
  
  // Process queued data
  processDataQueue();
}

void handleErrorState(unsigned long currentTime) {
  if (currentTime - stateStartTime > 60000) {
    // Reset after 1 minute
    currentState = STATE_DISCONNECTED;
    Serial.println(F("Recovering from error state"));
  }
}

// =====================================
// SECTION 6: MESSAGE PROCESSING
// =====================================

void processMegaMessages() {
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == MSG_START) {
      receivedMessage = "";
      messageStarted = true;
    } else if (c == MSG_END && messageStarted) {
      processMegaCommand(receivedMessage);
      receivedMessage = "";
      messageStarted = false;
    } else if (messageStarted) {
      receivedMessage += c;
    }
  }
}

void processMegaCommand(String message) {
  Serial.print(F("Received from Mega: "));
  Serial.println(message);
  
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print(F("JSON parse error: "));
    Serial.println(error.c_str());
    return;
  }
  
  String cmd = doc["cmd"];
  
  if (cmd == "ping") {
    sendToMega("{\"cmd\":\"pong\"}");
    
  } else if (cmd == "getUserProfile") {
    handleGetUserProfile(doc["uid"]);
    
  } else if (cmd == "saveTrainingData") {
    handleSaveTrainingData(doc);
    
  } else if (cmd == "saveCalibration") {
    handleSaveCalibration(doc);
    
  } else if (cmd == "sensorData") {
    handleSensorData(doc);
    
  } else {
    Serial.println(F("Unknown command from Mega"));
  }
}

void sendToMega(String message) {
  Serial.print(MSG_START);
  Serial.print(message);
  Serial.print(MSG_END);
}

// =====================================
// SECTION 7: USER PROFILE MANAGEMENT
// =====================================

void handleGetUserProfile(String uid) {
  Serial.print(F("Getting user profile for UID: "));
  Serial.println(uid);
  
  if (systemStatus.firebaseConnected) {
    getUserProfileFromFirebase(uid);
  } else {
    getUserProfileFromSD(uid);
  }
}

void getUserProfileFromFirebase(String uid) {
  String path = "/users/" + uid + "/profile";
  
  if (Firebase.RTDB.get(&fbdo, path.c_str())) {
    if (fbdo.dataType() == "json") {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, fbdo.jsonString());
      
      DynamicJsonDocument response(512);
      response["cmd"] = "userProfile";
      response["found"] = true;
      response["uid"] = uid;
      response["nickname"] = doc["nickname"];
      response["level"] = doc["level"];
      response["progress"] = doc["progress"];
      
      String responseStr;
      serializeJson(response, responseStr);
      sendToMega(responseStr);
      
      // Cache to SD card
      saveUserProfileToSD(uid, doc);
      
    } else {
      sendUserNotFound(uid);
    }
  } else {
    Serial.print(F("Firebase error: "));
    Serial.println(fbdo.errorReason());
    
    // Fallback to SD card
    getUserProfileFromSD(uid);
  }
}

void getUserProfileFromSD(String uid) {
  String filename = "/users/" + uid + ".json";
  
  if (systemStatus.sdCardReady && SD.exists(filename)) {
    File file = SD.open(filename, FILE_READ);
    if (file) {
      String content = file.readString();
      file.close();
      
      DynamicJsonDocument doc(512);
      deserializeJson(doc, content);
      
      DynamicJsonDocument response(512);
      response["cmd"] = "userProfile";
      response["found"] = true;
      response["uid"] = uid;
      response["nickname"] = doc["nickname"];
      response["level"] = doc["level"];
      response["progress"] = doc["progress"];
      
      String responseStr;
      serializeJson(response, responseStr);
      sendToMega(responseStr);
      
      Serial.println(F("User profile loaded from SD card"));
      return;
    }
  }
  
  sendUserNotFound(uid);
}

void sendUserNotFound(String uid) {
  DynamicJsonDocument response(256);
  response["cmd"] = "userProfile";
  response["found"] = false;
  response["uid"] = uid;
  
  String responseStr;
  serializeJson(response, responseStr);
  sendToMega(responseStr);
  
  Serial.println(F("User not found"));
}

void saveUserProfileToSD(String uid, DynamicJsonDocument& profile) {
  if (!systemStatus.sdCardReady) return;
  
  String filename = "/users/" + uid + ".json";
  File file = SD.open(filename, FILE_WRITE);
  if (file) {
    serializeJson(profile, file);
    file.close();
    Serial.println(F("User profile cached to SD"));
  }
}

// =====================================
// SECTION 8: DATA MANAGEMENT
// =====================================

void handleSaveTrainingData(DynamicJsonDocument& doc) {
  String uid = doc["uid"];
  
  if (systemStatus.firebaseConnected) {
    saveTrainingDataToFirebase(doc);
  } else {
    saveTrainingDataToSD(doc);
  }
  
  // Confirm save to Mega
  DynamicJsonDocument response(256);
  response["cmd"] = "trainingData";
  response["saved"] = true;
  response["timestamp"] = doc["timestamp"];
  
  String responseStr;
  serializeJson(response, responseStr);
  sendToMega(responseStr);
}

void saveTrainingDataToFirebase(DynamicJsonDocument& data) {
  String uid = data["uid"];
  unsigned long timestamp = data["timestamp"];
  String path = "/training_data/" + uid + "/" + String(timestamp);
  
  String jsonString;
  serializeJson(data, jsonString);
  
  if (Firebase.RTDB.setJSON(&fbdo, path.c_str(), &data)) {
    Serial.println(F("Training data saved to Firebase"));
    
    // Update user statistics
    updateUserStatistics(uid, data);
    
  } else {
    Serial.print(F("Firebase save error: "));
    Serial.println(fbdo.errorReason());
    
    // Queue for later sync
    String dataStr;
    serializeJson(data, dataStr);
    xQueueSend(dataQueue, &dataStr, 0);
  }
}

void saveTrainingDataToSD(DynamicJsonDocument& data) {
  if (!systemStatus.sdCardReady) return;
  
  String uid = data["uid"];
  String filename = "/data/" + uid + "_training.csv";
  
  File file = SD.open(filename, FILE_APPEND);
  if (file) {
    // Write CSV format
    file.print(data["timestamp"].as<String>());
    file.print(",");
    file.print(data["targetDistance"].as<String>());
    file.print(",");
    file.print(data["actualDistance"].as<String>());
    file.print(",");
    file.print(data["attempts"].as<String>());
    file.print(",");
    file.println(data["sessionType"].as<String>());
    file.close();
    
    Serial.println(F("Training data saved to SD"));
  }
}

void updateUserStatistics(String uid, DynamicJsonDocument& data) {
  String path = "/users/" + uid + "/statistics";
  
  if (Firebase.RTDB.get(&fbdo, path.c_str())) {
    DynamicJsonDocument stats(512);
    
    if (fbdo.dataType() == "json") {
      deserializeJson(stats, fbdo.jsonString());
    } else {
      // Initialize new statistics
      stats["totalSessions"] = 0;
      stats["totalAttempts"] = 0;
      stats["averageAccuracy"] = 0;
      stats["lastActivity"] = 0;
    }
    
    // Update statistics
    stats["totalSessions"] = stats["totalSessions"].as<int>() + 1;
    stats["totalAttempts"] = stats["totalAttempts"].as<int>() + data["attempts"].as<int>();
    stats["lastActivity"] = data["timestamp"];
    
    // Save updated statistics
    Firebase.RTDB.setJSON(&fbdo, path.c_str(), &stats);
  }
}

void handleSaveCalibration(DynamicJsonDocument& doc) {
  if (systemStatus.firebaseConnected) {
    String path = "/calibration/" + String(doc["timestamp"].as<unsigned long>());
    Firebase.RTDB.setJSON(&fbdo, path.c_str(), &doc);
  }
  
  // Save to SD card
  if (systemStatus.sdCardReady) {
    File file = SD.open("/calibration.json", FILE_WRITE);
    if (file) {
      serializeJson(doc, file);
      file.close();
    }
  }
  
  Serial.println(F("Calibration data saved"));
}

void handleSensorData(DynamicJsonDocument& doc) {
  // Process real-time sensor data if needed
  // This could be used for live monitoring or analysis
}

// =====================================
// SECTION 9: OFFLINE DATA SYNC
// =====================================

void syncOfflineData() {
  if (!systemStatus.firebaseConnected) return;
  
  Serial.println(F("Syncing offline data..."));
  
  // Sync queued data
  processDataQueue();
  
  // Sync SD card data
  syncSDCardData();
  
  systemStatus.lastSyncTime = millis();
}

void processDataQueue() {
  String dataStr;
  
  while (xQueueReceive(dataQueue, &dataStr, 0) == pdTRUE) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, dataStr);
    
    if (doc["cmd"] == "saveTrainingData") {
      saveTrainingDataToFirebase(doc);
    }
    
    systemStatus.dataQueueSize--;
  }
}

void syncSDCardData() {
  if (!systemStatus.sdCardReady) return;
  
  // Sync training data files
  File dir = SD.open("/data");
  if (dir) {
    while (true) {
      File entry = dir.openNextFile();
      if (!entry) break;
      
      if (String(entry.name()).endsWith(".csv")) {
        syncCSVFile(entry);
      }
      entry.close();
    }
    dir.close();
  }
}

void syncCSVFile(File& file) {
  // Read and parse CSV data, then upload to Firebase
  // This is a simplified implementation
  Serial.print(F("Syncing file: "));
  Serial.println(file.name());
  
  // Implementation would parse CSV and upload each record
  // For now, just mark as processed
}

// =====================================
// SECTION 10: PERIODIC TASKS
// =====================================

void handlePeriodicTasks(unsigned long currentTime) {
  // Heartbeat with Mega
  if (currentTime - lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeat = currentTime;
  }
  
  // Periodic sync
  if (currentTime - lastSyncTime > SYNC_INTERVAL) {
    if (systemStatus.firebaseConnected) {
      syncOfflineData();
    }
    lastSyncTime = currentTime;
  }
  
  // Update signal strength
  if (systemStatus.wifiConnected) {
    systemStatus.signalStrength = WiFi.RSSI();
  }
}

void sendHeartbeat() {
  DynamicJsonDocument doc(256);
  doc["cmd"] = "heartbeat";
  doc["timestamp"] = millis();
  doc["wifiConnected"] = systemStatus.wifiConnected;
  doc["firebaseConnected"] = systemStatus.firebaseConnected;
  doc["signalStrength"] = systemStatus.signalStrength;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["uptime"] = millis();
  
  String message;
  serializeJson(doc, message);
  sendToMega(message);
}

// =====================================
// SECTION 11: STATUS & UTILITIES
// =====================================

void updateStatusLEDs() {
  // Status LED: Heartbeat
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  
  if (millis() - lastBlink > 1000) {
    ledState = !ledState;
    setStatusLED(ledState);
    lastBlink = millis();
  }
  
  // WiFi LED
  setWiFiLED(systemStatus.wifiConnected);
  
  // Firebase LED
  setFirebaseLED(systemStatus.firebaseConnected);
}

void setStatusLED(bool state) {
  digitalWrite(LED_STATUS_PIN, state ? HIGH : LOW);
}

void setWiFiLED(bool state) {
  digitalWrite(LED_WIFI_PIN, state ? HIGH : LOW);
}

void setFirebaseLED(bool state) {
  digitalWrite(LED_FIREBASE_PIN, state ? HIGH : LOW);
}

void tokenStatusCallback(TokenInfo info) {
  Serial.printf("Token info: type = %s, status = %s\n", 
                getTokenType(info).c_str(), 
                getTokenStatus(info).c_str());
}

// =====================================
// SECTION 12: ERROR HANDLING & LOGGING
// =====================================

void logError(String error, String context) {
  Serial.print(F("ERROR ["));
  Serial.print(context);
  Serial.print(F("]: "));
  Serial.println(error);
  
  // Save to SD card if available
  if (systemStatus.sdCardReady) {
    File logFile = SD.open("/logs/error.log", FILE_APPEND);
    if (logFile) {
      logFile.print(millis());
      logFile.print(" [");
      logFile.print(context);
      logFile.print("]: ");
      logFile.println(error);
      logFile.close();
    }
  }
  
  // Send to Mega if needed
  DynamicJsonDocument doc(256);
  doc["cmd"] = "error";
  doc["message"] = error;
  doc["context"] = context;
  doc["timestamp"] = millis();
  
  String message;
  serializeJson(doc, message);
  sendToMega(message);
}

void logSystemStatus() {
  Serial.println(F("=== ESP32 System Status ==="));
  Serial.print(F("WiFi: "));
  Serial.println(systemStatus.wifiConnected ? "Connected" : "Disconnected");
  Serial.print(F("Firebase: "));
  Serial.println(systemStatus.firebaseConnected ? "Connected" : "Disconnected");
  Serial.print(F("SD Card: "));
  Serial.println(systemStatus.sdCardReady ? "Ready" : "Not Ready");
  Serial.print(F("Signal Strength: "));
  Serial.println(systemStatus.signalStrength);
  Serial.print(F("Free Heap: "));
  Serial.println(ESP.getFreeHeap());
  Serial.print(F("Uptime: "));
  Serial.println(millis());
  Serial.println(F("========================"));
}

// =====================================
// SECTION 13: CONFIGURATION MANAGEMENT
// =====================================

void loadConfiguration() {
  if (!systemStatus.sdCardReady) return;
  
  if (SD.exists("/config.json")) {
    File configFile = SD.open("/config.json", FILE_READ);
    if (configFile) {
      DynamicJsonDocument config(512);
      deserializeJson(config, configFile.readString());
      configFile.close();
      
      // Apply configuration settings
      // This could include WiFi credentials, Firebase settings, etc.
      Serial.println(F("Configuration loaded from SD card"));
    }
  }
}

void saveConfiguration() {
  if (!systemStatus.sdCardReady) return;
  
  DynamicJsonDocument config(512);
  config["wifiSSID"] = WIFI_SSID;
  config["lastSync"] = systemStatus.lastSyncTime;
  config["totalUptime"] = millis();
  
  File configFile = SD.open("/config.json", FILE_WRITE);
  if (configFile) {
    serializeJson(config, configFile);
    configFile.close();
    Serial.println(F("Configuration saved to SD card"));
  }
}

// =====================================
// END OF ESP32 CODE
// =====================================