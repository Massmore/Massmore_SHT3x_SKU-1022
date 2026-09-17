/*
  05_Factory_Test - Outgoing QA/QC สำหรับบอร์ด Massmore SHT3X (SKU-1022)

  รันอัตโนมัติหลังบูต  พิมพ์ 'r' ใน Serial Monitor เพื่อทดสอบซ้ำ
  Serial output เป็นภาษาอังกฤษล้วน ให้ Massmore Web Serial Monitor parse ได้
  บรรทัดที่ขึ้นต้นด้วย # คือบรรทัดที่เครื่อง parse  บรรทัดอื่นอ่านโดยคน

  Test sequence (Massmore Standard §7.1)
    0. BUS_RECOVER   กู้ I2C Bus ที่อาจค้างจากการรันครั้งก่อน (MCU reset ไม่ตัดไฟเซ็นเซอร์)
    1. BUS_SCAN      หา SHT3x ที่ 0x44 / 0x45
    2. CHIP_ID       Status Register: CRC ถูก + Reserved bit = 0 + Clear Status ทำงาน
    3. SERIAL        Serial Number 32-bit (Command 0x3780)
    4. AUTHENTICITY  Heuristic 9 ข้อ -> GENUINE / PARTIAL / SUSPECT
    5. RANGE_TEMP / RANGE_HUMI  ค่าอยู่ใน Physical range ของ Datasheet
    6. CONTINUOUS    20 samples ไม่มี NAN / TIMEOUT และ Noise สมเหตุสมผล
    7. VERDICT

  Default wiring (Primary MCU = ESP32 Classic, Massmore Standard §7.3)
    SDA = GPIO 21, SCL = GPIO 22, 3.3 V, GND
    ขาถูก hardcode เฉพาะใน Sketch นี้เท่านั้น ไม่มีในไลบรารี

  Copyright (c) 2026 Massmore Biz Co., Ltd.  |  MIT License
*/

#include <Massmore_SHT3x.h>
#include <Wire.h>
#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Configuration                                                             */
/* ------------------------------------------------------------------------- */

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define PIN_SDA 8
#define PIN_SCL 9
#define MCU_NAME "ESP32-S3"
#elif defined(ESP32)
#define PIN_SDA 21
#define PIN_SCL 22
#define MCU_NAME "ESP32"
#elif defined(__AVR_ATmega328P__)
#define MCU_NAME "AVR_NANO"
#else
#define MCU_NAME "UNKNOWN"
#endif

#define FT_VERSION "v1.0"
#define PRODUCT_NAME "Massmore_SHT3x"
#define I2C_FREQ_HZ 100000UL

#define CONTINUOUS_SAMPLES 20
#define CONTINUOUS_GAP_MS 100
/* Noise limit ระหว่าง sample ติดกัน (Repeatability HIGH: 0.04 °C / 0.08 %RH RMS ตาม Datasheet) */
#define MAX_STEP_T 1.0f
#define MAX_STEP_RH 3.0f

/* ------------------------------------------------------------------------- */

Massmore_SHT3x sht(Wire);

static bool allPass = true;
static char failReason[24] = "";

/* ------------------------------------------------------------------------- */
/* Output helpers (Serial.print only - works on AVR without printf)          */
/* ------------------------------------------------------------------------- */

static void printHex(uint32_t value, uint8_t digits) {
  Serial.print(F("0x"));
  for (int8_t i = (int8_t)digits - 1; i >= 0; i--) {
    uint8_t nibble = (uint8_t)((value >> (i * 4)) & 0x0F);
    Serial.print((char)(nibble < 10 ? '0' + nibble : 'A' + nibble - 10));
  }
}

static void setFail(const char *reason) {
  if (allPass) {
    allPass = false;
    strncpy(failReason, reason, sizeof(failReason) - 1);
    failReason[sizeof(failReason) - 1] = '\0';
  }
}

static void resultLine(const __FlashStringHelper *name, bool pass) {
  Serial.print(F("#RESULT "));
  Serial.print(name);
  Serial.print(pass ? F(" PASS ") : F(" FAIL "));
}

