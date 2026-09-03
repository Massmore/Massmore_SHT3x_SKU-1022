/*!
 * @file Massmore_SHT3x.cpp
 * @brief การทำงานภายในของไลบรารี Massmore SHT3x
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license MIT
 */

#include "Massmore_SHT3x.h"

#include <math.h>

/* คำสั่งที่ไม่มีในตารางของ datasheet ใช้ทดสอบว่าชิปตั้งบิต command failed จริง */
#define MASSMORE_SHT3X_CMD_BOGUS 0x3999

/* ค่าคงที่สำหรับสูตร Magnus (ใช้ทั้ง dewPoint และความดันไออิ่มตัว) */
#define MASSMORE_SHT3X_MAGNUS_A 17.62f
#define MASSMORE_SHT3X_MAGNUS_B 243.12f

/* ========================================================================= */
/* ตัวสร้าง                                                                  */
/* ========================================================================= */

MassmoreSHT3x::MassmoreSHT3x(TwoWire *wire)
    : _wire(wire), _address(MASSMORE_SHT3X_I2C_ADDR_DEFAULT), _begun(false),
      _resetPin(-1), _alertPin(-1), _timeoutMs(MASSMORE_SHT3X_TIMEOUT_DEFAULT_MS),
      _variant(MASSMORE_SHT3X_VARIANT_AUTO),
      _repeatability(MASSMORE_SHT3X_REPEATABILITY_HIGH),
      _mode(MASSMORE_SHT3X_MODE_IDLE), _rate(MASSMORE_SHT3X_RATE_1_HZ),
      _clockStretching(false), _temperatureOffset(0.0f), _humidityOffset(0.0f),
      _measurementStartMs(0), _measurementPending(false), _lastFetchMs(0),
      _serialNumber(0), _verifyMask(0),
      _genuine(MASSMORE_SHT3X_GENUINE_UNKNOWN), _callback(nullptr),
      _error(MASSMORE_SHT3X_OK) {
  _reading.temperature = NAN;
  _reading.humidity = NAN;
  _reading.rawTemperature = 0;
  _reading.rawHumidity = 0;
  _reading.timestampMs = 0;
}

/* ========================================================================= */
/* การเริ่มต้นใช้งาน                                                          */
/* ========================================================================= */

bool MassmoreSHT3x::begin(uint8_t address, massmore_sht3x_variant_t variant,
                          int8_t sdaPin, int8_t sclPin, uint32_t frequency) {
  if (_wire == nullptr) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  if (address != MASSMORE_SHT3X_I2C_ADDR_A && address != MASSMORE_SHT3X_I2C_ADDR_B) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
  /* ESP32 และ ESP8266 เลือกขา I2C ได้อิสระ */
  if (sdaPin >= 0 && sclPin >= 0) {
    _wire->begin((int)sdaPin, (int)sclPin);
  } else {
    _wire->begin();
  }
  _wire->setClock(frequency);
#else
  /* บอร์ดอื่นใช้ขาตายตัวของฮาร์ดแวร์ */
  (void)sdaPin;
  (void)sclPin;
  _wire->begin();
  _wire->setClock(frequency);
#endif

  return beginWithExistingBus(address, variant);
}

bool MassmoreSHT3x::beginWithExistingBus(uint8_t address,
                                         massmore_sht3x_variant_t variant) {
  if (_wire == nullptr) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  if (address != MASSMORE_SHT3X_I2C_ADDR_A && address != MASSMORE_SHT3X_I2C_ADDR_B) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }

  _address = address;
  _variant = variant;
  _begun = true;
  _mode = MASSMORE_SHT3X_MODE_SINGLE_SHOT;

  /* ชิปอาจค้างอยู่ในโหมด periodic จากการรันครั้งก่อน สั่งหยุดก่อนเสมอ */
  sendCommand(MASSMORE_SHT3X_CMD_BREAK);
  delay(MASSMORE_SHT3X_CMD_GAP_MS);

  if (!softReset()) {
    _begun = false;
    return false;
  }

  /* ยืนยันว่าชิปคุยได้จริงด้วยการอ่าน status register */
  uint16_t status = 0;
  if (!readStatus(&status)) {
    _begun = false;
    return false;
  }

  clearError();
  return true;
}

bool MassmoreSHT3x::isConnected() {
  if (_wire == nullptr) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  _wire->beginTransmission(_address);
  if (_wire->endTransmission() != 0) {
    return setError(MASSMORE_SHT3X_ERR_NO_DEVICE);
  }
  return true;
}

void MassmoreSHT3x::setResetPin(int8_t pin) {
  _resetPin = pin;
  if (_resetPin >= 0) {
    /* ปล่อยเป็น input ไว้ก่อน เพราะบนบอร์ดมี pull-up อยู่แล้ว
       จะดึงลงเฉพาะตอน hardReset() เท่านั้น */
    pinMode((uint8_t)_resetPin, INPUT_PULLUP);
  }
}

void MassmoreSHT3x::setAlertPin(int8_t pin) {
  _alertPin = pin;
  if (_alertPin >= 0) {
    /* ขา ALERT ของ SHT3x เป็น push-pull ไม่ต้องมี pull-up */
    pinMode((uint8_t)_alertPin, INPUT);
  }
}

void MassmoreSHT3x::setTimeout(uint16_t milliseconds) {
  _timeoutMs = milliseconds;
}

/* ========================================================================= */
/* การตั้งค่า                                                                */
/* ========================================================================= */

void MassmoreSHT3x::setRepeatability(massmore_sht3x_repeatability_t repeatability) {
  _repeatability = repeatability;
}

massmore_sht3x_repeatability_t MassmoreSHT3x::getRepeatability() const {
  return _repeatability;
}

void MassmoreSHT3x::setClockStretching(bool enabled) { _clockStretching = enabled; }

bool MassmoreSHT3x::getClockStretching() const { return _clockStretching; }

void MassmoreSHT3x::setTemperatureOffset(float offsetCelsius) {
  _temperatureOffset = offsetCelsius;
}

float MassmoreSHT3x::getTemperatureOffset() const { return _temperatureOffset; }

void MassmoreSHT3x::setHumidityOffset(float offsetPercent) {
  _humidityOffset = offsetPercent;
}

float MassmoreSHT3x::getHumidityOffset() const { return _humidityOffset; }

void MassmoreSHT3x::setVariant(massmore_sht3x_variant_t variant) { _variant = variant; }

massmore_sht3x_variant_t MassmoreSHT3x::getVariant() const { return _variant; }

const char *MassmoreSHT3x::getVariantName() const {
  switch (_variant) {
  case MASSMORE_SHT3X_VARIANT_SHT30:
    return "SHT30";
  case MASSMORE_SHT3X_VARIANT_SHT31:
    return "SHT31";
  case MASSMORE_SHT3X_VARIANT_SHT35:
    return "SHT35";
  case MASSMORE_SHT3X_VARIANT_AUTO:
  default:
    return "SHT3x";
  }
}

