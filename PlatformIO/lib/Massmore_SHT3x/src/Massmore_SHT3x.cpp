/**
 * @file    Massmore_SHT3x.cpp
 * @brief   Implementation ของ Massmore_SHT3x driver
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license   MIT
 */

#include "Massmore_SHT3x.h"

#include <math.h>

/* Command ที่ไม่มีในตาราง Datasheet ใช้ทดสอบว่าชิปตั้ง Command-failed bit จริง */
#define MASSMORE_SHT3X_CMD_BOGUS 0x3999

/* ค่าคงที่สูตร Magnus (dewPoint / absoluteHumidity) */
#define MASSMORE_SHT3X_MAGNUS_A 17.62f
#define MASSMORE_SHT3X_MAGNUS_B 243.12f

/* ========================================================================= */
/* Constructor & begin                                                       */
/* ========================================================================= */

Massmore_SHT3x::Massmore_SHT3x(TwoWire &wirePort)
    : _wire(&wirePort), _address(MASSMORE_SHT3X_I2C_ADDR_DEFAULT), _begun(false),
      _alertPin(-1), _resetPin(-1), _timeoutMs(MASSMORE_SHT3X_TIMEOUT_DEFAULT_MS),
      _repeatability(Repeatability::RPT_HIGH), _mode(Mode::IDLE), _rate(Rate::HZ_1),
      _clockStretching(false), _temperatureOffset(0.0f), _humidityOffset(0.0f),
      _fsmState(FsmState::IDLE), _fsmStartMs(0), _lastFetchMs(0), _serialNumber(0),
      _verifyMask(0), _genuine(Genuine::UNKNOWN), _error(ErrorCode::OK) {
  _reading.temperature = NAN;
  _reading.humidity = NAN;
  _reading.rawTemperature = 0;
  _reading.rawHumidity = 0;
  _reading.timestampMs = 0;
}

bool Massmore_SHT3x::begin(uint8_t address, int8_t alertPin, int8_t resetPin) {
  if (address != MASSMORE_SHT3X_I2C_ADDR_A && address != MASSMORE_SHT3X_I2C_ADDR_B) {
    return setError(ErrorCode::BAD_ARG);
  }
  _address = address;
  _alertPin = alertPin;
  _resetPin = resetPin;

  if (_alertPin >= 0) {
    /* ขา ALERT ของ SHT3x เป็น Push-pull ไม่ต้องใช้ Pull-up */
    pinMode((uint8_t)_alertPin, INPUT);
  }
  if (_resetPin >= 0) {
    /* ปล่อยเป็น Input ไว้ก่อน บนบอร์ดมี Pull-up ที่ nRESET อยู่แล้ว ดึงลงเฉพาะตอน hardReset() */
    pinMode((uint8_t)_resetPin, INPUT_PULLUP);
  }

  if (!isConnected()) {
    return false;
  }

  _begun = true;
  _mode = Mode::SINGLE_SHOT;
  _fsmState = FsmState::IDLE;

  /* ชิปอาจค้างอยู่ใน Periodic Mode จากการรันครั้งก่อน สั่ง Break ก่อนเสมอ */
  sendCommand(MASSMORE_SHT3X_CMD_BREAK);
  delay(MASSMORE_SHT3X_CMD_GAP_MS);

  if (!softReset()) {
    _begun = false;
    _mode = Mode::IDLE;
    return false;
  }

  /* ยืนยันตัวตนผ่าน Status Register (SHT3x ไม่มี CHIP_ID register) */
  if (!verifyChipID()) {
    _begun = false;
    _mode = Mode::IDLE;
    return false;
  }

  clearError();
  return true;
}

bool Massmore_SHT3x::isConnected() {
  _wire->beginTransmission(_address);
  if (_wire->endTransmission() != 0) {
    return setError(ErrorCode::NOT_FOUND);
  }
  return true;
}

/* ========================================================================= */
/* Command / timing helpers                                                  */
/* ========================================================================= */

uint16_t Massmore_SHT3x::singleShotCommand(bool stretch) const {
  if (stretch) {
    switch (_repeatability) {
    case Repeatability::RPT_LOW:    return MASSMORE_SHT3X_CMD_MEAS_LOW_STRETCH;
    case Repeatability::RPT_MEDIUM: return MASSMORE_SHT3X_CMD_MEAS_MED_STRETCH;
    default:                    return MASSMORE_SHT3X_CMD_MEAS_HIGH_STRETCH;
    }
  }
  switch (_repeatability) {
  case Repeatability::RPT_LOW:    return MASSMORE_SHT3X_CMD_MEAS_LOW;
  case Repeatability::RPT_MEDIUM: return MASSMORE_SHT3X_CMD_MEAS_MED;
  default:                    return MASSMORE_SHT3X_CMD_MEAS_HIGH;
  }
}

