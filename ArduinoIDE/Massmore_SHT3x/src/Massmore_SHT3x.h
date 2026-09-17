/**
 * @file    Massmore_SHT3x.h
 * @brief   Arduino / PlatformIO driver สำหรับ Sensirion SHT3x-DIS (SHT30 / SHT31 / SHT35)
 *          บนบอร์ด Massmore SHT3X (SKU-1022) ทั้งรุ่น -B และ -F (Outdoor)
 *
 * คุณสมบัติหลักตาม Massmore Standard Library Specification v1.0
 *   - Zero pin hardcoding และไม่เรียก Wire.begin() ในไลบรารี  Sketch เป็นเจ้าของ I2C Bus
 *   - Dual API: Simple Blocking (readTemperature / readAll) และ Non-blocking FSM
 *     (requestConversion / update / isDataReady / getReadings)
 *   - Zero heap: ไม่มี malloc / new / String   Buffer ใหญ่สุด 6 byte
 *   - ErrorCode แบบ enum class + lastError()  ค่า float คืน NAN เมื่อผิดพลาด
 *   - verifyChipID() / getSerialNumber() / isGenuine() สำหรับตรวจของแท้
 *   - ครอบคลุม Periodic Mode, ART, Heater, Status Register, ALERT threshold,
 *     Soft / General Call / Hard Reset
 *
 * รองรับ ESP32 (Classic), ESP32-S3 (Arduino-ESP32 Core 3.x+) และ AVR ATmega328P
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license   MIT
 */

#ifndef MASSMORE_SHT3X_H
#define MASSMORE_SHT3X_H

#include <Arduino.h>
#include <Wire.h>

#include "Massmore_SHT3x_Registers.h"

#define MASSMORE_SHT3X_VERSION_MAJOR  2
#define MASSMORE_SHT3X_VERSION_MINOR  0
#define MASSMORE_SHT3X_VERSION_PATCH  0
#define MASSMORE_SHT3X_VERSION_STRING "2.0.0"

/** Timeout เริ่มต้น (ms) ขณะรอข้อมูลจาก I2C Bus */
#define MASSMORE_SHT3X_TIMEOUT_DEFAULT_MS 100

/**
 * @class Massmore_SHT3x
 * @brief Driver หลักของเซ็นเซอร์ SHT3x   หนึ่ง object ต่อหนึ่งตัวเซ็นเซอร์
 */
class Massmore_SHT3x {
public:
  /* ======================================================================= */
  /* Types                                                                   */
  /* ======================================================================= */

  /** รหัสข้อผิดพลาด ดูค่าล่าสุดได้จาก lastError() */
  enum class ErrorCode : uint8_t {
    OK = 0,     /**< สำเร็จ */
    NOT_FOUND,  /**< ไม่มีอุปกรณ์ตอบ ACK ที่ address นี้ (สายหลุด / address ผิด) */
    WRONG_ID,   /**< อุปกรณ์ตอบ แต่ Status Register ไม่ตรงพฤติกรรม SHT3x */
    TIMEOUT,    /**< รอข้อมูลจาก Bus เกินเวลา */
    CRC_FAIL,   /**< CRC-8 ของข้อมูลที่อ่านมาไม่ตรง (สัญญาณรบกวน / สายยาว) */
    BUS_ERROR,  /**< endTransmission() / requestFrom() ล้มเหลว */
    NOT_READY,  /**< ยังไม่มีผลวัดใหม่ (ปกติใน Periodic / FSM) */
    NOT_BEGUN,  /**< ยังไม่ได้เรียก begin() สำเร็จ */
    WRONG_MODE, /**< เรียกฟังก์ชันที่ไม่ตรงกับ Mode ปัจจุบัน */
    BAD_ARG     /**< Argument ไม่ถูกต้อง */
  };

  /** ระดับ Repeatability (ความละเอียด / Noise ต่ำ) แลกกับเวลาวัด */
  enum class Repeatability : uint8_t {
    RPT_LOW = 0, /**< ~4 ms  Noise สูงสุด ประหยัดพลังงานที่สุด */
    RPT_MEDIUM, /**< ~6 ms */
    RPT_HIGH    /**< ~15 ms Noise ต่ำสุด (ค่าเริ่มต้น) */
  };

