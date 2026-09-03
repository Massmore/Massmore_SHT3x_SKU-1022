/*!
 * @file test_massmore_sht3x.cpp
 * @brief ชุดทดสอบที่รันบนเครื่อง PC ได้เลย ไม่ต้องมีบอร์ด
 *
 * คอมไพล์ไฟล์ .cpp ตัวเดียวกับที่ลงบอร์ดจริง โดยเปลี่ยนเฉพาะ Wire เป็นตัวจำลอง
 * ที่ทำตัวตาม datasheet ทำให้จับบั๊กเชิงตรรกะได้ก่อนเสียบสายจริง
 *
 *   cd PlatformIO/test && make
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license MIT
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "massmore_sht3x_host_shim.h"

#include "../lib/Massmore_SHT3x/src/Massmore_SHT3x.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(bool condition, const char *name) {
  if (condition) {
    g_pass++;
  } else {
    g_fail++;
    printf("  [FAIL] %s\n", name);
  }
}

static void checkNear(float actual, float expected, float tolerance,
                      const char *name) {
  bool ok = fabsf(actual - expected) <= tolerance;
  if (!ok) {
    printf("  [FAIL] %s (ได้ %.4f คาดว่า %.4f +/- %.4f)\n", name, (double)actual,
           (double)expected, (double)tolerance);
    g_fail++;
  } else {
    g_pass++;
  }
}

static void section(const char *name) { printf("\n== %s ==\n", name); }

/*! คืนชิปจำลองกลับสู่สภาพโรงงาน */
static void resetMock() {
  g_mockChip = MockSht3x();
  g_mockMillis = 1000;
  memset(g_mockPinMode, 0, sizeof(g_mockPinMode));
  memset(g_mockPinLevel, 0, sizeof(g_mockPinLevel));
}

/* ========================================================================= */

static void testCrc() {
  section("CRC-8");
  /* เวกเตอร์ทดสอบที่ datasheet ให้มาโดยตรง: CRC(0xBEEF) = 0x92 */
  const uint8_t beef[2] = {0xBE, 0xEF};
  check(MassmoreSHT3x::crc8(beef, 2) == 0x92, "CRC(0xBEEF) ต้องได้ 0x92");

  const uint8_t zeros[2] = {0x00, 0x00};
  check(MassmoreSHT3x::crc8(zeros, 2) == 0x81, "CRC(0x0000) ต้องได้ 0x81");

  const uint8_t ffff[2] = {0xFF, 0xFF};
  check(MassmoreSHT3x::crc8(ffff, 2) == 0xAC, "CRC(0xFFFF) ต้องได้ 0xAC");

  /* CRC ต้องตรงกับที่ฝั่งชิปจำลองคำนวณเสมอ */
  bool allMatch = true;
  for (uint32_t v = 0; v < 65536; v += 137) {
    uint8_t bytes[2] = {(uint8_t)(v >> 8), (uint8_t)(v & 0xFF)};
    if (MassmoreSHT3x::crc8(bytes, 2) != mockCrc8(bytes, 2)) {
      allMatch = false;
      break;
    }
  }
  check(allMatch, "CRC ตรงกับการคำนวณอิสระทุกค่าที่สุ่มตรวจ");
}

static void testConversion() {
  section("การแปลงสัญญาณตาม datasheet");
  /* จุดอ้างอิงจากสูตร T = -45 + 175 * S/(2^16-1) */
  checkNear(MassmoreSHT3x::rawToCelsius(0x0000), -45.0f, 0.001f, "raw 0x0000 = -45 C");
  checkNear(MassmoreSHT3x::rawToCelsius(0xFFFF), 130.0f, 0.001f, "raw 0xFFFF = 130 C");
  checkNear(MassmoreSHT3x::rawToCelsius(0x6666), 25.0f, 0.01f, "raw 0x6666 = 25.0 C พอดี");

  checkNear(MassmoreSHT3x::rawToFahrenheit(0x0000), -49.0f, 0.001f, "raw 0x0000 = -49 F");
  checkNear(MassmoreSHT3x::rawToFahrenheit(0xFFFF), 266.0f, 0.001f, "raw 0xFFFF = 266 F");

  checkNear(MassmoreSHT3x::rawToHumidity(0x0000), 0.0f, 0.001f, "raw 0x0000 = 0 %RH");
  checkNear(MassmoreSHT3x::rawToHumidity(0xFFFF), 100.0f, 0.001f, "raw 0xFFFF = 100 %RH");
  checkNear(MassmoreSHT3x::rawToHumidity(0x8000), 50.0f, 0.01f, "raw 0x8000 ประมาณ 50 %RH");

  /* เซลเซียสกับฟาเรนไฮต์ต้องสอดคล้องกันทุกค่าดิบ */
  bool consistent = true;
  for (uint32_t raw = 0; raw <= 0xFFFF; raw += 251) {
    float c = MassmoreSHT3x::rawToCelsius((uint16_t)raw);
    float f = MassmoreSHT3x::rawToFahrenheit((uint16_t)raw);
    if (fabsf(MassmoreSHT3x::celsiusToFahrenheit(c) - f) > 0.01f) {
      consistent = false;
      break;
    }
  }
  check(consistent, "สูตร C กับ F ให้ผลตรงกัน");

  /* แปลงกลับไปกลับมาต้องได้ค่าเดิม */
  bool roundTrip = true;
  for (uint32_t raw = 0; raw <= 0xFFFF; raw += 97) {
    if (MassmoreSHT3x::celsiusToRaw(MassmoreSHT3x::rawToCelsius((uint16_t)raw)) !=
        (uint16_t)raw) {
      roundTrip = false;
      break;
    }
  }
  check(roundTrip, "celsiusToRaw(rawToCelsius(x)) = x");

  bool roundTripRh = true;
  for (uint32_t raw = 0; raw <= 0xFFFF; raw += 97) {
    if (MassmoreSHT3x::humidityToRaw(MassmoreSHT3x::rawToHumidity((uint16_t)raw)) !=
        (uint16_t)raw) {
      roundTripRh = false;
      break;
    }
  }
  check(roundTripRh, "humidityToRaw(rawToHumidity(x)) = x");

  /* ค่านอกช่วงต้องถูกหนีบไว้ ไม่ให้ล้น */
  check(MassmoreSHT3x::celsiusToRaw(-1000.0f) == 0x0000, "อุณหภูมิต่ำเกินถูกหนีบที่ 0");
  check(MassmoreSHT3x::celsiusToRaw(1000.0f) == 0xFFFF, "อุณหภูมิสูงเกินถูกหนีบที่ FFFF");
  check(MassmoreSHT3x::humidityToRaw(-5.0f) == 0x0000, "ความชื้นติดลบถูกหนีบที่ 0");
  check(MassmoreSHT3x::humidityToRaw(150.0f) == 0xFFFF, "ความชื้นเกิน 100 ถูกหนีบ");
}

