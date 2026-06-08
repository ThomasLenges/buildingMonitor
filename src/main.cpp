#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include "credentials.h"

// ── WiFi ──────────────────────────────────────────────────────────
// Credentials in credetials.h (WIFI_SSID and WIFI_PASS)

// ── HiveMQ ────────────────────────────────────────────────────────
// Credentials in credetials.h (MQTT_HOST, MQTT_USER, MQTT_PASS)
const int   MQTT_PORT = 8883;                                // TLS encryption port (server assumes anything on port 8883 is MQTT over TLS)

// ── MQTT Topics ───────────────────────────────────────────────────
const char* TOPIC_R4 = "esp32/r4/status";
const char* TOPIC_Q  = "esp32/q/status";
const char* TOPIC_R4_TEMP = "esp32/r4/temperature";
const char* TOPIC_R4_LED       = "esp32/r4/led";          
const char* TOPIC_R4_LED_STATE = "esp32/r4/led/state";    

// ── BLE ───────────────────────────────────────────────────────────
const char* SERVICE_UUID = "181A";
const char* CHAR_UUID_COUNTER = "2A56";
const char* CHAR_UUID_TEMP = "2A1F";
const char* CHAR_UUID_LED      = "2A57";
const char* NAME_R4      = "UNO-R4";
const char* NAME_Q       = "UNO-Q";

NimBLEClient* pClientR4 = nullptr;
NimBLEClient* pClientQ  = nullptr;
NimBLEAddress addressR4;
NimBLEAddress addressQ;
bool foundR4 = false;
bool foundQ  = false;

WiFiClientSecure espClient;
PubSubClient     mqttClient(espClient);

struct R4Chars {
    NimBLERemoteCharacteristic* counter = nullptr;
    NimBLERemoteCharacteristic* temp    = nullptr;
    NimBLERemoteCharacteristic* led     = nullptr;
  };

R4Chars r4Chars;

// ── LED ───────────────────────────────────────────────────────────
Adafruit_NeoPixel LED_RGB(1, 38, NEO_GRBW + NEO_KHZ800);

// ── Scan Callback ─────────────────────────────────────────────────
class ScanCallback : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* device) override {
    std::string name = device->getName();
    if (name == NAME_R4) { addressR4 = device->getAddress(); foundR4 = true; Serial.println("Found UNO-R4"); }
    if (name == NAME_Q)  { addressQ  = device->getAddress(); foundQ  = true; Serial.println("Found UNO-Q");  }
    if (foundR4 && foundQ) { Serial.println("Both found — stopping scan"); NimBLEDevice::getScan()->stop(); }
  }
};

// ── WiFi ──────────────────────────────────────────────────────────
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected ✓  IP: " + WiFi.localIP());
}

// ── MQTT ──────────────────────────────────────────────────────────
  // Forward declaration
void mqttCallback(const char* topic, byte* payload, unsigned int length);

void connectMQTT() {
  espClient.setInsecure();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  Serial.print("Connecting to HiveMQ");
  while (!mqttClient.connected()) {
    if (mqttClient.connect("ESP32-S3-Gateway", MQTT_USER, MQTT_PASS)) {
      Serial.println("\nHiveMQ connected ✓");
      mqttClient.subscribe(TOPIC_R4_LED);
    } else {
      Serial.print(".");
      delay(500);
    }
  }
}

void maintainMQTT() {
  if (!mqttClient.connected()) {
    Serial.println("MQTT lost — reconnecting...");
    connectMQTT();
  }
  mqttClient.loop();
}

void publishStatus(const char* topic, const char* status) {
  if (mqttClient.connected()) {
    mqttClient.publish(topic, status, true); // true for retained message (when a client subscribes, it will receive the last retained message immediately (dashboard has last informations and is not stuck in waiting for status change))
    Serial.printf("Published [%s]: %s\n", topic, status);
  }
}

// PubSubClient calls callback whenever a message arrives on any subscribed topic with message "payload" an array of bytes of length "length"
void mqttCallback(const char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) message += (char)payload[i];

  Serial.printf("MQTT received [%s]: %s\n", topic, message.c_str());

  if (String(topic) == TOPIC_R4_LED) {
    if (r4Chars.led && r4Chars.led->canWrite()) {
      r4Chars.led->writeValue(message.c_str());
      mqttClient.publish(TOPIC_R4_LED_STATE, message.c_str(), true);
      Serial.printf("LED set to: %s\n", message.c_str());
    }
  }
}