  /** อัตราการวัดใน Periodic Mode */
  enum class Rate : uint8_t {
    HZ_0_5 = 0, /**< 0.5 ครั้ง/วินาที */
    HZ_1,       /**< 1 ครั้ง/วินาที */
    HZ_2,       /**< 2 ครั้ง/วินาที */
    HZ_4,       /**< 4 ครั้ง/วินาที */
    HZ_10       /**< 10 ครั้ง/วินาที */
  };

  /** Mode การทำงานปัจจุบันของชิป */
  enum class Mode : uint8_t {
    IDLE = 0,    /**< ยังไม่ begin */
    SINGLE_SHOT, /**< วัดทีละครั้งตามคำสั่ง */
    PERIODIC,    /**< ชิปวัดเองต่อเนื่อง */
    ART          /**< Periodic 4 Hz แบบ Accelerated Response Time */
  };

  /** State ของ Non-blocking FSM */
  enum class FsmState : uint8_t {
    IDLE = 0,   /**< ไม่มีงานค้าง */
    CONVERTING, /**< ส่ง Command แล้ว รอเวลาวัด */
    READY,      /**< ผลพร้อมให้ getReadings() */
    FAULT       /**< ล้มเหลว ดู lastError() */
  };

  /** ผลตรวจของแท้จาก isGenuine() / getGenuineVerdict() */
  enum class Genuine : uint8_t {
    UNKNOWN = 0, /**< ยังไม่ได้ตรวจ */
    PASS,        /**< ผ่านทุกข้อ */
    PARTIAL,     /**< ผ่านส่วนใหญ่ อาจเป็นปัญหาสัญญาณ */
    SUSPECT,     /**< ไม่ผ่านหลายข้อ */
    NOT_SHT3X    /**< ตอบ ACK แต่ไม่ใช่ SHT3x */
  };

  /** ผลการวัดหนึ่งชุด */
  struct Reading {
    float temperature;      /**< °C (รวม offset แล้ว) หรือ NAN */
    float humidity;         /**< %RH (รวม offset แล้ว, clamp 0..100) หรือ NAN */
    uint16_t rawTemperature; /**< ค่าดิบ 16-bit จากชิป */
    uint16_t rawHumidity;    /**< ค่าดิบ 16-bit จากชิป */
    uint32_t timestampMs;    /**< millis() ขณะอ่านสำเร็จ */
  };

  /** Status Register ที่ถอดเป็น bit แล้ว */
  struct StatusBits {
    uint16_t raw;
    bool alertPending;
    bool heaterOn;
    bool humidityAlert;
    bool temperatureAlert;
    bool resetDetected;
    bool commandFailed;
    bool checksumFailed;
  };

  /** Alert threshold ครบ 4 ชุด (ลำดับที่ถูกต้อง: lowSet < lowClear < highClear < highSet) */
  struct AlertLimits {
    float highSetTemperature;   float highSetHumidity;
    float highClearTemperature; float highClearHumidity;
    float lowClearTemperature;  float lowClearHumidity;
    float lowSetTemperature;    float lowSetHumidity;
  };

