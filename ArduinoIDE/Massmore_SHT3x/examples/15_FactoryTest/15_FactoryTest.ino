/*
  15_FactoryTest - ชุดทดสอบโรงงานสำหรับบอร์ด Massmore SHT3X (SKU-1022)

  รันเองทันทีหลังบูต ไม่ต้องพิมพ์อะไร  พิมพ์ r เพื่อทดสอบซ้ำ

  สิ่งที่ทดสอบ
    ด่านที่ 1  สแกนบัส I2C หา 0x44 หรือ 0x45
    ด่านที่ 2  ตรวจตัวตนของชิปว่าเป็น Sensirion SHT3x ของแท้
    ถ้าสองด่านนี้ไม่ผ่าน จะไม่เข้า RUN TEST เลย เพราะทดสอบต่อไปก็ไม่มีความหมาย
    RUN TEST   ทดสอบทุกความสามารถของชิปอีก 25 หัวข้อ

  การต่อสาย (ค่าปริยายของบอร์ด Massmore ESP32 Breakout)
    VCC -> 3V3 หรือ 5V     SDA -> GPIO 21
    GND -> GND             SCL -> GPIO 22
    ALRT -> GPIO 4  (ต่อหรือไม่ต่อก็ได้ ถ้าไม่ต่อหัวข้อ ALERT_PIN จะขึ้น WARN)
    RST  -> GPIO 5  (ต่อหรือไม่ต่อก็ได้)

  บรรทัดที่ขึ้นต้นด้วย # มีไว้ให้โปรแกรมฝั่งเว็บอ่านอัตโนมัติ
    #RESULT,<ลำดับ>,<ชื่อหัวข้อ>,<PASS|FAIL|WARN>,<รายละเอียด>
    #DEVICE,<addr>,<variant>,<serial>,<genuine>,<ผ่านกี่ข้อจาก10>
    #VERDICT,<PASS|FAIL>,<ผ่าน>,<ไม่ผ่าน>,<เตือน>

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>
#include <Wire.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
   การตั้งค่า
   ------------------------------------------------------------------------- */

#define PIN_SDA 21
#define PIN_SCL 22
#define PIN_ALERT 4   /* ใส่ -1 ถ้าไม่ได้ต่อ */
#define PIN_RST 5     /* ใส่ -1 ถ้าไม่ได้ต่อ */

#define I2C_FREQ_NORMAL 100000UL
#define I2C_FREQ_FAST 400000UL

/* รุ่นที่ติ๊กไว้บนซิลค์สกรีนของบอร์ดที่กำลังทดสอบ
   SHT30 / SHT31 / SHT35 เป็นซิลิคอนตัวเดียวกัน อ่านแยกรุ่นจาก I2C ไม่ได้
   ค่านี้ใช้กำหนดเกณฑ์ความแม่นยำที่ใช้ตรวจและใช้แสดงผลเท่านั้น */
#define BOARD_VARIANT MASSMORE_SHT3X_VARIANT_SHT31

/* ช่วงค่าที่ถือว่าสมเหตุสมผลสำหรับการทดสอบในโรงงาน (อุณหภูมิห้อง) */
#define PLAUSIBLE_T_MIN 5.0f
#define PLAUSIBLE_T_MAX 55.0f
#define PLAUSIBLE_RH_MIN 5.0f
#define PLAUSIBLE_RH_MAX 98.0f

#define FT_DETAIL_LEN 160
#define FT_MAX_TESTS 32

/* -------------------------------------------------------------------------
   โครงสร้างเก็บผล
   ------------------------------------------------------------------------- */

enum FtStatus { FT_PASS = 0, FT_WARN, FT_FAIL };

struct FtResult {
  const char *name;
  FtStatus status;
  char detail[FT_DETAIL_LEN];
};

static FtResult g_results[FT_MAX_TESTS];
static uint8_t g_resultCount = 0;

static MassmoreSHT3x sht;

/* ข้อมูลอุปกรณ์ที่ตรวจพบ */
static uint8_t g_foundAddress = 0;
static uint32_t g_serial = 0;
static massmore_sht3x_genuine_t g_genuine = MASSMORE_SHT3X_GENUINE_UNKNOWN;
static uint8_t g_genuinePassCount = 0;
static uint16_t g_statusAtStart = 0;

/* -------------------------------------------------------------------------
   ตัวช่วย
   ------------------------------------------------------------------------- */

/*!
 * แปลงเลขทศนิยมเป็นข้อความทศนิยมสองตำแหน่ง
 * ไม่ใช้ %f ใน snprintf เพราะ newlib-nano บนบางเป้าหมายตัดการรองรับ float ทิ้ง
 */
static const char *ftF2(char *buffer, size_t size, float value) {
  if (isnan(value)) {
    snprintf(buffer, size, "nan");
    return buffer;
  }
  bool negative = value < 0.0f;
  if (negative) {
    value = -value;
  }
  long scaled = (long)(value * 100.0f + 0.5f);
  snprintf(buffer, size, "%s%ld.%02ld", negative ? "-" : "", scaled / 100,
           scaled % 100);
  return buffer;
}

/*!
 * คัดลอกข้อความลงบัฟเฟอร์โดยไม่ตัดกลางตัวอักษรไทย
 * ตัวอักษรไทยใน UTF-8 ใช้ 3 ไบต์ ถ้าตัดตรงกลางจะขึ้นเป็นสี่เหลี่ยม
 * ฟังก์ชันนี้จะถอยกลับจนพ้นไบต์ต่อเนื่อง (10xxxxxx) ก่อนปิดท้ายด้วย 0
 */
static void ftCopyDetail(char *dest, size_t size, const char *source) {
  if (size == 0) {
    return;
  }
  size_t length = strlen(source);
  if (length < size) {
    memcpy(dest, source, length + 1);
    return;
  }
  size_t cut = size - 1;
  while (cut > 0 && ((unsigned char)source[cut] & 0xC0) == 0x80) {
    cut--;
  }
  memcpy(dest, source, cut);
  dest[cut] = '\0';
}