static void testCommandTable() {
  section("ตารางคำสั่งตรงกับ datasheet");
  check(MASSMORE_SHT3X_CMD_MEAS_HIGH_STRETCH == 0x2C06, "single shot สูง + stretch");
  check(MASSMORE_SHT3X_CMD_MEAS_MED_STRETCH == 0x2C0D, "single shot กลาง + stretch");
  check(MASSMORE_SHT3X_CMD_MEAS_LOW_STRETCH == 0x2C10, "single shot ต่ำ + stretch");
  check(MASSMORE_SHT3X_CMD_MEAS_HIGH == 0x2400, "single shot สูง");
  check(MASSMORE_SHT3X_CMD_MEAS_MED == 0x240B, "single shot กลาง");
  check(MASSMORE_SHT3X_CMD_MEAS_LOW == 0x2416, "single shot ต่ำ");
  check(MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_HIGH == 0x2032, "periodic 0.5 Hz สูง");
  check(MASSMORE_SHT3X_CMD_PERIODIC_1HZ_HIGH == 0x2130, "periodic 1 Hz สูง");
  check(MASSMORE_SHT3X_CMD_PERIODIC_2HZ_HIGH == 0x2236, "periodic 2 Hz สูง");
  check(MASSMORE_SHT3X_CMD_PERIODIC_4HZ_HIGH == 0x2334, "periodic 4 Hz สูง");
  check(MASSMORE_SHT3X_CMD_PERIODIC_10HZ_HIGH == 0x2737, "periodic 10 Hz สูง");
  check(MASSMORE_SHT3X_CMD_PERIODIC_10HZ_MED == 0x2721, "periodic 10 Hz กลาง");
  check(MASSMORE_SHT3X_CMD_PERIODIC_10HZ_LOW == 0x272A, "periodic 10 Hz ต่ำ");
  check(MASSMORE_SHT3X_CMD_FETCH_DATA == 0xE000, "fetch data");
  check(MASSMORE_SHT3X_CMD_ART == 0x2B32, "ART");
  check(MASSMORE_SHT3X_CMD_BREAK == 0x3093, "break");
  check(MASSMORE_SHT3X_CMD_SOFT_RESET == 0x30A2, "soft reset");
  check(MASSMORE_SHT3X_CMD_HEATER_ENABLE == 0x306D, "heater on");
  check(MASSMORE_SHT3X_CMD_HEATER_DISABLE == 0x3066, "heater off");
  check(MASSMORE_SHT3X_CMD_READ_STATUS == 0xF32D, "read status");
  check(MASSMORE_SHT3X_CMD_CLEAR_STATUS == 0x3041, "clear status");
  check(MASSMORE_SHT3X_CMD_READ_SERIAL == 0x3780, "read serial");
  check(MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_SET == 0x611D, "write alert high set");
  check(MASSMORE_SHT3X_CMD_READ_ALERT_LOW_SET == 0xE102, "read alert low set");
}

