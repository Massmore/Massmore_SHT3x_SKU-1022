/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  06_StatusRegister - อ่านและถอด status register ทีละบิต

  status register ขนาด 16 บิต คือหน้าต่างเดียวที่มองเห็นสภาพภายในของชิป
  ใช้ตรวจได้ว่า
    - ชิปเพิ่งรีเซ็ตหรือไฟตกไปหรือเปล่า (bit 4)  ->  ถ้าใช่ ต้องตั้งค่าใหม่ทั้งหมด
    - คำสั่งล่าสุดถูกปฏิเสธไหม (bit 1)
    - checksum ของข้อมูลที่เราเขียนไปผิดไหม (bit 0)  ->  สัญญาณว่าสายมีปัญหา
    - ฮีตเตอร์เปิดอยู่ไหม (bit 13)
    - มี alert ค้างไหม (bit 15, 11, 10)

  เทคนิคที่ใช้ได้จริงในงานเดินยาว ๆ
  ให้เช็ค bit 4 ทุกรอบ ถ้าขึ้นมาแปลว่าชิปรีบูตเอง (ไฟตก, สายหลวม)
  ต้องสั่ง startPeriodic() และตั้ง alert ใหม่ ไม่งั้นจะได้ค่าเก่าค้าง

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>

#define PIN_SDA 21
#define PIN_SCL 22

MassmoreSHT3x sht;

void printBit(const char *name, bool value, const char *meaningWhenSet) {
  Serial.print(F("  "));
  Serial.print(name);
  Serial.print(F(" = "));
  Serial.print(value ? '1' : '0');
  if (value) {
    Serial.print(F("   <- "));
    Serial.print(meaningWhenSet);
  }
  Serial.println();
}

void dumpStatus() {
  massmore_sht3x_status_bits_t bits;
  if (!sht.readStatus(bits)) {
    Serial.print(F("อ่าน status ไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    return;
  }

  Serial.print(F("status = 0x"));
  if (bits.raw < 0x1000) Serial.print('0');
  if (bits.raw < 0x0100) Serial.print('0');
  if (bits.raw < 0x0010) Serial.print('0');
  Serial.print(bits.raw, HEX);
  Serial.print(F("  ("));
  for (int8_t i = 15; i >= 0; i--) {
    Serial.print((bits.raw >> i) & 1);
    if (i == 8) Serial.print(' ');
  }
  Serial.println(F(")"));

  printBit("bit15 alert pending  ", bits.alertPending, "มี alert ค้างอยู่");
  printBit("bit13 heater         ", bits.heaterOn, "ฮีตเตอร์เปิดอยู่");
  printBit("bit11 RH alert       ", bits.humidityAlert, "ความชื้นเลยขอบเขตที่ตั้งไว้");
  printBit("bit10 T alert        ", bits.temperatureAlert, "อุณหภูมิเลยขอบเขตที่ตั้งไว้");
  printBit("bit4  reset detected ", bits.resetDetected, "ชิปเพิ่งรีเซ็ต ต้องตั้งค่าใหม่");
  printBit("bit1  command failed ", bits.commandFailed, "คำสั่งล่าสุดไม่ถูกประมวลผล");
  printBit("bit0  checksum failed", bits.checksumFailed, "checksum ที่เขียนไปผิด ตรวจสาย");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 06 status register"));

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }

  Serial.println(F("\n[1] หลังจ่ายไฟใหม่ ๆ bit4 ต้องขึ้นเป็น 1"));
  sht.softReset();
  delay(5);
  dumpStatus();

  Serial.println(F("[2] สั่ง clearStatus() แล้วธงต้องหายไป"));
  sht.clearStatus();
  dumpStatus();

  Serial.println(F("[3] เปิดฮีตเตอร์ bit13 ต้องขึ้น"));
  sht.heaterOn();
  dumpStatus();
  sht.heaterOff();

  Serial.println(F("จากนี้จะเฝ้าดู bit4 ทุกวินาที ลองถอดสาย VCC แล้วเสียบใหม่"));
  Serial.println(F("แล้วดูว่าโปรแกรมจับได้ว่าชิปรีบูต"));
}

void loop() {
  massmore_sht3x_status_bits_t bits;
  if (!sht.readStatus(bits)) {
    Serial.print(F("ติดต่อชิปไม่ได้: "));
    Serial.println(sht.lastErrorString());
    delay(1000);
    return;
  }

  if (bits.resetDetected) {
    Serial.println(F("!! ตรวจพบว่าชิปรีเซ็ตตัวเอง กำลังตั้งค่าใหม่..."));
    sht.clearStatus();
    // ในงานจริง ให้ตั้ง periodic, alert limit, offset ใหม่ทั้งหมดตรงนี้
  }

  float t = 0.0f;
  float h = 0.0f;
  if (sht.measure(&t, &h)) {
    Serial.print(t, 2);
    Serial.print(F(" C  "));
    Serial.print(h, 2);
    Serial.println(F(" %RH"));
  }
  delay(1000);
}
