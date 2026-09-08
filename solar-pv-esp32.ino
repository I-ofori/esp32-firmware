#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"

#if USE_WIFI
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#endif

#if USE_DHT22
#include <DHT.h>
#endif
#if USE_TSL2591
#include <Adafruit_TSL2591.h>
#endif
#if USE_MAX31865
#include <Adafruit_MAX31865.h>
#endif
#if USE_GSM
#include <HardwareSerial.h>
#endif

#if USE_DHT22
DHT dht(PIN_DHT_DATA, DHT22);
#endif

#if USE_TSL2591
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);
#endif

#if USE_MAX31865
Adafruit_MAX31865 rtd = Adafruit_MAX31865(PIN_RTD_CS, PIN_RTD_SDI, PIN_RTD_SDO, PIN_RTD_SCK);
#endif

#if USE_WIFI
WiFiClientSecure client;
#endif

#if USE_GSM
HardwareSerial sim800l(GSM_UART_NUM);
bool gsmReady = false;
#endif

float gVoltage = 0.0;
float gCurrent = 0.0;
float gTemp = 0.0;
float gHumidity = 0.0;
float gIrradiance = 0.0;

void initSensors() {
#if USE_DHT22
  dht.begin();
#endif
#if USE_TSL2591
  if (tsl.begin()) {
    tsl.setGain(TSL2591_GAIN_MED);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
    Serial.println("TSL2591 initialized");
  } else {
    Serial.println("TSL2591 not found - check I2C wiring");
  }
#endif
#if USE_MAX31865
  rtd.begin(MAX31865_3WIRE);
#endif
  analogSetPinAttenuation(PIN_ADC_CURRENT, ADC_11db);
  analogSetPinAttenuation(PIN_ADC_VOLTAGE, ADC_11db);
}

float readAnalogVolts(int pin) {
  return analogReadMilliVolts(pin) / 1000.0f;
}

void readSensors() {
  float vSens = readAnalogVolts(PIN_ADC_VOLTAGE);
  gVoltage = vSens / VOLTAGE_SENSOR_RATIO;
  if (gVoltage < 0) gVoltage = 0;

  float cSens = readAnalogVolts(PIN_ADC_CURRENT);
  gCurrent = (cSens - ACS712_MIDPOINT_VOLTS) / ACS712_SENSITIVITY_V_PER_A;
  if (gCurrent < 0) gCurrent = 0;

#if USE_DHT22
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) gTemp = t;
  if (!isnan(h)) gHumidity = h;
  if (isnan(t) || isnan(h)) Serial.println("DHT22 read failed");
#endif

#if USE_TSL2591
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t ir = lum >> 16;
  uint16_t full = lum & 0xFFFF;
  float lux = tsl.calculateLux(full, ir);
  if (lux > 0) gIrradiance = lux / LUX_PER_W_M2;
#endif

#if USE_MAX31865
  if (!rtd.readFault()) {
    float tRtd = rtd.temperature(RTD_RNOM_OHMS, RTD_RREF_OHMS);
    Serial.print("RTD temp: ");
    Serial.println(tRtd, 2);
  } else {
    Serial.println("MAX31865 fault");
  }
#endif

  Serial.printf(
    "Sample  V=%.2fV  I=%.2fA  T=%.1fC  H=%.1f%%  Irr=%.1f W/m2\n",
    gVoltage, gCurrent, gTemp, gHumidity, gIrradiance);
}

String buildPayload() {
  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["voltage"] = gVoltage;
  doc["current"] = gCurrent;
  doc["temperature"] = gTemp;
  doc["irradiance"] = gIrradiance;
  doc["humidity"] = gHumidity;
  String payload;
  serializeJson(doc, payload);
  return payload;
}

#if USE_GSM
// ---- SIM800L helpers --------------------------------------------------------
String gsmReply = "";

bool gsmCmd(const char* cmd, const char* expect, unsigned long timeoutMs = 5000) {
  gsmReply = "";
  Serial.println();
  Serial.print("AT> ");
  Serial.println(cmd);
  sim800l.println(cmd);
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (sim800l.available()) {
      char c = sim800l.read();
      gsmReply += c;
      if (gsmReply.indexOf("ERROR") >= 0) {
        Serial.print("GSM reply: ");
        Serial.println(gsmReply);
        return false;
      }
      if (gsmReply.indexOf(expect) >= 0) {
        unsigned long wait = millis();
        while (millis() - wait < 300 && sim800l.available()) {
          gsmReply += (char)sim800l.read();
        }
        Serial.print("GSM reply: ");
        Serial.println(gsmReply);
        return true;
      }
    }
  }
  Serial.print("GSM timeout for: ");
  Serial.println(cmd);
  return false;
}