static void ftRecord(const char *name, FtStatus status, const char *detail) {
  if (g_resultCount >= FT_MAX_TESTS) {
    return;
  }
  FtResult &result = g_results[g_resultCount];
  result.name = name;
  result.status = status;
  ftCopyDetail(result.detail, FT_DETAIL_LEN, detail ? detail : "");

  const char *tag = (status == FT_PASS) ? "[ OK ]"
                    : (status == FT_WARN) ? "[WARN]"
                                          : "[FAIL]";
  const char *word = (status == FT_PASS) ? "PASS"
                     : (status == FT_WARN) ? "WARN"
                                           : "FAIL";

  char line[288];
  snprintf(line, sizeof(line), "%s %02u %-18s %s", tag,
           (unsigned)(g_resultCount + 1), name, result.detail);
  Serial.println(line);

  snprintf(line, sizeof(line), "#RESULT,%u,%s,%s,%s",
           (unsigned)(g_resultCount + 1), name, word, result.detail);
  Serial.println(line);

  g_resultCount++;
}

static void ftBanner(const char *title) {
  Serial.println();
  Serial.print(F("--- "));
  Serial.print(title);
  Serial.println(F(" ---"));
}

/* -------------------------------------------------------------------------
   ด่านที่ 1: สแกนบัส I2C
   ------------------------------------------------------------------------- */

static bool gateBusScan() {
  ftBanner("GATE 1 : สแกนบัส I2C");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_FREQ_NORMAL);
  delay(50);

  uint8_t deviceCount = 0;
  uint8_t addresses[8];
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      if (deviceCount < 8) {
        addresses[deviceCount] = address;
      }
      deviceCount++;
      Serial.print(F("      พบอุปกรณ์ที่ 0x"));
      if (address < 0x10) Serial.print('0');
      Serial.println(address, HEX);
    }
  }

  char detail[FT_DETAIL_LEN];
  if (deviceCount == 0) {
    ftCopyDetail(detail, sizeof(detail),
                 "ไม่พบอุปกรณ์บนบัสเลย ตรวจ VCC GND SDA SCL และสาย Qwiic");
    ftRecord("I2C_SCAN", FT_FAIL, detail);
    return false;
  }

  for (uint8_t i = 0; i < deviceCount && i < 8; i++) {
    if (addresses[i] == MASSMORE_SHT3X_I2C_ADDR_A ||
        addresses[i] == MASSMORE_SHT3X_I2C_ADDR_B) {
      g_foundAddress = addresses[i];
      break;
    }
  }

  if (g_foundAddress == 0) {
    snprintf(detail, sizeof(detail),
             "พบ %u อุปกรณ์ แต่ไม่มีตัวไหนอยู่ที่ 0x44 หรือ 0x45",
             (unsigned)deviceCount);
    ftRecord("I2C_SCAN", FT_FAIL, detail);
    return false;
  }

  snprintf(detail, sizeof(detail), "พบ SHT3x ที่ 0x%02X (อุปกรณ์บนบัสทั้งหมด %u ตัว)",
           g_foundAddress, (unsigned)deviceCount);
  ftRecord("I2C_SCAN", FT_PASS, detail);
  return true;
}

/* -------------------------------------------------------------------------
   ด่านที่ 2: ตรวจตัวตนของชิป
   ------------------------------------------------------------------------- */

static bool gateIdentity() {
  ftBanner("GATE 2 : ตรวจตัวตนของชิป");

  char detail[FT_DETAIL_LEN];

  if (!sht.beginWithExistingBus(g_foundAddress, BOARD_VARIANT)) {
    snprintf(detail, sizeof(detail), "begin() ไม่ผ่าน: %s", sht.lastErrorString());
    ftRecord("IDENTITY", FT_FAIL, detail);
    return false;
  }

  sht.setResetPin(PIN_RST);
  sht.setAlertPin(PIN_ALERT);
  sht.readStatus(&g_statusAtStart);
  sht.readSerialNumber(&g_serial);

  g_genuine = sht.verifyChip();
  g_genuinePassCount = sht.getVerifyPassCount();

  /* พิมพ์ผลรายข้อให้ช่างเห็นว่าตกข้อไหน */
  uint16_t mask = sht.getVerifyMask();
  for (uint8_t i = 0; i < MASSMORE_SHT3X_CHK_COUNT; i++) {
    Serial.print((mask & (1u << i)) ? F("      [ok] ") : F("      [--] "));
    Serial.print(i + 1);
    Serial.print(F(". "));
    Serial.println(MassmoreSHT3x::getVerifyCheckName(i));
  }

  if (g_genuine == MASSMORE_SHT3X_GENUINE_NOT_SHT3X) {
    snprintf(detail, sizeof(detail), "ผ่าน %u/10 ข้อ : %s", (unsigned)g_genuinePassCount,
             MassmoreSHT3x::genuineToString(g_genuine));
    ftRecord("IDENTITY", FT_FAIL, detail);
    return false;
  }

  snprintf(detail, sizeof(detail), "ผ่าน %u/10 ข้อ : %s", (unsigned)g_genuinePassCount,
           MassmoreSHT3x::genuineToString(g_genuine));
  ftRecord("IDENTITY",
           (g_genuine == MASSMORE_SHT3X_GENUINE_PASS) ? FT_PASS : FT_WARN, detail);
  return true;
}

/* -------------------------------------------------------------------------
   หัวข้อทดสอบ
   ------------------------------------------------------------------------- */

static void testSoftReset() {
  char detail[FT_DETAIL_LEN];
  sht.clearStatus();
  if (!sht.softReset()) {
    ftRecord("SOFT_RESET", FT_FAIL, "ส่งคำสั่ง 0x30A2 ไม่สำเร็จ");
    return;
  }
  delay(5);

  massmore_sht3x_status_bits_t bits;
  if (!sht.readStatus(bits)) {
    ftRecord("SOFT_RESET", FT_FAIL, "อ่าน status หลังรีเซ็ตไม่ได้");
    return;
  }
  if (!bits.resetDetected) {
    ftRecord("SOFT_RESET", FT_FAIL, "หลัง soft reset บิต 4 ไม่ขึ้นเป็น 1");
    return;
  }
  snprintf(detail, sizeof(detail), "status หลังรีเซ็ต = 0x%04X มีธง reset ตามที่ควร",
           bits.raw);
  ftRecord("SOFT_RESET", FT_PASS, detail);
}