static void testStatusDecode() {
  section("การถอด status register");
  massmore_sht3x_status_bits_t bits = MassmoreSHT3x::decodeStatus(0x8010);
  check(bits.alertPending, "0x8010 มี alert pending");
  check(bits.resetDetected, "0x8010 มี reset detected");
  check(!bits.heaterOn, "0x8010 ฮีตเตอร์ปิด");
  check(!bits.commandFailed, "0x8010 ไม่มี command failed");

  bits = MassmoreSHT3x::decodeStatus(0x2000);
  check(bits.heaterOn, "บิต 13 = ฮีตเตอร์เปิด");

  bits = MassmoreSHT3x::decodeStatus(0x0C03);
  check(bits.humidityAlert, "บิต 11 = ความชื้นเลยขอบเขต");
  check(bits.temperatureAlert, "บิต 10 = อุณหภูมิเลยขอบเขต");
  check(bits.commandFailed, "บิต 1 = คำสั่งล้มเหลว");
  check(bits.checksumFailed, "บิต 0 = checksum ผิด");

  /* มาสก์ของบิต reserved ต้องไม่ทับกับบิตที่มีความหมาย */
  uint16_t meaningful = MASSMORE_SHT3X_STATUS_ALERT_PENDING |
                        MASSMORE_SHT3X_STATUS_HEATER_ON |
                        MASSMORE_SHT3X_STATUS_RH_ALERT |
                        MASSMORE_SHT3X_STATUS_T_ALERT |
                        MASSMORE_SHT3X_STATUS_RESET_DETECTED |
                        MASSMORE_SHT3X_STATUS_CMD_FAILED |
                        MASSMORE_SHT3X_STATUS_CRC_FAILED;
  check((MASSMORE_SHT3X_STATUS_RESERVED_MASK & meaningful) == 0,
        "มาสก์ reserved ไม่ทับบิตที่ใช้งาน");
  check((MASSMORE_SHT3X_STATUS_RESERVED_MASK | meaningful) == 0xFFFF,
        "มาสก์ reserved บวกบิตใช้งานครบ 16 บิต");
}

static void testAlertPacking() {
  section("การแพ็ก threshold ของ ALERT");
  /* ตัวอย่างจาก application note: RH 80 %, T 60 องศา
     raw RH = 0xCCCD, raw T = 0x999A
     เก็บ 7 บิตบนของ RH กับ 9 บิตบนของ T -> 0xCC00 | 0x0133 = 0xCD33 */
  uint16_t word = MassmoreSHT3x::packAlertLimit(60.0f, 80.0f);
  check(word == 0xCD33, "RH 80% / T 60C แพ็กได้ 0xCD33");

  float t = 0.0f;
  float h = 0.0f;
  MassmoreSHT3x::unpackAlertLimit(word, &t, &h);
  checkNear(t, 60.0f, 0.5f, "แตกกลับได้อุณหภูมิใกล้ 60 C");
  checkNear(h, 80.0f, 1.0f, "แตกกลับได้ความชื้นใกล้ 80 %RH");

  /* ความละเอียดที่ชิปเก็บได้ประมาณ 0.5 องศา และ 1 %RH */
  bool withinResolution = true;
  for (float temp = -20.0f; temp <= 100.0f; temp += 3.7f) {
    for (float rh = 5.0f; rh <= 95.0f; rh += 7.3f) {
      uint16_t packed = MassmoreSHT3x::packAlertLimit(temp, rh);
      float ot = 0.0f;
      float oh = 0.0f;
      MassmoreSHT3x::unpackAlertLimit(packed, &ot, &oh);
      if (fabsf(ot - temp) > 0.5f || fabsf(oh - rh) > 1.0f) {
        withinResolution = false;
      }
    }
  }
  check(withinResolution, "ค่าที่แตกกลับคลาดไม่เกิน 0.5 C และ 1 %RH");

  /* บิตของอุณหภูมิต้องไม่ล้นไปทับบิตของความชื้น */
  uint16_t maxWord = MassmoreSHT3x::packAlertLimit(130.0f, 0.0f);
  check((maxWord & MASSMORE_SHT3X_ALERT_RH_MASK) == 0,
        "อุณหภูมิสูงสุดไม่ล้นเข้าฟิลด์ความชื้น");
}