float MassmoreSHT3x::getTemperatureAccuracy() const {
  switch (_variant) {
  case MASSMORE_SHT3X_VARIANT_SHT35:
    return 0.1f;
  case MASSMORE_SHT3X_VARIANT_SHT30:
  case MASSMORE_SHT3X_VARIANT_SHT31:
  default:
    return 0.2f;
  }
}

float MassmoreSHT3x::getHumidityAccuracy() const {
  switch (_variant) {
  case MASSMORE_SHT3X_VARIANT_SHT35:
    return 1.5f;
  case MASSMORE_SHT3X_VARIANT_SHT30:
  case MASSMORE_SHT3X_VARIANT_SHT31:
  default:
    return 2.0f;
  }
}

/* ========================================================================= */
/* ตัวช่วยเลือกคำสั่งและเวลา                                                  */
/* ========================================================================= */

uint16_t MassmoreSHT3x::singleShotCommand() const {
  if (_clockStretching) {
    switch (_repeatability) {
    case MASSMORE_SHT3X_REPEATABILITY_LOW:
      return MASSMORE_SHT3X_CMD_MEAS_LOW_STRETCH;
    case MASSMORE_SHT3X_REPEATABILITY_MEDIUM:
      return MASSMORE_SHT3X_CMD_MEAS_MED_STRETCH;
    case MASSMORE_SHT3X_REPEATABILITY_HIGH:
    default:
      return MASSMORE_SHT3X_CMD_MEAS_HIGH_STRETCH;
    }
  }
  switch (_repeatability) {
  case MASSMORE_SHT3X_REPEATABILITY_LOW:
    return MASSMORE_SHT3X_CMD_MEAS_LOW;
  case MASSMORE_SHT3X_REPEATABILITY_MEDIUM:
    return MASSMORE_SHT3X_CMD_MEAS_MED;
  case MASSMORE_SHT3X_REPEATABILITY_HIGH:
  default:
    return MASSMORE_SHT3X_CMD_MEAS_HIGH;
  }
}

uint16_t MassmoreSHT3x::periodicCommand(massmore_sht3x_rate_t rate,
                                        massmore_sht3x_repeatability_t rep) const {
  switch (rate) {
  case MASSMORE_SHT3X_RATE_0_5_HZ:
    if (rep == MASSMORE_SHT3X_REPEATABILITY_LOW)
      return MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_LOW;
    if (rep == MASSMORE_SHT3X_REPEATABILITY_MEDIUM)
      return MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_MED;
    return MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_HIGH;
  case MASSMORE_SHT3X_RATE_1_HZ:
    if (rep == MASSMORE_SHT3X_REPEATABILITY_LOW)
      return MASSMORE_SHT3X_CMD_PERIODIC_1HZ_LOW;
    if (rep == MASSMORE_SHT3X_REPEATABILITY_MEDIUM)
      return MASSMORE_SHT3X_CMD_PERIODIC_1HZ_MED;
    return MASSMORE_SHT3X_CMD_PERIODIC_1HZ_HIGH;
  case MASSMORE_SHT3X_RATE_2_HZ:
    if (rep == MASSMORE_SHT3X_REPEATABILITY_LOW)
      return MASSMORE_SHT3X_CMD_PERIODIC_2HZ_LOW;
    if (rep == MASSMORE_SHT3X_REPEATABILITY_MEDIUM)
      return MASSMORE_SHT3X_CMD_PERIODIC_2HZ_MED;
    return MASSMORE_SHT3X_CMD_PERIODIC_2HZ_HIGH;
  case MASSMORE_SHT3X_RATE_4_HZ:
    if (rep == MASSMORE_SHT3X_REPEATABILITY_LOW)
      return MASSMORE_SHT3X_CMD_PERIODIC_4HZ_LOW;
    if (rep == MASSMORE_SHT3X_REPEATABILITY_MEDIUM)
      return MASSMORE_SHT3X_CMD_PERIODIC_4HZ_MED;
    return MASSMORE_SHT3X_CMD_PERIODIC_4HZ_HIGH;
  case MASSMORE_SHT3X_RATE_10_HZ:
  default:
    if (rep == MASSMORE_SHT3X_REPEATABILITY_LOW)
      return MASSMORE_SHT3X_CMD_PERIODIC_10HZ_LOW;
    if (rep == MASSMORE_SHT3X_REPEATABILITY_MEDIUM)
      return MASSMORE_SHT3X_CMD_PERIODIC_10HZ_MED;
    return MASSMORE_SHT3X_CMD_PERIODIC_10HZ_HIGH;
  }
}

uint16_t MassmoreSHT3x::measurementDurationMs() const {
  switch (_repeatability) {
  case MASSMORE_SHT3X_REPEATABILITY_LOW:
    return MASSMORE_SHT3X_MEAS_DURATION_LOW_MS;
  case MASSMORE_SHT3X_REPEATABILITY_MEDIUM:
    return MASSMORE_SHT3X_MEAS_DURATION_MED_MS;
  case MASSMORE_SHT3X_REPEATABILITY_HIGH:
  default:
    return MASSMORE_SHT3X_MEAS_DURATION_HIGH_MS;
  }
}

uint32_t MassmoreSHT3x::periodIntervalMs() const {
  switch (_rate) {
  case MASSMORE_SHT3X_RATE_0_5_HZ:
    return 2000UL;
  case MASSMORE_SHT3X_RATE_1_HZ:
    return 1000UL;
  case MASSMORE_SHT3X_RATE_2_HZ:
    return 500UL;
  case MASSMORE_SHT3X_RATE_4_HZ:
    return 250UL;
  case MASSMORE_SHT3X_RATE_10_HZ:
  default:
    return 100UL;
  }
}

/* ========================================================================= */
/* ชั้นสื่อสารกับบัส I2C                                                      */
/* ========================================================================= */

bool MassmoreSHT3x::sendCommand(uint16_t command) {
  if (_wire == nullptr) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  _wire->beginTransmission(_address);
  _wire->write((uint8_t)(command >> 8));
  _wire->write((uint8_t)(command & 0xFF));
  if (_wire->endTransmission() != 0) {
    return setError(MASSMORE_SHT3X_ERR_I2C_WRITE);
  }
  return true;
}

