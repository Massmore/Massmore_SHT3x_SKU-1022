/*
  02_CustomPins_BusRemap - ย้ายขา I2C และใช้ Bus ที่สอง (Wire1)

  ESP32 / ESP32-S3 (Arduino-ESP32 Core 3.x+)
    GPIO Matrix ให้เลือกขา SDA/SCL ได้เกือบทุกขา และมี Hardware I2C 2 ชุด (Wire, Wire1)
    ตัวอย่างนี้ใช้ Wire1 บนขาที่กำหนดเอง เพื่อแยกเซ็นเซอร์ออกจาก Bus หลัก

  Arduino Nano (ATmega328P)
    มี Hardware I2C ชุดเดียว ขาตายตัว A4 (SDA) / A5 (SCL) ย้ายไม่ได้
    ตัวอย่างนี้จึง fallback ไปใช้ Wire ปกติ

  Copyright (c) 2026 Massmore Biz Co., Ltd.  |  MIT License
*/

#include <Massmore_SHT3x.h>
#include <Wire.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)
/* ESP32-S3: เลือกขาที่ว่างจาก USB / PSRAM / Flash */
#define PIN_SDA 8
#define PIN_SCL 9
#define BUS Wire1
#elif defined(ESP32)
/* ESP32 Classic: ย้ายไปขา 25 / 26 เพื่อแสดงว่าไม่จำเป็นต้องใช้ 21 / 22 */
#define PIN_SDA 25
#define PIN_SCL 26
#define BUS Wire1
#else
/* AVR และบอร์ดอื่น: ใช้ Wire บนขาตายตัว */
#define BUS Wire
#endif

Massmore_SHT3x sht(BUS);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }
  Serial.println(F("Massmore_SHT3x - 02_CustomPins_BusRemap"));

#if defined(ESP32)
  BUS.begin(PIN_SDA, PIN_SCL, 400000UL); /* Core 3.x signature: begin(sda, scl, frequency) */
  Serial.print(F("Bus: Wire1  SDA=GPIO"));
  Serial.print(PIN_SDA);
  Serial.print(F("  SCL=GPIO"));
  Serial.println(PIN_SCL);
#else
  BUS.begin();
  BUS.setClock(100000UL);
  Serial.println(F("Bus: Wire  SDA=A4  SCL=A5 (fixed hardware pins)"));
#endif

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A)) {
    Serial.print(F("begin() failed: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }
  Serial.println(F("Sensor ready"));
}

void loop() {
  float t = sht.readTemperature();
  float h = sht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.print(F("Read failed: "));
    Serial.println(sht.lastErrorString());
  } else {
    Serial.print(F("Temp: "));
    Serial.print(t, 2);
    Serial.print(F(" C   Humi: "));
    Serial.print(h, 2);
    Serial.println(F(" %RH"));
  }
  delay(1000);
}
