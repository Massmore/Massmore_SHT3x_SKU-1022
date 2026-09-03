/*!
 * @file Massmore_SHT3x.h
 * @brief ไลบรารี Arduino / PlatformIO สำหรับเซ็นเซอร์อุณหภูมิและความชื้น
 *        Sensirion SHT3x-DIS (SHT30 / SHT31 / SHT35) ทั้งรุ่นปกติ (-B)
 *        และรุ่นกันฝุ่นกันน้ำ (-F / Outdoor)
 *
 * บอร์ด Massmore SHT3X SKU-1022
 * https://www.massmore.shop/products/f9e65fad-f86d-4ac3-9e95-6f575e1d33d3
 *
 * จุดเด่นของไลบรารีตัวนี้
 *   - เขียนขึ้นจาก datasheet โดยตรง ไม่พึ่งไลบรารีอื่นนอกจาก Wire
 *   - ไม่ใช้ heap เลย (ไม่มี new / malloc / String ในส่วนแกน)
 *   - ครอบคลุมทุกฟังก์ชันของชิป: single shot, periodic, ART, heater,
 *     status register, ALERT พร้อม threshold ทั้ง 4 ชุด, serial number,
 *     soft reset, general call reset, hard reset ผ่านขา RST
 *   - มีโหมด non-blocking สำหรับงานที่ห้ามค้าง
 *   - มี verifyChip() ตรวจว่าเป็นชิป Sensirion ของแท้ ไม่ใช่ของเลียนแบบ
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license MIT
 */

#ifndef MASSMORE_SHT3X_H
#define MASSMORE_SHT3X_H

#include "Massmore_SHT3x_Registers.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#else
/* ใช้ตอนคอมไพล์ host test บนเครื่อง PC ไฟล์ mock อยู่ในโฟลเดอร์ test/ */
#include "massmore_sht3x_host_shim.h"
#endif

/*! เวอร์ชันของไลบรารี */
#define MASSMORE_SHT3X_VERSION_MAJOR 1
#define MASSMORE_SHT3X_VERSION_MINOR 0
#define MASSMORE_SHT3X_VERSION_PATCH 0
#define MASSMORE_SHT3X_VERSION_STRING "1.0.0"

/*! ความถี่ I2C ที่แนะนำสำหรับบอร์ด Massmore (สาย Qwiic ยาวไม่เกิน 30 ซม.) */
#define MASSMORE_SHT3X_I2C_FREQ_DEFAULT 100000UL

/*! เวลารอสูงสุด (ms) ตอนอ่านข้อมูลจากบัส */
#define MASSMORE_SHT3X_TIMEOUT_DEFAULT_MS 100

/* ========================================================================= */
/* ชนิดข้อมูล                                                                */
/* ========================================================================= */

/*!
 * @brief รุ่นของชิปในตระกูล SHT3x
 *
 * ข้อเท็จจริงที่ผู้ใช้ควรทราบ: SHT30, SHT31 และ SHT35 คือซิลิคอนตัวเดียวกัน
 * ต่างกันที่เกรดความแม่นยำที่โรงงานคัดไว้ (binning) เท่านั้น ตัวชิปไม่มี
 * รีจิสเตอร์บอกรุ่น จึงอ่านแยกรุ่นผ่าน I2C ไม่ได้ทั้งในไลบรารีนี้และไลบรารีใด ๆ
 * ให้ดูช่องติ๊กบนซิลค์สกรีนของบอร์ด (SHT30 / SHT31 / SHT35) แล้วบอกไลบรารีเอง
 * ค่าที่ตั้งไว้ใช้กำหนดเกณฑ์ความแม่นยำที่ใช้ตรวจสอบและแสดงผลเท่านั้น
 * ไม่มีผลต่อการสื่อสารกับชิป
 */
typedef enum {
  MASSMORE_SHT3X_VARIANT_AUTO = 0, /*!< ไม่ระบุรุ่น ใช้เกณฑ์กว้างสุด (เท่า SHT30) */
  MASSMORE_SHT3X_VARIANT_SHT30,    /*!< ความชื้น +/-2 %RH, อุณหภูมิ +/-0.2 องศา */
  MASSMORE_SHT3X_VARIANT_SHT31,    /*!< ความชื้น +/-2 %RH, อุณหภูมิ +/-0.2 องศา ช่วงกว้างกว่า */
  MASSMORE_SHT3X_VARIANT_SHT35     /*!< ความชื้น +/-1.5 %RH, อุณหภูมิ +/-0.1 องศา */
} massmore_sht3x_variant_t;

/*!
 * @brief ระดับความละเอียดของการวัด
 *
 * ยิ่งสูงยิ่งใช้เวลาและพลังงานมากขึ้น แต่ noise ต่ำลง
 */
typedef enum {
  MASSMORE_SHT3X_REPEATABILITY_LOW = 0, /*!< เร็วสุด ~4 ms  noise สูงสุด */
  MASSMORE_SHT3X_REPEATABILITY_MEDIUM,  /*!< ~6 ms */
  MASSMORE_SHT3X_REPEATABILITY_HIGH     /*!< ~15 ms noise ต่ำสุด (ค่าเริ่มต้น) */
} massmore_sht3x_repeatability_t;

/*!
 * @brief อัตราการวัดในโหมด periodic
 */
typedef enum {
  MASSMORE_SHT3X_RATE_0_5_HZ = 0, /*!< 1 ครั้งทุก 2 วินาที */
  MASSMORE_SHT3X_RATE_1_HZ,       /*!< 1 ครั้ง/วินาที */
  MASSMORE_SHT3X_RATE_2_HZ,       /*!< 2 ครั้ง/วินาที */
  MASSMORE_SHT3X_RATE_4_HZ,       /*!< 4 ครั้ง/วินาที */
  MASSMORE_SHT3X_RATE_10_HZ       /*!< 10 ครั้ง/วินาที (ต้องระวังฮีตเตอร์ตัวเองของชิป) */
} massmore_sht3x_rate_t;