bool MassmoreSHT3x::readBytes(uint8_t *buffer, uint8_t length) {
  if (_wire == nullptr || buffer == nullptr) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  if (length == 0 || (length % 3) != 0) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }

  uint8_t got = _wire->requestFrom(_address, length);
  if (got != length) {
    return setError(MASSMORE_SHT3X_ERR_I2C_READ);
  }

  uint32_t deadline = millis() + _timeoutMs;
  for (uint8_t i = 0; i < length; i++) {
    while (_wire->available() == 0) {
      if ((int32_t)(millis() - deadline) >= 0) {
        return setError(MASSMORE_SHT3X_ERR_TIMEOUT);
      }
    }
    buffer[i] = (uint8_t)_wire->read();
  }

  /* ทุก 2 ไบต์ข้อมูลจะมี CRC ต่อท้าย 1 ไบต์เสมอ */
  for (uint8_t i = 0; i + 2 < length; i += 3) {
    if (crc8(&buffer[i], 2) != buffer[i + 2]) {
      return setError(MASSMORE_SHT3X_ERR_CRC);
    }
  }
  return true;
}

bool MassmoreSHT3x::readWord(uint16_t command, uint16_t *value) {
  if (value == nullptr) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  if (!sendCommand(command)) {
    return false;
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);

  uint8_t buffer[MASSMORE_SHT3X_WORD_FRAME_LEN];
  if (!readBytes(buffer, MASSMORE_SHT3X_WORD_FRAME_LEN)) {
    return false;
  }
  *value = (uint16_t)((uint16_t)buffer[0] << 8) | buffer[1];
  return true;
}

bool MassmoreSHT3x::writeWord(uint16_t command, uint16_t value) {
  if (_wire == nullptr) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  uint8_t payload[2];
  payload[0] = (uint8_t)(value >> 8);
  payload[1] = (uint8_t)(value & 0xFF);

  _wire->beginTransmission(_address);
  _wire->write((uint8_t)(command >> 8));
  _wire->write((uint8_t)(command & 0xFF));
  _wire->write(payload[0]);
  _wire->write(payload[1]);
  _wire->write(crc8(payload, 2));
  if (_wire->endTransmission() != 0) {
    return setError(MASSMORE_SHT3X_ERR_I2C_WRITE);
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  return true;
}

/* ========================================================================= */
/* CRC-8                                                                     */
/* ========================================================================= */

uint8_t MassmoreSHT3x::crc8(const uint8_t *data, uint8_t length) {
  uint8_t crc = MASSMORE_SHT3X_CRC8_INIT;
  for (uint8_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x80) {
        crc = (uint8_t)((crc << 1) ^ MASSMORE_SHT3X_CRC8_POLYNOMIAL);
      } else {
        crc = (uint8_t)(crc << 1);
      }
    }
  }
  return (uint8_t)(crc ^ MASSMORE_SHT3X_CRC8_FINAL_XOR);
}

/* ========================================================================= */
/* การแปลงค่า                                                                */
/* ========================================================================= */

float MassmoreSHT3x::rawToCelsius(uint16_t raw) {
  return MASSMORE_SHT3X_T_C_OFFSET +
         MASSMORE_SHT3X_T_C_SPAN * ((float)raw / MASSMORE_SHT3X_RAW_FULL_SCALE);
}

float MassmoreSHT3x::rawToFahrenheit(uint16_t raw) {
  return MASSMORE_SHT3X_T_F_OFFSET +
         MASSMORE_SHT3X_T_F_SPAN * ((float)raw / MASSMORE_SHT3X_RAW_FULL_SCALE);
}

float MassmoreSHT3x::rawToHumidity(uint16_t raw) {
  return MASSMORE_SHT3X_RH_SPAN * ((float)raw / MASSMORE_SHT3X_RAW_FULL_SCALE);
}

uint16_t MassmoreSHT3x::celsiusToRaw(float celsius) {
  float value = (celsius - MASSMORE_SHT3X_T_C_OFFSET) / MASSMORE_SHT3X_T_C_SPAN *
                MASSMORE_SHT3X_RAW_FULL_SCALE;
  if (value < 0.0f) {
    value = 0.0f;
  }
  if (value > MASSMORE_SHT3X_RAW_FULL_SCALE) {
    value = MASSMORE_SHT3X_RAW_FULL_SCALE;
  }
  return (uint16_t)(value + 0.5f);
}

uint16_t MassmoreSHT3x::humidityToRaw(float humidity) {
  float value = humidity / MASSMORE_SHT3X_RH_SPAN * MASSMORE_SHT3X_RAW_FULL_SCALE;
  if (value < 0.0f) {
    value = 0.0f;
  }
  if (value > MASSMORE_SHT3X_RAW_FULL_SCALE) {
    value = MASSMORE_SHT3X_RAW_FULL_SCALE;
  }
  return (uint16_t)(value + 0.5f);
}

float MassmoreSHT3x::celsiusToFahrenheit(float celsius) {
  return celsius * 1.8f + 32.0f;
}

float MassmoreSHT3x::fahrenheitToCelsius(float fahrenheit) {
  return (fahrenheit - 32.0f) / 1.8f;
}

/* ========================================================================= */
/* การวัดแบบ single shot                                                     */
/* ========================================================================= */

bool MassmoreSHT3x::readMeasurementFrame(uint16_t *rawT, uint16_t *rawRH) {
  uint8_t buffer[MASSMORE_SHT3X_MEAS_FRAME_LEN];
  if (!readBytes(buffer, MASSMORE_SHT3X_MEAS_FRAME_LEN)) {
    return false;
  }
  /* ลำดับที่ชิปส่งมา: อุณหภูมิก่อน แล้วตามด้วยความชื้น */
  *rawT = (uint16_t)((uint16_t)buffer[0] << 8) | buffer[1];
  *rawRH = (uint16_t)((uint16_t)buffer[3] << 8) | buffer[4];
  return true;
}

void MassmoreSHT3x::storeReading(uint16_t rawT, uint16_t rawRH) {
  _reading.rawTemperature = rawT;
  _reading.rawHumidity = rawRH;
  _reading.temperature = rawToCelsius(rawT) + _temperatureOffset;
  _reading.humidity = rawToHumidity(rawRH) + _humidityOffset;
  /* ความชื้นหลังชดเชยแล้วต้องไม่หลุดขอบเขตทางฟิสิกส์ */
  if (_reading.humidity < 0.0f) {
    _reading.humidity = 0.0f;
  }
  if (_reading.humidity > 100.0f) {
    _reading.humidity = 100.0f;
  }
  _reading.timestampMs = millis();
}

