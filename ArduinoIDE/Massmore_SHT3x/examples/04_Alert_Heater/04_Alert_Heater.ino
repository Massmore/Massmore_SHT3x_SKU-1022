/*
  04_Alert_Heater - Periodic Mode + ALERT threshold ผ่านขา ALRT + Heater + Status Register

  ALERT
    SHT3x เก็บ threshold 4 ชุดแบบมี Hysteresis (lowSet < lowClear < highClear < highSet)
    ทำงานเฉพาะ Periodic Mode เพราะชิปต้องวัดเองถึงจะเปรียบเทียบได้
    ขา ALRT เป็น Push-pull ไม่ต้องใส่ Pull-up   ความละเอียดที่ชิปเก็บ ~0.5 °C และ ~1 %RH

  Heater
    ใช้ไล่ความชื้นที่เกาะเซ็นเซอร์ (Outdoor) หรือทดสอบว่าเซ็นเซอร์ตอบสนอง
    เปิดแล้วอุณหภูมิที่อ่านได้จะสูงขึ้นชั่วคราว  ห้ามใช้ค่าขณะ Heater เปิดเป็นค่าจริง

  การต่อสาย
    ESP32: SDA -> 21, SCL -> 22, ALRT -> GPIO 4
    Nano : SDA -> A4, SCL -> A5, ALRT -> D2 (External interrupt)

  Copyright (c) 2026 Massmore Biz Co., Ltd.  |  MIT License
*/

#include <Massmore_SHT3x.h>
#include <Wire.h>

#if defined(ESP32)
#define PIN_SDA 21
#define PIN_SCL 22
#define PIN_ALERT 4
#define ISR_ATTR IRAM_ATTR
#else
#define PIN_ALERT 2
#define ISR_ATTR
#endif

#define HEATER_DEMO_INTERVAL_MS 30000UL
#define HEATER_ON_MS 3000UL

Massmore_SHT3x sht(Wire);

volatile bool alertFlag = false;
uint32_t lastHeaterDemoMs = 0;

/* ISR ต้องสั้นที่สุด ห้ามคุย I2C หรือ Serial ในนี้ */
void ISR_ATTR onAlertPin() { alertFlag = true; }

void printLimits() {
  Massmore_SHT3x::AlertLimits l;
  if (!sht.getAlertLimits(l)) {
    Serial.println(F("getAlertLimits failed"));
    return;
  }
  Serial.println(F("Alert limits stored in chip (rounded by hardware):"));
  Serial.print(F("  highSet   T=")); Serial.print(l.highSetTemperature, 1);
  Serial.print(F("  RH="));          Serial.println(l.highSetHumidity, 1);
  Serial.print(F("  highClear T=")); Serial.print(l.highClearTemperature, 1);
  Serial.print(F("  RH="));          Serial.println(l.highClearHumidity, 1);
  Serial.print(F("  lowClear  T=")); Serial.print(l.lowClearTemperature, 1);
  Serial.print(F("  RH="));          Serial.println(l.lowClearHumidity, 1);
  Serial.print(F("  lowSet    T=")); Serial.print(l.lowSetTemperature, 1);
  Serial.print(F("  RH="));          Serial.println(l.lowSetHumidity, 1);
}

void heaterDemo() {
  Serial.println(F("--- Heater demo: ON for 3 s ---"));
  float before = sht.getLastReading().temperature;
  sht.setHeater(true);
  Serial.print(F("Heater bit: "));
  Serial.println(sht.isHeaterOn() ? F("ON") : F("OFF"));

  uint32_t t0 = millis();
  float peak = before;
  while ((uint32_t)(millis() - t0) < HEATER_ON_MS) {
    Massmore_SHT3x::Reading r;
    if (sht.update() || sht.fetchData(r)) {
      float t = sht.getLastReading().temperature;
      if (t > peak) peak = t;
    }
    delay(100);
  }
  sht.setHeater(false);
  Serial.print(F("Temperature rise: +"));
  Serial.print(peak - before, 2);
  Serial.println(F(" C (expect > 0.5 C on a working sensor)"));
  Serial.println(F("--- Heater OFF ---"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }

#if defined(ESP32)
  Wire.begin(PIN_SDA, PIN_SCL);
#else
  Wire.begin();
#endif
  Wire.setClock(100000UL);

  Serial.println(F("Massmore_SHT3x - 04_Alert_Heater"));
  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, PIN_ALERT)) {
    Serial.print(F("begin() failed: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }

  attachInterrupt(digitalPinToInterrupt(PIN_ALERT), onAlertPin, CHANGE);

  /* หน้าต่างที่ยอมรับ: 20-30 °C, 40-70 %RH   Hysteresis 1 °C / 3 %RH กัน ALRT กระพริบที่ขอบ */
  if (!sht.setAlertWindow(20.0f, 30.0f, 40.0f, 70.0f, 1.0f, 3.0f)) {
    Serial.print(F("setAlertWindow failed: "));
    Serial.println(sht.lastErrorString());
  }
  printLimits();

  sht.startPeriodic(Massmore_SHT3x::Rate::HZ_1, Massmore_SHT3x::Repeatability::RPT_HIGH);
  sht.clearStatus();
  Serial.println(F("Periodic 1 Hz started. Breathe on the sensor to trigger ALERT."));
}

void loop() {
  if (alertFlag) {
    alertFlag = false;
    Serial.print(F(">>> ALRT pin = "));
    Serial.println(sht.isAlertPinActive() ? F("HIGH (alert)") : F("LOW (normal)"));

    Massmore_SHT3x::StatusBits s;
    if (sht.readStatus(s)) {
      if (s.temperatureAlert) Serial.println(F("    cause: temperature out of window"));
      if (s.humidityAlert)    Serial.println(F("    cause: humidity out of window"));
    }
  }

  if (sht.update()) {
    const Massmore_SHT3x::Reading &r = sht.getLastReading();
    Serial.print(r.temperature, 2);
    Serial.print(F(" C  "));
    Serial.print(r.humidity, 2);
    Serial.print(F(" %RH  ALRT="));
    Serial.println(sht.isAlertPinActive() ? F("HIGH") : F("LOW"));
  }

  if ((uint32_t)(millis() - lastHeaterDemoMs) >= HEATER_DEMO_INTERVAL_MS) {
    lastHeaterDemoMs = millis();
    heaterDemo();
  }
}
