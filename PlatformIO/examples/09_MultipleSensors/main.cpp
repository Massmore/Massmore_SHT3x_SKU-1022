/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  09_MultipleSensors - ใช้เซ็นเซอร์หลายตัวพร้อมกัน

  ทำได้สองแบบ
    แบบที่ 1  สองตัวบนบัสเดียวกัน ใช้ address ต่างกัน
              ตัวแรก  ADDR ปล่อยลอย        = 0x44
              ตัวที่สอง บัดกรีจัมเปอร์ ADDR  = 0x45
              ต่อพ่วงผ่านสาย Qwiic ได้เลย เพราะบอร์ดมีคอนเนกเตอร์สองฝั่ง

    แบบที่ 2  ใช้คนละบัส (ESP32 มี I2C สองชุด)
              ตัวแรกที่ Wire  (GPIO 21/22)
              ตัวที่สองที่ Wire1 (GPIO 25/26)
              ใช้เมื่อต้องการมากกว่าสองตัว หรือแยกสายยาวออกจากกัน

  ตัวอย่างนี้ทำทั้งสองแบบพร้อมกัน แล้วเทียบค่าให้ดูว่าต่างกันเท่าไร
  (มีประโยชน์มากตอนทำระบบวัดต่างอุณหภูมิ เช่น เข้า-ออกของเครื่องปรับอากาศ)

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>

#define BUS0_SDA 21
#define BUS0_SCL 22
#define BUS1_SDA 25
#define BUS1_SCL 26

/*
  ตรวจว่าชิปตัวนี้มีบัส I2C ชุดที่สองหรือไม่
  ESP32, ESP32-S2, ESP32-S3 และ ESP32-C6 มีสองชุด แต่ ESP32-C3 มีชุดเดียว
  ถ้าไม่มี Wire1 ส่วนของ "ตัว C" จะถูกตัดออกตอนคอมไพล์ทั้งหมด
*/
#if defined(ARDUINO_ARCH_ESP32)
#if defined(SOC_HP_I2C_NUM)
#define MASSMORE_HAS_WIRE1 (SOC_HP_I2C_NUM > 1)
#elif defined(SOC_I2C_NUM)
#define MASSMORE_HAS_WIRE1 (SOC_I2C_NUM > 1)
#else
#define MASSMORE_HAS_WIRE1 0
#endif
#else
#define MASSMORE_HAS_WIRE1 0
#endif

MassmoreSHT3x shtA(&Wire);   // 0x44 บนบัสหลัก
MassmoreSHT3x shtB(&Wire);   // 0x45 บนบัสหลัก
#if MASSMORE_HAS_WIRE1
MassmoreSHT3x shtC(&Wire1);  // 0x44 บนบัสที่สอง
bool hasC = false;
#endif

bool hasA = false;
bool hasB = false;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 09 เซ็นเซอร์หลายตัว"));

  // เปิดบัสหลักครั้งเดียว แล้วให้ทั้งสองอ็อบเจกต์ใช้ร่วมกัน
  Wire.begin(BUS0_SDA, BUS0_SCL);
  Wire.setClock(100000);

  uint8_t found[2] = {0, 0};
  uint8_t count = MassmoreSHT3x::scan(&Wire, found);
  Serial.print(F("สแกนบัสหลักพบ "));
  Serial.print(count);
  Serial.println(F(" ตัว"));
  for (uint8_t i = 0; i < count; i++) {
    Serial.print(F("  0x"));
    Serial.println(found[i], HEX);
  }

  // beginWithExistingBus() ใช้เมื่อเราเปิด Wire เองแล้ว
  hasA = shtA.beginWithExistingBus(MASSMORE_SHT3X_I2C_ADDR_A,
                                   MASSMORE_SHT3X_VARIANT_SHT31);
  hasB = shtB.beginWithExistingBus(MASSMORE_SHT3X_I2C_ADDR_B,
                                   MASSMORE_SHT3X_VARIANT_SHT31);

  Serial.print(F("ตัว A (0x44): "));
  Serial.println(hasA ? F("พร้อมใช้งาน") : F("ไม่พบ"));
  Serial.print(F("ตัว B (0x45): "));
  Serial.println(hasB ? F("พร้อมใช้งาน") : F("ไม่พบ (ต้องบัดกรีจัมเปอร์ ADDR)"));

#if MASSMORE_HAS_WIRE1
  Wire1.begin(BUS1_SDA, BUS1_SCL);
  Wire1.setClock(100000);
  hasC = shtC.beginWithExistingBus(MASSMORE_SHT3X_I2C_ADDR_A,
                                   MASSMORE_SHT3X_VARIANT_SHT31);
  Serial.print(F("ตัว C (บัสที่สอง 0x44): "));
  Serial.println(hasC ? F("พร้อมใช้งาน") : F("ไม่พบ"));
#endif

  if (!hasA && !hasB) {
    Serial.println(F("ไม่พบเซ็นเซอร์เลย ตรวจสายไฟก่อน"));
  }
}

void printOne(const char *label, MassmoreSHT3x &sensor, bool present, float *tOut) {
  if (!present) {
    return;
  }
  float t = 0.0f;
  float h = 0.0f;
  if (sensor.measure(&t, &h)) {
    Serial.print(label);
    Serial.print(F("  "));
    Serial.print(t, 2);
    Serial.print(F(" C  "));
    Serial.print(h, 2);
    Serial.print(F(" %RH"));
    if (tOut != nullptr) {
      *tOut = t;
    }
  } else {
    Serial.print(label);
    Serial.print(F("  อ่านไม่ได้: "));
    Serial.print(sensor.lastErrorString());
  }
  Serial.println();
}

void loop() {
  float tA = 0.0f;
  float tB = 0.0f;

  printOne("A", shtA, hasA, &tA);
  printOne("B", shtB, hasB, &tB);
#if MASSMORE_HAS_WIRE1
  printOne("C", shtC, hasC, nullptr);
#endif

  if (hasA && hasB) {
    Serial.print(F("ผลต่าง A-B = "));
    Serial.print(tA - tB, 3);
    Serial.println(F(" องศา"));
  }

  Serial.println();
  delay(2000);
}