bool MassmoreSHT3x::measure(float *temperature, float *humidity) {
  if (!_begun) {
    return setError(MASSMORE_SHT3X_ERR_NOT_BEGUN);
  }
  if (_mode == MASSMORE_SHT3X_MODE_PERIODIC || _mode == MASSMORE_SHT3X_MODE_ART) {
    /* อยู่ในโหมดต่อเนื่องอยู่แล้ว ใช้ fetchData() แทนจะถูกต้องกว่า */
    return fetchData(temperature, humidity);
  }

  if (!sendCommand(singleShotCommand())) {
    return false;
  }

  if (!_clockStretching) {
    delay(measurementDurationMs());
  }

  uint16_t rawT = 0;
  uint16_t rawRH = 0;
  if (!readMeasurementFrame(&rawT, &rawRH)) {
    return false;
  }

  storeReading(rawT, rawRH);
  if (temperature != nullptr) {
    *temperature = _reading.temperature;
  }
  if (humidity != nullptr) {
    *humidity = _reading.humidity;
  }
  clearError();
  return true;
}

bool MassmoreSHT3x::measure(massmore_sht3x_reading_t &reading) {
  if (!measure((float *)nullptr, (float *)nullptr)) {
    return false;
  }
  reading = _reading;
  return true;
}

float MassmoreSHT3x::readTemperature() {
  float t = NAN;
  if (!measure(&t, (float *)nullptr)) {
    return NAN;
  }
  return t;
}

float MassmoreSHT3x::readTemperatureF() {
  float t = readTemperature();
  if (isnan(t)) {
    return NAN;
  }
  return celsiusToFahrenheit(t);
}

float MassmoreSHT3x::readHumidity() {
  float h = NAN;
  if (!measure((float *)nullptr, &h)) {
    return NAN;
  }
  return h;
}

/* ========================================================================= */
/* การวัดแบบไม่บล็อก                                                          */
/* ========================================================================= */

bool MassmoreSHT3x::startMeasurement() {
  if (!_begun) {
    return setError(MASSMORE_SHT3X_ERR_NOT_BEGUN);
  }
  if (_mode == MASSMORE_SHT3X_MODE_PERIODIC || _mode == MASSMORE_SHT3X_MODE_ART) {
    return setError(MASSMORE_SHT3X_ERR_WRONG_MODE);
  }

  /* โหมดไม่บล็อกต้องไม่ให้ชิปดึง SCL ค้าง จึงใช้คำสั่งแบบไม่ stretch เสมอ */
  bool saved = _clockStretching;
  _clockStretching = false;
  bool ok = sendCommand(singleShotCommand());
  _clockStretching = saved;

  if (!ok) {
    _measurementPending = false;
    return false;
  }
  _measurementStartMs = millis();
  _measurementPending = true;
  return true;
}

bool MassmoreSHT3x::isMeasurementReady() const {
  if (!_measurementPending) {
    return false;
  }
  return (millis() - _measurementStartMs) >= (uint32_t)measurementDurationMs();
}

bool MassmoreSHT3x::readMeasurement(float *temperature, float *humidity) {
  if (!_measurementPending) {
    return setError(MASSMORE_SHT3X_ERR_WRONG_MODE);
  }
  if (!isMeasurementReady()) {
    return setError(MASSMORE_SHT3X_ERR_NOT_READY);
  }

  uint16_t rawT = 0;
  uint16_t rawRH = 0;
  if (!readMeasurementFrame(&rawT, &rawRH)) {
    _measurementPending = false;
    return false;
  }
  _measurementPending = false;

  storeReading(rawT, rawRH);
  if (temperature != nullptr) {
    *temperature = _reading.temperature;
  }
  if (humidity != nullptr) {
    *humidity = _reading.humidity;
  }
  if (_callback != nullptr) {
    _callback(_reading);
  }
  clearError();
  return true;
}

/* ========================================================================= */
/* โหมด periodic                                                             */
/* ========================================================================= */

bool MassmoreSHT3x::startPeriodic(massmore_sht3x_rate_t rate) {
  return startPeriodic(rate, _repeatability);
}

bool MassmoreSHT3x::startPeriodic(massmore_sht3x_rate_t rate,
                                  massmore_sht3x_repeatability_t repeatability) {
  if (!_begun) {
    return setError(MASSMORE_SHT3X_ERR_NOT_BEGUN);
  }

  /* ถ้ากำลังอยู่ในโหมดต่อเนื่องอยู่แล้ว ต้อง break ก่อนเปลี่ยนอัตรา */
  if (_mode == MASSMORE_SHT3X_MODE_PERIODIC || _mode == MASSMORE_SHT3X_MODE_ART) {
    sendCommand(MASSMORE_SHT3X_CMD_BREAK);
    delay(MASSMORE_SHT3X_CMD_GAP_MS);
  }

  _repeatability = repeatability;
  if (!sendCommand(periodicCommand(rate, repeatability))) {
    return false;
  }

  _rate = rate;
  _mode = MASSMORE_SHT3X_MODE_PERIODIC;
  _lastFetchMs = millis();
  _measurementPending = false;
  clearError();
  return true;
}

bool MassmoreSHT3x::startART() {
  if (!_begun) {
    return setError(MASSMORE_SHT3X_ERR_NOT_BEGUN);
  }
  if (_mode == MASSMORE_SHT3X_MODE_PERIODIC || _mode == MASSMORE_SHT3X_MODE_ART) {
    sendCommand(MASSMORE_SHT3X_CMD_BREAK);
    delay(MASSMORE_SHT3X_CMD_GAP_MS);
  }
  if (!sendCommand(MASSMORE_SHT3X_CMD_ART)) {
    return false;
  }
  /* ART ทำงานที่ 4 ครั้งต่อวินาทีเสมอ ตาม datasheet */
  _rate = MASSMORE_SHT3X_RATE_4_HZ;
  _mode = MASSMORE_SHT3X_MODE_ART;
  _lastFetchMs = millis();
  _measurementPending = false;
  clearError();
  return true;
}

bool MassmoreSHT3x::fetchData(float *temperature, float *humidity) {
  if (!_begun) {
    return setError(MASSMORE_SHT3X_ERR_NOT_BEGUN);
  }
  if (_mode != MASSMORE_SHT3X_MODE_PERIODIC && _mode != MASSMORE_SHT3X_MODE_ART) {
    return setError(MASSMORE_SHT3X_ERR_WRONG_MODE);
  }

  /* ถ้าชิปยังวัดไม่เสร็จจะ NACK คำสั่ง fetch ซึ่งเป็นพฤติกรรมปกติ
     จึงแยกออกจากความผิดพลาดจริงด้วยรหัส ERR_NOT_READY */
  if (!sendCommand(MASSMORE_SHT3X_CMD_FETCH_DATA)) {
    return setError(MASSMORE_SHT3X_ERR_NOT_READY);
  }

  uint16_t rawT = 0;
  uint16_t rawRH = 0;
  if (!readMeasurementFrame(&rawT, &rawRH)) {
    return false;
  }

  storeReading(rawT, rawRH);
  if (temperature != nullptr) {
    *temperature = _reading.temperature;
  }
  if (humidity != nullptr) {
    *humidity = _reading.humidity;
  }
  clearError();
  return true;
}