  /**
   * Bit ของแต่ละข้อใน Authenticity heuristic (ดู getVerifyMask())
   *
   * ทุกข้อเลือกมาแล้วว่า **ปลอดภัยกับ I2C Bus** คือไม่มีข้อไหนจงใจให้ชิป NACK
   * เพราะการทดสอบบนฮาร์ดแวร์พบว่า NACK หนึ่งครั้งทำให้ i2c driver ของ
   * Arduino-ESP32 Core 3.x ค้างถาวรจนกว่าจะเรียก recoverBus()
   */
  enum VerifyCheck : uint16_t {
    CHK_ACK          = 1u << 0, /**< ตอบ ACK ที่ address */
    CHK_STATUS_CRC   = 1u << 1, /**< อ่าน Status Register ได้ CRC ถูก */
    CHK_STATUS_RSVD  = 1u << 2, /**< Reserved bit ของ Status เป็น 0 */
    CHK_CLEAR_STATUS = 1u << 3, /**< Clear Status ลบ bit ที่ค้างได้จริง */
    CHK_SERIAL       = 1u << 4, /**< Serial Number อ่านได้และไม่ใช่ 0x00000000 / 0xFFFFFFFF */
    CHK_HEATER       = 1u << 5, /**< Heater bit 13 ตอบสนองคำสั่งเปิด/ปิด */
    CHK_ALERT_RW     = 1u << 6, /**< เขียน Alert threshold แล้วอ่านกลับได้ค่าเดิม */
    CHK_MEAS_CRC     = 1u << 7, /**< ผลวัด CRC ถูก */
    CHK_MEAS_RANGE   = 1u << 8  /**< ผลวัดอยู่ในช่วง Physical range */
  };
  static const uint8_t VERIFY_CHECK_COUNT = 9;

  /* ======================================================================= */
  /* Constructor & begin                                                     */
  /* ======================================================================= */

  /**
   * @brief  สร้าง object โดยฉีด I2C Bus เข้ามา (ไลบรารีไม่เรียก begin() ของ Bus เอง)
   * @param  wirePort  TwoWire ที่ Sketch เปิดไว้แล้ว เช่น Wire หรือ Wire1
   */
  explicit Massmore_SHT3x(TwoWire &wirePort = Wire);

  /**
   * @brief  เริ่มต้นเซ็นเซอร์: Break -> Soft Reset -> ตรวจ Status Register
   *         ต้องเรียก Wire.begin() ใน Sketch ก่อนเสมอ
   *
   *         ถ้าไม่พบชิปในครั้งแรก จะเรียก recoverBus() หนึ่งครั้งแล้วลองใหม่
   *         เพื่อกู้ Bus ที่ค้างมาจากการรันครั้งก่อน (ดูคำอธิบายที่ recoverBus())
   * @param  address   0x44 (ค่าเริ่มต้น) หรือ 0x45 (Jumper ADDR ปิด)
   * @param  alertPin  GPIO ที่ต่อขา ALRT หรือ -1 ถ้าไม่ต่อ
   * @param  resetPin  GPIO ที่ต่อขา RST หรือ -1 ถ้าไม่ต่อ
   * @return true เมื่อชิปตอบและ Status Register อ่านได้ถูกต้อง
   */
  bool begin(uint8_t address = MASSMORE_SHT3X_I2C_ADDR_DEFAULT, int8_t alertPin = -1,
             int8_t resetPin = -1);

  /** @brief ตรวจว่ามีอุปกรณ์ตอบ ACK ที่ address ปัจจุบันหรือไม่ (ไม่ต้อง begin ก่อน) */
  bool isConnected();

  /* ======================================================================= */
  /* Configuration                                                           */
  /* ======================================================================= */

  /** @brief ตั้ง Repeatability สำหรับ Single Shot และ FSM (ค่าเริ่มต้น HIGH) */
  void setRepeatability(Repeatability repeatability) { _repeatability = repeatability; }
  Repeatability getRepeatability() const { return _repeatability; }

  /**
   * @brief  เปิด/ปิด Clock Stretching ใน Blocking API
   *         ปิด (ค่าเริ่มต้น) = ไลบรารี delay() รอเอง ปลอดภัยกับทุก MCU
   *         เปิด = ชิปดึง SCL ค้างจนวัดเสร็จ (บาง Core ไม่รองรับ)
   */
  void setClockStretching(bool enabled) { _clockStretching = enabled; }
  bool getClockStretching() const { return _clockStretching; }

  /** @brief Timeout (ms) ขณะรอข้อมูลจาก Bus */
  void setTimeout(uint16_t milliseconds) { _timeoutMs = milliseconds; }

  /** @brief ชดเชยอุณหภูมิ (°C) กรณีติดตั้งใกล้แหล่งความร้อน */
  void setTemperatureOffset(float offsetCelsius) { _temperatureOffset = offsetCelsius; }
  float getTemperatureOffset() const { return _temperatureOffset; }

