#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// --- WiFi credentials ---
const char* ssid        = "NuiGates Time Stone";
const char* password    = "Nile1234";

// --- HiveMQ Cloud settings ---
const char* mqtt_server = "c9c7d0abee0f468baa2813d506a916be.s1.eu.hivemq.cloud"; // ตัวอย่าง s2.eu.hivemq.cloud
const int   mqtt_port   = 8883;
const char* mqtt_user   = "hivemq.webclient.1748935600153";   // กำหนดจาก HiveMQ Cloud
const char* mqtt_pass   = "%gy3sJ6cdFTK:05>hD&X";   // กำหนดจาก HiveMQ Cloud
const char* clientID    = "ESP32_HiveMQ_TLS";

// สร้างตัวแปรสำหรับ WiFiClientSecure
WiFiClientSecure secureClient;
PubSubClient client(secureClient);

long lastMsg = 0;
const int ledPin    = 4;
const int buttonPin = 14;

// Thermistor settings (เหมือนตัวอย่างด้านบน)
const int thermistorPin       = 36;
const float referenceVoltage  = 3.3;
const float referenceResistor = 10000;
const float beta              = 3950;
const float nominalTemp       = 25;
const float nominalRes        = 10000;

void setup() {
  Serial.begin(115200);

  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  setup_wifi();

  // ตั้งค่า CA certificate ของ HiveMQ Cloud (Root CA ของ Let's Encrypt หรืออื่น ๆ)
  // ให้นำค่า CA Cert มาใส่ไว้ในรูปแบบ multiline string ตามตัวอย่าง  [oai_citation:30‡community.hivemq.com](https://community.hivemq.com/t/connecting-esp32-to-cloud-hivemq-using-root-certificate/1770?utm_source=chatgpt.com)
  // secureClient.setCACert(hivemq_ca_cert); 

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// ตัวอย่าง CA certificate (แค่ตัวอย่าง อย่าลืมหา CA จริงจาก HiveMQ Cloud ฯลฯ)
const char* hivemq_ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgISA0J3lWui5KpX2grasdfsdf...
-----END CERTIFICATE-----
)EOF";

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("], Message: ");
  String msgTemp;
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    msgTemp += (char)message[i];
  }
  Serial.println();

  if (String(topic) == "SF/LED") {
    if (msgTemp == "on") {
      digitalWrite(ledPin, HIGH);
      Serial.println("LED ON");
    } else if (msgTemp == "off") {
      digitalWrite(ledPin, LOW);
      Serial.println("LED OFF");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT TLS connection...");
    // ใช้พารามิเตอร์ 4 ตัว (ClientID, Username, Password, CleanSession = true)
    if (client.connect(clientID, mqtt_user, mqtt_pass)) {
      Serial.println("connected");
      client.subscribe("SF/LED");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

float thermistor() {
  int adcValue = analogRead(thermistorPin);
  float voltage = (adcValue * referenceVoltage) / 4095.0;
  float resistance = (voltage * referenceResistor) / (referenceVoltage - voltage);

  float tempK = 1.0 / (((log(resistance / nominalRes)) / beta) + (1.0 / (nominalTemp + 273.15)));
  float tempC = tempK - 273.15;

  Serial.print("Temp: ");
  Serial.print(tempC);
  Serial.println(" °C");
  delay(200);
  return tempC;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (digitalRead(buttonPin) == HIGH) {
    long now = millis();
    if (now - lastMsg > 5000) {
      lastMsg = now;
      char tempString[8];
      dtostrf(thermistor(), 1, 2, tempString);
      client.publish("SF/TEMP", tempString);
    }
  }
}