static void testDerived() {
  section("ค่าที่คำนวณต่อ");
  /* อากาศอิ่มตัวเต็มที่ จุดน้ำค้างต้องเท่ากับอุณหภูมิ */
  checkNear(MassmoreSHT3x::dewPoint(25.0f, 100.0f), 25.0f, 0.05f,
            "100 %RH จุดน้ำค้าง = อุณหภูมิ");
  /* ค่าอ้างอิงที่ใช้กันทั่วไป: 25 C 50 %RH ได้ประมาณ 13.9 C */
  checkNear(MassmoreSHT3x::dewPoint(25.0f, 50.0f), 13.9f, 0.3f,
            "25 C 50 %RH จุดน้ำค้างประมาณ 13.9 C");
  check(MassmoreSHT3x::dewPoint(25.0f, 30.0f) < MassmoreSHT3x::dewPoint(25.0f, 60.0f),
        "ความชื้นสูงขึ้น จุดน้ำค้างสูงขึ้น");

  /* ความชื้นสัมบูรณ์ที่ 20 C 50 %RH อยู่ราว 8.6 g/m3 */
  checkNear(MassmoreSHT3x::absoluteHumidity(20.0f, 50.0f), 8.6f, 0.4f,
            "20 C 50 %RH ประมาณ 8.6 g/m3");
  check(MassmoreSHT3x::absoluteHumidity(30.0f, 50.0f) >
            MassmoreSHT3x::absoluteHumidity(20.0f, 50.0f),
        "อุณหภูมิสูงขึ้นที่ RH เท่ากัน ความชื้นสัมบูรณ์สูงขึ้น");

  /* ดัชนีความร้อนที่อากาศเย็นต้องใกล้อุณหภูมิจริง */
  checkNear(MassmoreSHT3x::heatIndex(20.0f, 40.0f), 20.0f, 2.0f,
            "20 C 40 %RH heat index ใกล้ของจริง");
  /* 35 C 70 %RH รู้สึกร้อนกว่าจริงมาก */
  check(MassmoreSHT3x::heatIndex(35.0f, 70.0f) > 40.0f,
        "35 C 70 %RH heat index สูงกว่า 40 C");

  check(isnan(MassmoreSHT3x::dewPoint(NAN, 50.0f)), "อินพุต NAN คืน NAN");

  checkNear(MassmoreSHT3x::celsiusToFahrenheit(100.0f), 212.0f, 0.001f, "100 C = 212 F");
  checkNear(MassmoreSHT3x::fahrenheitToCelsius(32.0f), 0.0f, 0.001f, "32 F = 0 C");
}

static void testBasicFlow() {
  section("การใช้งานพื้นฐานกับชิปจำลอง");
  resetMock();
  MassmoreSHT3x sensor(&Wire);

  check(sensor.begin(0x44, MASSMORE_SHT3X_VARIANT_SHT31, 21, 22), "begin() สำเร็จ");
  check(sensor.getAddress() == 0x44, "address ถูกเก็บไว้");
  check(Wire.begun(), "Wire ถูกเปิดใช้งาน");
  check(strcmp(sensor.getVariantName(), "SHT31") == 0, "ชื่อรุ่นถูกต้อง");
  check(sensor.getMode() == MASSMORE_SHT3X_MODE_SINGLE_SHOT, "เริ่มที่โหมด single shot");

  float t = 0.0f;
  float h = 0.0f;
  check(sensor.measure(&t, &h), "measure() สำเร็จ");
  checkNear(t, 25.0f, 0.05f, "อุณหภูมิตรงกับค่าดิบที่ชิปจำลองส่งมา");
  checkNear(h, 50.0f, 0.1f, "ความชื้นตรงกับค่าดิบ");
  check(sensor.getRawTemperature() == 0x6666, "เก็บค่าดิบอุณหภูมิไว้");
  check(sensor.getRawHumidity() == 0x8000, "เก็บค่าดิบความชื้นไว้");
  check(sensor.lastError() == MASSMORE_SHT3X_OK, "ไม่มีข้อผิดพลาดค้าง");

  /* address ที่ไม่ถูกต้องต้องถูกปฏิเสธ */
  MassmoreSHT3x bad(&Wire);
  check(!bad.begin(0x50), "address นอกเหนือ 0x44/0x45 ถูกปฏิเสธ");
  check(bad.lastError() == MASSMORE_SHT3X_ERR_BAD_ARG, "รหัสข้อผิดพลาดถูกต้อง");
}

static void testRepeatabilityCommands() {
  section("การเลือกคำสั่งตามความละเอียดและ clock stretching");
  resetMock();
  MassmoreSHT3x sensor(&Wire);
  sensor.begin();

  sensor.setClockStretching(false);
  sensor.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_LOW);
  sensor.measure((float *)nullptr, (float *)nullptr);
  check(g_mockChip.lastCommand == 0x2416, "ต่ำ ไม่ stretch ใช้ 0x2416");

  sensor.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_MEDIUM);
  sensor.measure((float *)nullptr, (float *)nullptr);
  check(g_mockChip.lastCommand == 0x240B, "กลาง ไม่ stretch ใช้ 0x240B");

  sensor.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_HIGH);
  sensor.measure((float *)nullptr, (float *)nullptr);
  check(g_mockChip.lastCommand == 0x2400, "สูง ไม่ stretch ใช้ 0x2400");

  sensor.setClockStretching(true);
  sensor.measure((float *)nullptr, (float *)nullptr);
  check(g_mockChip.lastCommand == 0x2C06, "สูง + stretch ใช้ 0x2C06");
  check(sensor.getClockStretching(), "ธง clock stretching ถูกเก็บ");

  /* ไม่ stretch ต้องมีการหน่วงเวลาก่อนอ่าน */
  sensor.setClockStretching(false);
  sensor.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_HIGH);
  uint32_t before = millis();
  sensor.measure((float *)nullptr, (float *)nullptr);
  check((millis() - before) >= 15, "หน่วงอย่างน้อย 15 ms สำหรับความละเอียดสูง");
}

