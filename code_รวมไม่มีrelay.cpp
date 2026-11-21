#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> // ติดตั้งไลบรารี ArduinoJson (โดย Benoit Blanchon) เพิ่มใน Arduino IDE

// =====================================
// 1. LCD SETTINGS
// =====================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================
// 2. PH SENSOR SETTINGS
// =====================================

const int phPin = 34;

float calibration_value = 21.34;

unsigned long int avgval;
int buffer_arr[10];
int temp;

float volt = 0;
float ph_act = 0;

// =====================================
// 3. WIFI & SERVER SETTINGS
// =====================================

// ใส่ชื่อ Hotspot โทรศัพท์
const char* ssid = "YOUR_WIFI_SSID";

// ใส่รหัสผ่าน Hotspot
const char* password = "YOUR_WIFI_PASSWORD";

// Google Apps Script Base URL
const char* serverUrl =
"https://script.google.com/macros/s/AKfycbwAsg9cyE-qX98INyIKs9LH0bRYIt6oqNsW4obXDtApahIE3NOoLMFQ1qVXRpR4zaC0/exec";

// ตัวแปร Config (ค่าเริ่มต้นก่อนดึงจาก Server)
String sensorID = "esp32_ph_01";
String systemStatus = "on"; // "on" = ส่งข้อมูล, "off" = หยุดส่งข้อมูล

// =====================================
// 4. TIMER SETTINGS
// =====================================

unsigned long previousSend = 0;
const unsigned long sendInterval = 5000;     // ส่งค่า pH ทุก 5 วินาที

unsigned long previousConfig = 0;
const unsigned long configInterval = 30000;  // ดึง Config ใหม่ทุก 30 วินาที

// =====================================
// 5. WIFI FUNCTION
// =====================================

void connectWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println();
  Serial.print("Connecting to WiFi");

  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttempt < 15000) {

    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println();
    Serial.println("WiFi Connected!");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    delay(1000);

  } else {

    Serial.println();
    Serial.println("WiFi Connection Failed");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed");
    lcd.setCursor(0, 1);
    lcd.print("Check Hotspot");
    delay(1500);
  }
}

// =====================================
// 6. FETCH CONFIG FROM GOOGLE APPS SCRIPT
// =====================================

void fetchConfig() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, cannot fetch config");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);

  String configUrl = String(serverUrl) + "?config=true";
  Serial.println();
  Serial.print("Fetching Config from: ");
  Serial.println(configUrl);

  if (!http.begin(client, configUrl)) {
    Serial.println("HTTP Begin Failed (Config)");
    return;
  }

  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.print("Config Response: ");
    Serial.println(payload);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      // รองรับทั้งกรณีที่ Apps Script ส่ง {"status":"on","sensor_id":"..."} โดยตรง
      // และกรณีที่ห่ออยู่ใน {"status":"success","data":{"status":"on","sensor_id":"..."}}
      JsonObject cfg = doc["data"].is<JsonObject>() ? doc["data"].as<JsonObject>() : doc.as<JsonObject>();

      if (cfg["status"].is<const char*>()) {
        systemStatus = cfg["status"].as<String>();
        systemStatus.toLowerCase(); // แปลงเป็นตัวพิมพ์เล็ก เช่น "ON" -> "on"
      }

      if (cfg["sensor_id"].is<const char*>()) {
        String newSensorId = cfg["sensor_id"].as<String>();
        if (newSensorId.length() > 0) {
          sensorID = newSensorId;
        }
      }

      Serial.println("--- Config Updated ---");
      Serial.print("Status    : ");
      Serial.println(systemStatus);
      Serial.print("Sensor ID : ");
      Serial.println(sensorID);
      Serial.println("----------------------");

    } else {
      Serial.print("JSON Parse Failed: ");
      Serial.println(error.c_str());
    }

  } else {
    Serial.print("Fetch Config Failed: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
}

// =====================================
// 7. READ PH SENSOR
// =====================================

void readPH() {

  // อ่านค่า Analog 10 ครั้ง
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(phPin);
    delay(30);
  }

  // เรียงค่าจากน้อยไปมาก
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }

  // เฉลี่ยค่าตรงกลาง 6 ค่า
  avgval = 0;
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i];
  }

  // แปลง ADC เป็น Voltage โดยประมาณ
  volt = (float)avgval * 3.3 / 4095.0 / 6.0;

  // คำนวณ pH ด้วยสูตรเดิมของคุณ
  ph_act = -5.70 * volt + calibration_value;
}

// =====================================
// 8. DISPLAY LCD
// =====================================

void displayLCD() {

  lcd.setCursor(0, 0);
  lcd.print("V:");
  lcd.print(volt, 2);
  lcd.print("V [");
  lcd.print(systemStatus == "on" ? "ON " : "OFF");
  lcd.print("] ");

  lcd.setCursor(0, 1);
  lcd.print("pH Val:");
  lcd.print(ph_act, 2);
  lcd.print("    ");
}

// =====================================
// 9. SEND DATA TO GOOGLE SHEETS
// =====================================

void sendToGoogleSheets() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);

  if (!http.begin(client, serverUrl)) {
    Serial.println("HTTP Begin Failed");
    return;
  }

  http.addHeader("Content-Type", "application/json");

  String jsonPayload = "{";
  jsonPayload += "\"sensor_id\":\"";
  jsonPayload += sensorID;
  jsonPayload += "\",";
  jsonPayload += "\"ph\":";
  jsonPayload += String(ph_act, 2);
  jsonPayload += "}";

  Serial.println();
  Serial.println("Sending Data:");
  Serial.println(jsonPayload);

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response Code: ");
    Serial.println(httpResponseCode);

    String responseBody = http.getString();
    Serial.print("Server Response: ");
    Serial.println(responseBody);
  } else {
    Serial.print("Connection Failed: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
}

// =====================================
// 10. SETUP
// =====================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  // ADC ESP32
  analogReadResolution(12);
  analogSetPinAttenuation(phPin, ADC_11db);

  // LCD I2C
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("PH Meter System");
  lcd.setCursor(0, 1);
  lcd.print("Initializing");
  delay(2000);

  // Connect WiFi
  connectWiFi();

  // ดึง Config ครั้งแรกเมื่อเริ่มทำงาน
  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Loading Config..");
    fetchConfig();
  }

  lcd.clear();
  Serial.println("System Ready");
}

// =====================================
// 11. MAIN LOOP
// =====================================

void loop() {

  // อ่านค่า pH
  readPH();

  // แสดงค่า LCD
  displayLCD();

  // แสดงค่า Serial Monitor
  Serial.println("====================");
  Serial.print("Sensor ID: ");
  Serial.print(sensorID);
  Serial.print(" | Status: ");
  Serial.println(systemStatus);
  Serial.print("Voltage: ");
  Serial.print(volt, 3);
  Serial.println(" V");
  Serial.print("pH Value: ");
  Serial.println(ph_act, 2);
  Serial.println("====================");

  // ตรวจสอบ WiFi
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // อัปเดต Config จาก Server ทุก 30 วินาที
  if (millis() - previousConfig >= configInterval) {
    previousConfig = millis();
    fetchConfig();
  }

  // ส่งข้อมูลทุก 5 วินาที เฉพาะเมื่อ status เป็น "on"
  if (millis() - previousSend >= sendInterval) {
    previousSend = millis();

    if (systemStatus == "on") {
      sendToGoogleSheets();
    } else {
      Serial.println("Status is OFF -> Skip sending data to server.");
    }
  }

  delay(5000);
}