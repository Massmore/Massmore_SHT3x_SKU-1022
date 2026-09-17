/**
 * @file    Massmore_SHT3x_Registers.h
 * @brief   ตาราง Command, Status Register bit และค่า Timing ของ Sensirion SHT3x-DIS
 *
 * SHT3x เป็นเซ็นเซอร์แบบ "command based" ไม่ใช่ register map
 * ทุก Command เป็นเลข 16-bit ส่ง MSB ก่อนแล้วตามด้วย LSB
 * ข้อมูลที่อ่านกลับทุก 2 byte จะมี CRC-8 ต่อท้าย 1 byte เสมอ
 *
 * แหล่งอ้างอิง (Reference priority ตาม Massmore Standard §3):
 *   1. Datasheet SHT3x-DIS, Sensirion, Version 7 (December 2022)
 *      https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf
 *   2. Application Note "Alert Mode of SHT3x- and STS3x-DIS" (รูปแบบ Alert limit word)
 *   3. Sensirion embedded-sht reference code (Command 0x3780 อ่าน Serial Number
 *      ไม่ปรากฏใน Datasheet แต่เป็น Command ที่ Sensirion เผยแพร่เอง)
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license   MIT
 */

#ifndef MASSMORE_SHT3X_REGISTERS_H
#define MASSMORE_SHT3X_REGISTERS_H

#include <stdint.h>

/* ------------------------------------------------------------------------- */
/* I2C address (Datasheet Table 7)                                           */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_I2C_ADDR_A       0x44 /**< ADDR = LOW (ค่าเริ่มต้นของบอร์ด Massmore) */
#define MASSMORE_SHT3X_I2C_ADDR_B       0x45 /**< ADDR = HIGH (บัดกรี Jumper ADDR ให้ปิด) */
#define MASSMORE_SHT3X_I2C_ADDR_DEFAULT MASSMORE_SHT3X_I2C_ADDR_A

/* ------------------------------------------------------------------------- */
/* Single Shot Mode - Clock Stretching enabled (Datasheet Table 8)           */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_CMD_MEAS_HIGH_STRETCH 0x2C06
#define MASSMORE_SHT3X_CMD_MEAS_MED_STRETCH  0x2C0D
#define MASSMORE_SHT3X_CMD_MEAS_LOW_STRETCH  0x2C10

/* ------------------------------------------------------------------------- */
/* Single Shot Mode - Clock Stretching disabled (Datasheet Table 8)          */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_CMD_MEAS_HIGH 0x2400
#define MASSMORE_SHT3X_CMD_MEAS_MED  0x240B
#define MASSMORE_SHT3X_CMD_MEAS_LOW  0x2416

/* ------------------------------------------------------------------------- */
/* Periodic Data Acquisition Mode (Datasheet Table 9)                        */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_HIGH 0x2032
#define MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_MED  0x2024
#define MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_LOW  0x202F
#define MASSMORE_SHT3X_CMD_PERIODIC_1HZ_HIGH  0x2130
#define MASSMORE_SHT3X_CMD_PERIODIC_1HZ_MED   0x2126
#define MASSMORE_SHT3X_CMD_PERIODIC_1HZ_LOW   0x212D
#define MASSMORE_SHT3X_CMD_PERIODIC_2HZ_HIGH  0x2236
#define MASSMORE_SHT3X_CMD_PERIODIC_2HZ_MED   0x2220
#define MASSMORE_SHT3X_CMD_PERIODIC_2HZ_LOW   0x222B
#define MASSMORE_SHT3X_CMD_PERIODIC_4HZ_HIGH  0x2334
#define MASSMORE_SHT3X_CMD_PERIODIC_4HZ_MED   0x2322
#define MASSMORE_SHT3X_CMD_PERIODIC_4HZ_LOW   0x2329
#define MASSMORE_SHT3X_CMD_PERIODIC_10HZ_HIGH 0x2737
#define MASSMORE_SHT3X_CMD_PERIODIC_10HZ_MED  0x2721
#define MASSMORE_SHT3X_CMD_PERIODIC_10HZ_LOW  0x272A

/* ------------------------------------------------------------------------- */
/* System / control commands (Datasheet Table 10-17)                         */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_CMD_FETCH_DATA     0xE000 /**< ดึงผลล่าสุดใน Periodic Mode (NACK ถ้ายังไม่มีผลใหม่) */
#define MASSMORE_SHT3X_CMD_ART            0x2B32 /**< Accelerated Response Time (Periodic 4 Hz) */
#define MASSMORE_SHT3X_CMD_BREAK          0x3093 /**< หยุด Periodic Mode กลับสู่ Single Shot */
#define MASSMORE_SHT3X_CMD_SOFT_RESET     0x30A2 /**< Soft Reset */
#define MASSMORE_SHT3X_CMD_HEATER_ENABLE  0x306D /**< เปิด Heater ในตัว */
#define MASSMORE_SHT3X_CMD_HEATER_DISABLE 0x3066 /**< ปิด Heater */
#define MASSMORE_SHT3X_CMD_READ_STATUS    0xF32D /**< อ่าน Status Register (2 byte + CRC) */
#define MASSMORE_SHT3X_CMD_CLEAR_STATUS   0x3041 /**< เคลียร์ bit ค้างใน Status Register */
#define MASSMORE_SHT3X_CMD_READ_SERIAL    0x3780 /**< อ่าน Serial Number (2 word + CRC ต่อ word) - จาก Sensirion reference code */

/** General Call Reset: ส่งไปที่ address 0x00 ด้วย data 1 byte = 0x06 (Datasheet 4.10) */
#define MASSMORE_SHT3X_GENERAL_CALL_ADDR       0x00
#define MASSMORE_SHT3X_GENERAL_CALL_RESET_BYTE 0x06

