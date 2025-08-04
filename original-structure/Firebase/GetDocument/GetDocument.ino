/**
 * Complete Firestore Arduino Sketch
 * - Connects to Wi-Fi
 * - Authenticates with Firebase (Email/Password)
 * - Reads the document at path "account/ID" from Cloud Firestore
 * 
 * Dependencies:
 *   • Firebase_ESP_Client (https://github.com/mobizt/Firebase-ESP-Client)
 *   • addons/TokenHelper.h (comes with the above library)
 */

#include <Arduino.h>
#include <WiFi.h>

#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>  // for tokenStatusCallback()

// 1. Your Wi-Fi credentials
#define WIFI_SSID       "Boat"
#define WIFI_PASS       "klahan2546*"

// 2. Your Firebase project credentials
#define API_KEY             "AIzaSyAoW2WhkaqjdY8QJsOasKAiGpm1Tnpzaf4"
#define FIREBASE_PROJECT_ID "test-robo-flect"

// 3. Email/Password user you registered in Firebase Authentication
#define USER_EMAIL      "chwathik@gmail.com"
#define USER_PASSWORD   "123456790"

// ————————————————————————————————
// Global Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void setup() {
  Serial.begin(9600);
  delay(1000);

  // Connect to Wi-Fi
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(300);
  }
  Serial.println();
  Serial.print("✓ WiFi connected, IP: ");
  Serial.println(WiFi.localIP());

  // Configure Firebase
  config.api_key = API_KEY;
  auth.user.email    = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;  // optional

  // Initialize Firebase library
  Firebase.begin(&config, &auth);
}

void loop() {
  // Ensure we run the read only once after auth is ready
  static bool hasRead = false;
  if (!hasRead && Firebase.ready()) {
    hasRead = true;

    // Path in Firestore: collection "account", document "ID"
    const char* documentPath = "account/ID";

    Serial.printf("Reading document \"%s\" … ", documentPath);
    if (Firebase.Firestore.getDocument(
          &fbdo,
          "test-robo-flect",  // your project ID
          "",                   // databaseId, leave empty for (default)
          documentPath,
          ""                    // fieldMask, leave empty for all fields
        )) {
      Serial.println("OK");

      // Raw JSON payload
      String payload = fbdo.payload();
      Serial.println(payload);

      // — Optional: parse specific fields from JSON
      /*
      FirebaseJson json = fbdo.jsonObject();
      FirebaseJsonData data;

      // Read integer field "level"
      json.get(data, "fields/level/integerValue");
      int level = data.intValue;

      // Read string field "name"
      json.get(data, "fields/name/stringValue");
      String name = data.stringValue;

      Serial.printf("Parsed → level: %d, name: %s\n", level, name.c_str());
      */
    }
    else {
      Serial.print("Error: ");
      Serial.println(fbdo.errorReason());
    }
  }

  // Nothing else to do in loop
  delay(15000);
}