/* ------------------------------------------------------------------------- */
/* Tests                                                                     */
/* ------------------------------------------------------------------------- */

static bool testBusScan(uint8_t &addressOut) {
  uint8_t found[2];
  uint8_t n = Massmore_SHT3x::scan(Wire, found);
  bool pass = n > 0;
  resultLine(F("BUS_SCAN"), pass);
  if (pass) {
    addressOut = found[0];
    printHex(addressOut, 2);
  } else {
    Serial.print(F("NONE"));
  }
  Serial.println();
  if (!pass) setFail("NO_DEVICE");
  return pass;
}

static bool testChipId() {
  /* SHT3x ไม่มี CHIP_ID register จึงยืนยันตัวตนผ่านพฤติกรรมของ Status Register:
     อ่านได้ CRC ถูก + Reserved bit เป็น 0 + Clear Status ลบ bit ที่ค้างได้จริง
     ไม่ใช้ Reset-detected bit เป็นเกณฑ์ เพราะวัดบนบอร์ดจริงแล้วพบว่า Soft Reset
     ของชิปนี้ไม่ตั้ง bit ดังกล่าว (ตั้งเฉพาะ power-up และ General Call Reset) */
  uint16_t status = 0;
  bool readOk = sht.softReset() && sht.readStatus(status);
  bool reservedOk = readOk && (status & MASSMORE_SHT3X_STATUS_RESERVED_MASK) == 0;
  bool clearOk = false;
  if (reservedOk && sht.clearStatus()) {
    uint16_t after = 0xFFFF;
    clearOk = sht.readStatus(after) &&
              (after & (MASSMORE_SHT3X_STATUS_ALERT_PENDING |
                        MASSMORE_SHT3X_STATUS_RESET_DETECTED |
                        MASSMORE_SHT3X_STATUS_CMD_FAILED)) == 0;
  }
  bool pass = readOk && reservedOk && clearOk;
  resultLine(F("CHIP_ID"), pass);
  printHex(status, 4);
  Serial.println();
  if (!pass) setFail("CHIP_ID_MISMATCH");
  return pass;
}

static bool testSerial() {
  uint32_t serial = sht.getSerialNumber();
  bool pass = serial != 0x00000000UL && serial != 0xFFFFFFFFUL;
  resultLine(F("SERIAL"), pass);
  printHex(serial, 8);
  Serial.println();
  if (!pass) setFail("SERIAL_READ_FAIL");
  return pass;
}

static bool testAuthenticity() {
  bool pass = sht.isGenuine();
  Massmore_SHT3x::Genuine v = sht.getGenuineVerdict();
  /* PARTIAL (ผ่าน 8 จาก 9) ยอมรับได้ในสายยาว แต่ FAIL เมื่อ SUSPECT / NOT_SHT3X */
  bool accept = pass || v == Massmore_SHT3x::Genuine::PARTIAL;
  resultLine(F("AUTHENTICITY"), accept);
  Serial.println(Massmore_SHT3x::genuineToString(v));

  /* รายละเอียดสำหรับคนอ่าน */
  uint16_t mask = sht.getVerifyMask();
  for (uint8_t i = 0; i < Massmore_SHT3x::VERIFY_CHECK_COUNT; i++) {
    Serial.print((mask & (1u << i)) ? F("  [ok]   ") : F("  [FAIL] "));
    Serial.println(Massmore_SHT3x::getVerifyCheckName(i));
  }
  if (!accept) setFail("AUTHENTICITY_FAIL");
  return accept;
}

static bool testRange(Massmore_SHT3x::Reading &r) {
  bool ok = sht.readAll(r);
  bool tPass = ok && !isnan(r.temperature) && r.temperature > MASSMORE_SHT3X_T_MIN_C &&
               r.temperature < MASSMORE_SHT3X_T_MAX_C;
  bool hPass = ok && !isnan(r.humidity) && r.humidity >= MASSMORE_SHT3X_RH_MIN &&
               r.humidity <= MASSMORE_SHT3X_RH_MAX && r.rawHumidity != 0xFFFF;

  resultLine(F("RANGE_TEMP"), tPass);
  if (ok) Serial.println(r.temperature, 1); else Serial.println(sht.lastErrorString());
  resultLine(F("RANGE_HUMI"), hPass);
  if (ok) Serial.println(r.humidity, 1); else Serial.println(sht.lastErrorString());

  if (!tPass) setFail("TEMP_OUT_OF_RANGE");
  if (!hPass) setFail("HUMI_OUT_OF_RANGE");
  return tPass && hPass;
}