  /** @brief ชดเชยความชื้น (%RH) */
  void setHumidityOffset(float offsetPercent) { _humidityOffset = offsetPercent; }
  float getHumidityOffset() const { return _humidityOffset; }

  /* ======================================================================= */
  /* Simple Blocking API                                                     */
  /* ======================================================================= */

  /**
   * @brief  อ่านอุณหภูมิแบบ Blocking (รอจน conversion เสร็จ)
   * @return float องศาเซลเซียส หรือ NAN หากอ่านไม่สำเร็จ (ดู lastError())
   */
  float readTemperature();

  /**
   * @brief  อ่านความชื้นสัมพัทธ์แบบ Blocking
   * @return float %RH หรือ NAN หากอ่านไม่สำเร็จ
   */
  float readHumidity();

  /**
   * @brief  อ่านทั้งอุณหภูมิและความชื้นในหนึ่ง Transaction (แนะนำ ประหยัดเวลากว่าอ่านแยก)
   * @param  reading  struct รับผล
   * @return true เมื่อสำเร็จ
   */
  bool readAll(Reading &reading);

  /* ======================================================================= */
  /* Advanced Non-blocking FSM API                                           */
  /* ======================================================================= */

  /**
   * @brief  สั่งเริ่มวัด 1 ครั้งแล้วคืนทันที ไม่ Block loop()
   *         ใช้ได้เฉพาะ Single Shot Mode
   * @return true เมื่อส่ง Command สำเร็จ (State -> CONVERTING)
   */
  bool requestConversion();

  /**
   * @brief  ขับ State machine  ต้องเรียกใน loop() ทุกครั้ง
   *         - Single Shot: เมื่อครบเวลาวัดจะอ่านผลและ State -> READY
   *         - Periodic / ART: ดึงผลใหม่ตาม Rate อัตโนมัติ
   * @return true เมื่อมีผลวัดใหม่ในรอบนี้
   */
  bool update();

  /** @brief มีผลวัดใหม่ที่ยังไม่ได้ getReadings() หรือไม่ */
  bool isDataReady() const { return _fsmState == FsmState::READY; }

  /**
   * @brief  ดึงผลวัดล่าสุดแล้วเคลียร์ flag ready
   * @return true เมื่อมีผลใหม่ให้อ่าน
   */
  bool getReadings(Reading &reading);

  /** @brief State ปัจจุบันของ FSM */
  FsmState getFsmState() const { return _fsmState; }

  /* ======================================================================= */
  /* Periodic / ART Mode                                                     */
  /* ======================================================================= */

  /** @brief เริ่ม Periodic Mode ให้ชิปวัดเองต่อเนื่อง แล้วใช้ update() หรือ fetchData() ดึงผล */
  bool startPeriodic(Rate rate, Repeatability repeatability = Repeatability::RPT_HIGH);

  /** @brief เริ่ม ART Mode (Periodic 4 Hz, Response time เร็วขึ้น) */
  bool startART();

  /** @brief ดึงผลล่าสุดจาก Periodic / ART Mode ทันที (NOT_READY ถ้าชิปยังวัดไม่เสร็จ) */
  bool fetchData(Reading &reading);

  /** @brief ส่ง Break กลับสู่ Single Shot Mode */
  bool stopPeriodic();

  Mode getMode() const { return _mode; }

  /** @brief ผลวัดล่าสุดที่ไลบรารีเก็บไว้ (ไม่คุยกับ Bus) */
  const Reading &getLastReading() const { return _reading; }

  /* ======================================================================= */
  /* Heater & Status Register                                                */
  /* ======================================================================= */

  bool setHeater(bool on);
  bool isHeaterOn();

  bool readStatus(uint16_t &status);
  bool readStatus(StatusBits &bits);
  bool clearStatus();
  static StatusBits decodeStatus(uint16_t raw);

  /* ======================================================================= */
  /* ALERT                                                                   */
  /* ======================================================================= */