uint16_t Massmore_SHT3x::periodicCommand(Rate rate, Repeatability rep) {
  /* ตาราง 5 rate x 3 repeatability เก็บใน PROGMEM-friendly static const */
  static const uint16_t table[5][3] = {
      {MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_LOW, MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_MED,
       MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_HIGH},
      {MASSMORE_SHT3X_CMD_PERIODIC_1HZ_LOW, MASSMORE_SHT3X_CMD_PERIODIC_1HZ_MED,
       MASSMORE_SHT3X_CMD_PERIODIC_1HZ_HIGH},
      {MASSMORE_SHT3X_CMD_PERIODIC_2HZ_LOW, MASSMORE_SHT3X_CMD_PERIODIC_2HZ_MED,
       MASSMORE_SHT3X_CMD_PERIODIC_2HZ_HIGH},
      {MASSMORE_SHT3X_CMD_PERIODIC_4HZ_LOW, MASSMORE_SHT3X_CMD_PERIODIC_4HZ_MED,
       MASSMORE_SHT3X_CMD_PERIODIC_4HZ_HIGH},
      {MASSMORE_SHT3X_CMD_PERIODIC_10HZ_LOW, MASSMORE_SHT3X_CMD_PERIODIC_10HZ_MED,
       MASSMORE_SHT3X_CMD_PERIODIC_10HZ_HIGH},
  };
  uint8_t r = (uint8_t)rate;
  uint8_t p = (uint8_t)rep;
  if (r > 4) r = 4;
  if (p > 2) p = 2;
  return table[r][p];
}

uint16_t Massmore_SHT3x::measurementDurationMs() const {
  switch (_repeatability) {
  case Repeatability::RPT_LOW:    return MASSMORE_SHT3X_MEAS_DURATION_LOW_MS;
  case Repeatability::RPT_MEDIUM: return MASSMORE_SHT3X_MEAS_DURATION_MED_MS;
  default:                    return MASSMORE_SHT3X_MEAS_DURATION_HIGH_MS;
  }
}

uint32_t Massmore_SHT3x::periodIntervalMs() const {
  switch (_rate) {
  case Rate::HZ_0_5: return 2000UL;
  case Rate::HZ_1:   return 1000UL;
  case Rate::HZ_2:   return 500UL;
  case Rate::HZ_4:   return 250UL;
  default:           return 100UL;
  }
}

/* ========================================================================= */
/* I2C transport layer                                                       */
/* ========================================================================= */

bool Massmore_SHT3x::sendCommand(uint16_t command) {
  _wire->beginTransmission(_address);
  _wire->write((uint8_t)(command >> 8));
  _wire->write((uint8_t)(command & 0xFF));
  if (_wire->endTransmission() != 0) {
    return setError(ErrorCode::BUS_ERROR);
  }
  return true;
}

bool Massmore_SHT3x::readBytes(uint8_t *buffer, uint8_t length) {
  if (buffer == nullptr || length == 0 || (length % 3) != 0) {
    return setError(ErrorCode::BAD_ARG);
  }

  uint8_t got = _wire->requestFrom(_address, length);
  if (got != length) {
    return setError(ErrorCode::BUS_ERROR);
  }

  /* Rollover-safe timeout */
  uint32_t t0 = millis();
  for (uint8_t i = 0; i < length; i++) {
    while (_wire->available() == 0) {
      if ((uint32_t)(millis() - t0) >= _timeoutMs) {
        return setError(ErrorCode::TIMEOUT);
      }
    }
    buffer[i] = (uint8_t)_wire->read();
  }

  /* ทุก 2 byte ข้อมูลมี CRC ต่อท้าย 1 byte */
  for (uint8_t i = 0; i + 2 < length; i += 3) {
    if (crc8(&buffer[i], 2) != buffer[i + 2]) {
      return setError(ErrorCode::CRC_FAIL);
    }
  }
  return true;
}

