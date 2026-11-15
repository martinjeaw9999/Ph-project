
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

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
// 3. WIFI SETTINGS
// =====================================

// ใส่ชื่อ Hotspot โทรศัพท์
const char* ssid = "YOUR_WIFI_SSID";

// ใส่รหัสผ่าน Hotspot
const char* password = "YOUR_WIFI_PASSWORD";

// Google Apps Script URL
const char* serverUrl =
"https://script.google.com/macros/s/AKfycbzlQFWL8iaMYqgoSj2n7gRbKq2ODfi2hxeIM5ksJK718_t88qfq4_S6L1GKYCj3ZYF3/exec";

// Sensor ID
const char* sensorID = "esp32_ph_01";

// =====================================
// 4. TIMER SETTINGS
// =====================================

unsigned long previousSend = 0;

const unsigned long sendInterval = 5000;

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
// 6. READ PH SENSOR
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
// 7. DISPLAY LCD
// =====================================

void displayLCD() {

  lcd.setCursor(0, 0);

  lcd.print("Voltage:");

  lcd.print(volt, 2);

  lcd.print("V   ");

  lcd.setCursor(0, 1);

  lcd.print("pH Val:");

  lcd.print(ph_act, 2);

  lcd.print("    ");
}

// =====================================
// 8. SEND DATA TO GOOGLE SHEETS
// =====================================

void sendToGoogleSheets() {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi disconnected");

    return;
  }

  WiFiClientSecure client;

  // ใช้สำหรับทดสอบ HTTPS
  // ระบบจริงควรตรวจสอบใบรับรอง SSL

  client.setInsecure();

  HTTPClient http;

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  http.setTimeout(15000);

  if (!http.begin(client, serverUrl)) {

    Serial.println("HTTP Begin Failed");

    return;
  }

  http.addHeader("Content-Type", "application/json");

  // ส่งค่า pH จริง ไม่ใช่ค่าจำลอง

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
// 9. SETUP
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

  lcd.clear();

  Serial.println("System Ready");
}

// =====================================
// 10. MAIN LOOP
// =====================================

void loop() {

  // อ่านค่า pH

  readPH();

  // แสดงค่า LCD

  displayLCD();

  // แสดงค่า Serial Monitor

  Serial.println("====================");

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

  // ส่งข้อมูลทุก 5 วินาที

  if (millis() - previousSend >= sendInterval) {

    previousSend = millis();

    sendToGoogleSheets();
  }

  delay(500);
}