static void testClearStatus() {
  char detail[FT_DETAIL_LEN];
  if (!sht.clearStatus()) {
    ftRecord("CLEAR_STATUS", FT_FAIL, "ส่งคำสั่ง 0x3041 ไม่สำเร็จ");
    return;
  }
  massmore_sht3x_status_bits_t bits;
  if (!sht.readStatus(bits)) {
    ftRecord("CLEAR_STATUS", FT_FAIL, "อ่าน status หลังเคลียร์ไม่ได้");
    return;
  }
  if (bits.resetDetected || bits.alertPending) {
    snprintf(detail, sizeof(detail), "เคลียร์แล้วยังมีธงค้าง status = 0x%04X", bits.raw);
    ftRecord("CLEAR_STATUS", FT_FAIL, detail);
    return;
  }
  snprintf(detail, sizeof(detail), "ธงถูกล้างหมด status = 0x%04X", bits.raw);
  ftRecord("CLEAR_STATUS", FT_PASS, detail);
}

static void testSerialNumber() {
  char detail[FT_DETAIL_LEN];
  uint32_t serial = 0;
  if (!sht.readSerialNumber(&serial)) {
    snprintf(detail, sizeof(detail), "อ่านคำสั่ง 0x3780 ไม่ได้: %s",
             sht.lastErrorString());
    ftRecord("SERIAL_NUMBER", FT_FAIL, detail);
    return;
  }
  if (serial == 0x00000000UL || serial == 0xFFFFFFFFUL) {
    snprintf(detail, sizeof(detail), "ซีเรียลผิดปกติ = 0x%08lX", (unsigned long)serial);
    ftRecord("SERIAL_NUMBER", FT_FAIL, detail);
    return;
  }

  /* อ่านซ้ำต้องได้ค่าเดิมเสมอ ถ้าเปลี่ยนแปลว่าอ่านมาผิด */
  uint32_t again = 0;
  if (!sht.readSerialNumber(&again) || again != serial) {
    snprintf(detail, sizeof(detail), "อ่านสองครั้งได้ไม่ตรงกัน 0x%08lX vs 0x%08lX",
             (unsigned long)serial, (unsigned long)again);
    ftRecord("SERIAL_NUMBER", FT_FAIL, detail);
    return;
  }

  g_serial = serial;
  snprintf(detail, sizeof(detail), "0x%08lX (อ่านซ้ำได้ค่าเดิม)", (unsigned long)serial);
  ftRecord("SERIAL_NUMBER", FT_PASS, detail);
}

static void testCrcIntegrity() {
  char detail[FT_DETAIL_LEN];
  const uint16_t attempts = 100;
  uint16_t crcErrors = 0;
  uint16_t otherErrors = 0;

  sht.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_LOW);
  for (uint16_t i = 0; i < attempts; i++) {
    if (!sht.measure((float *)nullptr, (float *)nullptr)) {
      if (sht.lastError() == MASSMORE_SHT3X_ERR_CRC) {
        crcErrors++;
      } else {
        otherErrors++;
      }
    }
  }
  sht.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_HIGH);

  snprintf(detail, sizeof(detail), "อ่าน %u ครั้ง CRC ผิด %u ครั้ง ผิดอื่น %u ครั้ง",
           (unsigned)attempts, (unsigned)crcErrors, (unsigned)otherErrors);
  if (crcErrors == 0 && otherErrors == 0) {
    ftRecord("CRC_INTEGRITY", FT_PASS, detail);
  } else if (crcErrors + otherErrors <= 2) {
    ftRecord("CRC_INTEGRITY", FT_WARN, detail);
  } else {
    ftRecord("CRC_INTEGRITY", FT_FAIL, detail);
  }
}

/* ทดสอบ single shot หนึ่งระดับความละเอียด */
static void testSingleShot(const char *name, massmore_sht3x_repeatability_t rep) {
  char detail[FT_DETAIL_LEN];
  char bufT[16];
  char bufH[16];

  sht.setClockStretching(false);
  sht.setRepeatability(rep);

  float t = 0.0f;
  float h = 0.0f;
  uint32_t start = micros();
  bool ok = sht.measure(&t, &h);
  uint32_t elapsed = micros() - start;

  if (!ok) {
    snprintf(detail, sizeof(detail), "วัดไม่สำเร็จ: %s", sht.lastErrorString());
    ftRecord(name, FT_FAIL, detail);
    return;
  }

  snprintf(detail, sizeof(detail), "%s C  %s %%RH  ใช้เวลา %lu us",
           ftF2(bufT, sizeof(bufT), t), ftF2(bufH, sizeof(bufH), h),
           (unsigned long)elapsed);

  if (t < PLAUSIBLE_T_MIN || t > PLAUSIBLE_T_MAX || h < PLAUSIBLE_RH_MIN ||
      h > PLAUSIBLE_RH_MAX) {
    ftRecord(name, FT_WARN, detail);
    return;
  }
  ftRecord(name, FT_PASS, detail);
}

static void testClockStretching() {
  char detail[FT_DETAIL_LEN];
  char bufT[16];

  sht.setClockStretching(true);
  sht.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_HIGH);

  float t = 0.0f;
  float h = 0.0f;
  bool ok = sht.measure(&t, &h);
  sht.setClockStretching(false);

  if (!ok) {
    snprintf(detail, sizeof(detail), "โหมด clock stretching อ่านไม่ได้: %s",
             sht.lastErrorString());
    /* MCU บางรุ่นรองรับ clock stretching ได้ไม่ดี จึงเป็นแค่คำเตือน */
    ftRecord("CLOCK_STRETCH", FT_WARN, detail);
    return;
  }
  snprintf(detail, sizeof(detail), "อ่านได้ %s C ผ่านคำสั่ง 0x2C06",
           ftF2(bufT, sizeof(bufT), t));
  ftRecord("CLOCK_STRETCH", FT_PASS, detail);
}