bool Massmore_SHT3x::readWord(uint16_t command, uint16_t &value) {
  if (!sendCommand(command)) {
    return false;
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  uint8_t buffer[MASSMORE_SHT3X_WORD_FRAME_LEN];
  if (!readBytes(buffer, MASSMORE_SHT3X_WORD_FRAME_LEN)) {
    return false;
  }
  value = (uint16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
  return true;
}

bool Massmore_SHT3x::writeWord(uint16_t command, uint16_t value) {
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
    return setError(ErrorCode::BUS_ERROR);
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  return true;
}

uint8_t Massmore_SHT3x::crc8(const uint8_t *data, uint8_t length) {
  uint8_t crc = MASSMORE_SHT3X_CRC8_INIT;
  for (uint8_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ MASSMORE_SHT3X_CRC8_POLYNOMIAL)
                         : (uint8_t)(crc << 1);
    }
  }
  return (uint8_t)(crc ^ MASSMORE_SHT3X_CRC8_FINAL_XOR);
}

/* ========================================================================= */
/* Signal conversion                                                         */
/* ========================================================================= */

float Massmore_SHT3x::rawToCelsius(uint16_t raw) {
  return MASSMORE_SHT3X_T_C_OFFSET +
         MASSMORE_SHT3X_T_C_SPAN * ((float)raw / MASSMORE_SHT3X_RAW_FULL_SCALE);
}

float Massmore_SHT3x::rawToHumidity(uint16_t raw) {
  return MASSMORE_SHT3X_RH_SPAN * ((float)raw / MASSMORE_SHT3X_RAW_FULL_SCALE);
}

uint16_t Massmore_SHT3x::celsiusToRaw(float celsius) {
  float v = (celsius - MASSMORE_SHT3X_T_C_OFFSET) / MASSMORE_SHT3X_T_C_SPAN *
            MASSMORE_SHT3X_RAW_FULL_SCALE;
  if (v < 0.0f) v = 0.0f;
  if (v > MASSMORE_SHT3X_RAW_FULL_SCALE) v = MASSMORE_SHT3X_RAW_FULL_SCALE;
  return (uint16_t)(v + 0.5f);
}

uint16_t Massmore_SHT3x::humidityToRaw(float humidity) {
  float v = humidity / MASSMORE_SHT3X_RH_SPAN * MASSMORE_SHT3X_RAW_FULL_SCALE;
  if (v < 0.0f) v = 0.0f;
  if (v > MASSMORE_SHT3X_RAW_FULL_SCALE) v = MASSMORE_SHT3X_RAW_FULL_SCALE;
  return (uint16_t)(v + 0.5f);
}

/* ========================================================================= */
/* Measurement core                                                          */
/* ========================================================================= */

