/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  08_SerialNumber_Genuine - อ่านซีเรียลจากโรงงาน และตรวจว่าเป็นชิปแท้

  ทำไมต้องตรวจ
  ในตลาดมีบอร์ดที่เขียนว่า SHT3x แต่ข้างในเป็นชิปอื่นราคาถูกกว่าที่ทำท่า
  ตอบคำสั่งวัดได้เหมือนกัน แต่พอเจอคำสั่งลึก ๆ ของ Sensirion จะตอบไม่ถูก

  verifyChip() ทดสอบพฤติกรรม 10 ข้อที่ของเลียนแบบมักทำไม่ครบ
  รายละเอียดแต่ละข้อดูได้จาก getVerifyCheckName()

  ข้อจำกัดที่ต้องพูดตรง ๆ
    นี่คือการตรวจเชิงพฤติกรรมระดับโปรโตคอล ไม่ใช่ลายเซ็นดิจิทัล
    ตอบได้แค่ว่า "ชิปตัวนี้ทำตัวเหมือน SHT3x แท้ทุกประการหรือไม่"
    บอร์ด Massmore ใช้ชิปแท้จาก Sensirion ประกอบในประเทศไทย

  อีกเรื่องที่ต้องพูดตรง ๆ
    SHT30 SHT31 SHT35 คือซิลิคอนตัวเดียวกัน ต่างกันแค่เกรดความแม่นยำ
    ที่โรงงานคัดไว้ ไม่มีรีจิสเตอร์บอกรุ่น จึงอ่านแยกรุ่นผ่าน I2C ไม่ได้เลย
    ให้ดูช่องติ๊กบนซิลค์สกรีนของบอร์ดแทน

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>

#define PIN_SDA 21
#define PIN_SCL 22

MassmoreSHT3x sht;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 08 ซีเรียลและการตรวจของแท้"));
  Serial.println(F("========================================"));

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }

  // --- ซีเรียลจากโรงงาน ---
  uint32_t serial = 0;
  if (sht.readSerialNumber(&serial)) {
    Serial.print(F("ซีเรียล : 0x"));
    for (int8_t shift = 24; shift >= 0; shift -= 8) {
      uint8_t b = (uint8_t)(serial >> shift);
      if (b < 0x10) Serial.print('0');
      Serial.print(b, HEX);
    }
    Serial.print(F("   (ทศนิยม "));
    Serial.print(serial);
    Serial.println(F(")"));
    Serial.println(F("ซีเรียลนี้ไม่ซ้ำกันในแต่ละชิป ใช้ระบุตัวอุปกรณ์ในระบบได้เลย"));
  } else {
    Serial.print(F("อ่านซีเรียลไม่ได้: "));
    Serial.println(sht.lastErrorString());
    Serial.println(F("ชิปแท้ต้องรู้จักคำสั่ง 0x3780 เสมอ"));
  }

  // --- ตรวจของแท้ ---
  Serial.println(F("\nกำลังตรวจสอบพฤติกรรมของชิป 10 ข้อ..."));
  massmore_sht3x_genuine_t verdict = sht.verifyChip();
  uint16_t mask = sht.getVerifyMask();

  for (uint8_t i = 0; i < MASSMORE_SHT3X_CHK_COUNT; i++) {
    bool passed = (mask & (1u << i)) != 0;
    Serial.print(passed ? F("  [ผ่าน]     ") : F("  [ไม่ผ่าน] "));
    Serial.print(i + 1);
    Serial.print(F(". "));
    Serial.println(MassmoreSHT3x::getVerifyCheckName(i));
  }

  Serial.println(F("----------------------------------------"));
  Serial.print(F("ผ่าน "));
  Serial.print(sht.getVerifyPassCount());
  Serial.print(F(" จาก "));
  Serial.print(MASSMORE_SHT3X_CHK_COUNT);
  Serial.println(F(" ข้อ"));
  Serial.print(F("ผลสรุป : "));
  Serial.println(MassmoreSHT3x::genuineToString(verdict));

  if (verdict == MASSMORE_SHT3X_GENUINE_PASS) {
    Serial.println(F("\nชิปตัวนี้ตอบสนองครบทุกอย่างตามที่ datasheet ของ Sensirion กำหนด"));
    Serial.println(F("ประกอบเป็นบอร์ด by Massmore"));
  } else if (verdict == MASSMORE_SHT3X_GENUINE_PARTIAL) {
    Serial.println(F("\nส่วนใหญ่ผ่าน แต่มีบางข้อที่ไม่ผ่าน"));
    Serial.println(F("อาจเกิดจากสายยาวเกินไป สัญญาณรบกวน หรือแรงดันไฟไม่นิ่ง"));
    Serial.println(F("ลองย้ายมาเสียบสายสั้น ๆ แล้วรันใหม่อีกครั้ง"));
  } else {
    Serial.println(F("\nชิปตัวนี้ไม่ตอบสนองแบบ SHT3x ของแท้"));
  }

  Serial.println(F("\nพิมพ์ r เพื่อตรวจซ้ำ"));
}

void loop() {
  if (Serial.available() && (char)Serial.read() == 'r') {
    setup();
  }
  delay(50);
}
