/*
  01_BasicRead - อ่านอุณหภูมิและความชื้นด้วย Simple Blocking API

  บอร์ด: Massmore SHT3X (SKU-1022)  I2C address 0x44 (ค่าเริ่มต้น)

  การต่อสาย
    ESP32 / ESP32-S3 : SDA -> GPIO 21, SCL -> GPIO 22 (แก้ได้ที่ #define ด้านล่าง)
    Arduino Nano     : SDA -> A4, SCL -> A5 (ขาตายตัวของ Hardware I2C)
    VCC -> 3V3 หรือ 5V, GND -> GND  (บอร์ดรองรับ 3-5 V ทุกขา)

  หลักการ: Sketch เป็นเจ้าของ Wire.begin() ไลบรารีรับ Bus เข้ามาเท่านั้น

  Copyright (c) 2026 Massmore Biz Co., Ltd.  |  MIT License
*/

#include <Massmore_SHT3x.h>
#include <Wire.h>

#if defined(ESP32)
#define PIN_SDA 21
#define PIN_SCL 22
#endif

Massmore_SHT3x sht(Wire);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }

#if defined(ESP32)
  Wire.begin(PIN_SDA, PIN_SCL); /* Arduino-ESP32 Core 3.x: กำหนดขาได้ตาม GPIO Matrix */
#else
  Wire.begin();                 /* AVR: ขาตายตัว A4 / A5 */
#endif
  Wire.setClock(100000UL);      /* 100 kHz เหมาะกับสาย Qwiic ยาวไม่เกิน 30 cm */

  Serial.println(F("Massmore_SHT3x - 01_BasicRead"));

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A)) {
    Serial.print(F("begin() failed: "));
    Serial.println(sht.lastErrorString());
    Serial.println(F("ตรวจสาย SDA/SCL, ไฟเลี้ยง และ Jumper ADDR (0x44 = เปิด, 0x45 = ปิด)"));
    while (true) {
      delay(1000);
    }
  }
  Serial.println(F("Sensor ready"));
}

void loop() {
  Massmore_SHT3x::Reading r;

  if (sht.readAll(r)) {
    Serial.print(F("Temp: "));
    Serial.print(r.temperature, 2);
    Serial.print(F(" C   Humi: "));
    Serial.print(r.humidity, 2);
    Serial.print(F(" %RH   DewPoint: "));
    Serial.print(Massmore_SHT3x::dewPoint(r.temperature, r.humidity), 1);
    Serial.println(F(" C"));
  } else {
    Serial.print(F("Read failed: "));
    Serial.println(sht.lastErrorString());
  }

  delay(1000);
}