/*!
 * @brief โหมดการทำงานปัจจุบันของไลบรารี
 */
typedef enum {
  MASSMORE_SHT3X_MODE_IDLE = 0,      /*!< ยังไม่ begin() หรือหยุดอยู่ */
  MASSMORE_SHT3X_MODE_SINGLE_SHOT,   /*!< วัดทีละครั้งตามคำสั่ง */
  MASSMORE_SHT3X_MODE_PERIODIC,      /*!< ชิปวัดเองต่อเนื่อง */
  MASSMORE_SHT3X_MODE_ART            /*!< periodic 4 Hz แบบ Accelerated Response Time */
} massmore_sht3x_mode_t;

/*!
 * @brief รหัสผลลัพธ์ของทุกฟังก์ชันที่คุยกับชิป
 *
 * ฟังก์ชันส่วนใหญ่คืน bool เพื่อให้เขียนง่าย แล้วเก็บรหัสละเอียดไว้ที่
 * lastError() ให้ไปดูตอนเกิดปัญหา
 */
typedef enum {
  MASSMORE_SHT3X_OK = 0,           /*!< สำเร็จ */
  MASSMORE_SHT3X_ERR_NOT_BEGUN,    /*!< ยังไม่ได้เรียก begin() */
  MASSMORE_SHT3X_ERR_NO_DEVICE,    /*!< ไม่มีอุปกรณ์ตอบที่ address นี้ */
  MASSMORE_SHT3X_ERR_I2C_WRITE,    /*!< เขียนลงบัสไม่สำเร็จ */
  MASSMORE_SHT3X_ERR_I2C_READ,     /*!< อ่านได้ไบต์ไม่ครบ */
  MASSMORE_SHT3X_ERR_CRC,          /*!< checksum ของข้อมูลที่อ่านมาไม่ตรง */
  MASSMORE_SHT3X_ERR_TIMEOUT,      /*!< รอเกินเวลาที่ตั้งไว้ */
  MASSMORE_SHT3X_ERR_NOT_READY,    /*!< ยังไม่มีผลวัดใหม่ (โหมด periodic) */
  MASSMORE_SHT3X_ERR_WRONG_MODE,   /*!< เรียกผิดโหมด เช่น fetch ตอนอยู่ single shot */
  MASSMORE_SHT3X_ERR_BAD_ARG,      /*!< พารามิเตอร์ไม่ถูกต้อง */
  MASSMORE_SHT3X_ERR_OUT_OF_RANGE  /*!< ค่าที่อ่านได้อยู่นอกช่วงที่ชิปทำได้ */
} massmore_sht3x_error_t;

/*!
 * @brief ผลการตรวจสอบว่าเป็นชิป Sensirion แท้หรือไม่
 * @see MassmoreSHT3x::verifyChip()
 */
typedef enum {
  MASSMORE_SHT3X_GENUINE_UNKNOWN = 0, /*!< ยังไม่ได้ตรวจ */
  MASSMORE_SHT3X_GENUINE_PASS,        /*!< ผ่านครบทุกข้อ = เป็น SHT3x แท้ */
  MASSMORE_SHT3X_GENUINE_PARTIAL,     /*!< ตอบถูกเป็นส่วนใหญ่ แต่มีบางข้อไม่ผ่าน */
  MASSMORE_SHT3X_GENUINE_SUSPECT,     /*!< ตอบผิดหลายข้อ น่าสงสัยว่าไม่ใช่ของแท้ */
  MASSMORE_SHT3X_GENUINE_NOT_SHT3X    /*!< มีอุปกรณ์อยู่ แต่ไม่ใช่ SHT3x แน่นอน */
} massmore_sht3x_genuine_t;

/*! หมายเลขข้อของการตรวจ verifyChip() ใช้เป็นบิตใน getVerifyMask() */
#define MASSMORE_SHT3X_CHK_ACK (1u << 0)          /*!< ชิป ACK ที่ address */
#define MASSMORE_SHT3X_CHK_STATUS_CRC (1u << 1)   /*!< status register CRC ถูก */
#define MASSMORE_SHT3X_CHK_STATUS_RSVD (1u << 2)  /*!< บิต reserved เป็น 0 ตาม datasheet */
#define MASSMORE_SHT3X_CHK_RESET_FLAG (1u << 3)   /*!< หลัง soft reset บิต reset ขึ้น 1 */
#define MASSMORE_SHT3X_CHK_CLEAR_STATUS (1u << 4) /*!< clear status แล้วบิต reset ลง 0 */
#define MASSMORE_SHT3X_CHK_SERIAL (1u << 5)       /*!< อ่านซีเรียลได้ CRC ถูก ไม่ใช่ 0/FF */
#define MASSMORE_SHT3X_CHK_HEATER (1u << 6)       /*!< สั่งฮีตเตอร์แล้วบิต 13 ขยับตาม */
#define MASSMORE_SHT3X_CHK_MEAS_CRC (1u << 7)     /*!< วัดจริงแล้ว CRC ทั้งสอง word ถูก */
#define MASSMORE_SHT3X_CHK_MEAS_RANGE (1u << 8)   /*!< ค่าที่วัดได้อยู่ในช่วงที่เป็นไปได้ */
#define MASSMORE_SHT3X_CHK_CMD_ERROR (1u << 9)    /*!< ส่งคำสั่งมั่วแล้วบิต cmd failed ขึ้น */
#define MASSMORE_SHT3X_CHK_ALL 0x03FFu            /*!< ครบทั้ง 10 ข้อ */
#define MASSMORE_SHT3X_CHK_COUNT 10

