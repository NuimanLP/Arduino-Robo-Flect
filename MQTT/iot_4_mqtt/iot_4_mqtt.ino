/*********
  :ref: https://randomnerdtutorials.com/esp32-mqtt-publish-subscribe-arduino-ide/
        https://docs.sunfounder.com/projects/kepler-kit/en/latest/iotproject/5.mqtt_pub.html
*********/

#include <WiFi.h>
#include <PubSubClient.h>

// Replace the next variables with your SSID/Password combination
const char* ssid                = "NuiGates Time Stone";
const char* password            = "Nile1234";

// MQTT Broker address (example):
const char* mqtt_server         = "broker.hivemq.com";
const char* unique_identifier   = "hivemq.webclient.1748937368399";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;

// LED and button pins
const int ledPin                = 4;
const int buttonPin             = 14;

// Thermistor (ADC) settings
const int thermistorPin         = 36;    // ADC pin
const float referenceVoltage    = 3.3;
const float referenceResistor   = 10000; // 10 kΩ
const float beta                = 3950;  // Beta value
const float nominalTemperature  = 25;    // °C at nominalResistance
const float nominalResistance   = 10000; // 10 kΩ at nominalTemperature

void setup() {
  Serial.begin(115200);

  // Initialize pins
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);

  // Connect to Wi-Fi and set up MQTT server
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Force Station mode (disable default AP)
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

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // If a message is received on "SF/LED", toggle LED accordingly
  if (String(topic) == "SF/LED") {
    Serial.print("Changing state to ");
    if (messageTemp == "on") {
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    } else if (messageTemp == "off") {
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }
}

void reconnect() {
  // Loop until reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(unique_identifier)) {
      Serial.println("connected");
      // Once connected, subscribe to the LED topic
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

  // Calculate Kelvin via Beta equation
  float tempK = 1.0 / (((log(resistance / nominalResistance)) / beta) + (1.0 / (nominalTemperature + 273.15)));
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

  // If the button is pressed, publish temperature to "SF/TEMP" every 5 seconds
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