/* วัดซ้ำหลายครั้งแล้วคืนส่วนเบี่ยงเบนมาตรฐานของอุณหภูมิ */
static float measureNoise(massmore_sht3x_repeatability_t rep, uint8_t samples) {
  sht.setRepeatability(rep);
  float sum = 0.0f;
  float sumSquare = 0.0f;
  uint8_t ok = 0;
  for (uint8_t i = 0; i < samples; i++) {
    float t = 0.0f;
    if (sht.measure(&t, (float *)nullptr)) {
      sum += t;
      sumSquare += t * t;
      ok++;
    }
  }
  if (ok < 2) {
    return NAN;
  }
  float mean = sum / ok;
  float variance = (sumSquare / ok) - (mean * mean);
  return (variance > 0.0f) ? sqrtf(variance) : 0.0f;
}

static void testRepeatabilityNoise() {
  char detail[FT_DETAIL_LEN];
  char bufLow[16];
  char bufHigh[16];

  float noiseLow = measureNoise(MASSMORE_SHT3X_REPEATABILITY_LOW, 30);
  float noiseHigh = measureNoise(MASSMORE_SHT3X_REPEATABILITY_HIGH, 30);
  sht.setRepeatability(MASSMORE_SHT3X_REPEATABILITY_HIGH);

  if (isnan(noiseLow) || isnan(noiseHigh)) {
    ftRecord("NOISE_COMPARE", FT_FAIL, "วัดค่า noise ไม่ได้");
    return;
  }

  snprintf(detail, sizeof(detail), "sd ต่ำ=%s C  sd สูง=%s C",
           ftF2(bufLow, sizeof(bufLow), noiseLow),
           ftF2(bufHigh, sizeof(bufHigh), noiseHigh));

  /* ที่อุณหภูมิห้องนิ่ง ๆ ค่าอาจเท่ากันได้เพราะ noise ต่ำกว่า resolution
     จึงถือว่าผ่านถ้าโหมดสูงไม่ได้แย่กว่าโหมดต่ำอย่างชัดเจน */
  if (noiseHigh <= noiseLow + 0.01f) {
    ftRecord("NOISE_COMPARE", FT_PASS, detail);
  } else {
    ftRecord("NOISE_COMPARE", FT_WARN, detail);
  }
}

static void testPeriodic(const char *name, massmore_sht3x_rate_t rate,
                         uint32_t windowMs, uint16_t expectMin) {
  char detail[FT_DETAIL_LEN];

  if (!sht.startPeriodic(rate, MASSMORE_SHT3X_REPEATABILITY_HIGH)) {
    snprintf(detail, sizeof(detail), "สั่งโหมด periodic ไม่สำเร็จ: %s",
             sht.lastErrorString());
    ftRecord(name, FT_FAIL, detail);
    return;
  }

  uint16_t received = 0;
  uint32_t start = millis();
  while (millis() - start < windowMs) {
    if (sht.update()) {
      received++;
    }
  }
  sht.stopPeriodic();

  snprintf(detail, sizeof(detail), "ได้ %u ชุดใน %lu ms (คาดอย่างน้อย %u)",
           (unsigned)received, (unsigned long)windowMs, (unsigned)expectMin);
  if (received >= expectMin) {
    ftRecord(name, FT_PASS, detail);
  } else if (received > 0) {
    ftRecord(name, FT_WARN, detail);
  } else {
    ftRecord(name, FT_FAIL, detail);
  }
}

static void testArtMode() {
  char detail[FT_DETAIL_LEN];

  if (!sht.startART()) {
    snprintf(detail, sizeof(detail), "สั่ง ART ไม่สำเร็จ: %s", sht.lastErrorString());
    ftRecord("ART_MODE", FT_FAIL, detail);
    return;
  }

  uint16_t received = 0;
  uint32_t start = millis();
  while (millis() - start < 2000) {
    if (sht.update()) {
      received++;
    }
  }
  sht.stopPeriodic();

  snprintf(detail, sizeof(detail), "ART 4 Hz ได้ %u ชุดใน 2000 ms", (unsigned)received);
  if (received >= 5) {
    ftRecord("ART_MODE", FT_PASS, detail);
  } else if (received > 0) {
    ftRecord("ART_MODE", FT_WARN, detail);
  } else {
    ftRecord("ART_MODE", FT_FAIL, detail);
  }
}

static void testBreakCommand() {
  char detail[FT_DETAIL_LEN];

  sht.startPeriodic(MASSMORE_SHT3X_RATE_10_HZ, MASSMORE_SHT3X_REPEATABILITY_LOW);
  delay(300);
  if (!sht.stopPeriodic()) {
    ftRecord("BREAK_CMD", FT_FAIL, "ส่งคำสั่ง 0x3093 ไม่สำเร็จ");
    return;
  }
  delay(50);

  /* หลัง break ต้องกลับมาวัดแบบ single shot ได้ตามปกติ */
  float t = 0.0f;
  if (!sht.measure(&t, (float *)nullptr)) {
    snprintf(detail, sizeof(detail), "หลัง break วัด single shot ไม่ได้: %s",
             sht.lastErrorString());
    ftRecord("BREAK_CMD", FT_FAIL, detail);
    return;
  }
  ftRecord("BREAK_CMD", FT_PASS, "หยุด periodic แล้วกลับมา single shot ได้ปกติ");
}