static void testPeriodic() {
  section("โหมด periodic");
  resetMock();
  MassmoreSHT3x sensor(&Wire);
  sensor.begin();

  check(sensor.startPeriodic(MASSMORE_SHT3X_RATE_1_HZ,
                             MASSMORE_SHT3X_REPEATABILITY_HIGH),
        "startPeriodic() สำเร็จ");
  check(g_mockChip.lastCommand == 0x2130, "ส่งคำสั่ง 1 Hz ความละเอียดสูง");
  check(sensor.getMode() == MASSMORE_SHT3X_MODE_PERIODIC, "โหมดเปลี่ยนเป็น periodic");

  float t = 0.0f;
  check(sensor.fetchData(&t, (float *)nullptr), "fetchData() ได้ค่า");
  checkNear(t, 25.0f, 0.05f, "ค่าที่ fetch มาถูกต้อง");

  /* ชิปยังวัดไม่เสร็จ จะ NACK ซึ่งต้องแปลเป็น ERR_NOT_READY ไม่ใช่ error จริง */
  g_mockChip.periodicDataReady = false;
  check(!sensor.fetchData(&t, (float *)nullptr), "ยังไม่พร้อม fetch คืน false");
  check(sensor.lastError() == MASSMORE_SHT3X_ERR_NOT_READY,
        "NACK ถูกแปลเป็น ERR_NOT_READY");
  g_mockChip.periodicDataReady = true;

  /* update() ต้องไม่ยิงถี่กว่าอัตราที่ตั้งไว้ */
  uint32_t fetchesBefore = g_mockChip.commandCount;
  sensor.update();
  check(g_mockChip.commandCount == fetchesBefore, "update() ยังไม่ถึงเวลา ไม่ยิงคำสั่ง");
  delay(1100);
  check(sensor.update(), "ครบหนึ่งวินาทีแล้ว update() ได้ค่าใหม่");

  check(sensor.startART(), "startART() สำเร็จ");
  check(g_mockChip.lastCommand == 0x2B32, "ส่งคำสั่ง ART");
  check(sensor.getMode() == MASSMORE_SHT3X_MODE_ART, "โหมดเปลี่ยนเป็น ART");

  check(sensor.stopPeriodic(), "stopPeriodic() สำเร็จ");
  check(g_mockChip.lastCommand == 0x3093, "ส่งคำสั่ง break");
  check(sensor.getMode() == MASSMORE_SHT3X_MODE_SINGLE_SHOT, "กลับสู่ single shot");

  /* fetch ตอนไม่ได้อยู่โหมดต่อเนื่องต้องถูกปฏิเสธ */
  check(!sensor.fetchData(&t, (float *)nullptr), "fetch ผิดโหมดถูกปฏิเสธ");
  check(sensor.lastError() == MASSMORE_SHT3X_ERR_WRONG_MODE, "รหัส ERR_WRONG_MODE");
}

static void testNonBlocking() {
  section("การวัดแบบไม่บล็อก");
  resetMock();
  MassmoreSHT3x sensor(&Wire);
  sensor.begin();
  sensor.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_HIGH);

  check(sensor.startMeasurement(), "startMeasurement() สำเร็จ");
  check(!sensor.isMeasurementReady(), "ทันทีหลังสั่ง ยังไม่พร้อม");

  float t = 0.0f;
  check(!sensor.readMeasurement(&t, (float *)nullptr), "อ่านก่อนเวลาไม่ได้");
  check(sensor.lastError() == MASSMORE_SHT3X_ERR_NOT_READY, "รหัส ERR_NOT_READY");

  delay(20);
  check(sensor.isMeasurementReady(), "ครบเวลาแล้วพร้อมอ่าน");
  check(sensor.readMeasurement(&t, (float *)nullptr), "อ่านผลสำเร็จ");
  checkNear(t, 25.0f, 0.05f, "ค่าที่อ่านได้ถูกต้อง");
  check(!sensor.isMeasurementReady(), "อ่านแล้วธงถูกล้าง");
}

static void testOffsets() {
  section("ค่าชดเชย");
  resetMock();
  MassmoreSHT3x sensor(&Wire);
  sensor.begin();

  sensor.setTemperatureOffset(-2.5f);
  sensor.setHumidityOffset(3.0f);
  check(sensor.getTemperatureOffset() == -2.5f, "เก็บค่าชดเชยอุณหภูมิ");
  check(sensor.getHumidityOffset() == 3.0f, "เก็บค่าชดเชยความชื้น");

  float t = 0.0f;
  float h = 0.0f;
  sensor.measure(&t, &h);
  checkNear(t, 25.0f - 2.5f, 0.05f, "อุณหภูมิถูกชดเชยแล้ว");
  checkNear(h, 53.0f, 0.1f, "ความชื้นถูกชดเชยแล้ว");

  /* ชดเชยแล้วต้องไม่หลุดขอบเขตทางฟิสิกส์ */
  sensor.setHumidityOffset(80.0f);
  sensor.measure((float *)nullptr, &h);
  check(h <= 100.0f, "ความชื้นถูกหนีบไม่ให้เกิน 100 %RH");
  sensor.setHumidityOffset(-80.0f);
  sensor.measure((float *)nullptr, &h);
  check(h >= 0.0f, "ความชื้นถูกหนีบไม่ให้ติดลบ");
}