// ── BLE Connect ───────────────────────────────────────────────────
NimBLERemoteCharacteristic* connectToPeripheral(
    NimBLEClient*& pClient,
    const NimBLEAddress& address,
    const char* name,
    const char* mqttTopic,
    const char* charUUID) {        

  // Connect only if not already connected
  if (!pClient || !pClient->isConnected()) {
    pClient = NimBLEDevice::createClient();
    pClient->setConnectionParams(12, 12, 0, 300);

    if (!pClient->connect(address)) {
      Serial.printf("Connection failed: %s\n", name);
      NimBLEDevice::deleteClient(pClient);
      pClient = nullptr;
      return nullptr;
    }
    Serial.printf("Connected: %s\n", name);
    publishStatus(mqttTopic, "connected"); 
  }

  NimBLERemoteService* pService = pClient->getService(SERVICE_UUID);
  if (!pService) {
    pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    pClient = nullptr;
    publishStatus(mqttTopic, "disconnected service not found");
    return nullptr;
  }

  NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(charUUID);
  if (!pChar) {
    pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    pClient = nullptr;
    publishStatus(mqttTopic, "disconnected characteristic not found");
    return nullptr;
  }

  return pChar;
}

// ── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  connectWiFi();
  connectMQTT();

  NimBLEDevice::init("ESP32-Central");
  Serial.println("BLE Central ready");

  // LED
  LED_RGB.begin();
  LED_RGB.setBrightness(150);
  LED_RGB.setPixelColor(0, uint32_t(LED_RGB.Color(0, 0, 255))); // BLUE
  LED_RGB.show();
}

// ── Loop ──────────────────────────────────────────────────────────
void loop() {
  maintainMQTT();

  foundR4 = false;
  foundQ  = false;

  Serial.println("\nScanning...");
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(new ScanCallback(), false);
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(100);
  pScan->start(0);
  delay(20000);
  pScan->stop();
  pScan->clearResults();

  if (!foundR4 || !foundQ) { Serial.println("Needs to find all devices — retrying..."); return; }

  NimBLERemoteCharacteristic* pCharQ  = nullptr;

  r4Chars.counter = connectToPeripheral(pClientR4, addressR4, NAME_R4, TOPIC_R4, CHAR_UUID_COUNTER); delay(1000);
  r4Chars.temp    = connectToPeripheral(pClientR4, addressR4, NAME_R4, TOPIC_R4, CHAR_UUID_TEMP);    delay(1000);
  r4Chars.led     = connectToPeripheral(pClientR4, addressR4, NAME_R4, TOPIC_R4, CHAR_UUID_LED);     delay(1000);
  pCharQ  = connectToPeripheral(pClientQ,  addressQ,  NAME_Q,  TOPIC_Q,  CHAR_UUID_COUNTER);  delay(1000);

  // ── Read loop ──────────────────────────────────────────────────
  while (true) {
    maintainMQTT();   // keeps MQTT alive during BLE read loop

    bool anyDisconnected = false;

    if (pClientR4 && pClientR4->isConnected()) {
      if (r4Chars.counter && r4Chars.counter->canRead()) {
        Serial.printf("[R4] Counter: %d\n", r4Chars.counter->readValue<int>());
      }
      if (r4Chars.temp && r4Chars.temp->canRead()) {
        Serial.printf("[R4] Temp: %.2f\n", r4Chars.temp->readValue<float>());
        float t = r4Chars.temp->readValue<float>();  // float
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", t);        // float string "23.45"
        mqttClient.publish(TOPIC_R4_TEMP, buf, true); // publish string
      }
    } else if (pClientR4) {
      anyDisconnected = true;
      Serial.println("[R4] Disconnected");
      publishStatus(TOPIC_R4, "disconnected");   // publish on disconnect
      NimBLEDevice::deleteClient(pClientR4);
      pClientR4 = nullptr;
      Serial.println("[Q] Disconnecting due to R4 disconnect");
      publishStatus(TOPIC_Q, "disconnected");    // publish on disconnect
      NimBLEDevice::deleteClient(pClientQ);
      pClientQ = nullptr;
    }

    if (pClientQ && pClientQ->isConnected()) {
      if (pCharQ && pCharQ->canRead()) {
        Serial.printf("[Q]  Counter: %d\n", pCharQ->readValue<int>());
      }
    } else if (pClientQ) {
      anyDisconnected = true;
      Serial.println("[Q] Disconnected");
      publishStatus(TOPIC_Q, "disconnected");    // publish on disconnect
      NimBLEDevice::deleteClient(pClientQ);
      pClientQ = nullptr;
      Serial.println("[R4] Disconnecting due to Q disconnect");
      publishStatus(TOPIC_R4, "disconnected");   // publish on disconnect
      NimBLEDevice::deleteClient(pClientR4);
      pClientR4 = nullptr;
    }

    if (anyDisconnected) { Serial.println("A connection was lost — will rescan"); break; }

    delay(1000);
  }
}