  /** @brief เขียน threshold ครบ 4 ชุดลงชิป (ทำงานเฉพาะ Periodic Mode) */
  bool setAlertLimits(const AlertLimits &limits);

  /**
   * @brief  ตั้ง Alert แบบง่าย: ระบุขอบล่าง/บน ไลบรารีคำนวณ Hysteresis ให้
   */
  bool setAlertWindow(float lowTemperature, float highTemperature, float lowHumidity,
                      float highHumidity, float hysteresisT = 1.0f, float hysteresisRH = 3.0f);

  /** @brief อ่าน threshold กลับจากชิป (ค่าถูกปัดตามความละเอียดของ Hardware) */
  bool getAlertLimits(AlertLimits &limits);

  /** @brief สถานะขา ALRT (true = HIGH = กำลังเตือน)  คืน false ถ้าไม่ได้ต่อขา */
  bool isAlertPinActive() const;

  /* ======================================================================= */
  /* Reset                                                                   */
  /* ======================================================================= */

  /**
   * @brief  Soft Reset (Command 0x30A2) คืนค่า Register ทั้งหมดสู่ default
   * @note   วัดบนบอร์ดจริงแล้ว: Heater ถูกปิดจริงหลังคำสั่งนี้ แต่ชิป
   *         **ไม่ตั้ง** Reset-detected bit (bit 4) จึงห้ามใช้ bit นั้นยืนยันว่า Reset สำเร็จ
   *         ถ้าต้องการให้ bit 4 ขึ้น ให้ใช้ generalCallReset() หรือ hardReset()
   */
  bool softReset();

  /**
   * @brief  General Call Reset (เขียน 0x06 ไปที่ address 0x00) รีเซ็ตทุกอุปกรณ์บน Bus
   * @warning กระทบอุปกรณ์อื่นบน Bus เดียวกันทั้งหมด ใช้เมื่อจำเป็นเท่านั้น
   */
  bool generalCallReset();

  /** @brief กระตุกขา nRESET (ต้องส่ง resetPin ใน begin()) */
  bool hardReset();

  /**
   * @brief  กู้ I2C Bus ที่ค้าง แล้วคืน true เมื่อชิปกลับมาตอบ ACK
   *
   *         ใช้เมื่อ lastError() เป็น BUS_ERROR ติดกันหลายครั้ง  บนฮาร์ดแวร์จริงพบว่า
   *         เมื่อชิป NACK (เช่นได้รับ Command ที่ไม่รู้จัก หรือ Write ที่ CRC ผิด)
   *         i2c driver ของ Arduino-ESP32 Core 3.x จะคืน ESP_ERR_INVALID_STATE
   *         กับทุก Transaction ถัดไปอย่างถาวร และ `Wire.end()` + `Wire.begin()` ก็ไม่ช่วย
   *         การส่ง General Call Reset กู้กลับมาได้ทุกครั้ง
   *
   * @warning ใช้ General Call จึงรีเซ็ตอุปกรณ์อื่นบน Bus เดียวกันด้วย
   */
  bool recoverBus();

  /* ======================================================================= */
  /* Chip Identity Verification (ตรวจของแท้)                                 */
  /* ======================================================================= */

  /**
   * @brief  ตรวจตัวตนของชิป  SHT3x ไม่มี CHIP_ID / WHO_AM_I register
   *         จึงใช้ Status Register หลัง Soft Reset แทน:
   *         CRC ถูก + Reserved bit เป็น 0 + Reset-detected bit ขึ้น
   * @return true เมื่อพฤติกรรมตรงกับ SHT3x
   */
  bool verifyChipID();

  /**
   * @brief  อ่าน Serial Number 32-bit จากโรงงาน (Command 0x3780)
   * @return Serial Number หรือ 0 เมื่ออ่านไม่สำเร็จ
   */
  uint32_t getSerialNumber();

