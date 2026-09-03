/*
  12_LowPower_DeepSleep - วัดแล้วหลับ สำหรับงานใช้แบตเตอรี่

  หลักการประหยัดไฟกับ SHT3x
    1. ใช้ single shot ไม่ใช่ periodic  ระหว่างที่ไม่ได้วัด ชิปกินแค่ 0.2-2 uA
       (โหมด periodic กินตลอดเวลาประมาณ 45 uA)
    2. ใช้ความละเอียดต่ำเมื่อพอรับได้  ลดเวลาที่ชิปกิน 600-1500 uA จาก 15 ms เหลือ 4 ms
    3. ให้ MCU หลับ deep sleep ระหว่างรอบ  ตัวนี้กินไฟมากกว่าเซ็นเซอร์หลายเท่า
    4. อย่าลืมสั่ง break ก่อนหลับ ถ้าเคยเปิด periodic ไว้

  บอร์ด Massmore กินไฟเท่าไร
    ตัวเซ็นเซอร์เองตามสเปกด้านบน บวก quiescent ของ regulator บนบอร์ด
    ถ้าจะทำงานแบตยาวเป็นเดือน แนะนำให้ตัดไฟเลี้ยงบอร์ดด้วย MOSFET ตอนหลับ

  ตัวอย่างนี้ตั้งให้ตื่นทุก 60 วินาที วัดหนึ่งครั้ง แล้วหลับต่อ
  ค่าที่วัดได้เก็บไว้ใน RTC memory ซึ่งไม่หายตอน deep sleep

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>

#define PIN_SDA 21
#define PIN_SCL 22
#define SLEEP_SECONDS 60
#define HISTORY_SIZE 10

MassmoreSHT3x sht;

#if defined(ARDUINO_ARCH_ESP32)
// ตัวแปรที่ประกาศแบบนี้จะอยู่ใน RTC memory ไม่หายตอน deep sleep
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR float historyT[HISTORY_SIZE];
RTC_DATA_ATTR float historyH[HISTORY_SIZE];
RTC_DATA_ATTR uint8_t historyIndex = 0;
#else
uint32_t bootCount = 0;
float historyT[HISTORY_SIZE];
float historyH[HISTORY_SIZE];
uint8_t historyIndex = 0;
#endif

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) {
    ;
  }

  bootCount++;
  Serial.println();
  Serial.print(F("Massmore SHT3x - 12 ประหยัดพลังงาน  ตื่นครั้งที่ "));
  Serial.println(bootCount);

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    // ถึงจะพลาดก็ยังต้องหลับต่อ ไม่งั้นแบตหมดเร็ว
  } else {
    // ความละเอียดต่ำ ใช้เวลาแค่ประมาณ 4 ms ประหยัดพลังงานที่สุด
    sht.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_LOW);

    float t = 0.0f;
    float h = 0.0f;
    if (sht.measure(&t, &h)) {
      historyT[historyIndex] = t;
      historyH[historyIndex] = h;
      historyIndex = (uint8_t)((historyIndex + 1) % HISTORY_SIZE);

      Serial.print(F("วัดได้ "));
      Serial.print(t, 2);
      Serial.print(F(" C  "));
      Serial.print(h, 2);
      Serial.println(F(" %RH"));

      // แสดงประวัติที่เก็บไว้ข้าม deep sleep
      Serial.println(F("ประวัติล่าสุดที่เก็บไว้ใน RTC memory"));
      for (uint8_t i = 0; i < HISTORY_SIZE; i++) {
        if (historyT[i] != 0.0f || historyH[i] != 0.0f) {
          Serial.print(F("  "));
          Serial.print(i);
          Serial.print(F(": "));
          Serial.print(historyT[i], 2);
          Serial.print(F(" C  "));
          Serial.print(historyH[i], 2);
          Serial.println(F(" %RH"));
        }
      }
    } else {
      Serial.print(F("วัดไม่สำเร็จ: "));
      Serial.println(sht.lastErrorString());
    }

    // สำคัญ: ให้ชิปกลับสู่ idle ก่อนหลับ จะได้กินไฟระดับไมโครแอมป์
    sht.stopPeriodic();
  }

#if defined(ARDUINO_ARCH_ESP32)
  Serial.print(F("กำลังหลับ "));
  Serial.print(SLEEP_SECONDS);
  Serial.println(F(" วินาที"));
  Serial.flush();

  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
  // โค้ดหลังบรรทัดนี้จะไม่ถูกเรียก ตื่นแล้วเริ่มที่ setup() ใหม่เสมอ
#else
  Serial.println(F("บอร์ดนี้ไม่รองรับ deep sleep ใช้ delay() แทน"));
  delay((uint32_t)SLEEP_SECONDS * 1000UL);
#endif
}

void loop() {
  // ไม่ได้ใช้ เพราะ ESP32 เริ่มที่ setup() ใหม่ทุกครั้งที่ตื่น
}
