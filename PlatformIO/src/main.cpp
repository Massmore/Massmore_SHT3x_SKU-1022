/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  01_BasicReading - อ่านอุณหภูมิและความชื้นแบบง่ายที่สุด

  บอร์ด  : Massmore SHT3X (SKU-1022)  SHT30 / SHT31 / SHT35  ทั้งรุ่น -B และ -F
  สินค้า : https://www.massmore.shop/products/f9e65fad-f86d-4ac3-9e95-6f575e1d33d3

  การต่อสาย (ESP32 + สาย Qwiic หรือสายจัมเปอร์)
    VCC  -> 3.3V หรือ 5V   (บอร์ดมี regulator กับ level shifter ในตัว)
    GND  -> GND
    SDA  -> GPIO 21
    SCL  -> GPIO 22
    ADDR -> ปล่อยลอย = 0x44   (บัดกรีจัมเปอร์ ADDR = 0x45)

  ตัวอย่างนี้แสดงสิ่งที่ต้องใช้อย่างน้อยที่สุดสามบรรทัด
    1. ประกาศอ็อบเจกต์
    2. begin()
    3. measure()

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>

// ขา I2C ของบอร์ด Massmore ESP32 Breakout
#define PIN_SDA 21
#define PIN_SCL 22

MassmoreSHT3x sht;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;  // รอ Serial บนบอร์ดที่ใช้ USB CDC (S2/S3/C3)
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 01 การอ่านค่าพื้นฐาน"));

  // อาร์กิวเมนต์ตัวที่สองคือรุ่นของชิปตามที่ติ๊กไว้บนซิลค์สกรีนของบอร์ด
  // เปลี่ยนเป็น MASSMORE_SHT3X_VARIANT_SHT30 หรือ _SHT35 ตามของที่มีจริง
  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    Serial.println(F("ตรวจสายไฟ VCC GND SDA SCL และตำแหน่งจัมเปอร์ ADDR"));
    while (true) {
      delay(1000);
    }
  }

  Serial.print(F("พบเซ็นเซอร์ที่ address 0x"));
  Serial.println(sht.getAddress(), HEX);
}

void loop() {
  float temperature = 0.0f;
  float humidity = 0.0f;

  if (sht.measure(&temperature, &humidity)) {
    Serial.print(F("อุณหภูมิ "));
    Serial.print(temperature, 2);
    Serial.print(F(" C   ความชื้น "));
    Serial.print(humidity, 2);
    Serial.println(F(" %RH"));
  } else {
    Serial.print(F("อ่านค่าไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
  }

  delay(1000);
}