/*!
 * @brief status register ที่ถอดเป็นฟิลด์แล้ว
 */
typedef struct {
  uint16_t raw;         /*!< ค่าดิบ 16 บิต */
  bool alertPending;    /*!< bit 15 */
  bool heaterOn;        /*!< bit 13 */
  bool humidityAlert;   /*!< bit 11 */
  bool temperatureAlert;/*!< bit 10 */
  bool resetDetected;   /*!< bit 4 */
  bool commandFailed;   /*!< bit 1 */
  bool checksumFailed;  /*!< bit 0 */
} massmore_sht3x_status_bits_t;

/*!
 * @brief ผลวัดหนึ่งชุด
 */
typedef struct {
  float temperature;    /*!< องศาเซลเซียส (รวม offset ที่ตั้งไว้แล้ว) */
  float humidity;       /*!< %RH (รวม offset ที่ตั้งไว้แล้ว) */
  uint16_t rawTemperature; /*!< ค่าดิบ 16 บิตจากชิป */
  uint16_t rawHumidity;    /*!< ค่าดิบ 16 บิตจากชิป */
  uint32_t timestampMs;    /*!< millis() ตอนที่อ่านค่าได้ */
} massmore_sht3x_reading_t;

/*!
 * @brief ชุด threshold ของ ALERT ทั้ง 4 ค่า
 *
 * ตรรกะของชิป: ค่าจะทะลุ "set" แล้วขา ALERT ขึ้น และจะลงก็ต่อเมื่อค่ากลับมา
 * ผ่าน "clear" (มี hysteresis) จึงต้องตั้งให้ lowSet < lowClear < highClear < highSet
 */
typedef struct {
  float highSetTemperature;    /*!< เกินค่านี้แล้วเตือน */
  float highSetHumidity;
  float highClearTemperature;  /*!< ต่ำกว่าค่านี้แล้วเลิกเตือน */
  float highClearHumidity;
  float lowClearTemperature;   /*!< สูงกว่าค่านี้แล้วเลิกเตือน */
  float lowClearHumidity;
  float lowSetTemperature;     /*!< ต่ำกว่าค่านี้แล้วเตือน */
  float lowSetHumidity;
} massmore_sht3x_alert_limits_t;

/*! ต้นแบบฟังก์ชัน callback ที่จะถูกเรียกทุกครั้งที่ update() ได้ค่าใหม่ */
typedef void (*massmore_sht3x_callback_t)(const massmore_sht3x_reading_t &reading);

/* ========================================================================= */
/* คลาสหลัก                                                                  */
/* ========================================================================= */

/*!
 * @brief ไดรเวอร์ SHT3x-DIS หนึ่งตัว
 *
 * สร้างได้หลายอ็อบเจกต์ในโปรแกรมเดียวกัน (คนละ address หรือคนละบัส I2C)
 * ทุกอ็อบเจกต์ไม่จองหน่วยความจำจาก heap เลย
 */
class MassmoreSHT3x {
public:
  /*!
   * @brief สร้างอ็อบเจกต์ โดยระบุบัส I2C ที่จะใช้
   * @param wire ตัวชี้ไปยัง TwoWire เช่น &Wire หรือ &Wire1 (ค่าเริ่มต้น &Wire)
   */
  explicit MassmoreSHT3x(TwoWire *wire = &Wire);

  /* --------------------------------------------------------------------- */
  /* การเริ่มต้นใช้งาน                                                       */
  /* --------------------------------------------------------------------- */

  /*!
   * @brief เริ่มต้นใช้งาน โดยให้ไลบรารีเรียก Wire.begin() ให้
   * @param address  0x44 (ค่าปกติ) หรือ 0x45 เมื่อบัดกรีจัมเปอร์ ADDR
   * @param variant  รุ่นของชิปตามที่ติ๊กไว้บนบอร์ด
   * @param sdaPin   ขา SDA (-1 = ใช้ค่าปริยายของบอร์ด) ใช้ได้กับ ESP32/ESP8266
   * @param sclPin   ขา SCL (-1 = ใช้ค่าปริยายของบอร์ด)
   * @param frequency ความถี่บัส I2C เป็น Hz
   * @return true เมื่อพบชิปและอ่าน status register ได้
   */
  bool begin(uint8_t address = MASSMORE_SHT3X_I2C_ADDR_DEFAULT,
             massmore_sht3x_variant_t variant = MASSMORE_SHT3X_VARIANT_AUTO,
             int8_t sdaPin = -1, int8_t sclPin = -1,
             uint32_t frequency = MASSMORE_SHT3X_I2C_FREQ_DEFAULT);

  /*!
   * @brief เริ่มต้นใช้งานโดยที่โปรแกรมเรียก Wire.begin() เองไปแล้ว
   *
   * ใช้กรณีมีอุปกรณ์ I2C หลายตัวบนบัสเดียวกัน และอยากคุมการตั้งค่าบัสเอง
   * @param address address ของชิป
   * @param variant รุ่นของชิป
   * @return true เมื่อพบชิป
   */
  bool beginWithExistingBus(uint8_t address = MASSMORE_SHT3X_I2C_ADDR_DEFAULT,
                            massmore_sht3x_variant_t variant = MASSMORE_SHT3X_VARIANT_AUTO);

  /*!
   * @brief เช็คว่าชิปยังตอบอยู่บนบัสไหม (ส่ง address เปล่า ๆ)
   * @return true ถ้าได้ ACK
   */
  bool isConnected();

