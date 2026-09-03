/*
  04_ART_Mode - โหมดตอบสนองเร็ว (Accelerated Response Time)

  ART คือโหมด periodic ที่ 4 ครั้งต่อวินาที พร้อมตัวกรองภายในของชิป
  ที่ทำให้ค่าความชื้นตอบสนองเร็วขึ้นประมาณสองเท่า

  เหมาะกับ
    - วัดลมหายใจ หรือความชื้นที่เปลี่ยนเร็ว
    - ตู้อบ ตู้เพาะเห็ด ที่ต้องรู้ผลทันทีที่เปิดฝา
    - ระบบควบคุมที่ต้องการ feedback เร็ว

  ลองทดสอบ: หายใจรดเซ็นเซอร์เบา ๆ แล้วดูว่าค่าขึ้นเร็วแค่ไหน
  ตัวอย่างนี้จับเวลาให้ด้วยว่าใช้เวลากี่มิลลิวินาทีกว่าความชื้นจะขึ้นถึง 90%
  ของค่าสูงสุดที่เจอ

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>

#define PIN_SDA 21
#define PIN_SCL 22

MassmoreSHT3x sht;

float baselineHumidity = 0.0f;
float peakHumidity = 0.0f;
uint32_t eventStartMs = 0;
bool eventActive = false;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 04 โหมด ART"));

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }

  if (!sht.startART()) {
    Serial.println(F("สั่งโหมด ART ไม่สำเร็จ"));
    while (true) {
      delay(1000);
    }
  }

  Serial.println(F("โหมด ART ทำงานที่ 4 ครั้ง/วินาที"));
  Serial.println(F("กำลังวัดค่าตั้งต้น 5 วินาที อย่าเพิ่งหายใจรด..."));

  // เก็บค่าตั้งต้นเป็นค่าเฉลี่ยของ 5 วินาทีแรก
  uint32_t start = millis();
  float sum = 0.0f;
  uint16_t count = 0;
  while (millis() - start < 5000) {
    if (sht.update()) {
      sum += sht.getHumidity();
      count++;
    }
  }
  baselineHumidity = (count > 0) ? (sum / count) : 50.0f;

  Serial.print(F("ค่าตั้งต้น "));
  Serial.print(baselineHumidity, 2);
  Serial.println(F(" %RH"));
  Serial.println(F("ลองหายใจรดเซ็นเซอร์เบา ๆ ได้เลย"));
}

void loop() {
  if (!sht.update()) {
    return;
  }

  float humidity = sht.getHumidity();

  Serial.print(humidity, 2);
  Serial.print(F(" %RH   "));
  Serial.print(sht.getTemperature(), 2);
  Serial.print(F(" C"));

  // ตรวจจับว่ามีเหตุการณ์ความชื้นพุ่งขึ้นหรือไม่
  if (!eventActive && humidity > baselineHumidity + 3.0f) {
    eventActive = true;
    eventStartMs = millis();
    peakHumidity = humidity;
    Serial.print(F("   << เริ่มจับเวลา"));
  } else if (eventActive) {
    if (humidity > peakHumidity) {
      peakHumidity = humidity;
    }
    // กลับลงมาใกล้ค่าตั้งต้นแล้ว = จบเหตุการณ์
    if (humidity < baselineHumidity + 1.0f) {
      eventActive = false;
      Serial.print(F("   จบเหตุการณ์ ยอดสูงสุด "));
      Serial.print(peakHumidity, 1);
      Serial.print(F(" %RH  ใช้เวลารวม "));
      Serial.print(millis() - eventStartMs);
      Serial.print(F(" ms"));
    }
  }
  Serial.println();
}