static void testHeaterAndStatus() {
  section("ฮีตเตอร์และ status register");
  resetMock();
  MassmoreSHT3x sensor(&Wire);
  sensor.begin();

  check(sensor.heaterOn(), "สั่งเปิดฮีตเตอร์สำเร็จ");
  check(g_mockChip.lastCommand == 0x306D, "ใช้คำสั่ง 0x306D");
  check(sensor.isHeaterOn(), "status บอกว่าฮีตเตอร์เปิด");

  check(sensor.heaterOff(), "สั่งปิดฮีตเตอร์สำเร็จ");
  check(g_mockChip.lastCommand == 0x3066, "ใช้คำสั่ง 0x3066");
  check(!sensor.isHeaterOn(), "status บอกว่าฮีตเตอร์ปิด");

  massmore_sht3x_status_bits_t bits;
  check(sensor.readStatus(bits), "อ่าน status แบบถอดฟิลด์ได้");
  check(bits.raw == g_mockChip.status, "ค่าดิบตรงกับชิป");

  /* ค่าเริ่มต้นหลัง reset ต้องมีธง reset แล้วเคลียร์ได้ */
  sensor.softReset();
  check(sensor.readStatus(bits) && bits.resetDetected, "หลัง soft reset มีธง reset");
  check(sensor.clearStatus(), "clearStatus() สำเร็จ");
  check(sensor.readStatus(bits) && !bits.resetDetected, "เคลียร์ธง reset แล้ว");
}

static void testAlertRegisters() {
  section("การเขียนและอ่าน threshold ของ ALERT");
  resetMock();
  MassmoreSHT3x sensor(&Wire);
  sensor.begin();

  massmore_sht3x_alert_limits_t limits;
  limits.highSetTemperature = 40.0f;
  limits.highSetHumidity = 80.0f;
  limits.highClearTemperature = 38.0f;
  limits.highClearHumidity = 75.0f;
  limits.lowClearTemperature = 12.0f;
  limits.lowClearHumidity = 25.0f;
  limits.lowSetTemperature = 10.0f;
  limits.lowSetHumidity = 20.0f;

  check(sensor.setAlertLimits(limits), "เขียน threshold ครบ 4 ชุด");
  check(g_mockChip.alertRegs[0] == MassmoreSHT3x::packAlertLimit(40.0f, 80.0f),
        "ค่าใน high set ตรงกับที่แพ็ก");

  massmore_sht3x_alert_limits_t readBack;
  check(sensor.getAlertLimits(readBack), "อ่าน threshold กลับมาได้");
  checkNear(readBack.highSetTemperature, 40.0f, 0.5f, "high set T ใกล้เคียง");
  checkNear(readBack.highSetHumidity, 80.0f, 1.0f, "high set RH ใกล้เคียง");
  checkNear(readBack.lowSetTemperature, 10.0f, 0.5f, "low set T ใกล้เคียง");
  checkNear(readBack.lowSetHumidity, 20.0f, 1.0f, "low set RH ใกล้เคียง");

  /* setAlertWindow ต้องเว้น hysteresis ให้ถูกทิศ */
  check(sensor.setAlertWindow(10.0f, 40.0f, 20.0f, 80.0f, 1.0f, 2.0f),
        "setAlertWindow() สำเร็จ");
  check(sensor.getAlertLimits(readBack), "อ่านกลับได้");
  check(readBack.lowSetTemperature < readBack.lowClearTemperature,
        "lowSet ต่ำกว่า lowClear");
  check(readBack.highClearTemperature < readBack.highSetTemperature,
        "highClear ต่ำกว่า highSet");
  check(readBack.lowClearTemperature < readBack.highClearTemperature,
        "ลำดับ threshold ทั้งสี่ถูกต้อง");

  check(!sensor.setAlertWindow(40.0f, 10.0f, 20.0f, 80.0f), "ช่วงกลับหัวถูกปฏิเสธ");
  check(sensor.lastError() == MASSMORE_SHT3X_ERR_BAD_ARG, "รหัส ERR_BAD_ARG");

  /* ขา ALERT */
  sensor.setAlertPin(4);
  g_mockPinLevel[4] = LOW;
  check(!sensor.isAlertPinActive(), "ขา ALERT LOW = ไม่เตือน");
  g_mockPinLevel[4] = HIGH;
  check(sensor.isAlertPinActive(), "ขา ALERT HIGH = เตือน");
}