bool Massmore_SHT3x::readMeasurementFrame(uint16_t &rawT, uint16_t &rawRH) {
  uint8_t buffer[MASSMORE_SHT3X_MEAS_FRAME_LEN];
  if (!readBytes(buffer, MASSMORE_SHT3X_MEAS_FRAME_LEN)) {
    return false;
  }
  /* ชิปส่งอุณหภูมิก่อน ตามด้วยความชื้น */
  rawT = (uint16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
  rawRH = (uint16_t)(((uint16_t)buffer[3] << 8) | buffer[4]);
  return true;
}

void Massmore_SHT3x::storeReading(uint16_t rawT, uint16_t rawRH) {
  _reading.rawTemperature = rawT;
  _reading.rawHumidity = rawRH;
  _reading.temperature = rawToCelsius(rawT) + _temperatureOffset;
  _reading.humidity = rawToHumidity(rawRH) + _humidityOffset;
  if (_reading.humidity < MASSMORE_SHT3X_RH_MIN) _reading.humidity = MASSMORE_SHT3X_RH_MIN;
  if (_reading.humidity > MASSMORE_SHT3X_RH_MAX) _reading.humidity = MASSMORE_SHT3X_RH_MAX;
  _reading.timestampMs = millis();
}

bool Massmore_SHT3x::measureBlocking() {
  if (!_begun) {
    return setError(ErrorCode::NOT_BEGUN);
  }
  if (inContinuousMode()) {
    /* อยู่ใน Periodic อยู่แล้ว ดึงผลล่าสุดแทน */
    return fetchData(_reading);
  }

  /* ยกเลิก FSM ที่ค้างอยู่ เพราะ Blocking call จะเริ่มวัดใหม่ */
  _fsmState = FsmState::IDLE;

  if (!sendCommand(singleShotCommand(_clockStretching))) {
    return false;
  }
  if (!_clockStretching) {
    delay(measurementDurationMs());
  }

  uint16_t rawT = 0;
  uint16_t rawRH = 0;
  if (!readMeasurementFrame(rawT, rawRH)) {
    return false;
  }
  storeReading(rawT, rawRH);
  clearError();
  return true;
}

/* ========================================================================= */
/* Simple Blocking API                                                       */
/* ========================================================================= */

float Massmore_SHT3x::readTemperature() {
  return measureBlocking() ? _reading.temperature : NAN;
}

float Massmore_SHT3x::readHumidity() {
  return measureBlocking() ? _reading.humidity : NAN;
}

bool Massmore_SHT3x::readAll(Reading &reading) {
  if (!measureBlocking()) {
    return false;
  }
  reading = _reading;
  return true;
}

/* ========================================================================= */
/* Non-blocking FSM API                                                      */
/* ========================================================================= */

bool Massmore_SHT3x::requestConversion() {
  if (!_begun) {
    return setError(ErrorCode::NOT_BEGUN);
  }
  if (inContinuousMode()) {
    return setError(ErrorCode::WRONG_MODE);
  }
  /* FSM ต้องไม่ให้ชิปดึง SCL ค้าง จึงใช้ Command แบบไม่ Stretch เสมอ */
  if (!sendCommand(singleShotCommand(false))) {
    _fsmState = FsmState::FAULT;
    return false;
  }
  _fsmStartMs = millis();
  _fsmState = FsmState::CONVERTING;
  clearError();
  return true;
}

bool Massmore_SHT3x::update() {
  if (!_begun) {
    return false;
  }

  /* --- Periodic / ART: ดึงผลตาม Rate --- */
  if (inContinuousMode()) {
    if ((uint32_t)(millis() - _lastFetchMs) < periodIntervalMs()) {
      return false;
    }
    _lastFetchMs = millis();
    if (!fetchData(_reading)) {
      /* NOT_READY เป็นเรื่องปกติ รอรอบหน้า */
      return false;
    }
    _fsmState = FsmState::READY;
    return true;
  }

  /* --- Single Shot FSM --- */
  if (_fsmState != FsmState::CONVERTING) {
    return false;
  }
  if ((uint32_t)(millis() - _fsmStartMs) < (uint32_t)measurementDurationMs()) {
    return false;
  }

  uint16_t rawT = 0;
  uint16_t rawRH = 0;
  if (!readMeasurementFrame(rawT, rawRH)) {
    _fsmState = FsmState::FAULT;
    return false;
  }
  storeReading(rawT, rawRH);
  _fsmState = FsmState::READY;
  clearError();
  return true;
}

bool Massmore_SHT3x::getReadings(Reading &reading) {
  if (_fsmState != FsmState::READY) {
    return setError(ErrorCode::NOT_READY);
  }
  reading = _reading;
  _fsmState = FsmState::IDLE;
  return true;
}

/* ========================================================================= */
/* Periodic / ART                                                            */
/* ========================================================================= */

bool Massmore_SHT3x::startPeriodic(Rate rate, Repeatability repeatability) {
  if (!_begun) {
    return setError(ErrorCode::NOT_BEGUN);
  }
  if (inContinuousMode()) {
    sendCommand(MASSMORE_SHT3X_CMD_BREAK);
    delay(MASSMORE_SHT3X_CMD_GAP_MS);
  }
  _repeatability = repeatability;
  if (!sendCommand(periodicCommand(rate, repeatability))) {
    return false;
  }
  _rate = rate;
  _mode = Mode::PERIODIC;
  _lastFetchMs = millis();
  _fsmState = FsmState::IDLE;
  clearError();
  return true;
}

bool Massmore_SHT3x::startART() {
  if (!_begun) {
    return setError(ErrorCode::NOT_BEGUN);
  }
  if (inContinuousMode()) {
    sendCommand(MASSMORE_SHT3X_CMD_BREAK);
    delay(MASSMORE_SHT3X_CMD_GAP_MS);
  }
  if (!sendCommand(MASSMORE_SHT3X_CMD_ART)) {
    return false;
  }
  _rate = Rate::HZ_4; /* ART ทำงานที่ 4 Hz เสมอ (Datasheet 4.7) */
  _mode = Mode::ART;
  _lastFetchMs = millis();
  _fsmState = FsmState::IDLE;
  clearError();
  return true;
}

bool Massmore_SHT3x::fetchData(Reading &reading) {
  if (!_begun) {
    return setError(ErrorCode::NOT_BEGUN);
  }
  if (!inContinuousMode()) {
    return setError(ErrorCode::WRONG_MODE);
  }
  /* ชิป NACK คำสั่ง Fetch ถ้ายังไม่มีผลใหม่ ถือเป็น NOT_READY ไม่ใช่ Bus error */
  if (!sendCommand(MASSMORE_SHT3X_CMD_FETCH_DATA)) {
    return setError(ErrorCode::NOT_READY);
  }
  uint16_t rawT = 0;
  uint16_t rawRH = 0;
  if (!readMeasurementFrame(rawT, rawRH)) {
    return false;
  }
  storeReading(rawT, rawRH);
  reading = _reading;
  clearError();
  return true;
}

bool Massmore_SHT3x::stopPeriodic() {
  if (!_begun) {
    return setError(ErrorCode::NOT_BEGUN);
  }
  if (!sendCommand(MASSMORE_SHT3X_CMD_BREAK)) {
    return false;
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  _mode = Mode::SINGLE_SHOT;
  _fsmState = FsmState::IDLE;
  clearError();
  return true;
}

/* ========================================================================= */
/* Heater & Status                                                           */
/* ========================================================================= */

bool Massmore_SHT3x::setHeater(bool on) {
  if (!sendCommand(on ? MASSMORE_SHT3X_CMD_HEATER_ENABLE : MASSMORE_SHT3X_CMD_HEATER_DISABLE)) {
    return false;
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  clearError();
  return true;
}

bool Massmore_SHT3x::isHeaterOn() {
  uint16_t status = 0;
  if (!readStatus(status)) {
    return false;
  }
  return (status & MASSMORE_SHT3X_STATUS_HEATER_ON) != 0;
}

bool Massmore_SHT3x::readStatus(uint16_t &status) {
  return readWord(MASSMORE_SHT3X_CMD_READ_STATUS, status);
}

bool Massmore_SHT3x::readStatus(StatusBits &bits) {
  uint16_t raw = 0;
  if (!readStatus(raw)) {
    return false;
  }
  bits = decodeStatus(raw);
  return true;
}

bool Massmore_SHT3x::clearStatus() {
  if (!sendCommand(MASSMORE_SHT3X_CMD_CLEAR_STATUS)) {
    return false;
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  return true;
}

Massmore_SHT3x::StatusBits Massmore_SHT3x::decodeStatus(uint16_t raw) {
  StatusBits b;
  b.raw = raw;
  b.alertPending = (raw & MASSMORE_SHT3X_STATUS_ALERT_PENDING) != 0;
  b.heaterOn = (raw & MASSMORE_SHT3X_STATUS_HEATER_ON) != 0;
  b.humidityAlert = (raw & MASSMORE_SHT3X_STATUS_RH_ALERT) != 0;
  b.temperatureAlert = (raw & MASSMORE_SHT3X_STATUS_T_ALERT) != 0;
  b.resetDetected = (raw & MASSMORE_SHT3X_STATUS_RESET_DETECTED) != 0;
  b.commandFailed = (raw & MASSMORE_SHT3X_STATUS_CMD_FAILED) != 0;
  b.checksumFailed = (raw & MASSMORE_SHT3X_STATUS_CRC_FAILED) != 0;
  return b;
}

/* ========================================================================= */
/* ALERT                                                                     */
/* ========================================================================= */

uint16_t Massmore_SHT3x::packAlertLimit(float temperature, float humidity) {
  uint16_t rawT = celsiusToRaw(temperature);
  uint16_t rawRH = humidityToRaw(humidity);
  /* Read-Modify-Write ด้วย Bitmask: 7 bit บนของ RH | 9 bit บนของ T */
  return (uint16_t)((rawRH & MASSMORE_SHT3X_ALERT_RH_MASK) |
                    ((rawT >> MASSMORE_SHT3X_ALERT_T_SHIFT) & MASSMORE_SHT3X_ALERT_T_MASK));
}

void Massmore_SHT3x::unpackAlertLimit(uint16_t word, float &temperature, float &humidity) {
  uint16_t rawT = (uint16_t)((word & MASSMORE_SHT3X_ALERT_T_MASK) << MASSMORE_SHT3X_ALERT_T_SHIFT);
  uint16_t rawRH = (uint16_t)(word & MASSMORE_SHT3X_ALERT_RH_MASK);
  temperature = rawToCelsius(rawT);
  humidity = rawToHumidity(rawRH);
}

bool Massmore_SHT3x::setAlertLimits(const AlertLimits &l) {
  if (!_begun) {
    return setError(ErrorCode::NOT_BEGUN);
  }
  if (!writeWord(MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_SET,
                 packAlertLimit(l.highSetTemperature, l.highSetHumidity)))
    return false;
  if (!writeWord(MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_CLEAR,
                 packAlertLimit(l.highClearTemperature, l.highClearHumidity)))
    return false;
  if (!writeWord(MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_CLEAR,
                 packAlertLimit(l.lowClearTemperature, l.lowClearHumidity)))
    return false;
  if (!writeWord(MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_SET,
                 packAlertLimit(l.lowSetTemperature, l.lowSetHumidity)))
    return false;
  clearError();
  return true;
}

bool Massmore_SHT3x::setAlertWindow(float lowT, float highT, float lowRH, float highRH,
                                    float hysT, float hysRH) {
  if (highT <= lowT || highRH <= lowRH) {
    return setError(ErrorCode::BAD_ARG);
  }
  AlertLimits l;
  l.highSetTemperature = highT;          l.highSetHumidity = highRH;
  l.highClearTemperature = highT - hysT; l.highClearHumidity = highRH - hysRH;
  l.lowClearTemperature = lowT + hysT;   l.lowClearHumidity = lowRH + hysRH;
  l.lowSetTemperature = lowT;            l.lowSetHumidity = lowRH;
  return setAlertLimits(l);
}

bool Massmore_SHT3x::getAlertLimits(AlertLimits &l) {
  if (!_begun) {
    return setError(ErrorCode::NOT_BEGUN);
  }
  uint16_t w = 0;
  if (!readWord(MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_SET, w)) return false;
  unpackAlertLimit(w, l.highSetTemperature, l.highSetHumidity);
  if (!readWord(MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_CLEAR, w)) return false;
  unpackAlertLimit(w, l.highClearTemperature, l.highClearHumidity);
  if (!readWord(MASSMORE_SHT3X_CMD_READ_ALERT_LOW_CLEAR, w)) return false;
  unpackAlertLimit(w, l.lowClearTemperature, l.lowClearHumidity);
  if (!readWord(MASSMORE_SHT3X_CMD_READ_ALERT_LOW_SET, w)) return false;
  unpackAlertLimit(w, l.lowSetTemperature, l.lowSetHumidity);
  clearError();
  return true;
}

bool Massmore_SHT3x::isAlertPinActive() const {
  if (_alertPin < 0) {
    return false;
  }
  return digitalRead((uint8_t)_alertPin) == HIGH;
}

/* ========================================================================= */
/* Reset                                                                     */
/* ========================================================================= */

bool Massmore_SHT3x::softReset() {
  if (!sendCommand(MASSMORE_SHT3X_CMD_SOFT_RESET)) {
    return false;
  }
  delay(MASSMORE_SHT3X_SOFT_RESET_MS);
  if (_begun) _mode = Mode::SINGLE_SHOT;
  _fsmState = FsmState::IDLE;
  return true;
}

bool Massmore_SHT3x::generalCallReset() {
  _wire->beginTransmission(MASSMORE_SHT3X_GENERAL_CALL_ADDR);
  _wire->write((uint8_t)MASSMORE_SHT3X_GENERAL_CALL_RESET_BYTE);
  if (_wire->endTransmission() != 0) {
    return setError(ErrorCode::BUS_ERROR);
  }
  delay(MASSMORE_SHT3X_SOFT_RESET_MS);
  if (_begun) _mode = Mode::SINGLE_SHOT;
  _fsmState = FsmState::IDLE;
  return true;
}

bool Massmore_SHT3x::hardReset() {
  if (_resetPin < 0) {
    return setError(ErrorCode::BAD_ARG);
  }
  pinMode((uint8_t)_resetPin, OUTPUT);
  digitalWrite((uint8_t)_resetPin, LOW);
  delayMicroseconds(MASSMORE_SHT3X_HARD_RESET_PULSE_US);
  /* คืนเป็น Input ให้ Pull-up บนบอร์ดดึงขึ้นเอง ปลอดภัยกว่าขับ HIGH */
  pinMode((uint8_t)_resetPin, INPUT_PULLUP);
  delay(MASSMORE_SHT3X_POWER_UP_MS);
  if (_begun) _mode = Mode::SINGLE_SHOT;
  _fsmState = FsmState::IDLE;
  return true;
}

/* ========================================================================= */
/* Chip identity                                                             */
/* ========================================================================= */

bool Massmore_SHT3x::verifyChipID() {
  uint16_t status = 0;
  if (!readStatus(status)) {
    return false; /* lastError = BUS_ERROR / CRC_FAIL / TIMEOUT */
  }
  /* Reserved bit ต้องเป็น 0 ตาม Datasheet Table 18 */
  if ((status & MASSMORE_SHT3X_STATUS_RESERVED_MASK) != 0) {
    return setError(ErrorCode::WRONG_ID);
  }
  return true;
}

uint32_t Massmore_SHT3x::getSerialNumber() {
  if (!sendCommand(MASSMORE_SHT3X_CMD_READ_SERIAL)) {
    return 0;
  }
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  uint8_t buffer[MASSMORE_SHT3X_SERIAL_FRAME_LEN];
  if (!readBytes(buffer, MASSMORE_SHT3X_SERIAL_FRAME_LEN)) {
    return 0;
  }
  _serialNumber = ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16) |
                  ((uint32_t)buffer[3] << 8) | (uint32_t)buffer[4];
  clearError();
  return _serialNumber;
}

bool Massmore_SHT3x::isGenuine() {
  _verifyMask = 0;
  _genuine = Genuine::UNKNOWN;

  /* 1. ACK */
  if (!isConnected()) {
    _genuine = Genuine::NOT_SHT3X;
    return false;
  }
  _verifyMask |= CHK_ACK;

  /* จำ Mode เดิมไว้เพื่อคืนสภาพหลังตรวจ */
  Mode savedMode = _mode;
  Rate savedRate = _rate;
  bool wasBegun = _begun;
  _begun = true;

  sendCommand(MASSMORE_SHT3X_CMD_BREAK);
  delay(MASSMORE_SHT3X_CMD_GAP_MS);

  /* 2-4. Soft Reset แล้วดู Status Register */
  softReset();
  uint16_t status = 0;
  if (readStatus(status)) {
    _verifyMask |= CHK_STATUS_CRC;
    if ((status & MASSMORE_SHT3X_STATUS_RESERVED_MASK) == 0) _verifyMask |= CHK_STATUS_RSVD;
    if (status & MASSMORE_SHT3X_STATUS_RESET_DETECTED) _verifyMask |= CHK_RESET_FLAG;
  }

  /* 5. Clear Status ต้องลบ Reset bit ได้ */
  if (clearStatus() && readStatus(status)) {
    if ((status & MASSMORE_SHT3X_STATUS_RESET_DETECTED) == 0) _verifyMask |= CHK_CLEAR_STATUS;
  }

  /* 6. Serial Number */
  uint32_t serial = getSerialNumber();
  if (serial != 0x00000000UL && serial != 0xFFFFFFFFUL) _verifyMask |= CHK_SERIAL;

  /* 7. Heater bit 13 ต้องตามคำสั่ง */
  bool heaterOk = false;
  if (setHeater(true) && readStatus(status)) {
    heaterOk = (status & MASSMORE_SHT3X_STATUS_HEATER_ON) != 0;
  }
  if (setHeater(false) && readStatus(status)) {
    heaterOk = heaterOk && ((status & MASSMORE_SHT3X_STATUS_HEATER_ON) == 0);
  } else {
    heaterOk = false;
  }
  if (heaterOk) _verifyMask |= CHK_HEATER;

  /* 8-9. วัดจริง 1 ครั้ง: CRC และ Physical range */
  _mode = Mode::SINGLE_SHOT;
  if (measureBlocking()) {
    _verifyMask |= CHK_MEAS_CRC;
    bool rawSane = _reading.rawTemperature != 0x0000 && _reading.rawTemperature != 0xFFFF &&
                   _reading.rawHumidity != 0xFFFF;
    float t = _reading.temperature;
    float h = _reading.humidity;
    if (rawSane && t > MASSMORE_SHT3X_T_MIN_C && t < MASSMORE_SHT3X_T_MAX_C &&
        h >= MASSMORE_SHT3X_RH_MIN && h <= MASSMORE_SHT3X_RH_MAX) {
      _verifyMask |= CHK_MEAS_RANGE;
    }
  }

  /* 10. Command ที่ไม่มีในตารางต้องทำให้ Command-failed bit ขึ้น */
  clearStatus();
  sendCommand(MASSMORE_SHT3X_CMD_BOGUS);
  delay(MASSMORE_SHT3X_CMD_GAP_MS);
  if (readStatus(status) && (status & MASSMORE_SHT3X_STATUS_CMD_FAILED)) {
    _verifyMask |= CHK_CMD_ERROR;
  }
  clearStatus();

  /* คืนสภาพเดิม */
  _begun = wasBegun;
  if (savedMode == Mode::PERIODIC) {
    startPeriodic(savedRate, _repeatability);
  } else if (savedMode == Mode::ART) {
    startART();
  } else {
    _mode = savedMode;
  }

  uint8_t passed = getVerifyPassCount();
  bool coreOk = (_verifyMask & CHK_STATUS_CRC) && (_verifyMask & CHK_STATUS_RSVD);
  if (!coreOk) {
    _genuine = Genuine::NOT_SHT3X;
  } else if (passed == VERIFY_CHECK_COUNT) {
    _genuine = Genuine::PASS;
  } else if (passed >= 8) {
    _genuine = Genuine::PARTIAL;
  } else {
    _genuine = Genuine::SUSPECT;
  }
  return _genuine == Genuine::PASS;
}

uint8_t Massmore_SHT3x::getVerifyPassCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < VERIFY_CHECK_COUNT; i++) {
    if (_verifyMask & (1u << i)) count++;
  }
  return count;
}

const char *Massmore_SHT3x::getVerifyCheckName(uint8_t index) {
  switch (index) {
  case 0: return "ACK";
  case 1: return "STATUS_CRC";
  case 2: return "STATUS_RESERVED_ZERO";
  case 3: return "RESET_FLAG";
  case 4: return "CLEAR_STATUS";
  case 5: return "SERIAL_NUMBER";
  case 6: return "HEATER_BIT";
  case 7: return "MEAS_CRC";
  case 8: return "MEAS_RANGE";
  case 9: return "CMD_FAILED_FLAG";
  default: return "UNKNOWN";
  }
}

const char *Massmore_SHT3x::genuineToString(Genuine v) {
  switch (v) {
  case Genuine::PASS:      return "GENUINE";
  case Genuine::PARTIAL:   return "PARTIAL";
  case Genuine::SUSPECT:   return "SUSPECT";
  case Genuine::NOT_SHT3X: return "NOT_SHT3X";
  default:                 return "UNKNOWN";
  }
}

/* ========================================================================= */
/* Derived values                                                            */
/* ========================================================================= */

float Massmore_SHT3x::dewPoint(float temperature, float humidity) {
  if (isnan(temperature) || isnan(humidity) || humidity <= 0.0f) {
    return NAN;
  }
  float gamma = (MASSMORE_SHT3X_MAGNUS_A * temperature) / (MASSMORE_SHT3X_MAGNUS_B + temperature) +
                logf(humidity / 100.0f);
  return (MASSMORE_SHT3X_MAGNUS_B * gamma) / (MASSMORE_SHT3X_MAGNUS_A - gamma);
}

float Massmore_SHT3x::absoluteHumidity(float temperature, float humidity) {
  if (isnan(temperature) || isnan(humidity)) {
    return NAN;
  }
  /* e = ความดันไอจริง (hPa) จากสูตร Magnus แล้วแปลงเป็น g/m³ ด้วยกฎแก๊สอุดมคติ */
  float es = 6.112f * expf((MASSMORE_SHT3X_MAGNUS_A * temperature) /
                           (MASSMORE_SHT3X_MAGNUS_B + temperature));
  float e = es * (humidity / 100.0f);
  return 216.7f * (e / (temperature + 273.15f));
}

/* ========================================================================= */
/* Error & misc                                                              */
/* ========================================================================= */

bool Massmore_SHT3x::setError(ErrorCode error) {
  _error = error;
  return false;
}

const char *Massmore_SHT3x::errorToString(ErrorCode error) {
  switch (error) {
  case ErrorCode::OK:         return "OK";
  case ErrorCode::NOT_FOUND:  return "NOT_FOUND";
  case ErrorCode::WRONG_ID:   return "WRONG_ID";
  case ErrorCode::TIMEOUT:    return "TIMEOUT";
  case ErrorCode::CRC_FAIL:   return "CRC_FAIL";
  case ErrorCode::BUS_ERROR:  return "BUS_ERROR";
  case ErrorCode::NOT_READY:  return "NOT_READY";
  case ErrorCode::NOT_BEGUN:  return "NOT_BEGUN";
  case ErrorCode::WRONG_MODE: return "WRONG_MODE";
  case ErrorCode::BAD_ARG:    return "BAD_ARG";
  default:                    return "UNKNOWN";
  }
}

uint8_t Massmore_SHT3x::scan(TwoWire &wirePort, uint8_t *found) {
  if (found == nullptr) {
    return 0;
  }
  uint8_t count = 0;
  const uint8_t candidates[2] = {MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_I2C_ADDR_B};
  for (uint8_t i = 0; i < 2; i++) {
    wirePort.beginTransmission(candidates[i]);
    if (wirePort.endTransmission() == 0) {
      found[count++] = candidates[i];
    }
  }
  return count;
}
