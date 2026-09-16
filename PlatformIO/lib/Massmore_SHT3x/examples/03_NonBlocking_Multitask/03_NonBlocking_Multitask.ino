/*
  03_NonBlocking_Multitask - วัดด้วย Non-blocking FSM ขณะที่ loop() ยังทำงานอื่นได้

  แนวคิด
    requestConversion()  สั่งชิปเริ่มวัด แล้วคืนทันที
    update()             เรียกทุกรอบ loop() ไลบรารีเช็คเวลาเองด้วย millis() (rollover-safe)
    isDataReady()        true เมื่อผลพร้อม
    getReadings()        ดึงผลแล้วเคลียร์ flag

  งานที่สอง: กะพริบ LED ทุก 100 ms และนับจำนวนรอบ loop() ต่อวินาที
  ถ้า loop ถูก Block ตัวเลขรอบต่อวินาทีจะตกลงชัดเจน

  Copyright (c) 2026 Massmore Biz Co., Ltd.  |  MIT License
*/

#include <Massmore_SHT3x.h>
#include <Wire.h>

#if defined(ESP32)
#define PIN_SDA 21
#define PIN_SCL 22
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

#define MEASURE_INTERVAL_MS 500UL
#define BLINK_INTERVAL_MS 100UL

Massmore_SHT3x sht(Wire);

uint32_t lastRequestMs = 0;
uint32_t lastBlinkMs = 0;
uint32_t lastReportMs = 0;
uint32_t loopCounter = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }
  pinMode(LED_BUILTIN, OUTPUT);

#if defined(ESP32)
  Wire.begin(PIN_SDA, PIN_SCL);
#else
  Wire.begin();
#endif
  Wire.setClock(100000UL);

  Serial.println(F("Massmore_SHT3x - 03_NonBlocking_Multitask"));
  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A)) {
    Serial.print(F("begin() failed: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }
  sht.setRepeatability(Massmore_SHT3x::Repeatability::RPT_HIGH); /* ~15 ms ต่อครั้ง แต่ loop ไม่รอ */
}

void loop() {
  uint32_t now = millis();
  loopCounter++;

  /* --- Task 1: ขอวัดใหม่ทุก MEASURE_INTERVAL_MS --- */
  if ((uint32_t)(now - lastRequestMs) >= MEASURE_INTERVAL_MS) {
    lastRequestMs = now;
    if (!sht.requestConversion()) {
      Serial.print(F("requestConversion failed: "));
      Serial.println(sht.lastErrorString());
    }
  }

  /* --- Task 1 (ต่อ): ขับ FSM แล้วรับผลเมื่อพร้อม --- */
  sht.update();
  if (sht.isDataReady()) {
    Massmore_SHT3x::Reading r;
    sht.getReadings(r);
    Serial.print(F("T="));
    Serial.print(r.temperature, 2);
    Serial.print(F(" C  RH="));
    Serial.print(r.humidity, 2);
    Serial.print(F(" %  loops/s="));
    Serial.println(loopCounter * 1000UL / MEASURE_INTERVAL_MS);
    loopCounter = 0;
  } else if (sht.getFsmState() == Massmore_SHT3x::FsmState::FAULT) {
    Serial.print(F("FSM error: "));
    Serial.println(sht.lastErrorString());
    sht.requestConversion(); /* เริ่มใหม่ */
  }

  /* --- Task 2: กะพริบ LED โดยไม่ใช้ delay() --- */
  if ((uint32_t)(now - lastBlinkMs) >= BLINK_INTERVAL_MS) {
    lastBlinkMs = now;
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}