  /*!
   * @brief บอกไลบรารีว่าขา RST ของบอร์ดต่อกับ GPIO ใด เพื่อใช้ hardReset()
   * @param pin หมายเลขขา MCU (-1 = ไม่ได้ต่อ)
   * @note ขา RST บนบอร์ด Massmore เป็น active LOW และมี pull-up อยู่แล้ว
   */
  void setResetPin(int8_t pin);

  /*!
   * @brief บอกไลบรารีว่าขา ALRT ต่อกับ GPIO ใด เพื่อใช้ isAlertPinActive()
   * @param pin หมายเลขขา MCU (-1 = ไม่ได้ต่อ)
   * @note ขา ALERT ของ SHT3x เป็นแบบ push-pull และ active HIGH
   */
  void setAlertPin(int8_t pin);

  /*! @brief ตั้งเวลารอสูงสุดของการอ่านบัส (มิลลิวินาที) */
  void setTimeout(uint16_t milliseconds);

  /* --------------------------------------------------------------------- */
  /* การตั้งค่า                                                             */
  /* --------------------------------------------------------------------- */

  /*! @brief ตั้งระดับความละเอียดที่จะใช้กับการวัดครั้งถัดไป */
  void setRepeatability(massmore_sht3x_repeatability_t repeatability);
  /*! @brief อ่านระดับความละเอียดปัจจุบัน */
  massmore_sht3x_repeatability_t getRepeatability() const;

  /*!
   * @brief เปิด/ปิด clock stretching ในโหมด single shot
   *
   * เปิด = ชิปจะดึงสาย SCL ค้างไว้จนวัดเสร็จ อ่านค่าได้ทันทีไม่ต้องหน่วงเอง
   *        แต่จะบล็อกอุปกรณ์อื่นบนบัสช่วงสั้น ๆ และ MCU บางรุ่นไม่รองรับ
   * ปิด  = ไลบรารีจะ delay() ตามเวลาที่ datasheet กำหนดแล้วค่อยอ่าน (ค่าเริ่มต้น)
   */
  void setClockStretching(bool enabled);
  /*! @brief clock stretching เปิดอยู่หรือไม่ */
  bool getClockStretching() const;

  /*!
   * @brief ตั้งค่าชดเชยอุณหภูมิ (บวกเข้ากับค่าที่อ่านได้)
   *
   * ใช้แก้กรณีติดตั้งชิปใกล้แหล่งความร้อน เช่นวางติดกับ ESP32
   * @param offsetCelsius ค่าชดเชยเป็นองศาเซลเซียส (ปกติเป็นลบ)
   */
  void setTemperatureOffset(float offsetCelsius);
  /*! @brief อ่านค่าชดเชยอุณหภูมิที่ตั้งไว้ */
  float getTemperatureOffset() const;
  /*! @brief ตั้งค่าชดเชยความชื้นเป็น %RH */
  void setHumidityOffset(float offsetPercent);
  /*! @brief อ่านค่าชดเชยความชื้นที่ตั้งไว้ */
  float getHumidityOffset() const;

  /*! @brief ระบุรุ่นของชิปตามที่ติ๊กไว้บนซิลค์สกรีน */
  void setVariant(massmore_sht3x_variant_t variant);
  /*! @brief อ่านรุ่นที่ตั้งไว้ */
  massmore_sht3x_variant_t getVariant() const;
  /*! @brief ชื่อรุ่นเป็นข้อความ เช่น "SHT31" */
  const char *getVariantName() const;
  /*! @brief ความคลาดเคลื่อนอุณหภูมิตามสเปกของรุ่นที่ตั้งไว้ (องศาเซลเซียส) */
  float getTemperatureAccuracy() const;
  /*! @brief ความคลาดเคลื่อนความชื้นตามสเปกของรุ่นที่ตั้งไว้ (%RH) */
  float getHumidityAccuracy() const;

  /* --------------------------------------------------------------------- */
  /* การวัดแบบ single shot (บล็อก)                                          */
  /* --------------------------------------------------------------------- */

  /*!
   * @brief วัดหนึ่งครั้งแล้วคืนทั้งอุณหภูมิและความชื้น
   * @param temperature ตัวรับอุณหภูมิเป็นองศาเซลเซียส (ใส่ nullptr ได้ถ้าไม่ต้องการ)
   * @param humidity    ตัวรับความชื้นเป็น %RH (ใส่ nullptr ได้)
   * @return true เมื่อสำเร็จ
   */
  bool measure(float *temperature, float *humidity);

  /*!
   * @brief วัดหนึ่งครั้งแล้วเก็บผลลงโครงสร้าง reading (มีค่าดิบและ timestamp ด้วย)
   * @param reading ตัวรับผลวัด
   * @return true เมื่อสำเร็จ
   */
  bool measure(massmore_sht3x_reading_t &reading);

  /*!
   * @brief วัดหนึ่งครั้งแล้วคืนเฉพาะอุณหภูมิ
   * @return องศาเซลเซียส หรือ NAN เมื่ออ่านไม่สำเร็จ
   */
  float readTemperature();

  /*!
   * @brief วัดหนึ่งครั้งแล้วคืนเฉพาะอุณหภูมิเป็นฟาเรนไฮต์
   * @return องศาฟาเรนไฮต์ หรือ NAN เมื่ออ่านไม่สำเร็จ
   */
  float readTemperatureF();

  /*!
   * @brief วัดหนึ่งครั้งแล้วคืนเฉพาะความชื้น
   * @return %RH หรือ NAN เมื่ออ่านไม่สำเร็จ
   */
  float readHumidity();

  /* --------------------------------------------------------------------- */
  /* การวัดแบบ single shot (ไม่บล็อก)                                        */
  /* --------------------------------------------------------------------- */

