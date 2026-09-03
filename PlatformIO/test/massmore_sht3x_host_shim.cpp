/*!
 * @file massmore_sht3x_host_shim.cpp
 * @brief การทำงานของชิป SHT3x จำลอง (ใช้เฉพาะ host test)
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license MIT
 */

#include "massmore_sht3x_host_shim.h"

#include "../lib/Massmore_SHT3x/src/Massmore_SHT3x_Registers.h"

uint32_t g_mockMillis = 0;
uint8_t g_mockPinMode[64] = {0};
uint8_t g_mockPinLevel[64] = {0};
MockSht3x g_mockChip;
TwoWire Wire;

uint8_t mockCrc8(const uint8_t *data, uint8_t length) {
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

/*! ใส่ word พร้อม CRC ลงบัฟเฟอร์ตอบกลับ */
static void pushWord(uint16_t value) {
  uint8_t *b = g_mockChip.rxBuffer;
  uint8_t i = g_mockChip.rxLength;
  b[i] = (uint8_t)(value >> 8);
  b[i + 1] = (uint8_t)(value & 0xFF);
  b[i + 2] = mockCrc8(&b[i], 2);
  if (g_mockChip.corruptCrc) {
    b[i + 2] = (uint8_t)(b[i + 2] ^ 0xFF);
  }
  g_mockChip.rxLength = (uint8_t)(i + 3);
}

uint8_t TwoWire::endTransmission() {
  MockSht3x &chip = g_mockChip;

  /* general call reset */
  if (_address == MASSMORE_SHT3X_GENERAL_CALL_ADDR) {
    if (chip.txLength == 1 && chip.txBuffer[0] == MASSMORE_SHT3X_GENERAL_CALL_RESET_BYTE) {
      chip.status = 0x8010;
      chip.periodic = false;
      return 0;
    }
    return 2;
  }

  if (!chip.present || _address != MASSMORE_SHT3X_I2C_ADDR_A) {
    return 2; /* NACK ที่ address */
  }

  /* ping เปล่า ๆ ใช้ตรวจว่ามีอุปกรณ์อยู่ */
  if (chip.txLength == 0) {
    return 0;
  }
  if (chip.txLength < 2) {
    return 3;
  }

  uint16_t cmd = (uint16_t)((uint16_t)chip.txBuffer[0] << 8) | chip.txBuffer[1];
  chip.lastCommand = cmd;
  chip.commandCount++;

  switch (cmd) {
  case MASSMORE_SHT3X_CMD_SOFT_RESET:
    chip.status = 0x8010; /* alert pending + reset detected */
    chip.periodic = false;
    return 0;

  case MASSMORE_SHT3X_CMD_CLEAR_STATUS:
    if (chip.supportsClearStatus) {
      chip.status &= (uint16_t)~(MASSMORE_SHT3X_STATUS_ALERT_PENDING |
                                 MASSMORE_SHT3X_STATUS_RESET_DETECTED |
                                 MASSMORE_SHT3X_STATUS_CMD_FAILED |
                                 MASSMORE_SHT3X_STATUS_CRC_FAILED);
    }
    return 0;

  case MASSMORE_SHT3X_CMD_HEATER_ENABLE:
    if (chip.supportsHeater) {
      chip.status |= MASSMORE_SHT3X_STATUS_HEATER_ON;
    }
    return 0;

  case MASSMORE_SHT3X_CMD_HEATER_DISABLE:
    chip.status &= (uint16_t)~MASSMORE_SHT3X_STATUS_HEATER_ON;
    return 0;

  case MASSMORE_SHT3X_CMD_BREAK:
    chip.periodic = false;
    return 0;

  case MASSMORE_SHT3X_CMD_ART:
    chip.periodic = true;
    return 0;

  case MASSMORE_SHT3X_CMD_READ_STATUS:
  case MASSMORE_SHT3X_CMD_READ_SERIAL:
  case MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_SET:
  case MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_CLEAR:
  case MASSMORE_SHT3X_CMD_READ_ALERT_LOW_CLEAR:
  case MASSMORE_SHT3X_CMD_READ_ALERT_LOW_SET:
    if (cmd == MASSMORE_SHT3X_CMD_READ_SERIAL && !chip.supportsSerial) {
      chip.status |= MASSMORE_SHT3X_STATUS_CMD_FAILED;
      return 2;
    }
    return 0; /* ข้อมูลจะถูกเตรียมตอน requestFrom() */

  case MASSMORE_SHT3X_CMD_MEAS_HIGH:
  case MASSMORE_SHT3X_CMD_MEAS_MED:
  case MASSMORE_SHT3X_CMD_MEAS_LOW:
  case MASSMORE_SHT3X_CMD_MEAS_HIGH_STRETCH:
  case MASSMORE_SHT3X_CMD_MEAS_MED_STRETCH:
  case MASSMORE_SHT3X_CMD_MEAS_LOW_STRETCH:
    return 0;

  case MASSMORE_SHT3X_CMD_FETCH_DATA:
    if (!chip.periodic || !chip.periodicDataReady) {
      chip.lastFetchNacked = true;
      return 2; /* ชิปจริงจะ NACK เมื่อยังไม่มีผลใหม่ */
    }
    chip.lastFetchNacked = false;
    return 0;

  case MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_SET:
  case MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_CLEAR:
  case MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_CLEAR:
  case MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_SET: {
    if (chip.txLength != 5) {
      return 3;
    }
    if (mockCrc8(&chip.txBuffer[2], 2) != chip.txBuffer[4]) {
      chip.status |= MASSMORE_SHT3X_STATUS_CRC_FAILED;
      return 0;
    }
    uint16_t value = (uint16_t)((uint16_t)chip.txBuffer[2] << 8) | chip.txBuffer[3];
    uint8_t slot = 0;
    if (cmd == MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_CLEAR) slot = 1;
    if (cmd == MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_CLEAR) slot = 2;
    if (cmd == MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_SET) slot = 3;
    chip.alertRegs[slot] = value;
    return 0;
  }

  default:
    /* คำสั่งที่ไม่อยู่ในตาราง */
    if (cmd >= 0x2000 && cmd <= 0x2799) {
      chip.periodic = true; /* คำสั่ง periodic ทั้งหมด */
      return 0;
    }
    if (chip.supportsCmdError) {
      chip.status |= MASSMORE_SHT3X_STATUS_CMD_FAILED;
    }
    return 0;
  }
}

uint8_t TwoWire::requestFrom(uint8_t address, uint8_t length) {
  MockSht3x &chip = g_mockChip;
  chip.rxLength = 0;
  chip.rxIndex = 0;

  if (!chip.present || address != MASSMORE_SHT3X_I2C_ADDR_A) {
    return 0;
  }

  switch (chip.lastCommand) {
  case MASSMORE_SHT3X_CMD_READ_STATUS:
    pushWord((uint16_t)(chip.status | chip.reservedGarbage));
    break;

  case MASSMORE_SHT3X_CMD_READ_SERIAL:
    if (!chip.supportsSerial) {
      return 0;
    }
    pushWord((uint16_t)(chip.serial >> 16));
    pushWord((uint16_t)(chip.serial & 0xFFFF));
    break;

  case MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_SET:
    pushWord(chip.alertRegs[0]);
    break;
  case MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_CLEAR:
    pushWord(chip.alertRegs[1]);
    break;
  case MASSMORE_SHT3X_CMD_READ_ALERT_LOW_CLEAR:
    pushWord(chip.alertRegs[2]);
    break;
  case MASSMORE_SHT3X_CMD_READ_ALERT_LOW_SET:
    pushWord(chip.alertRegs[3]);
    break;

  case MASSMORE_SHT3X_CMD_MEAS_HIGH:
  case MASSMORE_SHT3X_CMD_MEAS_MED:
  case MASSMORE_SHT3X_CMD_MEAS_LOW:
  case MASSMORE_SHT3X_CMD_MEAS_HIGH_STRETCH:
  case MASSMORE_SHT3X_CMD_MEAS_MED_STRETCH:
  case MASSMORE_SHT3X_CMD_MEAS_LOW_STRETCH:
  case MASSMORE_SHT3X_CMD_FETCH_DATA:
    if (chip.lastCommand == MASSMORE_SHT3X_CMD_FETCH_DATA && chip.lastFetchNacked) {
      return 0;
    }
    pushWord(chip.rawT);
    pushWord(chip.rawRH);
    break;

  default:
    return 0;
  }

  if (chip.rxLength != length) {
    /* ความยาวไม่ตรงกับที่ไลบรารีขอ คืนเท่าที่มีเหมือนฮาร์ดแวร์จริง */
    return chip.rxLength;
  }
  return chip.rxLength;
}
