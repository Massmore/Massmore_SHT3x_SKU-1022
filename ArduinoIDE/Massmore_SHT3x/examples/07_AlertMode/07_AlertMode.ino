/*
  07_AlertMode - ให้ชิปเตือนเองผ่านขา ALRT โดย MCU ไม่ต้องคอยอ่าน

  SHT3x เก็บ threshold ไว้ได้ 4 ค่า ทำงานแบบมี hysteresis
    highSet    เกินค่านี้  -> ขา ALRT ขึ้น HIGH
    highClear  ต่ำกว่าค่านี้ -> ขา ALRT กลับลง LOW
    lowClear   สูงกว่าค่านี้ -> ขา ALRT กลับลง LOW
    lowSet     ต่ำกว่าค่านี้ -> ขา ALRT ขึ้น HIGH

  ลำดับที่ถูกต้องคือ  lowSet < lowClear < highClear < highSet

  ข้อสำคัญ: ALERT ทำงานเฉพาะตอนอยู่ในโหมด periodic เท่านั้น
  เพราะชิปต้องวัดเองถึงจะรู้ว่าค่าเกินหรือยัง

  ความละเอียดที่ชิปเก็บได้คือประมาณ 0.5 องศา และ 1 %RH
  (เพราะเก็บแค่ 9 บิตบนของอุณหภูมิ และ 7 บิตบนของความชื้น)

  การต่อสาย: ALRT -> GPIO 4  (ขานี้เป็น push-pull ไม่ต้องใส่ pull-up)

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>

#define PIN_SDA 21
#define PIN_SCL 22
#define PIN_ALERT 4

MassmoreSHT3x sht;

volatile bool alertFlag = false;

// ISR ต้องสั้นที่สุด ห้ามคุย I2C หรือ Serial ในนี้
void IRAM_ATTR onAlertPin() { alertFlag = true; }

void printLimits() {
  massmore_sht3x_alert_limits_t limits;
  if (!sht.getAlertLimits(limits)) {
    Serial.println(F("อ่าน threshold กลับมาไม่ได้"));
    return;
  }
  Serial.println(F("threshold ที่อยู่ในชิปจริง (ปัดตามความละเอียดของ hardware แล้ว)"));
  Serial.print(F("  high set   T ")); Serial.print(limits.highSetTemperature, 1);
  Serial.print(F(" C   RH ")); Serial.println(limits.highSetHumidity, 1);
  Serial.print(F("  high clear T ")); Serial.print(limits.highClearTemperature, 1);
  Serial.print(F(" C   RH ")); Serial.println(limits.highClearHumidity, 1);
  Serial.print(F("  low clear  T ")); Serial.print(limits.lowClearTemperature, 1);
  Serial.print(F(" C   RH ")); Serial.println(limits.lowClearHumidity, 1);
  Serial.print(F("  low set    T ")); Serial.print(limits.lowSetTemperature, 1);
  Serial.print(F(" C   RH ")); Serial.println(limits.lowSetHumidity, 1);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 07 โหมด ALERT"));

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }

  sht.setAlertPin(PIN_ALERT);
  attachInterrupt(digitalPinToInterrupt(PIN_ALERT), onAlertPin, CHANGE);

  // ตั้งช่วงที่ยอมรับได้: 20-30 องศา, 40-70 %RH
  // เว้น hysteresis 1 องศา และ 3 %RH เพื่อไม่ให้ขา ALRT กระพริบถี่ตอนค่าแกว่งอยู่ที่ขอบ
  if (!sht.setAlertWindow(20.0f, 30.0f, 40.0f, 70.0f, 1.0f, 3.0f)) {
    Serial.print(F("ตั้ง threshold ไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
  }
  printLimits();

  // ALERT ทำงานได้เฉพาะในโหมด periodic
  sht.startPeriodic(MASSMORE_SHT3X_RATE_1_HZ, MASSMORE_SHT3X_REPEATABILITY_HIGH);
  sht.clearStatus();

  Serial.println(F("\nลองใช้มือกำเซ็นเซอร์ หรือหายใจรด แล้วดูว่าขา ALRT ขึ้นไหม"));
}

void loop() {
  if (alertFlag) {
    alertFlag = false;
    Serial.print(F(">>> ขา ALRT เปลี่ยนสถานะเป็น "));
    Serial.println(sht.isAlertPinActive() ? F("HIGH (เตือน)") : F("LOW (ปกติ)"));

    // อ่าน status เพื่อดูว่าเป็นเพราะอุณหภูมิหรือความชื้น
    massmore_sht3x_status_bits_t bits;
    if (sht.readStatus(bits)) {
      if (bits.temperatureAlert) {
        Serial.println(F("    สาเหตุ: อุณหภูมิเลยขอบเขต"));
      }
      if (bits.humidityAlert) {
        Serial.println(F("    สาเหตุ: ความชื้นเลยขอบเขต"));
      }
    }
  }

  if (sht.update()) {
    Serial.print(sht.getTemperature(), 2);
    Serial.print(F(" C  "));
    Serial.print(sht.getHumidity(), 2);
    Serial.print(F(" %RH   ALRT="));
    Serial.println(sht.isAlertPinActive() ? F("HIGH") : F("LOW"));
  }
}