  /*!
   * @brief สั่งชิปเริ่มวัดแล้วคืนทันที ไม่รอ
   *
   * ใช้คู่กับ isMeasurementReady() และ readMeasurement()
   * บังคับให้ clock stretching ปิดชั่วคราวเพราะการรอเป็นหน้าที่ของโปรแกรม
   * @return true เมื่อส่งคำสั่งสำเร็จ
   */
  bool startMeasurement();

  /*!
   * @brief ถึงเวลาที่ผลวัดน่าจะพร้อมหรือยัง (คำนวณจาก repeatability)
   * @return true เมื่อครบเวลาแล้ว
   */
  bool isMeasurementReady() const;

  /*!
   * @brief อ่านผลของ startMeasurement()
   * @param temperature ตัวรับอุณหภูมิ (ใส่ nullptr ได้)
   * @param humidity    ตัวรับความชื้น (ใส่ nullptr ได้)
   * @return true เมื่ออ่านได้และ CRC ถูก, false พร้อม lastError() = ERR_NOT_READY
   *         ถ้ายังไม่ถึงเวลา
   */
  bool readMeasurement(float *temperature, float *humidity);

  /* --------------------------------------------------------------------- */
  /* โหมด periodic                                                          */
  /* --------------------------------------------------------------------- */

  /*!
   * @brief สั่งชิปให้วัดเองต่อเนื่องตามอัตราที่กำหนด
   * @param rate          อัตราการวัด
   * @param repeatability ระดับความละเอียด (ถ้าไม่ระบุใช้ค่าที่ตั้งไว้)
   * @return true เมื่อส่งคำสั่งสำเร็จ
   * @note ที่ 10 Hz ความละเอียดสูง ชิปจะอุ่นตัวเองประมาณ 0.1-0.2 องศา
   */
  bool startPeriodic(massmore_sht3x_rate_t rate = MASSMORE_SHT3X_RATE_1_HZ);
  bool startPeriodic(massmore_sht3x_rate_t rate,
                     massmore_sht3x_repeatability_t repeatability);

  /*!
   * @brief เริ่มโหมด ART (Accelerated Response Time)
   *
   * ชิปวัดที่ 4 Hz พร้อมกรองภายในให้ตอบสนองเร็วขึ้นประมาณ 2 เท่า
   * เหมาะกับงานที่ความชื้นเปลี่ยนเร็ว เช่น ลมหายใจ หรือประตูตู้เปิดปิด
   * @return true เมื่อส่งคำสั่งสำเร็จ
   */
  bool startART();

  /*!
   * @brief ดึงผลล่าสุดจากชิปในโหมด periodic/ART
   * @param temperature ตัวรับอุณหภูมิ (ใส่ nullptr ได้)
   * @param humidity    ตัวรับความชื้น (ใส่ nullptr ได้)
   * @return true เมื่อมีผลใหม่, false พร้อม lastError() = ERR_NOT_READY เมื่อ
   *         ชิปยังวัดไม่เสร็จ (ชิปจะ NACK ซึ่งเป็นพฤติกรรมปกติ ไม่ใช่ความผิดพลาด)
   */
  bool fetchData(float *temperature, float *humidity);
  bool fetchData(massmore_sht3x_reading_t &reading);

  /*!
   * @brief ปั๊มข้อมูลตามจังหวะของโหมด periodic เรียกบ่อย ๆ ใน loop()
   *
   * ฟังก์ชันนี้จะ fetch ให้เองเมื่อถึงเวลาตามอัตราที่ตั้งไว้ ถ้าได้ค่าใหม่
   * จะอัปเดตค่าภายในและเรียก callback ที่ลงทะเบียนไว้
   * @return true เมื่อรอบนี้ได้ค่าใหม่
   */
  bool update();

  /*!
   * @brief หยุดโหมด periodic กลับสู่ single shot
   * @return true เมื่อส่งคำสั่งสำเร็จ
   */
  bool stopPeriodic();

  /*! @brief โหมดปัจจุบัน */
  massmore_sht3x_mode_t getMode() const;

  /*! @brief ลงทะเบียนฟังก์ชันที่จะถูกเรียกเมื่อ update() ได้ค่าใหม่ */
  void setCallback(massmore_sht3x_callback_t callback);

  /* --------------------------------------------------------------------- */
  /* ค่าล่าสุดที่เก็บไว้ภายใน (ไม่คุยกับบัส)                                   */
  /* --------------------------------------------------------------------- */

  /*! @brief อุณหภูมิล่าสุดเป็นองศาเซลเซียส */
  float getTemperature() const;
  /*! @brief อุณหภูมิล่าสุดเป็นองศาฟาเรนไฮต์ */
  float getTemperatureF() const;
  /*! @brief ความชื้นล่าสุดเป็น %RH */
  float getHumidity() const;
  /*! @brief ค่าดิบอุณหภูมิล่าสุด */
  uint16_t getRawTemperature() const;
  /*! @brief ค่าดิบความชื้นล่าสุด */
  uint16_t getRawHumidity() const;
  /*! @brief millis() ตอนที่ได้ค่าล่าสุด */
  uint32_t getLastUpdateMs() const;
  /*! @brief ผลวัดล่าสุดทั้งชุด */
  const massmore_sht3x_reading_t &getLastReading() const;

  /* --------------------------------------------------------------------- */
  /* ฮีตเตอร์                                                               */
  /* --------------------------------------------------------------------- */