static void testHeater() {
  char detail[FT_DETAIL_LEN];
  char bufRise[16];

  /* ตรวจว่าบิต 13 ขยับตามคำสั่งก่อน */
  sht.heaterOn();
  bool bitOn = sht.isHeaterOn();
  sht.heaterOff();
  bool bitOff = !sht.isHeaterOn();

  if (!bitOn || !bitOff) {
    snprintf(detail, sizeof(detail), "บิต 13 ไม่ขยับตามคำสั่ง (on=%u off=%u)",
             (unsigned)bitOn, (unsigned)(!bitOff));
    ftRecord("HEATER", FT_FAIL, detail);
    return;
  }

  /* แล้วค่อยดูว่าอุณหภูมิขึ้นจริงไหม */
  float rise = 0.0f;
  bool rose = sht.runHeaterSelfTest(3000, 0.3f, &rise);

  snprintf(detail, sizeof(detail), "บิต 13 ทำงานถูกต้อง อุณหภูมิขึ้น %s องศา",
           ftF2(bufRise, sizeof(bufRise), rise));
  if (rose) {
    ftRecord("HEATER", FT_PASS, detail);
  } else {
    /* ลมพัดแรงหรือชิปติดแผ่นระบายความร้อนก็ทำให้ขึ้นน้อยได้ ไม่ถือว่าบอร์ดเสีย */
    ftRecord("HEATER", FT_WARN, detail);
  }
}

static void testAlertLimits() {
  char detail[FT_DETAIL_LEN];

  massmore_sht3x_alert_limits_t wrote;
  wrote.highSetTemperature = 45.0f;
  wrote.highSetHumidity = 85.0f;
  wrote.highClearTemperature = 42.0f;
  wrote.highClearHumidity = 80.0f;
  wrote.lowClearTemperature = 12.0f;
  wrote.lowClearHumidity = 25.0f;
  wrote.lowSetTemperature = 8.0f;
  wrote.lowSetHumidity = 20.0f;

  if (!sht.setAlertLimits(wrote)) {
    snprintf(detail, sizeof(detail), "เขียน threshold ไม่สำเร็จ: %s",
             sht.lastErrorString());
    ftRecord("ALERT_LIMITS", FT_FAIL, detail);
    return;
  }

  massmore_sht3x_alert_limits_t readBack;
  if (!sht.getAlertLimits(readBack)) {
    snprintf(detail, sizeof(detail), "อ่าน threshold กลับไม่ได้: %s",
             sht.lastErrorString());
    ftRecord("ALERT_LIMITS", FT_FAIL, detail);
    return;
  }

  /* ชิปเก็บได้ความละเอียดประมาณ 0.5 องศา และ 1 %RH จึงต้องเผื่อไว้ */
  bool ok = fabsf(readBack.highSetTemperature - wrote.highSetTemperature) <= 0.6f &&
            fabsf(readBack.highSetHumidity - wrote.highSetHumidity) <= 1.2f &&
            fabsf(readBack.highClearTemperature - wrote.highClearTemperature) <= 0.6f &&
            fabsf(readBack.lowClearTemperature - wrote.lowClearTemperature) <= 0.6f &&
            fabsf(readBack.lowSetTemperature - wrote.lowSetTemperature) <= 0.6f &&
            fabsf(readBack.lowSetHumidity - wrote.lowSetHumidity) <= 1.2f;

  char bufA[16];
  char bufB[16];
  snprintf(detail, sizeof(detail), "เขียน/อ่านครบ 4 ชุด highSet %s C คืนมา %s C",
           ftF2(bufA, sizeof(bufA), wrote.highSetTemperature),
           ftF2(bufB, sizeof(bufB), readBack.highSetTemperature));
  ftRecord("ALERT_LIMITS", ok ? FT_PASS : FT_FAIL, detail);
}

static void testAlertPin() {
  char detail[FT_DETAIL_LEN];

  if (PIN_ALERT < 0) {
    ftRecord("ALERT_PIN", FT_WARN, "ไม่ได้ต่อขา ALRT ข้ามการทดสอบ");
    return;
  }

  /* ตั้งขอบเขตให้แคบจนค่าปัจจุบันหลุดออกไปแน่นอน แล้วดูว่าขาขึ้นไหม */
  float t = 0.0f;
  float h = 0.0f;
  if (!sht.measure(&t, &h)) {
    ftRecord("ALERT_PIN", FT_FAIL, "วัดค่าปัจจุบันไม่ได้");
    return;
  }

  sht.setAlertWindow(t + 20.0f, t + 30.0f, h + 20.0f, h + 30.0f, 1.0f, 2.0f);
  sht.clearStatus();
  sht.startPeriodic(MASSMORE_SHT3X_RATE_4_HZ, MASSMORE_SHT3X_REPEATABILITY_LOW);

  bool wentHigh = false;
  uint32_t start = millis();
  while (millis() - start < 1500) {
    sht.update();
    if (sht.isAlertPinActive()) {
      wentHigh = true;
      break;
    }
  }

  massmore_sht3x_status_bits_t bits;
  bool flagSet = sht.readStatus(bits) && (bits.temperatureAlert || bits.humidityAlert);

  sht.stopPeriodic();
  /* คืนขอบเขตให้กว้างไว้ ไม่ให้บอร์ดเตือนค้างหลังทดสอบเสร็จ */
  sht.setAlertWindow(-30.0f, 120.0f, 1.0f, 99.0f, 1.0f, 2.0f);
  sht.clearStatus();

  snprintf(detail, sizeof(detail), "ขา ALRT %s, ธงใน status %s",
           wentHigh ? "ขึ้น HIGH" : "ไม่ขึ้น", flagSet ? "ขึ้น" : "ไม่ขึ้น");

  if (wentHigh && flagSet) {
    ftRecord("ALERT_PIN", FT_PASS, detail);
  } else if (flagSet) {
    /* ธงขึ้นแต่ขาไม่ขึ้น = ตรรกะในชิปทำงาน แต่สายขา ALRT อาจไม่ได้ต่อ */
    ftRecord("ALERT_PIN", FT_WARN, detail);
  } else {
    ftRecord("ALERT_PIN", FT_FAIL, detail);
  }
}

static void testCommandErrorFlag() {
  char detail[FT_DETAIL_LEN];

  sht.clearStatus();
  sht.sendCommand(0x3999); /* คำสั่งที่ไม่มีในตารางของ datasheet */
  delay(2);

  massmore_sht3x_status_bits_t bits;
  if (!sht.readStatus(bits)) {
    ftRecord("CMD_ERROR_FLAG", FT_FAIL, "อ่าน status ไม่ได้");
    return;
  }

  snprintf(detail, sizeof(detail), "หลังส่งคำสั่ง 0x3999 status = 0x%04X", bits.raw);
  ftRecord("CMD_ERROR_FLAG", bits.commandFailed ? FT_PASS : FT_FAIL, detail);
  sht.clearStatus();
}