  /**
   * @brief  Heuristic ตรวจของแท้ 9 ข้อ (Status Register + Serial + Heater + Alert R/W + CRC + Range)
   *         ใช้เวลาประมาณ 80 ms คืน Mode เดิมและคืนค่า Alert threshold เดิมให้หลังตรวจเสร็จ
   *         ทุกข้อปลอดภัยกับ Bus ไม่มีข้อไหนจงใจให้ชิป NACK
   * @return true เมื่อผ่านครบทุกข้อ (Genuine::PASS)
   */
  bool isGenuine();

  /** @brief ผลละเอียดจาก isGenuine() ครั้งล่าสุด */
  Genuine getGenuineVerdict() const { return _genuine; }
  uint16_t getVerifyMask() const { return _verifyMask; }
  uint8_t getVerifyPassCount() const;
  static const char *getVerifyCheckName(uint8_t index);
  static const char *genuineToString(Genuine verdict);

  /* ======================================================================= */
  /* Derived values (static, ไม่ต้องมี object)                                */
  /* ======================================================================= */

  /** @brief จุดน้ำค้าง (°C) สูตร Magnus */
  static float dewPoint(float temperature, float humidity);
  /** @brief ความชื้นสัมบูรณ์ (g/m³) */
  static float absoluteHumidity(float temperature, float humidity);
  static float celsiusToFahrenheit(float celsius) { return celsius * 1.8f + 32.0f; }

  /* ======================================================================= */
  /* Error & info                                                            */
  /* ======================================================================= */

  ErrorCode lastError() const { return _error; }
  const char *lastErrorString() const { return errorToString(_error); }
  static const char *errorToString(ErrorCode error);
  uint8_t getAddress() const { return _address; }
  static const char *getLibraryVersion() { return MASSMORE_SHT3X_VERSION_STRING; }

  /**
   * @brief  สแกนหา SHT3x บน Bus ที่ 0x44 และ 0x45
   * @param  wirePort  Bus ที่เปิดแล้ว
   * @param  found     array ขนาด 2 รับ address ที่พบ
   * @return จำนวนที่พบ (0-2)
   */
  static uint8_t scan(TwoWire &wirePort, uint8_t *found);

  /* ======================================================================= */
  /* Low-level (public เพื่อให้ Factory Test เข้าถึงได้)                       */
  /* ======================================================================= */

  bool sendCommand(uint16_t command);
  bool readWord(uint16_t command, uint16_t &value);
  static uint8_t crc8(const uint8_t *data, uint8_t length);
  static float rawToCelsius(uint16_t raw);
  static float rawToHumidity(uint16_t raw);
  static uint16_t celsiusToRaw(float celsius);
  static uint16_t humidityToRaw(float humidity);

private:
  TwoWire *_wire;
  uint8_t _address;
  bool _begun;
  int8_t _alertPin;
  int8_t _resetPin;
  uint16_t _timeoutMs;
  Repeatability _repeatability;
  Mode _mode;
  Rate _rate;
  bool _clockStretching;
  float _temperatureOffset;
  float _humidityOffset;

  FsmState _fsmState;
  uint32_t _fsmStartMs;
  uint32_t _lastFetchMs;

  Reading _reading;
  uint32_t _serialNumber;
  uint16_t _verifyMask;
  Genuine _genuine;
  ErrorCode _error;

  bool setError(ErrorCode error);
  void clearError() { _error = ErrorCode::OK; }
  bool readBytes(uint8_t *buffer, uint8_t length);
  bool writeWord(uint16_t command, uint16_t value);
  bool readMeasurementFrame(uint16_t &rawT, uint16_t &rawRH);
  void storeReading(uint16_t rawT, uint16_t rawRH);
  bool measureBlocking();
  uint16_t singleShotCommand(bool stretch) const;
  static uint16_t periodicCommand(Rate rate, Repeatability rep);
  uint16_t measurementDurationMs() const;
  uint32_t periodIntervalMs() const;
  static uint16_t packAlertLimit(float temperature, float humidity);
  static void unpackAlertLimit(uint16_t word, float &temperature, float &humidity);
  bool inContinuousMode() const { return _mode == Mode::PERIODIC || _mode == Mode::ART; }
};

#endif /* MASSMORE_SHT3X_H */