  /*!
   * @brief เปิดหรือปิดฮีตเตอร์ในตัวชิป
   *
   * ฮีตเตอร์ใช้ไล่ความชื้นที่เกาะบนเซ็นเซอร์หลังอยู่ในที่ชื้นจัดนาน ๆ
   * และใช้ตรวจว่าเซ็นเซอร์ยังทำงานปกติ (plausibility check)
   * ห้ามเปิดค้าง ระหว่างเปิดค่าอุณหภูมิจะสูงกว่าจริงหลายองศา
   * @param on true = เปิด
   * @return true เมื่อส่งคำสั่งสำเร็จ
   */
  bool setHeater(bool on);
  /*! @brief เปิดฮีตเตอร์ */
  bool heaterOn();
  /*! @brief ปิดฮีตเตอร์ */
  bool heaterOff();
  /*!
   * @brief อ่านจาก status register ว่าฮีตเตอร์เปิดอยู่จริงไหม
   * @return true เมื่อบิต 13 เป็น 1
   */
  bool isHeaterOn();

  /* --------------------------------------------------------------------- */
  /* status register                                                       */
  /* --------------------------------------------------------------------- */

  /*!
   * @brief อ่าน status register ดิบ 16 บิต
   * @param status ตัวรับค่า
   * @return true เมื่ออ่านได้และ CRC ถูก
   */
  bool readStatus(uint16_t *status);

  /*!
   * @brief อ่าน status register แล้วถอดเป็นฟิลด์ให้เลย
   * @param bits ตัวรับค่า
   * @return true เมื่ออ่านได้และ CRC ถูก
   */
  bool readStatus(massmore_sht3x_status_bits_t &bits);

  /*! @brief เคลียร์บิตค้างใน status register (alert, reset, cmd, crc) */
  bool clearStatus();

  /*! @brief ถอดค่า 16 บิตเป็นฟิลด์ โดยไม่ต้องคุยกับบัส */
  static massmore_sht3x_status_bits_t decodeStatus(uint16_t raw);

  /* --------------------------------------------------------------------- */
  /* ALERT                                                                 */
  /* --------------------------------------------------------------------- */

  /*!
   * @brief ตั้ง threshold ของ ALERT ครบทั้ง 4 ชุดในครั้งเดียว
   * @param limits ค่าที่ต้องการ
   * @return true เมื่อเขียนครบทั้ง 4 ชุดสำเร็จ
   * @note ความละเอียดที่ชิปเก็บได้คือประมาณ 0.5 องศา และ 1 %RH
   *       ค่าที่อ่านกลับมาจึงอาจไม่เท่ากับที่เขียนไปเป๊ะ ๆ
   */
  bool setAlertLimits(const massmore_sht3x_alert_limits_t &limits);

  /*!
   * @brief ตั้ง threshold แบบง่าย ให้ระบุแค่ขอบบน/ขอบล่างแล้วไลบรารีเว้น
   *        hysteresis ให้เอง
   * @param lowTemperature  อุณหภูมิต่ำสุดที่ยอมรับได้
   * @param highTemperature อุณหภูมิสูงสุดที่ยอมรับได้
   * @param lowHumidity     ความชื้นต่ำสุดที่ยอมรับได้
   * @param highHumidity    ความชื้นสูงสุดที่ยอมรับได้
   * @param hysteresisT     ระยะปลอดภัยของอุณหภูมิ (ค่าเริ่มต้น 1.0 องศา)
   * @param hysteresisRH    ระยะปลอดภัยของความชื้น (ค่าเริ่มต้น 2.0 %RH)
   * @return true เมื่อเขียนสำเร็จ
   */
  bool setAlertWindow(float lowTemperature, float highTemperature,
                      float lowHumidity, float highHumidity,
                      float hysteresisT = 1.0f, float hysteresisRH = 2.0f);

  /*!
   * @brief อ่าน threshold ทั้ง 4 ชุดกลับมาจากชิป
   * @param limits ตัวรับค่า
   * @return true เมื่ออ่านครบและ CRC ถูก
   */
  bool getAlertLimits(massmore_sht3x_alert_limits_t &limits);

  /*!
   * @brief อ่านสถานะขา ALRT ทางไฟฟ้า (ต้องเรียก setAlertPin() ก่อน)
   * @return true เมื่อขาอยู่ในสถานะเตือน (HIGH)
   */
  bool isAlertPinActive() const;

  /*! @brief แพ็กคู่ (อุณหภูมิ, ความชื้น) เป็น word 16 บิตตามรูปแบบของชิป */
  static uint16_t packAlertLimit(float temperature, float humidity);
  /*! @brief แตก word 16 บิตกลับเป็นคู่ (อุณหภูมิ, ความชื้น) */
  static void unpackAlertLimit(uint16_t word, float *temperature, float *humidity);

  /* --------------------------------------------------------------------- */
  /* รีเซ็ต                                                                 */
  /* --------------------------------------------------------------------- */

  /*! @brief รีเซ็ตด้วยคำสั่ง 0x30A2 */
  bool softReset();

  /*!
   * @brief รีเซ็ตอุปกรณ์ทุกตัวบนบัสด้วย I2C general call (address 0x00 data 0x06)
   * @warning อุปกรณ์ I2C ตัวอื่นบนบัสเดียวกันจะถูกรีเซ็ตไปด้วย
   */
  bool generalCallReset();

  /*!
   * @brief รีเซ็ตทางฮาร์ดแวร์ด้วยการดึงขา RST ลง (ต้องเรียก setResetPin() ก่อน)
   * @return true เมื่อทำได้ (มีการตั้งขาไว้)
   */
  bool hardReset();

  /* --------------------------------------------------------------------- */
  /* ตัวตนของชิป                                                            */
  /* --------------------------------------------------------------------- */

