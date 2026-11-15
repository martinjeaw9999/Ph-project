#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ตั้งค่าจอ LCD ที่ Address 0x27 ขนาด 16 ตัวอักษร 2 บรรทัด
// (หากจอไม่ขึ้นตัวอักษร ให้ลองเปลี่ยน 0x27 เป็น 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int phPin = 34; // ขา Po ต่อเข้า GPIO 34

// ตัวแปรสำหรับการตั้งค่า (Calibration)
// หากอ่านค่า pH 7.0 (น้ำเปล่า) แล้วค่าไม่ตรง ให้ปรับค่า calibration_value ขึ้นหรือลง
float calibration_value = 21.34; 

unsigned long int avgval;
int buffer_arr[10], temp;

void setup() {
  Serial.begin(115200);
  
  // เริ่มต้นการทำงานของจอ LCD
  lcd.init();
  lcd.backlight();
  
  lcd.setCursor(0, 0);
  lcd.print("pH Meter System");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();
}

void loop() {
  // 1. อ่านค่า Analog 10 ครั้งเพื่อนำมาหาค่าเฉลี่ยให้นิ่งขึ้น
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(phPin);
    delay(30);
  }
  
  // 2. เรียงลำดับค่าจากน้อยไปมาก (ตัดค่าที่แกว่งผิดปกติออก)
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }
  
  // 3. นำค่าตรงกลาง 6 ค่ามารวมกันเพื่อหาค่าเฉลี่ย
  avgval = 0;
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i];
  }
  
  // 4. แปลงค่า Analog ของ ESP32 (12-bit: 0-4095, แรงดัน 3.3V) เป็นแรงดันไฟฟ้า (Voltage)
  float volt = (float)avgval * 3.3 / 4095.0 / 6;
  
  // 5. คำนวณเป็นค่า pH
  float ph_act = -5.70 * volt + calibration_value;

  // แสดงผลทาง Serial Monitor (เผื่อเช็คค่าผ่านคอมพิวเตอร์)
  Serial.print("Voltage: ");
  Serial.print(volt, 2);
  Serial.print(" V | pH Value: ");
  Serial.println(ph_act, 2);

  // แสดงผลขึ้นจอ LCD
  lcd.setCursor(0, 0);
  lcd.print("Voltage: ");
  lcd.print(volt, 2);
  lcd.print("V  ");

  lcd.setCursor(0, 1);
  lcd.print("pH Val : ");
  lcd.print(ph_act, 2);
  lcd.print("   ");

  delay(1000); // อัปเดตค่าทุกๆ 1 วินาที
}