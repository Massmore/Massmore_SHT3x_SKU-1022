/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  02_Repeatability - เปรียบเทียบความละเอียดสามระดับ และโหมด clock stretching

  ชิป SHT3x วัดได้สามระดับ ยิ่งละเอียดยิ่งใช้เวลานานและ noise ต่ำลง
    ต่ำ    ประมาณ 4 ms    เหมาะกับงานประหยัดพลังงาน
    กลาง   ประมาณ 6 ms
    สูง    ประมาณ 15 ms   ค่าเริ่มต้นของไลบรารี

  clock stretching
    เปิด  ชิปจะดึงสาย SCL ค้างไว้จนวัดเสร็จ โปรแกรมไม่ต้องหน่วงเอง
          แต่จะบล็อกอุปกรณ์อื่นบนบัสช่วงสั้น ๆ
    ปิด   ไลบรารีหน่วงเวลาให้ตาม datasheet (ค่าเริ่มต้น ปลอดภัยกว่า)

  ตัวอย่างนี้วัดซ้ำหลายรอบในแต่ละระดับ แล้วรายงานเวลาที่ใช้กับส่วนเบี่ยงเบน
  ให้เห็นความต่างด้วยตาตัวเอง

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>
#include <math.h>

#define PIN_SDA 21
#define PIN_SCL 22
#define SAMPLES 20

MassmoreSHT3x sht;

// วัดซ้ำ SAMPLES ครั้ง แล้วรายงานค่าเฉลี่ย ส่วนเบี่ยงเบน และเวลาต่อครั้ง
void benchmark(const char *label, massmore_sht3x_repeatability_t repeatability,
               bool clockStretching) {
  sht.setRepeatability(repeatability);
  sht.setClockStretching(clockStretching);

  float sum = 0.0f;
  float sumSquare = 0.0f;
  uint8_t ok = 0;

  uint32_t start = micros();
  for (uint8_t i = 0; i < SAMPLES; i++) {
    float t = 0.0f;
    if (sht.measure(&t, nullptr)) {
      sum += t;
      sumSquare += t * t;
      ok++;
    }
  }
  uint32_t elapsed = micros() - start;

  if (ok == 0) {
    Serial.print(label);
    Serial.println(F("  อ่านไม่สำเร็จเลย"));
    return;
  }

  float mean = sum / ok;
  float variance = (sumSquare / ok) - (mean * mean);
  if (variance < 0.0f) {
    variance = 0.0f;  // กันความคลาดเคลื่อนของเลขทศนิยม
  }

  Serial.print(label);
  Serial.print(F("  เฉลี่ย "));
  Serial.print(mean, 3);
  Serial.print(F(" C   ส่วนเบี่ยงเบน "));
  Serial.print(sqrtf(variance), 4);
  Serial.print(F(" C   เวลาต่อครั้ง "));
  Serial.print(elapsed / SAMPLES / 1000.0f, 2);
  Serial.println(F(" ms"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 02 เปรียบเทียบความละเอียด"));

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }

  Serial.print(F("รุ่นที่ตั้งไว้ "));
  Serial.print(sht.getVariantName());
  Serial.print(F("  สเปกอุณหภูมิ +/-"));
  Serial.print(sht.getTemperatureAccuracy(), 1);
  Serial.print(F(" C  ความชื้น +/-"));
  Serial.print(sht.getHumidityAccuracy(), 1);
  Serial.println(F(" %RH"));
  Serial.println();
}

void loop() {
  Serial.println(F("--- ไม่ใช้ clock stretching ---"));
  benchmark("ต่ำ  ", MASSMORE_SHT3X_REPEATABILITY_LOW, false);
  benchmark("กลาง ", MASSMORE_SHT3X_REPEATABILITY_MEDIUM, false);
  benchmark("สูง  ", MASSMORE_SHT3X_REPEATABILITY_HIGH, false);

  Serial.println(F("--- ใช้ clock stretching ---"));
  benchmark("ต่ำ  ", MASSMORE_SHT3X_REPEATABILITY_LOW, true);
  benchmark("กลาง ", MASSMORE_SHT3X_REPEATABILITY_MEDIUM, true);
  benchmark("สูง  ", MASSMORE_SHT3X_REPEATABILITY_HIGH, true);

  Serial.println();
  delay(5000);
}