static void testWriteChecksumFlag() {
  char detail[FT_DETAIL_LEN];

  sht.clearStatus();

  /* เขียน alert limit พร้อม CRC ที่ผิดโดยตั้งใจ ชิปต้องตั้งบิต 0 */
  Wire.beginTransmission(g_foundAddress);
  Wire.write(0x61);
  Wire.write(0x1D);
  Wire.write(0xAA);
  Wire.write(0x55);
  Wire.write(0x00); /* CRC ที่ถูกต้องของ 0xAA55 ไม่ใช่ 0x00 */
  uint8_t transmissionResult = Wire.endTransmission();
  delay(2);

  massmore_sht3x_status_bits_t bits;
  if (!sht.readStatus(bits)) {
    ftRecord("WRITE_CRC_FLAG", FT_FAIL, "อ่าน status ไม่ได้");
    return;
  }

  snprintf(detail, sizeof(detail), "ส่ง CRC ผิดตั้งใจ (ack=%u) status = 0x%04X",
           (unsigned)transmissionResult, bits.raw);
  ftRecord("WRITE_CRC_FLAG", bits.checksumFailed ? FT_PASS : FT_WARN, detail);

  sht.clearStatus();
  /* คืนค่า alert limit ให้กว้างเหมือนเดิม */
  sht.setAlertWindow(-30.0f, 120.0f, 1.0f, 99.0f, 1.0f, 2.0f);
  sht.clearStatus();
}

static void testGeneralCallReset() {
  char detail[FT_DETAIL_LEN];

  sht.clearStatus();
  if (!sht.generalCallReset()) {
    ftRecord("GENERAL_CALL", FT_FAIL, "ส่ง general call ไม่สำเร็จ");
    return;
  }
  delay(5);

  massmore_sht3x_status_bits_t bits;
  if (!sht.readStatus(bits)) {
    ftRecord("GENERAL_CALL", FT_FAIL, "หลัง general call อ่าน status ไม่ได้");
    return;
  }
  snprintf(detail, sizeof(detail), "status หลัง general call = 0x%04X", bits.raw);
  ftRecord("GENERAL_CALL", bits.resetDetected ? FT_PASS : FT_WARN, detail);
  sht.clearStatus();
}

static void testHardReset() {
  if (PIN_RST < 0) {
    ftRecord("HARD_RESET", FT_WARN, "ไม่ได้ต่อขา RST ข้ามการทดสอบ");
    return;
  }

  char detail[FT_DETAIL_LEN];
  sht.clearStatus();
  if (!sht.hardReset()) {
    ftRecord("HARD_RESET", FT_FAIL, "สั่ง hard reset ไม่ได้");
    return;
  }
  delay(5);

  massmore_sht3x_status_bits_t bits;
  if (!sht.readStatus(bits)) {
    ftRecord("HARD_RESET", FT_FAIL, "หลัง hard reset ติดต่อชิปไม่ได้");
    return;
  }
  snprintf(detail, sizeof(detail), "status หลังกระตุกขา RST = 0x%04X", bits.raw);
  /* ถ้าไม่ได้ต่อขา RST จริง ธงจะไม่ขึ้น ซึ่งไม่ใช่ความผิดของบอร์ด */
  ftRecord("HARD_RESET", bits.resetDetected ? FT_PASS : FT_WARN, detail);
  sht.clearStatus();
}

static void testPlausibility() {
  char detail[FT_DETAIL_LEN];
  char bufT[16];
  char bufH[16];
  char bufD[16];

  float t = 0.0f;
  float h = 0.0f;
  if (!sht.measure(&t, &h)) {
    ftRecord("PLAUSIBILITY", FT_FAIL, "วัดค่าไม่ได้");
    return;
  }
  float dew = MassmoreSHT3x::dewPoint(t, h);

  snprintf(detail, sizeof(detail), "%s C  %s %%RH  จุดน้ำค้าง %s C",
           ftF2(bufT, sizeof(bufT), t), ftF2(bufH, sizeof(bufH), h),
           ftF2(bufD, sizeof(bufD), dew));

  bool inRange = t >= PLAUSIBLE_T_MIN && t <= PLAUSIBLE_T_MAX &&
                 h >= PLAUSIBLE_RH_MIN && h <= PLAUSIBLE_RH_MAX;
  /* จุดน้ำค้างต้องไม่สูงกว่าอุณหภูมิจริง ถ้าสูงกว่าแปลว่าค่าใดค่าหนึ่งผิด */
  bool physical = dew <= t + 0.5f;

  if (inRange && physical) {
    ftRecord("PLAUSIBILITY", FT_PASS, detail);
  } else if (physical) {
    ftRecord("PLAUSIBILITY", FT_WARN, detail);
  } else {
    ftRecord("PLAUSIBILITY", FT_FAIL, detail);
  }
}

static void testStability() {
  char detail[FT_DETAIL_LEN];
  char bufT[16];
  char bufH[16];

  const uint8_t samples = 20;
  float minT = NAN, maxT = NAN, minH = NAN, maxH = NAN;
  uint8_t ok = 0;

  for (uint8_t i = 0; i < samples; i++) {
    float t = 0.0f;
    float h = 0.0f;
    if (!sht.measure(&t, &h)) {
      continue;
    }
    ok++;
    if (isnan(minT) || t < minT) minT = t;
    if (isnan(maxT) || t > maxT) maxT = t;
    if (isnan(minH) || h < minH) minH = h;
    if (isnan(maxH) || h > maxH) maxH = h;
    delay(20);
  }

  if (ok < samples / 2) {
    snprintf(detail, sizeof(detail), "อ่านสำเร็จแค่ %u จาก %u ครั้ง", (unsigned)ok,
             (unsigned)samples);
    ftRecord("STABILITY", FT_FAIL, detail);
    return;
  }

  float spreadT = maxT - minT;
  float spreadH = maxH - minH;
  snprintf(detail, sizeof(detail), "%u ครั้ง ช่วง T %s C  ช่วง RH %s %%", (unsigned)ok,
           ftF2(bufT, sizeof(bufT), spreadT), ftF2(bufH, sizeof(bufH), spreadH));

  /* ในอากาศนิ่ง ค่าไม่ควรแกว่งเกิน 0.5 องศา หรือ 3 %RH ในเวลาไม่ถึงวินาที */
  if (spreadT <= 0.5f && spreadH <= 3.0f) {
    ftRecord("STABILITY", FT_PASS, detail);
  } else if (spreadT <= 2.0f && spreadH <= 10.0f) {
    ftRecord("STABILITY", FT_WARN, detail);
  } else {
    ftRecord("STABILITY", FT_FAIL, detail);
  }
}