bool initGsm() {
  sim800l.begin(GSM_BAUD, SERIAL_8N1, PIN_GSM_RX, PIN_GSM_TX);
  delay(1000);

  Serial.println("Initializing SIM800L...");
  bool alive = false;
  for (int i = 0; i < 3; i++) {
    if (gsmCmd("AT", "OK", 3000)) { alive = true; break; }
    delay(1500);
  }
  if (!alive) {
    Serial.println("SIM800L not responding");
    return false;
  }

  gsmCmd("ATE0", "OK");              // echo off
  gsmCmd("AT+CFUN=1", "OK", 8000);   // full functionality

  char cmd[64];
  snprintf(cmd, sizeof cmd, "AT+CSTT=\"%s\",\"%s\",\"%s\"", GSM_APN, GSM_USER, GSM_PASS);
  gsmCmd(cmd, "OK", 8000);

  gsmCmd("AT+CIICR", "OK", 30000);   // activate GPRS
  if (!gsmCmd("AT+CIFSR", ".", 10000)) {
    Serial.println("GPRS attach failed - check APN and SIM credit");
    return false;
  }
  Serial.println("SIM800L GPRS ready");
  return true;
}

bool postViaGsm(const String& payload) {
  if (!gsmReady) {
    gsmReady = initGsm();
    if (!gsmReady) return false;
  }

  String command = "AT+HTTPPARA=\"URL\",\"" + String(GSM_HTTP_ENDPOINT) + "\"";
  gsmCmd("AT+HTTPINIT", "OK", 8000);
  gsmCmd("AT+HTTPPARA=\"CID\",1", "OK");
  gsmCmd(command.c_str(), "OK");
  gsmCmd("AT+HTTPPARA=\"CONTENT\",\"application/json\"", "OK");

  String auth = "AT+HTTPPARA=\"USERDATA\",\"Authorization: Bearer " + String(API_DEVICE_KEY) + "\"";
  gsmCmd(auth.c_str(), "OK");

#ifdef RELAY_SECRET
  if (strlen(RELAY_SECRET) > 0) {
    String secret = "AT+HTTPPARA=\"USERDATA\",\"x-relay-secret: " + String(RELAY_SECRET) + "\"";
    gsmCmd(secret.c_str(), "OK");
  }
#endif

  String dataCmd = "AT+HTTPDATA=" + String(payload.length()) + ",30000";
  gsmCmd(dataCmd.c_str(), "DOWNLOAD", 15000);
  sim800l.print(payload);
  delay(500);

  gsmCmd("AT+HTTPACTION=1", "+HTTPACTION:", 90000);

  int httpCode = -1;
  int idx = gsmReply.indexOf("+HTTPACTION:");
  if (idx >= 0) {
    // +HTTPACTION: <method>,<httpcode>,<len>   e.g. "+HTTPACTION: 1,200,57"
    int p1 = gsmReply.indexOf(',', idx);
    int p2 = gsmReply.indexOf(',', p1 + 1);
    if (p1 >= 0 && p2 >= 0) {
      httpCode = gsmReply.substring(p1 + 1, p2).toInt();
    }
  }
  Serial.print("GSM POST -> HTTP ");
  Serial.println(httpCode);

  bool success = (httpCode == 200 || httpCode == 201);
  if (httpCode == 401) Serial.println("REJECTED 401: API_DEVICE_KEY mismatch");
  else if (httpCode == 400) Serial.println("REJECTED 400: payload/device problem");
  else if (httpCode == 409) Serial.println("REJECTED 409: duplicate device_id + recorded_at");
  else if (httpCode == 429) Serial.println("REJECTED 429: rate limited");
  else if (httpCode == 503) Serial.println("REJECTED 503: database unavailable");

  gsmCmd("AT+HTTPTERM", "OK", 5000);
  return success;
}
#endif

bool sendReading() {
  String payload = buildPayload();

#if USE_WIFI
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(client, API_ENDPOINT);
    http.setTimeout(15000);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + API_DEVICE_KEY);
    int code = http.POST(payload);
    String body = code > 0 ? http.getString() : "";
    http.end();
    Serial.print("WiFi POST -> HTTP ");
    Serial.println(code);
    if (code == 200 || code == 201) return true;
  }
#endif

#if USE_GSM
  return postViaGsm(payload);
#else
  (void)payload;
  Serial.println("No transport available (USE_WIFI and USE_GSM both off/disabled)");
  return false;
#endif
}

#if USE_WIFI
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("WiFi FAILED - will use GSM if enabled");
  }
}
#endif

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nSolar PV Logger booting (GSM mode)");

  initSensors();

#if USE_GSM
  gsmReady = initGsm();
#endif

#if USE_WIFI
  connectWiFi();
  client.setInsecure();
#endif
}

unsigned long lastPost = 0;

void loop() {
  if (millis() - lastPost >= POST_INTERVAL_SEC * 1000UL) {
    lastPost = millis();
    readSensors();
    sendReading();
  }
}