bool MassmoreSHT3x::fetchData(massmore_sht3x_reading_t &reading) {
  if (!fetchData((float *)nullptr, (float *)nullptr)) {
    return false;
  }
  reading = _reading;
  return true;
}

bool MassmoreSHT3x::update() {
  if (_mode != MASSMORE_SHT3X_MODE_PERIODIC && _mode != MASSMORE_SHT3X_MODE_ART) {
    return false;
  }
  if ((millis() - _lastFetchMs) < periodIntervalMs()) {
    return false;
  }

  if (!fetchData((float *)nullptr, (float *)nullptr)) {
    /* ยังไม่พร้อมก็แค่รอรอบหน้า ไม่ถือว่าเป็นความผิดพลาด */
    _lastFetchMs = millis();
    return false;
  }

  _lastFetchMs = _reading.timestampMs;
  if (_callback != nullptr) {
    _callback(_reading);
  }
  return true;
}

bool MassmoreSHT3x::stopPeriodic() {
  if (!_begun) {
    return setError(MASSMORE_SHT3X_ERR_NOT_BEGUN);
  }
  if (!sendCommand(MASSMORE_SHT3X_CMD_BREAK)) {
    return false;
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  _mode = MASSMORE_SHT3X_MODE_SINGLE_SHOT;
  clearError();
  return true;
}

massmore_sht3x_mode_t MassmoreSHT3x::getMode() const { return _mode; }

void MassmoreSHT3x::setCallback(massmore_sht3x_callback_t callback) {
  _callback = callback;
}

/* ========================================================================= */
/* ค่าล่าสุด                                                                 */
/* ========================================================================= */

float MassmoreSHT3x::getTemperature() const { return _reading.temperature; }

float MassmoreSHT3x::getTemperatureF() const {
  if (isnan(_reading.temperature)) {
    return NAN;
  }
  return celsiusToFahrenheit(_reading.temperature);
}

float MassmoreSHT3x::getHumidity() const { return _reading.humidity; }

uint16_t MassmoreSHT3x::getRawTemperature() const { return _reading.rawTemperature; }

uint16_t MassmoreSHT3x::getRawHumidity() const { return _reading.rawHumidity; }

uint32_t MassmoreSHT3x::getLastUpdateMs() const { return _reading.timestampMs; }

const massmore_sht3x_reading_t &MassmoreSHT3x::getLastReading() const {
  return _reading;
}

/* ========================================================================= */
/* ฮีตเตอร์                                                                  */
/* ========================================================================= */

bool MassmoreSHT3x::setHeater(bool on) {
  if (!_begun) {
    return setError(MASSMORE_SHT3X_ERR_NOT_BEGUN);
  }
  uint16_t cmd = on ? MASSMORE_SHT3X_CMD_HEATER_ENABLE
                    : MASSMORE_SHT3X_CMD_HEATER_DISABLE;
  if (!sendCommand(cmd)) {
    return false;
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  clearError();
  return true;
}

bool MassmoreSHT3x::heaterOn() { return setHeater(true); }

bool MassmoreSHT3x::heaterOff() { return setHeater(false); }

bool MassmoreSHT3x::isHeaterOn() {
  uint16_t status = 0;
  if (!readStatus(&status)) {
    return false;
  }
  return (status & MASSMORE_SHT3X_STATUS_HEATER_ON) != 0;
}

/* ========================================================================= */
/* status register                                                           */
/* ========================================================================= */

bool MassmoreSHT3x::readStatus(uint16_t *status) {
  if (status == nullptr) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  return readWord(MASSMORE_SHT3X_CMD_READ_STATUS, status);
}

bool MassmoreSHT3x::readStatus(massmore_sht3x_status_bits_t &bits) {
  uint16_t raw = 0;
  if (!readStatus(&raw)) {
    return false;
  }
  bits = decodeStatus(raw);
  return true;
}

bool MassmoreSHT3x::clearStatus() {
  if (!sendCommand(MASSMORE_SHT3X_CMD_CLEAR_STATUS)) {
    return false;
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  return true;
}

massmore_sht3x_status_bits_t MassmoreSHT3x::decodeStatus(uint16_t raw) {
  massmore_sht3x_status_bits_t bits;
  bits.raw = raw;
  bits.alertPending = (raw & MASSMORE_SHT3X_STATUS_ALERT_PENDING) != 0;
  bits.heaterOn = (raw & MASSMORE_SHT3X_STATUS_HEATER_ON) != 0;
  bits.humidityAlert = (raw & MASSMORE_SHT3X_STATUS_RH_ALERT) != 0;
  bits.temperatureAlert = (raw & MASSMORE_SHT3X_STATUS_T_ALERT) != 0;
  bits.resetDetected = (raw & MASSMORE_SHT3X_STATUS_RESET_DETECTED) != 0;
  bits.commandFailed = (raw & MASSMORE_SHT3X_STATUS_CMD_FAILED) != 0;
  bits.checksumFailed = (raw & MASSMORE_SHT3X_STATUS_CRC_FAILED) != 0;
  return bits;
}

/* ========================================================================= */
/* ALERT                                                                     */
/* ========================================================================= */

uint16_t MassmoreSHT3x::packAlertLimit(float temperature, float humidity) {
  uint16_t rawT = celsiusToRaw(temperature);
  uint16_t rawRH = humidityToRaw(humidity);
  /* เก็บได้แค่ 7 บิตบนของความชื้น และ 9 บิตบนของอุณหภูมิ */
  return (uint16_t)((rawRH & MASSMORE_SHT3X_ALERT_RH_MASK) |
                    ((rawT >> MASSMORE_SHT3X_ALERT_T_SHIFT) &
                     MASSMORE_SHT3X_ALERT_T_MASK));
}

void MassmoreSHT3x::unpackAlertLimit(uint16_t word, float *temperature,
                                     float *humidity) {
  if (temperature != nullptr) {
    uint16_t rawT = (uint16_t)((word & MASSMORE_SHT3X_ALERT_T_MASK)
                               << MASSMORE_SHT3X_ALERT_T_SHIFT);
    *temperature = rawToCelsius(rawT);
  }
  if (humidity != nullptr) {
    uint16_t rawRH = (uint16_t)(word & MASSMORE_SHT3X_ALERT_RH_MASK);
    *humidity = rawToHumidity(rawRH);
  }
}

bool MassmoreSHT3x::setAlertLimits(const massmore_sht3x_alert_limits_t &limits) {
  if (!_begun) {
    return setError(MASSMORE_SHT3X_ERR_NOT_BEGUN);
  }
  if (!writeWord(MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_SET,
                 packAlertLimit(limits.highSetTemperature, limits.highSetHumidity))) {
    return false;
  }
  if (!writeWord(MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_CLEAR,
                 packAlertLimit(limits.highClearTemperature,
                                limits.highClearHumidity))) {
    return false;
  }
  if (!writeWord(MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_CLEAR,
                 packAlertLimit(limits.lowClearTemperature,
                                limits.lowClearHumidity))) {
    return false;
  }
  if (!writeWord(MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_SET,
                 packAlertLimit(limits.lowSetTemperature, limits.lowSetHumidity))) {
    return false;
  }
  clearError();
  return true;
}

bool MassmoreSHT3x::setAlertWindow(float lowTemperature, float highTemperature,
                                   float lowHumidity, float highHumidity,
                                   float hysteresisT, float hysteresisRH) {
  if (highTemperature <= lowTemperature || highHumidity <= lowHumidity) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  massmore_sht3x_alert_limits_t limits;
  limits.highSetTemperature = highTemperature;
  limits.highSetHumidity = highHumidity;
  limits.highClearTemperature = highTemperature - hysteresisT;
  limits.highClearHumidity = highHumidity - hysteresisRH;
  limits.lowClearTemperature = lowTemperature + hysteresisT;
  limits.lowClearHumidity = lowHumidity + hysteresisRH;
  limits.lowSetTemperature = lowTemperature;
  limits.lowSetHumidity = lowHumidity;
  return setAlertLimits(limits);
}

bool MassmoreSHT3x::getAlertLimits(massmore_sht3x_alert_limits_t &limits) {
  if (!_begun) {
    return setError(MASSMORE_SHT3X_ERR_NOT_BEGUN);
  }
  uint16_t word = 0;

  if (!readWord(MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_SET, &word)) {
    return false;
  }
  unpackAlertLimit(word, &limits.highSetTemperature, &limits.highSetHumidity);

  if (!readWord(MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_CLEAR, &word)) {
    return false;
  }
  unpackAlertLimit(word, &limits.highClearTemperature, &limits.highClearHumidity);

  if (!readWord(MASSMORE_SHT3X_CMD_READ_ALERT_LOW_CLEAR, &word)) {
    return false;
  }
  unpackAlertLimit(word, &limits.lowClearTemperature, &limits.lowClearHumidity);

  if (!readWord(MASSMORE_SHT3X_CMD_READ_ALERT_LOW_SET, &word)) {
    return false;
  }
  unpackAlertLimit(word, &limits.lowSetTemperature, &limits.lowSetHumidity);

  clearError();
  return true;
}

bool MassmoreSHT3x::isAlertPinActive() const {
  if (_alertPin < 0) {
    return false;
  }
  return digitalRead((uint8_t)_alertPin) == HIGH;
}

/* ========================================================================= */
/* รีเซ็ต                                                                    */
/* ========================================================================= */

bool MassmoreSHT3x::softReset() {
  if (!sendCommand(MASSMORE_SHT3X_CMD_SOFT_RESET)) {
    return false;
  }
  delay(MASSMORE_SHT3X_SOFT_RESET_MS);
  _mode = MASSMORE_SHT3X_MODE_SINGLE_SHOT;
  _measurementPending = false;
  return true;
}

bool MassmoreSHT3x::generalCallReset() {
  if (_wire == nullptr) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  _wire->beginTransmission(MASSMORE_SHT3X_GENERAL_CALL_ADDR);
  _wire->write((uint8_t)MASSMORE_SHT3X_GENERAL_CALL_RESET_BYTE);
  if (_wire->endTransmission() != 0) {
    return setError(MASSMORE_SHT3X_ERR_I2C_WRITE);
  }
  delay(MASSMORE_SHT3X_SOFT_RESET_MS);
  _mode = MASSMORE_SHT3X_MODE_SINGLE_SHOT;
  _measurementPending = false;
  return true;
}

bool MassmoreSHT3x::hardReset() {
  if (_resetPin < 0) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  pinMode((uint8_t)_resetPin, OUTPUT);
  digitalWrite((uint8_t)_resetPin, LOW);
  delayMicroseconds(MASSMORE_SHT3X_HARD_RESET_PULSE_US);
  /* คืนขาเป็น input ให้ pull-up บนบอร์ดดึงขึ้นเอง ปลอดภัยกว่าขับ HIGH */
  pinMode((uint8_t)_resetPin, INPUT_PULLUP);
  delay(MASSMORE_SHT3X_POWER_UP_MS);
  _mode = MASSMORE_SHT3X_MODE_SINGLE_SHOT;
  _measurementPending = false;
  return true;
}

/* ========================================================================= */
/* ตัวตนของชิป                                                               */
/* ========================================================================= */

bool MassmoreSHT3x::readSerialNumber(uint32_t *serial) {
  if (serial == nullptr) {
    return setError(MASSMORE_SHT3X_ERR_BAD_ARG);
  }
  if (!sendCommand(MASSMORE_SHT3X_CMD_READ_SERIAL)) {
    return false;
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);

  uint8_t buffer[MASSMORE_SHT3X_SERIAL_FRAME_LEN];
  if (!readBytes(buffer, MASSMORE_SHT3X_SERIAL_FRAME_LEN)) {
    return false;
  }

  uint32_t value = ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16) |
                   ((uint32_t)buffer[3] << 8) | (uint32_t)buffer[4];
  _serialNumber = value;
  *serial = value;
  clearError();
  return true;
}

uint32_t MassmoreSHT3x::getSerialNumber() const { return _serialNumber; }

massmore_sht3x_genuine_t MassmoreSHT3x::verifyChip() {
  _verifyMask = 0;
  _genuine = MASSMORE_SHT3X_GENUINE_UNKNOWN;

  /* --- ข้อ 1: มีอุปกรณ์ตอบที่ address นี้ไหม --- */
  if (!isConnected()) {
    _genuine = MASSMORE_SHT3X_GENUINE_NOT_SHT3X;
    return _genuine;
  }
  _verifyMask |= MASSMORE_SHT3X_CHK_ACK;

  /* จำโหมดเดิมไว้ เพื่อคืนสภาพให้เหมือนก่อนเรียกฟังก์ชันนี้ */
  massmore_sht3x_mode_t savedMode = _mode;
  massmore_sht3x_rate_t savedRate = _rate;
  bool wasBegun = _begun;
  _begun = true;

  sendCommand(MASSMORE_SHT3X_CMD_BREAK);
  delay(MASSMORE_SHT3X_CMD_GAP_MS);

  /* --- ข้อ 2-4: soft reset แล้วดู status register --- */
  softReset();
  uint16_t status = 0;
  if (readStatus(&status)) {
    _verifyMask |= MASSMORE_SHT3X_CHK_STATUS_CRC;
    if ((status & MASSMORE_SHT3X_STATUS_RESERVED_MASK) == 0) {
      _verifyMask |= MASSMORE_SHT3X_CHK_STATUS_RSVD;
    }
    if (status & MASSMORE_SHT3X_STATUS_RESET_DETECTED) {
      _verifyMask |= MASSMORE_SHT3X_CHK_RESET_FLAG;
    }
  }

  /* --- ข้อ 5: เคลียร์ status แล้วบิต reset ต้องหายไป --- */
  if (clearStatus() && readStatus(&status)) {
    if ((status & MASSMORE_SHT3X_STATUS_RESET_DETECTED) == 0) {
      _verifyMask |= MASSMORE_SHT3X_CHK_CLEAR_STATUS;
    }
  }

  /* --- ข้อ 6: อ่านซีเรียลจากโรงงาน --- */
  uint32_t serial = 0;
  if (readSerialNumber(&serial)) {
    if (serial != 0x00000000UL && serial != 0xFFFFFFFFUL) {
      _verifyMask |= MASSMORE_SHT3X_CHK_SERIAL;
    }
  }

  /* --- ข้อ 7: ฮีตเตอร์ต้องขยับบิต 13 ตามคำสั่งจริง --- */
  bool heaterOk = false;
  if (setHeater(true) && readStatus(&status)) {
    heaterOk = (status & MASSMORE_SHT3X_STATUS_HEATER_ON) != 0;
  }
  if (setHeater(false) && readStatus(&status)) {
    heaterOk = heaterOk && ((status & MASSMORE_SHT3X_STATUS_HEATER_ON) == 0);
  } else {
    heaterOk = false;
  }
  if (heaterOk) {
    _verifyMask |= MASSMORE_SHT3X_CHK_HEATER;
  }

  /* --- ข้อ 8-9: วัดจริงหนึ่งครั้ง ตรวจ CRC และความสมเหตุสมผลของค่า --- */
  _mode = MASSMORE_SHT3X_MODE_SINGLE_SHOT;
  float t = NAN;
  float h = NAN;
  if (measure(&t, &h)) {
    _verifyMask |= MASSMORE_SHT3X_CHK_MEAS_CRC;
    bool rawSane = (_reading.rawTemperature != 0x0000) &&
                   (_reading.rawTemperature != 0xFFFF) &&
                   (_reading.rawHumidity != 0xFFFF);
    if (rawSane && t > -40.0f && t < 125.0f && h >= 0.0f && h <= 100.0f) {
      _verifyMask |= MASSMORE_SHT3X_CHK_MEAS_RANGE;
    }
  }

  /* --- ข้อ 10: คำสั่งที่ไม่มีในตารางต้องทำให้บิต command failed ขึ้น --- */
  clearStatus();
  sendCommand(MASSMORE_SHT3X_CMD_BOGUS);
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  if (readStatus(&status)) {
    if (status & MASSMORE_SHT3X_STATUS_CMD_FAILED) {
      _verifyMask |= MASSMORE_SHT3X_CHK_CMD_ERROR;
    }
  }
  clearStatus();

  /* คืนสภาพเดิม */
  _begun = wasBegun;
  if (savedMode == MASSMORE_SHT3X_MODE_PERIODIC) {
    startPeriodic(savedRate, _repeatability);
  } else if (savedMode == MASSMORE_SHT3X_MODE_ART) {
    startART();
  } else {
    _mode = savedMode;
  }

  /* --- สรุปผล --- */
  uint8_t passed = getVerifyPassCount();
  bool coreOk = (_verifyMask & MASSMORE_SHT3X_CHK_STATUS_CRC) &&
                (_verifyMask & MASSMORE_SHT3X_CHK_STATUS_RSVD);
  if (!coreOk) {
    /* ตอบ ACK แต่ไม่ตอบ status register แบบ SHT3x = คนละชิปแน่นอน */
    _genuine = MASSMORE_SHT3X_GENUINE_NOT_SHT3X;
  } else if (passed == MASSMORE_SHT3X_CHK_COUNT) {
    _genuine = MASSMORE_SHT3X_GENUINE_PASS;
  } else if (passed >= 8) {
    _genuine = MASSMORE_SHT3X_GENUINE_PARTIAL;
  } else {
    _genuine = MASSMORE_SHT3X_GENUINE_SUSPECT;
  }
  return _genuine;
}

uint16_t MassmoreSHT3x::getVerifyMask() const { return _verifyMask; }

uint8_t MassmoreSHT3x::getVerifyPassCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < MASSMORE_SHT3X_CHK_COUNT; i++) {
    if (_verifyMask & (1u << i)) {
      count++;
    }
  }
  return count;
}