static void testBusSpeed() {
  char detail[FT_DETAIL_LEN];

  Wire.setClock(I2C_FREQ_FAST);
  delay(5);
  uint8_t okFast = 0;
  for (uint8_t i = 0; i < 10; i++) {
    if (sht.measure((float *)nullptr, (float *)nullptr)) {
      okFast++;
    }
  }

  Wire.setClock(I2C_FREQ_NORMAL);
  delay(5);
  uint8_t okNormal = 0;
  for (uint8_t i = 0; i < 10; i++) {
    if (sht.measure((float *)nullptr, (float *)nullptr)) {
      okNormal++;
    }
  }

  snprintf(detail, sizeof(detail), "400 kHz ผ่าน %u/10, 100 kHz ผ่าน %u/10",
           (unsigned)okFast, (unsigned)okNormal);

  if (okNormal == 10 && okFast == 10) {
    ftRecord("BUS_SPEED", FT_PASS, detail);
  } else if (okNormal == 10) {
    /* 400 kHz ต้องการสายสั้นและ pull-up ที่เหมาะสม ไม่ผ่านไม่ได้แปลว่าบอร์ดเสีย */
    ftRecord("BUS_SPEED", FT_WARN, detail);
  } else {
    ftRecord("BUS_SPEED", FT_FAIL, detail);
  }
}

static void testAccuracySpec() {
  char detail[FT_DETAIL_LEN];
  char bufT[16];
  char bufH[16];

  snprintf(detail, sizeof(detail), "%s สเปก +/-%s C, +/-%s %%RH", sht.getVariantName(),
           ftF2(bufT, sizeof(bufT), sht.getTemperatureAccuracy()),
           ftF2(bufH, sizeof(bufH), sht.getHumidityAccuracy()));
  ftRecord("VARIANT_SPEC", FT_PASS, detail);
}

static void testOffsetApi() {
  char detail[FT_DETAIL_LEN];
  char buf[16];

  float base = 0.0f;
  if (!sht.measure(&base, (float *)nullptr)) {
    ftRecord("OFFSET_API", FT_FAIL, "วัดค่าฐานไม่ได้");
    return;
  }

  sht.setTemperatureOffset(-3.0f);
  float shifted = 0.0f;
  bool ok = sht.measure(&shifted, (float *)nullptr);
  sht.setTemperatureOffset(0.0f);

  if (!ok) {
    ftRecord("OFFSET_API", FT_FAIL, "วัดหลังใส่ offset ไม่ได้");
    return;
  }

  float delta = base - shifted;
  snprintf(detail, sizeof(detail), "ใส่ offset -3.00 C แล้วค่าลดลง %s C",
           ftF2(buf, sizeof(buf), delta));
  /* เผื่ออุณหภูมิจริงขยับระหว่างสองครั้งไว้ 0.5 องศา */
  ftRecord("OFFSET_API", (fabsf(delta - 3.0f) <= 0.5f) ? FT_PASS : FT_FAIL, detail);
}

/* -------------------------------------------------------------------------
   รายงานสรุป
   ------------------------------------------------------------------------- */

