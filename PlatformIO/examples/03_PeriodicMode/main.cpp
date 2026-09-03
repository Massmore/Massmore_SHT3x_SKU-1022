/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  03_PeriodicMode - ให้ชิปวัดเองต่อเนื่อง แล้ว MCU แค่มาดึงค่า

  ต่างจาก single shot ตรงที่ MCU ไม่ต้องสั่งวัดทุกครั้ง ชิปจะวัดเองตามจังหวะ
  ที่ตั้งไว้และเก็บผลล่าสุดไว้ให้ ทำให้ loop() ไม่ต้องรอการวัดเลย
  เหมาะกับงานที่ต้องทำอย่างอื่นไปพร้อมกัน เช่นต่อ WiFi หรือขับจอ

  อัตราที่เลือกได้: 0.5, 1, 2, 4, 10 ครั้งต่อวินาที

  ข้อควรระวัง
    ที่ 10 ครั้ง/วินาที ความละเอียดสูง ตัวชิปจะอุ่นตัวเองประมาณ 0.1-0.2 องศา
    ถ้าต้องการค่าอุณหภูมิที่แม่นที่สุด ให้ใช้ 1 ครั้ง/วินาทีหรือช้ากว่านั้น

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>

#define PIN_SDA 21
#define PIN_SCL 22

MassmoreSHT3x sht;

// ฟังก์ชันนี้จะถูกเรียกอัตโนมัติทุกครั้งที่ update() ดึงค่าใหม่ได้สำเร็จ
void onNewReading(const massmore_sht3x_reading_t &reading) {
  Serial.print(F("["));
  Serial.print(reading.timestampMs);
  Serial.print(F(" ms]  "));
  Serial.print(reading.temperature, 2);
  Serial.print(F(" C  "));
  Serial.print(reading.humidity, 2);
  Serial.print(F(" %RH   (ค่าดิบ 0x"));
  Serial.print(reading.rawTemperature, HEX);
  Serial.print(F(" / 0x"));
  Serial.print(reading.rawHumidity, HEX);
  Serial.println(F(")"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 03 โหมด periodic"));

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }

  sht.setCallback(onNewReading);

  // 2 ครั้งต่อวินาที ความละเอียดสูง
  if (!sht.startPeriodic(MASSMORE_SHT3X_RATE_2_HZ,
                         MASSMORE_SHT3X_REPEATABILITY_HIGH)) {
    Serial.println(F("สั่งโหมด periodic ไม่สำเร็จ"));
    while (true) {
      delay(1000);
    }
  }
  Serial.println(F("เริ่มวัดต่อเนื่องที่ 2 ครั้ง/วินาที  พิมพ์ s เพื่อหยุด, r เพื่อเริ่มใหม่"));
}

void loop() {
  // update() ไม่บล็อก เรียกถี่แค่ไหนก็ได้ ไลบรารีคุมจังหวะให้เอง
  sht.update();

  // ทำงานอย่างอื่นได้ตามปกติ เพราะไม่มีการรอที่ไหนเลย
  if (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 's') {
      sht.stopPeriodic();
      Serial.println(F("หยุดแล้ว"));
    } else if (c == 'r') {
      sht.startPeriodic(MASSMORE_SHT3X_RATE_2_HZ, MASSMORE_SHT3X_REPEATABILITY_HIGH);
      Serial.println(F("เริ่มใหม่แล้ว"));
    }
  }
}
