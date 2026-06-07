#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

// ── WiFi ──────────────────────────────────────────────────────────
const char* WIFI_SSID = "Zyxel_57B9";      
const char* WIFI_PASS = "4k8hkk7c7rc7t8e3"; 

// ── HiveMQ ────────────────────────────────────────────────────────
const char* MQTT_HOST = "a7248e8d9e4740ed8c35e534e8b7ed36.s1.eu.hivemq.cloud";
const int   MQTT_PORT = 8883;                                // TLS encryption port (server assumes anything on port 8883 is MQTT over TLS)
const char* MQTT_USER = "oAswYJC19j3KYdPsFRkk";                     
const char* MQTT_PASS = "9xzDXuAWF4aQzVVXG8FG";  

// ── MQTT Topics ───────────────────────────────────────────────────
const char* TOPIC_R4 = "esp32/r4/status";
const char* TOPIC_Q  = "esp32/q/status";

// ── BLE ───────────────────────────────────────────────────────────
const char* SERVICE_UUID = "181A";
const char* CHAR_UUID    = "2A56";
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
void connectMQTT() {
  espClient.setInsecure();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  Serial.print("Connecting to HiveMQ");
  while (!mqttClient.connected()) {
    if (mqttClient.connect("ESP32-S3-Gateway", MQTT_USER, MQTT_PASS)) {
      Serial.println("\nHiveMQ connected ✓");
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

// ── BLE Connect ───────────────────────────────────────────────────
NimBLERemoteCharacteristic* connectToPeripheral(
    NimBLEClient*& pClient,
    const NimBLEAddress& address,
    const char* name,
    const char* mqttTopic) {        

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

  NimBLERemoteService* pService = pClient->getService(SERVICE_UUID);
  if (!pService) {
    pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    pClient = nullptr;
    publishStatus(mqttTopic, "disconnected service not found");
    return nullptr;
  }

  NimBLERemoteCharacteristic* pChar = pService->getCharacteristic(CHAR_UUID);
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

  NimBLERemoteCharacteristic* pCharR4 = nullptr;
  NimBLERemoteCharacteristic* pCharQ  = nullptr;

  pCharR4 = connectToPeripheral(pClientR4, addressR4, NAME_R4, TOPIC_R4); delay(1000);
  pCharQ  = connectToPeripheral(pClientQ,  addressQ,  NAME_Q,  TOPIC_Q);  delay(1000);

  // ── Read loop ──────────────────────────────────────────────────
  while (true) {
    maintainMQTT();   // keeps MQTT alive during BLE read loop

    bool anyDisconnected = false;

    if (pClientR4 && pClientR4->isConnected()) {
      if (pCharR4 && pCharR4->canRead()) {
        Serial.printf("[R4] Counter: %d\n", pCharR4->readValue<int>());
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