const char *MassmoreSHT3x::getVerifyCheckName(uint8_t index) {
  switch (index) {
  case 0:
    return "ตอบ ACK ที่ address";
  case 1:
    return "CRC ของ status register";
  case 2:
    return "บิต reserved เป็นศูนย์";
  case 3:
    return "ธง reset ขึ้นหลัง soft reset";
  case 4:
    return "clear status ลบธงได้";
  case 5:
    return "ซีเรียลจากโรงงาน";
  case 6:
    return "ฮีตเตอร์ตอบสนอง";
  case 7:
    return "CRC ของผลวัด";
  case 8:
    return "ค่าที่วัดอยู่ในช่วง";
  case 9:
    return "ธง command failed";
  default:
    return "ไม่ทราบ";
  }
}

const char *MassmoreSHT3x::genuineToString(massmore_sht3x_genuine_t result) {
  switch (result) {
  case MASSMORE_SHT3X_GENUINE_PASS:
    return "ของแท้ Sensirion SHT3x";
  case MASSMORE_SHT3X_GENUINE_PARTIAL:
    return "น่าจะแท้ แต่มีบางข้อไม่ผ่าน";
  case MASSMORE_SHT3X_GENUINE_SUSPECT:
    return "น่าสงสัย ตอบไม่ตรงหลายข้อ";
  case MASSMORE_SHT3X_GENUINE_NOT_SHT3X:
    return "ไม่ใช่ SHT3x";
  case MASSMORE_SHT3X_GENUINE_UNKNOWN:
  default:
    return "ยังไม่ได้ตรวจ";
  }
}