  /*!
   * @brief อ่านหมายเลขซีเรียล 32 บิตที่โรงงานเขียนไว้
   * @param serial ตัวรับค่า
   * @return true เมื่ออ่านได้และ CRC ทั้งสอง word ถูก
   * @note ชิปเลียนแบบส่วนใหญ่ไม่รู้จักคำสั่ง 0x3780 จึงจะ NACK หรือคืน 0x00000000
   */
  bool readSerialNumber(uint32_t *serial);

  /*! @brief ซีเรียลที่อ่านได้ล่าสุด (0 = ยังไม่เคยอ่านสำเร็จ) */
  uint32_t getSerialNumber() const;

  /*!
   * @brief ตรวจสอบว่าเป็นชิป Sensirion SHT3x ของแท้หรือไม่
   *
   * ทดสอบพฤติกรรมของชิป 10 ข้อ ที่ของเลียนแบบมักทำไม่ครบ
   *   1. ตอบ ACK ที่ address
   *   2. status register ส่ง CRC มาถูกต้อง
   *   3. บิต reserved ใน status register เป็น 0 ตาม datasheet
   *   4. หลัง soft reset บิต "reset detected" ต้องขึ้นเป็น 1
   *   5. สั่ง clear status แล้วบิตนั้นต้องกลับเป็น 0
   *   6. อ่านซีเรียลด้วยคำสั่ง 0x3780 ได้ CRC ถูก และไม่ใช่ 0 หรือ FFFFFFFF
   *   7. สั่งเปิด/ปิดฮีตเตอร์แล้วบิต 13 ขยับตามจริง
   *   8. วัดจริงแล้ว CRC ของทั้งสอง word ถูก
   *   9. ค่าที่วัดได้อยู่ในช่วงที่ซิลิคอนตัวนี้ทำได้
   *  10. ส่งคำสั่งที่ไม่มีในตารางแล้วบิต "command failed" ต้องขึ้น
   *
   * @warning นี่คือการตรวจสอบเชิงพฤติกรรมระดับโปรโตคอล ไม่ใช่ลายเซ็นดิจิทัล
   *          ตอบได้ว่า "ชิปตัวนี้ทำตัวเหมือน SHT3x แท้ทุกประการหรือไม่"
   *          บอร์ด Massmore ใช้ชิปแท้จาก Sensirion ประกอบในไทย
   * @return ผลสรุป ดูรายละเอียดรายข้อได้จาก getVerifyMask()
   */
  massmore_sht3x_genuine_t verifyChip();

  /*! @brief บิตแมสก์ผลการตรวจรายข้อจาก verifyChip() ครั้งล่าสุด */
  uint16_t getVerifyMask() const;
  /*! @brief จำนวนข้อที่ผ่านจาก verifyChip() ครั้งล่าสุด */
  uint8_t getVerifyPassCount() const;
  /*! @brief ชื่อข้อตรวจลำดับที่ index (0..9) เป็นข้อความสั้น ๆ */
  static const char *getVerifyCheckName(uint8_t index);
  /*! @brief ผลสรุปของ verifyChip() เป็นข้อความ */
  static const char *genuineToString(massmore_sht3x_genuine_t result);

  /* --------------------------------------------------------------------- */
  /* self test                                                             */
  /* --------------------------------------------------------------------- */

  /*!
   * @brief ทดสอบว่าเซ็นเซอร์ยังตอบสนองจริงด้วยการเปิดฮีตเตอร์แล้วดูอุณหภูมิ
   *
   * เปิดฮีตเตอร์ วัดซ้ำจนครบเวลา แล้วปิด ถ้าอุณหภูมิไม่ขยับขึ้นแปลว่า
   * เซ็นเซอร์อาจเสียหรือค่าที่อ่านมาไม่ได้มาจากการวัดจริง
   * @param heatMs        เวลาที่เปิดฮีตเตอร์ (ค่าเริ่มต้น 3000 ms)
   * @param minRiseC      อุณหภูมิต้องขึ้นอย่างน้อยกี่องศาจึงถือว่าผ่าน
   * @param riseOut       ตัวรับค่าอุณหภูมิที่ขึ้นจริง (ใส่ nullptr ได้)
   * @return true เมื่อผ่าน
   * @note ฟังก์ชันนี้บล็อกนานหลายวินาที เหมาะกับใช้ตอนทดสอบเท่านั้น
   */
  bool runHeaterSelfTest(uint16_t heatMs = 3000, float minRiseC = 0.5f,
                         float *riseOut = nullptr);

  /* --------------------------------------------------------------------- */
  /* ค่าที่คำนวณต่อจากอุณหภูมิและความชื้น                                     */
  /* --------------------------------------------------------------------- */

  /*!
   * @brief จุดน้ำค้าง (dew point) ด้วยสูตร Magnus
   * @param temperature องศาเซลเซียส
   * @param humidity    %RH
   * @return องศาเซลเซียส
   */
  static float dewPoint(float temperature, float humidity);

  /*!
   * @brief ความชื้นสัมบูรณ์ (กรัมของไอน้ำต่ออากาศหนึ่งลูกบาศก์เมตร)
   * @param temperature องศาเซลเซียส
   * @param humidity    %RH
   * @return g/m^3
   */
  static float absoluteHumidity(float temperature, float humidity);

  /*!
   * @brief ดัชนีความร้อน (heat index) สูตร Rothfusz ของ NOAA
   * @param temperature องศาเซลเซียส
   * @param humidity    %RH
   * @return องศาเซลเซียส
   */
  static float heatIndex(float temperature, float humidity);

  /*!
   * @brief ความดันไออิ่มตัวที่อุณหภูมินั้น
   * @param temperature องศาเซลเซียส
   * @return hPa
   */
  static float saturationVaporPressure(float temperature);

