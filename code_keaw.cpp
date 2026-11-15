#include <WiFi.h>
#include <HTTPClient.h>

// 1. Replace with your Wi-Fi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// 2. Replace with your server API endpoint (use http:// for local/IP testing)
const char* serverUrl = "https://script.google.com/macros/s/AKfycbzlQFWL8iaMYqgoSj2n7gRbKq2ODfi2hxeIM5ksJK718_t88qfq4_S6L1GKYCj3ZYF3/exec";

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Connected!");
  Serial.print("Board IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    //Initialize connection
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    //Generate dummy test data (random pH between 6.50 and 7.50)
    float dummyPh = random(650, 751) / 100.0;
    String jsonPayload = "{\"sensor_id\":\"esp32_test_01\",\"ph\":" + String(dummyPh, 2) + "}";//dummy เอาค่า ph จริง

    Serial.println("Sending payload: " + jsonPayload);

    // Send HTTP POST request
    int httpResponseCode = http.POST(jsonPayload);

    // Check response
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response Code: ");
      Serial.println(httpResponseCode);
      
      String responseBody = http.getString();
      Serial.println("Server Response: " + responseBody);
    } else {
      Serial.print("Connection Failed. Error: ");
      Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("Wi-Fi Disconnected. Reconnecting...");
    WiFi.reconnect();
  }

  // Wait 5 seconds before the next test
  delay(5000);
}