/* ========================================================================= */
/* self test                                                                 */
/* ========================================================================= */

bool MassmoreSHT3x::runHeaterSelfTest(uint16_t heatMs, float minRiseC,
                                      float *riseOut) {
  if (!_begun) {
    return setError(MASSMORE_SHT3X_ERR_NOT_BEGUN);
  }

  massmore_sht3x_mode_t savedMode = _mode;
  if (savedMode == MASSMORE_SHT3X_MODE_PERIODIC || savedMode == MASSMORE_SHT3X_MODE_ART) {
    stopPeriodic();
  }

  float before = NAN;
  if (!measure(&before, (float *)nullptr)) {
    return false;
  }

  if (!setHeater(true)) {
    return false;
  }

  float peak = before;
  uint32_t start = millis();
  while ((millis() - start) < (uint32_t)heatMs) {
    float now = NAN;
    if (measure(&now, (float *)nullptr) && now > peak) {
      peak = now;
    }
    delay(100);
  }

  setHeater(false);

  float rise = peak - before;
  if (riseOut != nullptr) {
    *riseOut = rise;
  }

  if (savedMode == MASSMORE_SHT3X_MODE_PERIODIC) {
    startPeriodic(_rate, _repeatability);
  } else if (savedMode == MASSMORE_SHT3X_MODE_ART) {
    startART();
  }

  if (rise < minRiseC) {
    return setError(MASSMORE_SHT3X_ERR_OUT_OF_RANGE);
  }
  clearError();
  return true;
}

