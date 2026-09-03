/*
  11_NonBlocking - วัดโดยไม่บล็อก loop() แม้แต่มิลลิวินาทีเดียว

  measure() แบบธรรมดาจะ delay() รอชิปวัดเสร็จ (สูงสุด 16 ms)
  ถ้างานของเราต้องตอบสนองเร็ว เช่นขับ LED matrix, อ่านปุ่ม, หรือรัน state machine
  16 ms คือนานเกินไป

  วิธีแก้มีสองทาง
    ทาง A  startMeasurement() -> isMeasurementReady() -> readMeasurement()
           คุมเองทุกขั้น เหมาะกับ single shot ที่อยากวัดตามจังหวะของเราเอง
    ทาง B  startPeriodic() แล้วเรียก update() ถี่ ๆ
           ชิปวัดเองตามจังหวะ เราแค่มาเก็บ ง่ายกว่าและใช้ในงานจริงบ่อยกว่า

  ตัวอย่างนี้ทำทาง A พร้อมนับว่า loop() หมุนได้กี่รอบต่อวินาที
  เพื่อพิสูจน์ว่าไม่มีการรอเกิดขึ้นเลย

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>

#define PIN_SDA 21
#define PIN_SCL 22
#define MEASURE_INTERVAL_MS 500

MassmoreSHT3x sht;

// state machine ของการวัด
enum MeasureState { STATE_IDLE, STATE_WAITING };
MeasureState state = STATE_IDLE;

uint32_t lastStartMs = 0;
uint32_t loopCount = 0;
uint32_t lastReportMs = 0;
uint32_t maxLoopTimeUs = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 11 การวัดแบบไม่บล็อก"));

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }

  sht.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_HIGH);
  lastReportMs = millis();
  Serial.println(F("จะรายงานจำนวนรอบ loop ต่อวินาที และเวลาที่นานที่สุดของหนึ่งรอบ"));
}

void loop() {
  uint32_t loopStartUs = micros();

  switch (state) {
  case STATE_IDLE:
    // ถึงเวลาวัดรอบใหม่หรือยัง
    if (millis() - lastStartMs >= MEASURE_INTERVAL_MS) {
      if (sht.startMeasurement()) {
        lastStartMs = millis();
        state = STATE_WAITING;
      }
    }
    break;

  case STATE_WAITING:
    // เช็คเฉย ๆ ไม่รอ ถ้ายังไม่พร้อมก็ผ่านไปทำอย่างอื่น
    if (sht.isMeasurementReady()) {
      float t = 0.0f;
      float h = 0.0f;
      if (sht.readMeasurement(&t, &h)) {
        Serial.print(F("["));
        Serial.print(millis());
        Serial.print(F("] "));
        Serial.print(t, 2);
        Serial.print(F(" C  "));
        Serial.print(h, 2);
        Serial.println(F(" %RH"));
      } else {
        Serial.print(F("อ่านผลไม่ได้: "));
        Serial.println(sht.lastErrorString());
      }
      state = STATE_IDLE;
    }
    break;
  }

  // งานอื่น ๆ ของเราวางตรงนี้ได้เต็มที่ ไม่มีอะไรมาขวาง
  // ตัวอย่าง: กะพริบไฟ อ่านปุ่ม อัปเดตจอ ส่งข้อมูลขึ้นเน็ต

  loopCount++;
  uint32_t elapsed = micros() - loopStartUs;
  if (elapsed > maxLoopTimeUs) {
    maxLoopTimeUs = elapsed;
  }

  if (millis() - lastReportMs >= 5000) {
    Serial.print(F("   >> loop หมุน "));
    Serial.print(loopCount / 5);
    Serial.print(F(" รอบ/วินาที   รอบที่ช้าที่สุด "));
    Serial.print(maxLoopTimeUs);
    Serial.println(F(" us"));
    loopCount = 0;
    maxLoopTimeUs = 0;
    lastReportMs = millis();
  }
}
