/*
  13_ErrorHandling - รับมือกับความผิดพลาดให้ระบบเดินต่อได้เอง

  ในงานจริงที่ต้องเดิน 24 ชั่วโมง สิ่งที่เกิดขึ้นได้เสมอคือ
    - สายหลวมชั่วขณะ  -> I2C timeout
    - สัญญาณรบกวนจากมอเตอร์ -> CRC ไม่ตรง
    - ไฟตกวูบเดียว -> ชิปรีบูตเอง ค่าที่ตั้งไว้หายหมด
    - บัส I2C ค้างเพราะอุปกรณ์ตัวอื่นบนบัสเดียวกัน

  ตัวอย่างนี้แสดงชั้นการกู้คืนแบบไล่ระดับ
    ชั้นที่ 1  อ่านผิด 1-2 ครั้ง  -> ลองใหม่เฉย ๆ
    ชั้นที่ 2  ผิดติดกันหลายครั้ง -> soft reset แล้วตั้งค่าใหม่
    ชั้นที่ 3  ยังไม่หาย         -> hard reset ผ่านขา RST
    ชั้นที่ 4  ยังไม่หายอีก      -> รีสตาร์ท MCU

  พร้อมกับเฝ้าดู bit4 (reset detected) เพื่อจับกรณีที่ชิปรีบูตเองโดยเราไม่ได้สั่ง

  การต่อสาย: RST -> GPIO 5 (ต่อหรือไม่ต่อก็ได้ ถ้าไม่ต่อให้ตั้งเป็น -1)

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>

#define PIN_SDA 21
#define PIN_SCL 22
#define PIN_RST 5

#define FAIL_BEFORE_SOFT_RESET 3
#define FAIL_BEFORE_HARD_RESET 6
#define FAIL_BEFORE_REBOOT 12

MassmoreSHT3x sht;

uint16_t consecutiveFailures = 0;
uint32_t totalReads = 0;
uint32_t totalFailures = 0;
uint32_t recoveryCount = 0;

// ตั้งค่าทุกอย่างที่ต้องตั้งหลังชิปรีเซ็ต รวมไว้ที่เดียวเพื่อเรียกซ้ำได้
bool configureSensor() {
  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    return false;
  }
  sht.setResetPin(PIN_RST);
  sht.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_HIGH);
  sht.setTimeout(50);
  sht.clearStatus();
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 13 การจัดการข้อผิดพลาด"));
  Serial.println(F("ลองถอดสาย SDA หรือ VCC ระหว่างที่โปรแกรมรัน แล้วเสียบกลับ"));
  Serial.println(F("เพื่อดูว่าโปรแกรมกู้คืนตัวเองได้จริง"));

  if (!configureSensor()) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    Serial.println(F("จะพยายามต่อไปใน loop()"));
  }
}

void handleFailure() {
  consecutiveFailures++;
  totalFailures++;

  Serial.print(F("  ผิดพลาดครั้งที่ "));
  Serial.print(consecutiveFailures);
  Serial.print(F(" ติดกัน: "));
  Serial.println(sht.lastErrorString());

  if (consecutiveFailures == FAIL_BEFORE_SOFT_RESET) {
    Serial.println(F("  >> ชั้นที่ 2: soft reset แล้วตั้งค่าใหม่"));
    sht.softReset();
    delay(10);
    configureSensor();
    recoveryCount++;
  } else if (consecutiveFailures == FAIL_BEFORE_HARD_RESET) {
    Serial.println(F("  >> ชั้นที่ 3: hard reset ผ่านขา RST"));
    if (!sht.hardReset()) {
      Serial.println(F("     (ไม่ได้ต่อขา RST ข้ามขั้นนี้)"));
    }
    delay(10);
    configureSensor();
    recoveryCount++;
  } else if (consecutiveFailures >= FAIL_BEFORE_REBOOT) {
    Serial.println(F("  >> ชั้นที่ 4: กู้ไม่ได้แล้ว รีสตาร์ท MCU"));
    Serial.flush();
    delay(100);
#if defined(ARDUINO_ARCH_ESP32)
    ESP.restart();
#else
    while (true) {
      ;  // ปล่อยให้ watchdog จัดการ
    }
#endif
  }
}

void loop() {
  totalReads++;

  // เฝ้าดูว่าชิปรีบูตเองหรือเปล่า ต้องเช็คก่อนใช้ค่า
  massmore_sht3x_status_bits_t bits;
  if (sht.readStatus(bits) && bits.resetDetected) {
    Serial.println(F("!! ชิปรีเซ็ตตัวเอง (ไฟตกหรือสายหลวม) กำลังตั้งค่าใหม่"));
    configureSensor();
    recoveryCount++;
  }

  float t = 0.0f;
  float h = 0.0f;
  if (sht.measure(&t, &h)) {
    if (consecutiveFailures > 0) {
      Serial.print(F("  กลับมาปกติแล้วหลังผิดพลาด "));
      Serial.print(consecutiveFailures);
      Serial.println(F(" ครั้ง"));
    }
    consecutiveFailures = 0;

    Serial.print(t, 2);
    Serial.print(F(" C  "));
    Serial.print(h, 2);
    Serial.print(F(" %RH   สถิติ: อ่าน "));
    Serial.print(totalReads);
    Serial.print(F(" ครั้ง ผิด "));
    Serial.print(totalFailures);
    Serial.print(F(" ครั้ง กู้คืน "));
    Serial.print(recoveryCount);
    Serial.println(F(" ครั้ง"));
  } else {
    handleFailure();
  }

  delay(500);
}
