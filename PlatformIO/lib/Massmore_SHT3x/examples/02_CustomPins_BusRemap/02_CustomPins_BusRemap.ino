/*
  02_CustomPins_BusRemap - ย้ายขา I2C และใช้ Bus ที่สอง (Wire1)

  ESP32 / ESP32-S3 (Arduino-ESP32 Core 3.x+)
    GPIO Matrix ให้เลือกขา SDA/SCL ได้เกือบทุกขา และมี Hardware I2C 2 ชุด (Wire, Wire1)
    ตัวอย่างนี้ใช้ Wire1 เพื่อแยกเซ็นเซอร์ออกจาก Bus หลัก โดยยังใช้ขา 21/22 ตามที่ต่ออยู่จริง
    ต้องการขาอื่นให้แก้ PIN_SDA / PIN_SCL ด้านล่างได้เลย ไม่ต้องแก้ไลบรารี

  Arduino Nano (ATmega328P)
    มี Hardware I2C ชุดเดียว ขาตายตัว A4 (SDA) / A5 (SCL) ย้ายไม่ได้
    ตัวอย่างนี้จึง fallback ไปใช้ Wire ปกติ

  Copyright (c) 2026 Massmore Biz Co., Ltd.  |  MIT License
*/

#include <Massmore_SHT3x.h>
#include <Wire.h>

#if defined(ESP32)
/* แก้สองบรรทัดนี้เป็นขาใดก็ได้ที่ว่างบนบอร์ดของคุณ */
#define PIN_SDA 21
#define PIN_SCL 22
#define BUS Wire1          /* ใช้ Hardware I2C ชุดที่สอง */
#define BUS_NAME "Wire1"
#define I2C_FREQ 400000UL  /* Fast mode - SHT3x รองรับถึง 1 MHz */
#else
#define BUS Wire           /* AVR: ขาตายตัว A4 / A5 */
#define BUS_NAME "Wire"
#define I2C_FREQ 100000UL
#endif

Massmore_SHT3x sht(BUS);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }
  Serial.println(F("Massmore_SHT3x - 02_CustomPins_BusRemap"));

#if defined(ESP32)
  BUS.begin(PIN_SDA, PIN_SCL, I2C_FREQ); /* Core 3.x signature: begin(sda, scl, frequency) */
  Serial.print(F("Bus: " BUS_NAME "  SDA=GPIO"));
  Serial.print(PIN_SDA);
  Serial.print(F("  SCL=GPIO"));
  Serial.print(PIN_SCL);
  Serial.print(F("  freq="));
  Serial.print(I2C_FREQ / 1000UL);
  Serial.println(F(" kHz"));
#else
  BUS.begin();
  BUS.setClock(I2C_FREQ);
  Serial.println(F("Bus: " BUS_NAME "  SDA=A4  SCL=A5 (fixed hardware pins)"));
#endif

  /* สแกนก่อนเพื่อยืนยันว่าเซ็นเซอร์อยู่บน Bus ที่เลือกจริง */
  uint8_t found[2];
  uint8_t n = Massmore_SHT3x::scan(BUS, found);
  Serial.print(F("Devices found: "));
  Serial.println(n);
  for (uint8_t i = 0; i < n; i++) {
    Serial.print(F("  address 0x"));
    Serial.println(found[i], HEX);
  }

  if (!sht.begin(n > 0 ? found[0] : MASSMORE_SHT3X_I2C_ADDR_A)) {
    Serial.print(F("begin() failed: "));
    Serial.println(sht.lastErrorString());
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
    Serial.println(F(" %RH"));
  } else {
    Serial.print(F("Read failed: "));
    Serial.println(sht.lastErrorString());
  }
  delay(1000);
}