/* ========================================================================= */
/* ค่าที่คำนวณต่อ                                                             */
/* ========================================================================= */

float MassmoreSHT3x::saturationVaporPressure(float temperature) {
  /* สูตร Magnus ให้ผลเป็น hPa */
  return 6.112f * expf((MASSMORE_SHT3X_MAGNUS_A * temperature) /
                       (MASSMORE_SHT3X_MAGNUS_B + temperature));
}

float MassmoreSHT3x::dewPoint(float temperature, float humidity) {
  if (isnan(temperature) || isnan(humidity) || humidity <= 0.0f) {
    return NAN;
  }
  float gamma = (MASSMORE_SHT3X_MAGNUS_A * temperature) /
                    (MASSMORE_SHT3X_MAGNUS_B + temperature) +
                logf(humidity / 100.0f);
  return (MASSMORE_SHT3X_MAGNUS_B * gamma) / (MASSMORE_SHT3X_MAGNUS_A - gamma);
}

float MassmoreSHT3x::absoluteHumidity(float temperature, float humidity) {
  if (isnan(temperature) || isnan(humidity)) {
    return NAN;
  }
  /* ความดันไอจริง (hPa) แปลงเป็นกรัมต่อลูกบาศก์เมตรด้วยกฎแก๊สอุดมคติ
     AH = 216.7 * (e / (T + 273.15))  โดย e มีหน่วย hPa */
  float e = saturationVaporPressure(temperature) * (humidity / 100.0f);
  return 216.7f * (e / (temperature + 273.15f));
}

float MassmoreSHT3x::heatIndex(float temperature, float humidity) {
  if (isnan(temperature) || isnan(humidity)) {
    return NAN;
  }
  /* สูตร Rothfusz ของ NOAA ใช้หน่วยฟาเรนไฮต์ */
  float tF = celsiusToFahrenheit(temperature);
  float rh = humidity;

  /* ที่อุณหภูมิต่ำกว่า 80 F ใช้สูตรอย่างง่าย */
  float simple = 0.5f * (tF + 61.0f + ((tF - 68.0f) * 1.2f) + (rh * 0.094f));
  if ((simple + tF) / 2.0f < 80.0f) {
    return fahrenheitToCelsius(simple);
  }

  float hi = -42.379f + 2.04901523f * tF + 10.14333127f * rh -
             0.22475541f * tF * rh - 0.00683783f * tF * tF -
             0.05481717f * rh * rh + 0.00122874f * tF * tF * rh +
             0.00085282f * tF * rh * rh - 0.00000199f * tF * tF * rh * rh;

  /* ปรับแก้ตามที่ NOAA กำหนดในสองกรณีขอบ */
  if (rh < 13.0f && tF >= 80.0f && tF <= 112.0f) {
    hi -= ((13.0f - rh) / 4.0f) * sqrtf((17.0f - fabsf(tF - 95.0f)) / 17.0f);
  } else if (rh > 85.0f && tF >= 80.0f && tF <= 87.0f) {
    hi += ((rh - 85.0f) / 10.0f) * ((87.0f - tF) / 5.0f);
  }
  return fahrenheitToCelsius(hi);
}

/* ========================================================================= */
/* ข้อผิดพลาดและข้อมูลทั่วไป                                                  */
/* ========================================================================= */

bool MassmoreSHT3x::setError(massmore_sht3x_error_t error) {
  _error = error;
  return false;
}

void MassmoreSHT3x::clearError() { _error = MASSMORE_SHT3X_OK; }

massmore_sht3x_error_t MassmoreSHT3x::lastError() const { return _error; }

const char *MassmoreSHT3x::lastErrorString() const { return errorToString(_error); }

const char *MassmoreSHT3x::errorToString(massmore_sht3x_error_t error) {
  switch (error) {
  case MASSMORE_SHT3X_OK:
    return "ปกติ";
  case MASSMORE_SHT3X_ERR_NOT_BEGUN:
    return "ยังไม่ได้เรียก begin()";
  case MASSMORE_SHT3X_ERR_NO_DEVICE:
    return "ไม่พบอุปกรณ์บนบัส I2C";
  case MASSMORE_SHT3X_ERR_I2C_WRITE:
    return "เขียนลงบัส I2C ไม่สำเร็จ";
  case MASSMORE_SHT3X_ERR_I2C_READ:
    return "อ่านจากบัส I2C ได้ไม่ครบ";
  case MASSMORE_SHT3X_ERR_CRC:
    return "checksum ของข้อมูลไม่ตรง";
  case MASSMORE_SHT3X_ERR_TIMEOUT:
    return "รอข้อมูลเกินเวลาที่กำหนด";
  case MASSMORE_SHT3X_ERR_NOT_READY:
    return "ยังไม่มีผลวัดใหม่";
  case MASSMORE_SHT3X_ERR_WRONG_MODE:
    return "เรียกใช้ผิดโหมด";
  case MASSMORE_SHT3X_ERR_BAD_ARG:
    return "พารามิเตอร์ไม่ถูกต้อง";
  case MASSMORE_SHT3X_ERR_OUT_OF_RANGE:
    return "ค่าที่ได้อยู่นอกช่วงที่คาดไว้";
  default:
    return "ข้อผิดพลาดที่ไม่รู้จัก";
  }
}

uint8_t MassmoreSHT3x::getAddress() const { return _address; }

const char *MassmoreSHT3x::getLibraryVersion() { return MASSMORE_SHT3X_VERSION_STRING; }

uint8_t MassmoreSHT3x::scan(TwoWire *wire, uint8_t *found) {
  if (wire == nullptr || found == nullptr) {
    return 0;
  }
  uint8_t count = 0;
  const uint8_t candidates[2] = {MASSMORE_SHT3X_I2C_ADDR_A,
                                 MASSMORE_SHT3X_I2C_ADDR_B};
  for (uint8_t i = 0; i < 2; i++) {
    wire->beginTransmission(candidates[i]);
    if (wire->endTransmission() == 0) {
      found[count++] = candidates[i];
    }
  }
  return count;
}