  /*! @brief แปลงเซลเซียสเป็นฟาเรนไฮต์ */
  static float celsiusToFahrenheit(float celsius);
  /*! @brief แปลงฟาเรนไฮต์เป็นเซลเซียส */
  static float fahrenheitToCelsius(float fahrenheit);

  /* --------------------------------------------------------------------- */
  /* ยูทิลิตี้ระดับล่าง เผื่อผู้ใช้ต่อยอดเอง                                    */
  /* --------------------------------------------------------------------- */

  /*!
   * @brief คำนวณ CRC-8 ตามสเปกของ Sensirion
   * @param data ตัวชี้ข้อมูล
   * @param length จำนวนไบต์
   * @return ค่า CRC
   */
  static uint8_t crc8(const uint8_t *data, uint8_t length);

  /*!
   * @brief ส่งคำสั่ง 16 บิตดิบ ๆ ไปยังชิป
   * @param command คำสั่ง เช่น MASSMORE_SHT3X_CMD_BREAK
   * @return true เมื่อชิปตอบ ACK
   */
  bool sendCommand(uint16_t command);

  /*!
   * @brief อ่านข้อมูลดิบต่อจากคำสั่งล่าสุด พร้อมตรวจ CRC ทุก 3 ไบต์
   * @param buffer  บัฟเฟอร์ปลายทาง
   * @param length  จำนวนไบต์ที่ต้องการ (ต้องหารด้วย 3 ลงตัว)
   * @return true เมื่ออ่านครบและ CRC ถูกทุก word
   */
  bool readBytes(uint8_t *buffer, uint8_t length);

  /*!
   * @brief ส่งคำสั่งแล้วอ่าน word 16 บิตหนึ่งค่ากลับมา (ตรวจ CRC ให้แล้ว)
   * @param command คำสั่ง
   * @param value   ตัวรับค่า
   * @return true เมื่อสำเร็จ
   */
  bool readWord(uint16_t command, uint16_t *value);

  /*!
   * @brief ส่งคำสั่งพร้อมข้อมูล 16 บิตและ CRC (ใช้กับคำสั่งเขียน alert limit)
   * @param command คำสั่ง
   * @param value   ข้อมูลที่จะเขียน
   * @return true เมื่อสำเร็จ
   */
  bool writeWord(uint16_t command, uint16_t value);

  /*! @brief แปลงค่าดิบ 16 บิตเป็นองศาเซลเซียส */
  static float rawToCelsius(uint16_t raw);
  /*! @brief แปลงค่าดิบ 16 บิตเป็นองศาฟาเรนไฮต์ */
  static float rawToFahrenheit(uint16_t raw);
  /*! @brief แปลงค่าดิบ 16 บิตเป็น %RH */
  static float rawToHumidity(uint16_t raw);
  /*! @brief แปลงองศาเซลเซียสกลับเป็นค่าดิบ 16 บิต */
  static uint16_t celsiusToRaw(float celsius);
  /*! @brief แปลง %RH กลับเป็นค่าดิบ 16 บิต */
  static uint16_t humidityToRaw(float humidity);

  /* --------------------------------------------------------------------- */
  /* การรายงานข้อผิดพลาด                                                    */
  /* --------------------------------------------------------------------- */

  /*! @brief รหัสข้อผิดพลาดล่าสุด */
  massmore_sht3x_error_t lastError() const;
  /*! @brief คำอธิบายข้อผิดพลาดล่าสุดเป็นภาษาไทย */
  const char *lastErrorString() const;
  /*! @brief แปลงรหัสข้อผิดพลาดเป็นข้อความ */
  static const char *errorToString(massmore_sht3x_error_t error);
  /*! @brief ล้างรหัสข้อผิดพลาดกลับเป็น OK */
  void clearError();

  /*! @brief address ที่ใช้อยู่ */
  uint8_t getAddress() const;
  /*! @brief เวอร์ชันไลบรารีเป็นข้อความ */
  static const char *getLibraryVersion();

  /*!
   * @brief สแกนหา SHT3x บนบัสที่กำหนด (ลองทั้ง 0x44 และ 0x45)
   * @param wire  บัสที่จะสแกน
   * @param found อาเรย์รับ address ที่พบ ต้องมีที่ว่างอย่างน้อย 2 ช่อง
   * @return จำนวนที่พบ (0-2)
   */
  static uint8_t scan(TwoWire *wire, uint8_t *found);

private:
  TwoWire *_wire;
  uint8_t _address;
  bool _begun;
  int8_t _resetPin;
  int8_t _alertPin;
  uint16_t _timeoutMs;

  massmore_sht3x_variant_t _variant;
  massmore_sht3x_repeatability_t _repeatability;
  massmore_sht3x_mode_t _mode;
  massmore_sht3x_rate_t _rate;
  bool _clockStretching;

  float _temperatureOffset;
  float _humidityOffset;

  massmore_sht3x_reading_t _reading;
  uint32_t _measurementStartMs;
  bool _measurementPending;
  uint32_t _lastFetchMs;

  uint32_t _serialNumber;
  uint16_t _verifyMask;
  massmore_sht3x_genuine_t _genuine;

  massmore_sht3x_callback_t _callback;
  massmore_sht3x_error_t _error;

  /* ตัวช่วยภายใน */
  uint16_t singleShotCommand() const;
  uint16_t periodicCommand(massmore_sht3x_rate_t rate,
                           massmore_sht3x_repeatability_t repeatability) const;
  uint16_t measurementDurationMs() const;
  uint32_t periodIntervalMs() const;
  bool readMeasurementFrame(uint16_t *rawT, uint16_t *rawRH);
  void storeReading(uint16_t rawT, uint16_t rawRH);
  bool setError(massmore_sht3x_error_t error);
};

#endif /* MASSMORE_SHT3X_H */
