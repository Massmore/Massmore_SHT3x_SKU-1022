/*
  14_DataLogger_Advanced - รวมทุกอย่างเข้าด้วยกันเป็นเครื่องบันทึกจริง ๆ

  ตัวอย่างนี้เอาไปต่อยอดเป็นงานจริงได้เลย ประกอบด้วย
    - โหมด periodic + callback  ไม่บล็อก loop()
    - ตัวกรองค่าเฉลี่ยเคลื่อนที่ (moving average) ลด noise
    - เก็บค่าต่ำสุด/สูงสุดตั้งแต่เริ่มทำงาน
    - ตรวจจับค่ากระโดดผิดปกติ (spike) แล้วตัดทิ้ง
    - ส่งออกเป็น CSV ให้เอาไปเปิดใน Excel ได้ทันที
    - คำสั่งผ่าน Serial สำหรับสั่งงานตอนรัน

  คำสั่งที่พิมพ์เข้ามาทาง Serial ได้
    c   ล้างสถิติทั้งหมด
    s   แสดงสรุปสถิติ
    h   เปิด/ปิดฮีตเตอร์ (ไล่ความชื้นที่เกาะ)
    +   เพิ่มอัตราการวัด
    -   ลดอัตราการวัด

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>
#include <math.h>

#define PIN_SDA 21
#define PIN_SCL 22
#define FILTER_SIZE 8
#define SPIKE_THRESHOLD_C 5.0f
#define REPORT_INTERVAL_MS 2000

MassmoreSHT3x sht;

// ตัวกรองค่าเฉลี่ยเคลื่อนที่ ใช้อาเรย์วนรอบ ไม่ใช้ heap
struct MovingAverage {
  float buffer[FILTER_SIZE];
  uint8_t count = 0;
  uint8_t index = 0;
  float sum = 0.0f;

  void add(float value) {
    if (count == FILTER_SIZE) {
      sum -= buffer[index];
    } else {
      count++;
    }
    buffer[index] = value;
    sum += value;
    index = (uint8_t)((index + 1) % FILTER_SIZE);
  }

  float average() const { return (count > 0) ? (sum / count) : NAN; }
  void reset() {
    count = 0;
    index = 0;
    sum = 0.0f;
  }
};

MovingAverage filterT;
MovingAverage filterH;

float minT = NAN, maxT = NAN;
float minH = NAN, maxH = NAN;
float lastAcceptedT = NAN;
uint32_t sampleCount = 0;
uint32_t spikeCount = 0;
uint32_t lastReportMs = 0;
uint8_t rateIndex = 2;  // เริ่มที่ 2 ครั้ง/วินาที

const massmore_sht3x_rate_t RATES[5] = {
    MASSMORE_SHT3X_RATE_0_5_HZ, MASSMORE_SHT3X_RATE_1_HZ,
    MASSMORE_SHT3X_RATE_2_HZ, MASSMORE_SHT3X_RATE_4_HZ,
    MASSMORE_SHT3X_RATE_10_HZ};
const char *const RATE_NAMES[5] = {"0.5", "1", "2", "4", "10"};

void resetStats() {
  filterT.reset();
  filterH.reset();
  minT = maxT = minH = maxH = NAN;
  lastAcceptedT = NAN;
  sampleCount = 0;
  spikeCount = 0;
}

// ถูกเรียกทุกครั้งที่ update() ดึงค่าใหม่ได้
void onReading(const massmore_sht3x_reading_t &reading) {
  // ตัดค่าที่กระโดดผิดปกติทิ้ง (สัญญาณรบกวน หรือสายหลวมชั่วขณะ)
  if (!isnan(lastAcceptedT) &&
      fabsf(reading.temperature - lastAcceptedT) > SPIKE_THRESHOLD_C) {
    spikeCount++;
    return;
  }
  lastAcceptedT = reading.temperature;

  filterT.add(reading.temperature);
  filterH.add(reading.humidity);
  sampleCount++;

  if (isnan(minT) || reading.temperature < minT) minT = reading.temperature;
  if (isnan(maxT) || reading.temperature > maxT) maxT = reading.temperature;
  if (isnan(minH) || reading.humidity < minH) minH = reading.humidity;
  if (isnan(maxH) || reading.humidity > maxH) maxH = reading.humidity;
}

void printSummary() {
  Serial.println(F("\n===== สรุปสถิติ ====="));
  Serial.print(F("จำนวนตัวอย่าง  ")); Serial.println(sampleCount);
  Serial.print(F("ตัดทิ้ง (spike) ")); Serial.println(spikeCount);
  Serial.print(F("อุณหภูมิ  ต่ำสุด ")); Serial.print(minT, 2);
  Serial.print(F("  สูงสุด ")); Serial.print(maxT, 2);
  Serial.print(F("  เฉลี่ย ")); Serial.println(filterT.average(), 2);
  Serial.print(F("ความชื้น  ต่ำสุด ")); Serial.print(minH, 2);
  Serial.print(F("  สูงสุด ")); Serial.print(maxH, 2);
  Serial.print(F("  เฉลี่ย ")); Serial.println(filterH.average(), 2);
  Serial.print(F("เวลาทำงาน ")); Serial.print(millis() / 1000); Serial.println(F(" วินาที"));
  Serial.println(F("=====================\n"));
}

void applyRate() {
  sht.startPeriodic(RATES[rateIndex], MASSMORE_SHT3X_REPEATABILITY_HIGH);
  Serial.print(F("อัตราการวัด "));
  Serial.print(RATE_NAMES[rateIndex]);
  Serial.println(F(" ครั้ง/วินาที"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 14 เครื่องบันทึกข้อมูล"));

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }

  uint32_t serial = 0;
  sht.readSerialNumber(&serial);
  Serial.print(F("อุปกรณ์ซีเรียล 0x"));
  Serial.println(serial, HEX);

  sht.setCallback(onReading);
  resetStats();
  applyRate();

  Serial.println(F("คำสั่ง: c=ล้างสถิติ  s=สรุป  h=ฮีตเตอร์  +/-=ปรับอัตรา"));
  Serial.println(F("\n# CSV เริ่มตรงนี้ คัดลอกไปวางใน Excel ได้เลย"));
  Serial.println(F("millis,temperature_c,humidity_rh,temp_filtered,rh_filtered,dew_point_c"));
  lastReportMs = millis();
}

void loop() {
  sht.update();

  // ออก CSV ตามช่วงเวลาที่กำหนด ไม่ใช่ทุกตัวอย่าง เพื่อไม่ให้ Serial ล้น
  if (millis() - lastReportMs >= REPORT_INTERVAL_MS) {
    lastReportMs = millis();

    float t = sht.getTemperature();
    float h = sht.getHumidity();
    if (!isnan(t)) {
      Serial.print(millis());       Serial.print(',');
      Serial.print(t, 3);           Serial.print(',');
      Serial.print(h, 3);           Serial.print(',');
      Serial.print(filterT.average(), 3); Serial.print(',');
      Serial.print(filterH.average(), 3); Serial.print(',');
      Serial.println(MassmoreSHT3x::dewPoint(t, h), 3);
    }
  }

  if (Serial.available()) {
    char c = (char)Serial.read();
    switch (c) {
    case 'c':
      resetStats();
      Serial.println(F("# ล้างสถิติแล้ว"));
      break;
    case 's':
      printSummary();
      break;
    case 'h':
      if (sht.isHeaterOn()) {
        sht.heaterOff();
        Serial.println(F("# ปิดฮีตเตอร์"));
      } else {
        sht.heaterOn();
        Serial.println(F("# เปิดฮีตเตอร์ ค่าที่วัดได้ช่วงนี้จะไม่ใช่ค่าจริง"));
      }
      break;
    case '+':
      if (rateIndex < 4) {
        rateIndex++;
        applyRate();
      }
      break;
    case '-':
      if (rateIndex > 0) {
        rateIndex--;
        applyRate();
      }
      break;
    default:
      break;
    }
  }
}