static bool testContinuous() {
  uint8_t good = 0;
  float prevT = NAN;
  float prevH = NAN;
  bool noiseOk = true;

  for (uint8_t i = 0; i < CONTINUOUS_SAMPLES; i++) {
    Massmore_SHT3x::Reading r;
    if (sht.readAll(r) && !isnan(r.temperature) && !isnan(r.humidity)) {
      if (!isnan(prevT)) {
        if (fabsf(r.temperature - prevT) > MAX_STEP_T || fabsf(r.humidity - prevH) > MAX_STEP_RH) {
          noiseOk = false;
        }
      }
      prevT = r.temperature;
      prevH = r.humidity;
      good++;
    }
    delay(CONTINUOUS_GAP_MS);
  }

  bool pass = good == CONTINUOUS_SAMPLES && noiseOk;
  resultLine(F("CONTINUOUS"), pass);
  Serial.print(good);
  Serial.print('/');
  Serial.println(CONTINUOUS_SAMPLES);
  if (good != CONTINUOUS_SAMPLES) setFail("CONTINUOUS_READ_FAIL");
  else if (!noiseOk) setFail("NOISE_TOO_HIGH");
  return pass;
}

/* ------------------------------------------------------------------------- */

static void runFactoryTest() {
  allPass = true;
  failReason[0] = '\0';

  Serial.println();
  Serial.print(F("#MASSMORE_FACTORY_TEST "));
  Serial.println(F(FT_VERSION));
  Serial.print(F("#PRODUCT "));
  Serial.println(F(PRODUCT_NAME));
  Serial.print(F("#MCU "));
  Serial.println(F(MCU_NAME));
  Serial.print(F("#LIB "));
  Serial.println(Massmore_SHT3x::getLibraryVersion());

  /* กู้ Bus ก่อนเสมอ: การรันครั้งก่อนอาจทิ้ง i2c driver ไว้ในสถานะค้าง
     และ MCU reset ไม่ได้ตัดไฟเลี้ยงเซ็นเซอร์ */
  bool recovered = sht.recoverBus();
  Serial.print(F("#RESULT BUS_RECOVER "));
  Serial.println(recovered ? F("PASS READY") : F("PASS NO_DEVICE_YET"));

  uint8_t address = MASSMORE_SHT3X_I2C_ADDR_A;
  bool ok = testBusScan(address);

  if (ok) {
    ok = sht.begin(address);
    if (!ok) {
      resultLine(F("CHIP_ID"), false);
      Serial.println(sht.lastErrorString());
      setFail("CHIP_ID_MISMATCH");
    }
  }

  if (ok) {
    testChipId();
    testSerial();
    testAuthenticity();
    Massmore_SHT3x::Reading r;
    testRange(r);
    testContinuous();
  }

  Serial.print(F("#VERDICT "));
  if (allPass) {
    Serial.println(F("PASS"));
    Serial.println(F("[PASS] SENSOR QA PASSED - READY TO SHIP"));
  } else {
    Serial.print(F("FAIL "));
    Serial.println(failReason);
    Serial.print(F("[FAIL] QA CHECK FAILED: "));
    Serial.println(failReason);
  }
  Serial.println(F("Press 'r' to run again"));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }

#if defined(ESP32)
  Wire.begin(PIN_SDA, PIN_SCL, I2C_FREQ_HZ);
#else
  Wire.begin();
  Wire.setClock(I2C_FREQ_HZ);
#endif

  delay(50); /* รอ Power-up ของเซ็นเซอร์ (max 1.5 ms) และ Serial Monitor เปิด */
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