static void testSerialAndVerify() {
  section("ซีเรียลและการตรวจของแท้");
  resetMock();
  MassmoreSHT3x sensor(&Wire);
  sensor.begin();

  uint32_t serial = 0;
  check(sensor.readSerialNumber(&serial), "อ่านซีเรียลสำเร็จ");
  check(serial == 0x0A1B2C3D, "ซีเรียลตรงกับที่ชิปจำลองเก็บไว้");
  check(sensor.getSerialNumber() == 0x0A1B2C3D, "ซีเรียลถูกเก็บไว้ในอ็อบเจกต์");

  /* ชิปแท้ต้องผ่านครบทั้ง 10 ข้อ */
  massmore_sht3x_genuine_t verdict = sensor.verifyChip();
  if (verdict != MASSMORE_SHT3X_GENUINE_PASS) {
    printf("  (ผ่าน %u/10 mask=0x%03X)\n", sensor.getVerifyPassCount(),
           sensor.getVerifyMask());
    for (uint8_t i = 0; i < MASSMORE_SHT3X_CHK_COUNT; i++) {
      if ((sensor.getVerifyMask() & (1u << i)) == 0) {
        printf("  (ข้อที่ไม่ผ่าน: %s)\n", MassmoreSHT3x::getVerifyCheckName(i));
      }
    }
  }
  check(verdict == MASSMORE_SHT3X_GENUINE_PASS, "ชิปแท้ผ่านการตรวจ");
  check(sensor.getVerifyPassCount() == 10, "ผ่านครบ 10 ข้อ");
  check(sensor.getVerifyMask() == MASSMORE_SHT3X_CHK_ALL, "บิตครบทุกข้อ");

  /* ชิปที่ไม่รู้จักคำสั่งอ่านซีเรียล */
  resetMock();
  g_mockChip.supportsSerial = false;
  MassmoreSHT3x fake1(&Wire);
  fake1.begin();
  verdict = fake1.verifyChip();
  check((fake1.getVerifyMask() & MASSMORE_SHT3X_CHK_SERIAL) == 0,
        "จับได้ว่าอ่านซีเรียลไม่ผ่าน");
  check(verdict == MASSMORE_SHT3X_GENUINE_PARTIAL, "ขาดหนึ่งข้อได้ผล PARTIAL");

  /* ชิปที่ทำอะไรไม่ได้หลายอย่าง */
  resetMock();
  g_mockChip.supportsSerial = false;
  g_mockChip.supportsHeater = false;
  g_mockChip.supportsCmdError = false;
  g_mockChip.supportsClearStatus = false;
  MassmoreSHT3x fake2(&Wire);
  fake2.begin();
  verdict = fake2.verifyChip();
  check(verdict == MASSMORE_SHT3X_GENUINE_SUSPECT, "ขาดหลายข้อได้ผล SUSPECT");

  /* ชิปที่ยัดขยะไว้ในบิต reserved = ไม่ใช่ SHT3x แน่นอน */
  resetMock();
  g_mockChip.reservedGarbage = 0x0044;
  MassmoreSHT3x fake3(&Wire);
  fake3.begin();
  verdict = fake3.verifyChip();
  check(verdict == MASSMORE_SHT3X_GENUINE_NOT_SHT3X, "บิต reserved ผิด = ไม่ใช่ SHT3x");

  /* ไม่มีชิปบนบัสเลย */
  resetMock();
  g_mockChip.present = false;
  MassmoreSHT3x missing(&Wire);
  check(!missing.begin(), "ไม่มีชิป begin() ต้องไม่ผ่าน");
  check(missing.verifyChip() == MASSMORE_SHT3X_GENUINE_NOT_SHT3X,
        "ไม่มีชิป = NOT_SHT3X");
}

static void testCrcRejection() {
  section("การปฏิเสธข้อมูลที่ CRC ผิด");
  resetMock();
  MassmoreSHT3x sensor(&Wire);
  sensor.begin();

  g_mockChip.corruptCrc = true;
  float t = 0.0f;
  check(!sensor.measure(&t, (float *)nullptr), "CRC ผิดต้องอ่านไม่ผ่าน");
  check(sensor.lastError() == MASSMORE_SHT3X_ERR_CRC, "รหัส ERR_CRC");

  uint16_t status = 0;
  check(!sensor.readStatus(&status), "status ที่ CRC ผิดถูกปฏิเสธ");
  check(isnan(sensor.readTemperature()), "readTemperature() คืน NAN เมื่ออ่านไม่ได้");

  g_mockChip.corruptCrc = false;
  check(sensor.measure(&t, (float *)nullptr), "CRC กลับมาถูกแล้วอ่านได้ปกติ");
}

static void testResets() {
  section("การรีเซ็ต");
  resetMock();
  MassmoreSHT3x sensor(&Wire);
  sensor.begin();

  check(sensor.softReset(), "softReset() สำเร็จ");
  check(g_mockChip.lastCommand == 0x30A2, "ใช้คำสั่ง 0x30A2");

  check(sensor.generalCallReset(), "generalCallReset() สำเร็จ");

  /* hard reset ต้องปฏิเสธถ้ายังไม่ได้ตั้งขา */
  check(!sensor.hardReset(), "ยังไม่ตั้งขา RST ต้องทำไม่ได้");
  sensor.setResetPin(5);
  check(g_mockPinMode[5] == INPUT_PULLUP, "ตั้งขา RST เป็น input pull-up");
  check(sensor.hardReset(), "hardReset() สำเร็จเมื่อตั้งขาแล้ว");
  check(g_mockPinLevel[5] == LOW, "เคยดึงขา RST ลงจริง");
  check(g_mockPinMode[5] == INPUT_PULLUP, "คืนขาเป็น input pull-up หลังปล่อย");
}