/* ------------------------------------------------------------------------- */
/* ALERT limit registers (Application Note Alert Mode)                       */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_SET   0x611D
#define MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_CLEAR 0x6116
#define MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_CLEAR  0x610B
#define MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_SET    0x6100
#define MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_SET    0xE11F
#define MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_CLEAR  0xE114
#define MASSMORE_SHT3X_CMD_READ_ALERT_LOW_CLEAR   0xE109
#define MASSMORE_SHT3X_CMD_READ_ALERT_LOW_SET     0xE102

/**
 * รูปแบบ Alert limit word 16-bit
 *   bit 15..9 = 7 bit บนของ raw humidity     (ความละเอียดประมาณ 1 %RH)
 *   bit  8..0 = 9 bit บนของ raw temperature  (ความละเอียดประมาณ 0.5 °C)
 */
#define MASSMORE_SHT3X_ALERT_RH_MASK 0xFE00
#define MASSMORE_SHT3X_ALERT_T_MASK  0x01FF
#define MASSMORE_SHT3X_ALERT_T_SHIFT 7

/* ------------------------------------------------------------------------- */
/* Status Register 16-bit (Datasheet Table 18)                               */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_STATUS_ALERT_PENDING  (1u << 15) /**< มีอย่างน้อย 1 Alert ค้าง */
#define MASSMORE_SHT3X_STATUS_HEATER_ON      (1u << 13) /**< Heater เปิดอยู่ */
#define MASSMORE_SHT3X_STATUS_RH_ALERT       (1u << 11) /**< Humidity tracking alert */
#define MASSMORE_SHT3X_STATUS_T_ALERT        (1u << 10) /**< Temperature tracking alert */
#define MASSMORE_SHT3X_STATUS_RESET_DETECTED (1u << 4)  /**< System reset detected (power-up / soft reset) */
#define MASSMORE_SHT3X_STATUS_CMD_FAILED     (1u << 1)  /**< Command ล่าสุดไม่ถูกประมวลผล */
#define MASSMORE_SHT3X_STATUS_CRC_FAILED     (1u << 0)  /**< Write data checksum ผิด */

/**
 * Reserved bit ที่ใช้ยืนยันตัวตนของชิป: bit 14, 12, 9..7, 3..2
 *
 * Datasheet Table 18 ระบุ reserved ไว้ที่ bit 14, 12, 9..5, 3..2 แต่การวัดบนบอร์ดจริง
 * (Massmore SHT3X SKU-1022, SHT30) พบว่า **bit 6 และ bit 5 อ่านได้เป็น 1** ขณะอยู่ใน
 * Periodic Mode (status = 0x8C60) จึงตัดสอง bit นี้ออกจากการตรวจ ไม่เช่นนั้น
 * verifyChipID() จะคืน WRONG_ID ทั้งที่ชิปทำงานปกติ
 */
#define MASSMORE_SHT3X_STATUS_RESERVED_MASK 0x538Cu

/* ------------------------------------------------------------------------- */
/* CRC-8 (Datasheet Table 20 "Checksum properties")                          */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_CRC8_POLYNOMIAL 0x31 /**< x^8 + x^5 + x^4 + 1 */
#define MASSMORE_SHT3X_CRC8_INIT       0xFF
#define MASSMORE_SHT3X_CRC8_FINAL_XOR  0x00

/* ------------------------------------------------------------------------- */
/* Timing (ms) - ใช้ค่า max จาก Datasheet Table 4 และเผื่อไว้แล้ว             */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_MEAS_DURATION_LOW_MS  5  /**< max 4 ms   + margin */
#define MASSMORE_SHT3X_MEAS_DURATION_MED_MS  7  /**< max 6 ms   + margin */
#define MASSMORE_SHT3X_MEAS_DURATION_HIGH_MS 16 /**< max 15 ms  + margin */
#define MASSMORE_SHT3X_SOFT_RESET_MS         2  /**< max 1.5 ms + margin */
#define MASSMORE_SHT3X_POWER_UP_MS           2  /**< max 1.5 ms + margin */
#define MASSMORE_SHT3X_CMD_GAP_MS            1  /**< เว้นระหว่าง Command อย่างน้อย 1 ms */
#define MASSMORE_SHT3X_HARD_RESET_PULSE_US   20 /**< nRESET pulse min 1 us ใช้ 20 us เผื่อสายยาว */

/* ------------------------------------------------------------------------- */
/* Signal conversion (Datasheet 4.13)                                        */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_RAW_FULL_SCALE 65535.0f /**< 2^16 - 1 */
#define MASSMORE_SHT3X_T_C_OFFSET     (-45.0f)
#define MASSMORE_SHT3X_T_C_SPAN       175.0f
#define MASSMORE_SHT3X_RH_SPAN        100.0f

/* ------------------------------------------------------------------------- */
/* Physical range (Datasheet Table 1-3) - ใช้โดย Factory Test range check    */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_T_MIN_C  (-40.0f)
#define MASSMORE_SHT3X_T_MAX_C  125.0f
#define MASSMORE_SHT3X_RH_MIN   0.0f
#define MASSMORE_SHT3X_RH_MAX   100.0f

/* ------------------------------------------------------------------------- */
/* Frame length                                                              */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_MEAS_FRAME_LEN   6 /**< T(2) + CRC(1) + RH(2) + CRC(1) */
#define MASSMORE_SHT3X_WORD_FRAME_LEN   3 /**< word(2) + CRC(1) */
#define MASSMORE_SHT3X_SERIAL_FRAME_LEN 6 /**< word(2) + CRC(1) + word(2) + CRC(1) */

#endif /* MASSMORE_SHT3X_REGISTERS_H */
