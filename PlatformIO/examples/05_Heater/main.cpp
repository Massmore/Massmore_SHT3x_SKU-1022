/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  05_Heater - ฮีตเตอร์ในตัวชิป ใช้ไล่ความชื้นและตรวจสภาพเซ็นเซอร์

  SHT3x มีฮีตเตอร์เล็ก ๆ อยู่ข้างเซ็นเซอร์ กำลังประมาณ 3.6 mW
  ใช้ได้สองเรื่อง
    1. ไล่ความชื้นที่ควบแน่นเกาะบนชิป หลังอยู่ในที่ชื้นจัดนาน ๆ
       (สำคัญมากกับรุ่น -F กันฝุ่นที่ใช้กลางแจ้ง)
    2. ตรวจว่าเซ็นเซอร์ยังทำงานจริง ถ้าเปิดฮีตเตอร์แล้วอุณหภูมิไม่ขึ้น
       แปลว่าเซ็นเซอร์อาจเสีย หรือค่าที่อ่านมาไม่ได้มาจากการวัดจริง

  ข้อควรระวัง
    - ระหว่างเปิดฮีตเตอร์ ค่าที่อ่านได้จะไม่ใช่ค่าจริงของสิ่งแวดล้อม
    - ห้ามเปิดค้าง ควรเปิดเป็นช่วงสั้น ๆ ไม่เกินไม่กี่วินาที
    - หลังปิดแล้วต้องรอให้ชิปเย็นลงสักพักก่อนใช้ค่าจริง

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
  Serial.println(F("Massmore SHT3x - 05 ฮีตเตอร์ในตัว"));

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }

  Serial.println(F("พิมพ์ h = เปิดฮีตเตอร์, o = ปิด, t = ทดสอบอัตโนมัติ"));
}

// ทดสอบอัตโนมัติ: วัดก่อน เปิดฮีตเตอร์ วัดระหว่างเปิด ปิด แล้วดูว่าเย็นลงไหม
void runAutoTest() {
  Serial.println(F("\n--- เริ่มทดสอบฮีตเตอร์อัตโนมัติ ---"));

  float rise = 0.0f;
  // ไลบรารีมีฟังก์ชันสำเร็จรูปให้แล้ว เปิด 4 วินาที ต้องขึ้นอย่างน้อย 0.5 องศา
  bool passed = sht.runHeaterSelfTest(4000, 0.5f, &rise);

  Serial.print(F("อุณหภูมิขึ้น "));
  Serial.print(rise, 3);
  Serial.print(F(" องศา  ->  "));
  Serial.println(passed ? F("ผ่าน เซ็นเซอร์ตอบสนองปกติ")
                        : F("ไม่ผ่าน อุณหภูมิขึ้นน้อยเกินไป"));

  if (!passed) {
    Serial.println(F("สาเหตุที่เป็นไปได้: มีลมพัดผ่านแรง, ชิปติดกับแผ่นระบายความร้อน,"));
    Serial.println(F("หรือเซ็นเซอร์ทำงานผิดปกติจริง"));
  }

  Serial.println(F("รอให้ชิปเย็นลง 10 วินาที..."));
  for (uint8_t i = 0; i < 10; i++) {
    float t = 0.0f;
    sht.measure(&t, nullptr);
    Serial.print(F("  "));
    Serial.print(t, 2);
    Serial.println(F(" C"));
    delay(1000);
  }
  Serial.println(F("--- จบการทดสอบ ---\n"));
}

void loop() {
  if (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'h') {
      sht.heaterOn();
      Serial.println(F("เปิดฮีตเตอร์แล้ว"));
    } else if (c == 'o') {
      sht.heaterOff();
      Serial.println(F("ปิดฮีตเตอร์แล้ว"));
    } else if (c == 't') {
      runAutoTest();
    }
  }

  float t = 0.0f;
  float h = 0.0f;
  if (sht.measure(&t, &h)) {
    Serial.print(t, 2);
    Serial.print(F(" C   "));
    Serial.print(h, 2);
    Serial.print(F(" %RH   ฮีตเตอร์: "));
    // อ่านสถานะจริงจาก status register ไม่ใช่จำจากคำสั่งที่เคยส่ง
    Serial.println(sht.isHeaterOn() ? F("เปิด") : F("ปิด"));
  }

  delay(1000);
}