static void testScanAndMeta() {
  section("การสแกนบัสและข้อมูลทั่วไป");
  resetMock();
  uint8_t found[2] = {0, 0};
  check(MassmoreSHT3x::scan(&Wire, found) == 1, "พบชิปหนึ่งตัว");
  check(found[0] == 0x44, "อยู่ที่ address 0x44");

  g_mockChip.present = false;
  check(MassmoreSHT3x::scan(&Wire, found) == 0, "ไม่มีชิป สแกนได้ศูนย์");

  check(strcmp(MassmoreSHT3x::getLibraryVersion(), MASSMORE_SHT3X_VERSION_STRING) == 0,
        "เวอร์ชันไลบรารีตรงกับมาโคร");

  /* ทุกรหัสข้อผิดพลาดต้องมีข้อความ ไม่มีตัวไหนตกหล่น */
  bool allNamed = true;
  for (int i = 0; i <= (int)MASSMORE_SHT3X_ERR_OUT_OF_RANGE; i++) {
    const char *text = MassmoreSHT3x::errorToString((massmore_sht3x_error_t)i);
    if (text == nullptr || strcmp(text, "ข้อผิดพลาดที่ไม่รู้จัก") == 0) {
      allNamed = false;
    }
  }
  check(allNamed, "รหัสข้อผิดพลาดทุกตัวมีคำอธิบาย");

  bool allChecksNamed = true;
  for (uint8_t i = 0; i < MASSMORE_SHT3X_CHK_COUNT; i++) {
    if (strcmp(MassmoreSHT3x::getVerifyCheckName(i), "ไม่ทราบ") == 0) {
      allChecksNamed = false;
    }
  }
  check(allChecksNamed, "ข้อตรวจของ verifyChip ทุกข้อมีชื่อ");

  /* สเปกความแม่นยำต้องต่างกันตามรุ่น */
  MassmoreSHT3x s30(&Wire);
  s30.setVariant(MASSMORE_SHT3X_VARIANT_SHT30);
  MassmoreSHT3x s35(&Wire);
  s35.setVariant(MASSMORE_SHT3X_VARIANT_SHT35);
  check(s35.getHumidityAccuracy() < s30.getHumidityAccuracy(),
        "SHT35 แม่นกว่า SHT30 ด้านความชื้น");
  check(s35.getTemperatureAccuracy() < s30.getTemperatureAccuracy(),
        "SHT35 แม่นกว่า SHT30 ด้านอุณหภูมิ");
}

static void testCallback() {
  section("callback");
  resetMock();
  static int callCount = 0;
  static float lastT = 0.0f;
  callCount = 0;

  struct Local {
    static void onReading(const massmore_sht3x_reading_t &reading) {
      callCount++;
      lastT = reading.temperature;
    }
  };

  MassmoreSHT3x sensor(&Wire);
  sensor.begin();
  sensor.setCallback(Local::onReading);
  sensor.startPeriodic(MASSMORE_SHT3X_RATE_1_HZ);

  delay(1100);
  sensor.update();
  check(callCount == 1, "callback ถูกเรียกหนึ่งครั้ง");
  checkNear(lastT, 25.0f, 0.05f, "callback ได้รับค่าที่ถูกต้อง");

  sensor.update();
  check(callCount == 1, "ยังไม่ถึงรอบถัดไป callback ไม่ถูกเรียกซ้ำ");
}

static void testHeaterSelfTest() {
  section("self test ด้วยฮีตเตอร์");
  resetMock();
  MassmoreSHT3x sensor(&Wire);
  sensor.begin();

  /* ชิปจำลองไม่ได้ทำให้อุณหภูมิขึ้นจริง จึงต้องไม่ผ่าน */
  float rise = 0.0f;
  check(!sensor.runHeaterSelfTest(300, 0.5f, &rise), "อุณหภูมิไม่ขึ้น = ไม่ผ่าน");
  check(sensor.lastError() == MASSMORE_SHT3X_ERR_OUT_OF_RANGE, "รหัส ERR_OUT_OF_RANGE");
  check(!g_mockChip.status || (g_mockChip.status & 0x2000) == 0,
        "ฮีตเตอร์ถูกปิดคืนหลังทดสอบเสมอ");

  /* เกณฑ์ที่ยอมรับค่าใดก็ได้ต้องผ่าน */
  check(sensor.runHeaterSelfTest(300, -100.0f, &rise), "เกณฑ์หลวมแล้วผ่าน");
}

/* ========================================================================= */

int main() {
  printf("Massmore_SHT3x host test  (ไลบรารีเวอร์ชัน %s)\n",
         MassmoreSHT3x::getLibraryVersion());

  testCrc();
  testConversion();
  testCommandTable();
  testStatusDecode();
  testAlertPacking();
  testDerived();
  testBasicFlow();
  testRepeatabilityCommands();
  testPeriodic();
  testNonBlocking();
  testOffsets();
  testHeaterAndStatus();
  testAlertRegisters();
  testSerialAndVerify();
  testCrcRejection();
  testResets();
  testScanAndMeta();
  testCallback();
  testHeaterSelfTest();

  printf("\n----------------------------------------\n");
  printf("ผ่าน %d ข้อ  ไม่ผ่าน %d ข้อ  รวม %d ข้อ\n", g_pass, g_fail, g_pass + g_fail);
  printf("----------------------------------------\n");
  return (g_fail == 0) ? 0 : 1;
}
