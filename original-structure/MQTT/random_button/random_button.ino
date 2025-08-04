#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

//────────────────────────
//   WiFi Credentials
//────────────────────────
const char* ssid     = "NuiGates Time Stone";
const char* password = "Nile1234";

//────────────────────────
//   MQTT (HiveMQ Cloud)  
//────────────────────────
const char* mqtt_server   = "c9c7d0abee0f468baa2813d506a916be.s1.eu.hivemq.cloud";
const int   mqtt_port     = 8883;
const char* mqtt_user     = "hivemq.webclient.1749090827904";
const char* mqtt_password = "p9q3:CWI,@B5.4udwUAe";
const char* mqtt_topic    = "esp8266/timer";

//────────────────────────
//   Pin Definitions (ESP32 GPIOs)
//────────────────────────
// (Adjust these to match your wiring!)
//────────────────────────
const int BUTTON_PIN = 0;   // e.g. GPIO0 (BOOT button on some boards), or change to your actual input pin
const int HIGH_PIN   = 2;   // e.g. GPIO2 (on‐board LED on many ESP32 devkits), or change to your chosen output pin

//────────────────────────
//   Globals
//────────────────────────
WiFiClientSecure  espClient;
PubSubClient      client(espClient);

unsigned long pressStartTime = 0;
bool         lastButtonState = HIGH;
bool         buttonPressed   = false;

//────────────────────────
//   Forward Declarations
//────────────────────────
void setup_wifi();
void reconnect();

void setup() {
  Serial.begin(115200);

  //── Configure pins ─────────────────────────────────
  // Use INPUT_PULLUP so the pin isn't floating when the button is not pressed
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(HIGH_PIN,   OUTPUT);
  digitalWrite(HIGH_PIN, LOW); // start LOW

  //── Connect to Wi-Fi ─────────────────────────────────
  setup_wifi();

  //── Skip TLS certificate validation (for testing only!) ─
  // If you want proper cert validation, load root CA instead of setInsecure()
  espClient.setInsecure();

  //── Configure MQTT server and callback ─────────────────
  client.setServer(mqtt_server, mqtt_port);

  // No callback() needed here unless you want to SUBSCRIBE to topics
  // client.setCallback(yourCallbackFunction);
}

void setup_wifi() {
  delay(10);
  Serial.printf("Connecting to WiFi SSID \"%s\" …\n", ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until MQTT is connected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection… ");
    // Use a unique client ID (change “ESP32Client” if you run multiple devices)
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("connected!");
      // If you want to SUBSCRIBE, do it here:
      // client.subscribe("some/topic");
    } else {
      Serial.printf("failed, rc=%d; retry in 5 s\n", client.state());
      delay(5000);
    }
  }
}

// A simple helper to simulate a “button press”.
// (You can remove this if you wire up a real button and use digitalRead(BUTTON_PIN).)
bool randomBoolean() {
  return random(2) == 1; // 50/50 true or false
}

void loop() {
  //── Reconnect MQTT if needed ─────────────────────────
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  //────────────────────────────────────────────────────────
  // Replace the two calls to randomBoolean() below with:
  //    digitalRead(BUTTON_PIN)
  // once you have a real button wired to BUTTON_PIN.
  //
  // The code below just simulates “press” and “release” events randomly.
  //────────────────────────────────────────────────────────

  bool simulatedState = randomBoolean() ? HIGH : LOW;

  // Detect “press” (HIGH → LOW) transition:
  if (simulatedState == LOW && lastButtonState == HIGH) {
    pressStartTime = millis();
    buttonPressed = true;
    Serial.println("🔴 Button pressed (simulated)");
  }

  // Detect “release” (LOW → HIGH) transition:
  if (simulatedState == HIGH && lastButtonState == LOW && buttonPressed) {
    unsigned long pressDuration = millis() - pressStartTime;
    buttonPressed = false;
    Serial.printf("⚪ Button released (simulated), duration = %lu ms\n", pressDuration);

    // Publish to MQTT:
    char payload[50];
    snprintf(payload, sizeof(payload), "Button held for %lu ms", pressDuration);
    client.publish(mqtt_topic, payload);
    Serial.printf("→ Published \"%s\" to topic \"%s\"\n", payload, mqtt_topic);

    // Toggle the HIGH_PIN each time the button is released:
    digitalWrite(HIGH_PIN, !digitalRead(HIGH_PIN));
  }

  lastButtonState = simulatedState;
  delay(1000);

  //──────────────────────────────────────────────
  // If you have a *real* button wired to BUTTON_PIN:
  //
  // Remove the lines above (randomBoolean simulation) and replace with:
  //
  //   bool currentState = digitalRead(BUTTON_PIN);
  //
  //   if (currentState == LOW && lastButtonState == HIGH) {
  //     // … button just went from HIGH to LOW (pressed) …
  //   }
  //   if (currentState == HIGH && lastButtonState == LOW && buttonPressed) {
  //     // … button just went from LOW to HIGH (released) …
  //   }
  //
  //   lastButtonState = currentState;
  //   delay(50); // small debounce
  //
  //──────────────────────────────────────────────
}