static void printSummary(bool gatesPassed) {
  uint8_t pass = 0;
  uint8_t warn = 0;
  uint8_t fail = 0;
  for (uint8_t i = 0; i < g_resultCount; i++) {
    if (g_results[i].status == FT_PASS) pass++;
    else if (g_results[i].status == FT_WARN) warn++;
    else fail++;
  }

  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F("  รายงานสรุป Massmore SHT3X (SKU-1022)"));
  Serial.println(F("=========================================================="));

  Serial.println(F("\n[ อุปกรณ์ที่ตรวจพบ ]"));
  Serial.print(F("  I2C address     : 0x"));
  if (g_foundAddress < 0x10) Serial.print('0');
  Serial.println(g_foundAddress, HEX);
  Serial.print(F("  รุ่นที่ตั้งไว้     : "));
  Serial.println(sht.getVariantName());
  Serial.print(F("  ซีเรียล         : 0x"));
  for (int8_t shift = 24; shift >= 0; shift -= 8) {
    uint8_t b = (uint8_t)(g_serial >> shift);
    if (b < 0x10) Serial.print('0');
    Serial.print(b, HEX);
  }
  Serial.println();
  Serial.print(F("  status ตอนเริ่ม  : 0x"));
  Serial.println(g_statusAtStart, HEX);
  Serial.print(F("  ผลตรวจของแท้     : "));
  Serial.print(MassmoreSHT3x::genuineToString(g_genuine));
  Serial.print(F("  ("));
  Serial.print(g_genuinePassCount);
  Serial.println(F("/10)"));
  Serial.print(F("  ไลบรารีเวอร์ชัน   : "));
  Serial.println(MassmoreSHT3x::getLibraryVersion());
  Serial.print(F("  เฟิร์มแวร์สร้างเมื่อ: "));
  Serial.print(F(__DATE__));
  Serial.print(' ');
  Serial.println(F(__TIME__));

  Serial.println(F("\n[ ผลรายหัวข้อ ]"));
  for (uint8_t i = 0; i < g_resultCount; i++) {
    const char *tag = (g_results[i].status == FT_PASS) ? "[ OK ]"
                      : (g_results[i].status == FT_WARN) ? "[WARN]"
                                                         : "[FAIL]";
    char line[288];
    snprintf(line, sizeof(line), "  %s %02u %-18s %s", tag, (unsigned)(i + 1),
             g_results[i].name, g_results[i].detail);
    Serial.println(line);
  }

  Serial.println(F("\n[ สรุป ]"));
  Serial.print(F("  ผ่าน "));
  Serial.print(pass);
  Serial.print(F("   เตือน "));
  Serial.print(warn);
  Serial.print(F("   ไม่ผ่าน "));
  Serial.println(fail);

  if (fail > 0) {
    Serial.println(F("\n  หัวข้อที่ไม่ผ่าน:"));
    for (uint8_t i = 0; i < g_resultCount; i++) {
      if (g_results[i].status == FT_FAIL) {
        Serial.print(F("    - "));
        Serial.print(g_results[i].name);
        Serial.print(F(" : "));
        Serial.println(g_results[i].detail);
      }
    }
  }
  if (warn > 0) {
    Serial.println(F("\n  หัวข้อที่เตือน (ไม่ถือว่าบอร์ดเสีย):"));
    for (uint8_t i = 0; i < g_resultCount; i++) {
      if (g_results[i].status == FT_WARN) {
        Serial.print(F("    - "));
        Serial.print(g_results[i].name);
        Serial.print(F(" : "));
        Serial.println(g_results[i].detail);
      }
    }
  }

  bool overallPass = (fail == 0) && gatesPassed;
  Serial.println();
  Serial.println(overallPass ? F("  >>> ผลรวม: ผ่าน  บอร์ดนี้ใช้งานได้ปกติ <<<")
                             : F("  >>> ผลรวม: ไม่ผ่าน  ดูหัวข้อที่ไม่ผ่านด้านบน <<<"));
  Serial.println(F("=========================================================="));

  char line[288];
  snprintf(line, sizeof(line), "#DEVICE,0x%02X,%s,0x%08lX,%s,%u", g_foundAddress,
           sht.getVariantName(), (unsigned long)g_serial,
           MassmoreSHT3x::genuineToString(g_genuine), (unsigned)g_genuinePassCount);
  Serial.println(line);

  snprintf(line, sizeof(line), "#VERDICT,%s,%u,%u,%u", overallPass ? "PASS" : "FAIL",
           (unsigned)pass, (unsigned)fail, (unsigned)warn);
  Serial.println(line);

  Serial.println(F("\nพิมพ์ r แล้วกด Enter เพื่อทดสอบซ้ำ"));
}

/* -------------------------------------------------------------------------
   ตัวหลัก
   ------------------------------------------------------------------------- */

static void runFactoryTest() {
  g_resultCount = 0;
  g_foundAddress = 0;
  g_serial = 0;
  g_genuine = MASSMORE_SHT3X_GENUINE_UNKNOWN;
  g_genuinePassCount = 0;
  g_statusAtStart = 0;

  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F("  Massmore SHT3X (SKU-1022) - Factory Test"));
  Serial.println(F("  Temperature & Humidity Sensor  |  Sensirion SHT3x-DIS"));
  Serial.println(F("=========================================================="));
  Serial.print(F("  I2C  SDA=GPIO"));
  Serial.print(PIN_SDA);
  Serial.print(F("  SCL=GPIO"));
  Serial.print(PIN_SCL);
  Serial.print(F("  ALRT=GPIO"));
  Serial.print(PIN_ALERT);
  Serial.print(F("  RST=GPIO"));
  Serial.println(PIN_RST);

  if (!gateBusScan()) {
    Serial.println(F("\n  หยุดการทดสอบ เพราะไม่พบเซ็นเซอร์บนบัส"));
    Serial.println(F("  สิ่งที่ต้องตรวจ"));
    Serial.println(F("    1. VCC ต่อกับ 3V3 หรือ 5V แล้วหรือยัง"));
    Serial.println(F("    2. GND ร่วมกันหรือยัง"));
    Serial.println(F("    3. SDA เข้า GPIO21 และ SCL เข้า GPIO22 ถูกด้านหรือไม่"));
    Serial.println(F("    4. สาย Qwiic เสียบแน่นทั้งสองหัวหรือไม่"));
    Serial.println(F("    5. ถ้าบัดกรีจัมเปอร์ ADDR ไว้ ต้องหาที่ 0x45"));
    printSummary(false);
    return;
  }

  if (!gateIdentity()) {
    Serial.println(F("\n  หยุดการทดสอบ เพราะชิปไม่ตอบสนองแบบ SHT3x"));
    Serial.println(F("  มีอุปกรณ์ตอบที่ address แต่ไม่ใช่ SHT3x หรือชิปเสีย"));
    printSummary(false);
    return;
  }

  ftBanner("RUN TEST");

  testSoftReset();
  testClearStatus();
  testSerialNumber();
  testCrcIntegrity();
  testSingleShot("SS_HIGH", MASSMORE_SHT3X_REPEATABILITY_HIGH);
  testSingleShot("SS_MEDIUM", MASSMORE_SHT3X_REPEATABILITY_MEDIUM);
  testSingleShot("SS_LOW", MASSMORE_SHT3X_REPEATABILITY_LOW);
  testClockStretching();
  testRepeatabilityNoise();
  testPeriodic("PERIODIC_1HZ", MASSMORE_SHT3X_RATE_1_HZ, 3000, 2);
  testPeriodic("PERIODIC_10HZ", MASSMORE_SHT3X_RATE_10_HZ, 2000, 10);
  testArtMode();
  testBreakCommand();
  testHeater();
  testAlertLimits();
  testAlertPin();
  testCommandErrorFlag();
  testWriteChecksumFlag();
  testGeneralCallReset();
  testHardReset();
  testPlausibility();
  testStability();
  testBusSpeed();
  testOffsetApi();
  testAccuracySpec();

  printSummary(true);
}

void setup() {
  Serial.begin(115200);
  uint32_t start = millis();
  while (!Serial && (millis() - start) < 3000) {
    ;
  }
  delay(300);
  runFactoryTest();
}

void loop() {
  if (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'r' || c == 'R') {
      runFactoryTest();
    }
  }
  delay(20);
}
