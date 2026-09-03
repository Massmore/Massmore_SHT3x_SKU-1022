/*!
 * @file Massmore_SHT3x_Registers.h
 * @brief ตารางคำสั่ง I2C, บิตของ status register และค่าเวลาของ SHT3x-DIS
 *
 * ทุกค่าในไฟล์นี้อ้างอิงจากเอกสารต้นทางโดยตรง ไม่ได้คัดลอกมาจากไลบรารีเจ้าอื่น
 *   - Datasheet SHT3x-DIS (Sensirion, December 2022 - Version 7)
 *     https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf
 *   - Application Note "Alert Mode of SHT3x- and STS3x-DIS"
 *     https://sensirion.com/media/documents/40D749F7/65D61534/HT_AN_AlertMode.pdf
 *
 * SHT3x เป็นเซ็นเซอร์แบบ "command based" ไม่ใช่แบบ register map
 * ทุกคำสั่งคือเลข 16 บิต ส่ง MSB ก่อน ตามด้วย LSB
 *
 * @copyright Copyright (c) 2026 Massmore Biz Co., Ltd.
 * @license MIT
 */

#ifndef MASSMORE_SHT3X_REGISTERS_H
#define MASSMORE_SHT3X_REGISTERS_H

#include <stdint.h>

/* ------------------------------------------------------------------------- */
/* I2C address                                                               */
/* ------------------------------------------------------------------------- */

/*! ADDR ปล่อยลอย / ต่อลง GND (ค่าปกติของบอร์ด Massmore) */
#define MASSMORE_SHT3X_I2C_ADDR_A 0x44
/*! ADDR ต่อขึ้น VDD (บอร์ด Massmore = บัดกรีจัมเปอร์ ADDR ให้ปิด) */
#define MASSMORE_SHT3X_I2C_ADDR_B 0x45
/*! ค่าเริ่มต้นที่ begin() ใช้เมื่อไม่ระบุ */
#define MASSMORE_SHT3X_I2C_ADDR_DEFAULT MASSMORE_SHT3X_I2C_ADDR_A

/* ------------------------------------------------------------------------- */
/* Single shot mode - clock stretching เปิด (ชิปดึง SCL ค้างจนวัดเสร็จ)       */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_CMD_MEAS_HIGH_STRETCH 0x2C06 /*!< ความละเอียดสูง */
#define MASSMORE_SHT3X_CMD_MEAS_MED_STRETCH 0x2C0D  /*!< ความละเอียดกลาง */
#define MASSMORE_SHT3X_CMD_MEAS_LOW_STRETCH 0x2C10  /*!< ความละเอียดต่ำ */

/* ------------------------------------------------------------------------- */
/* Single shot mode - clock stretching ปิด (ต้องหน่วงเวลาเองก่อนอ่าน)         */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_CMD_MEAS_HIGH 0x2400 /*!< ความละเอียดสูง */
#define MASSMORE_SHT3X_CMD_MEAS_MED 0x240B  /*!< ความละเอียดกลาง */
#define MASSMORE_SHT3X_CMD_MEAS_LOW 0x2416  /*!< ความละเอียดต่ำ */

/* ------------------------------------------------------------------------- */
/* Periodic data acquisition mode                                            */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_HIGH 0x2032 /*!< 0.5 ครั้ง/วินาที สูง */
#define MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_MED 0x2024  /*!< 0.5 ครั้ง/วินาที กลาง */
#define MASSMORE_SHT3X_CMD_PERIODIC_0HZ5_LOW 0x202F  /*!< 0.5 ครั้ง/วินาที ต่ำ */

#define MASSMORE_SHT3X_CMD_PERIODIC_1HZ_HIGH 0x2130 /*!< 1 ครั้ง/วินาที สูง */
#define MASSMORE_SHT3X_CMD_PERIODIC_1HZ_MED 0x2126  /*!< 1 ครั้ง/วินาที กลาง */
#define MASSMORE_SHT3X_CMD_PERIODIC_1HZ_LOW 0x212D  /*!< 1 ครั้ง/วินาที ต่ำ */

#define MASSMORE_SHT3X_CMD_PERIODIC_2HZ_HIGH 0x2236 /*!< 2 ครั้ง/วินาที สูง */
#define MASSMORE_SHT3X_CMD_PERIODIC_2HZ_MED 0x2220  /*!< 2 ครั้ง/วินาที กลาง */
#define MASSMORE_SHT3X_CMD_PERIODIC_2HZ_LOW 0x222B  /*!< 2 ครั้ง/วินาที ต่ำ */

#define MASSMORE_SHT3X_CMD_PERIODIC_4HZ_HIGH 0x2334 /*!< 4 ครั้ง/วินาที สูง */
#define MASSMORE_SHT3X_CMD_PERIODIC_4HZ_MED 0x2322  /*!< 4 ครั้ง/วินาที กลาง */
#define MASSMORE_SHT3X_CMD_PERIODIC_4HZ_LOW 0x2329  /*!< 4 ครั้ง/วินาที ต่ำ */

#define MASSMORE_SHT3X_CMD_PERIODIC_10HZ_HIGH 0x2737 /*!< 10 ครั้ง/วินาที สูง */
#define MASSMORE_SHT3X_CMD_PERIODIC_10HZ_MED 0x2721  /*!< 10 ครั้ง/วินาที กลาง */
#define MASSMORE_SHT3X_CMD_PERIODIC_10HZ_LOW 0x272A  /*!< 10 ครั้ง/วินาที ต่ำ */

/* ------------------------------------------------------------------------- */
/* System / control commands                                                 */
/* ------------------------------------------------------------------------- */

/*! ดึงผลล่าสุดในโหมด periodic (ถ้ายังไม่มีผลใหม่ ชิปจะ NACK) */
#define MASSMORE_SHT3X_CMD_FETCH_DATA 0xE000
/*! ART = Accelerated Response Time ทำงานที่ 4 ครั้ง/วินาที */
#define MASSMORE_SHT3X_CMD_ART 0x2B32
/*! หยุดโหมด periodic กลับสู่ single shot */
#define MASSMORE_SHT3X_CMD_BREAK 0x3093
/*! รีเซ็ตซอฟต์แวร์ */
#define MASSMORE_SHT3X_CMD_SOFT_RESET 0x30A2
/*! เปิดฮีตเตอร์ในตัว (~3.6 mW / ทำให้อุณหภูมิสูงขึ้น 0.5-1.5 องศา) */
#define MASSMORE_SHT3X_CMD_HEATER_ENABLE 0x306D
/*! ปิดฮีตเตอร์ */
#define MASSMORE_SHT3X_CMD_HEATER_DISABLE 0x3066
/*! อ่าน status register (ตอบ 2 ไบต์ + CRC) */
#define MASSMORE_SHT3X_CMD_READ_STATUS 0xF32D
/*! เคลียร์บิตค้างใน status register */
#define MASSMORE_SHT3X_CMD_CLEAR_STATUS 0x3041
/*! อ่านหมายเลขซีเรียลจากโรงงาน (ตอบ 2 word + CRC ต่อ word) */
#define MASSMORE_SHT3X_CMD_READ_SERIAL 0x3780

/*! General call reset: ส่งไปที่ address 0x00 ด้วยข้อมูล 1 ไบต์ = 0x06 */
#define MASSMORE_SHT3X_GENERAL_CALL_ADDR 0x00
#define MASSMORE_SHT3X_GENERAL_CALL_RESET_BYTE 0x06

/* ------------------------------------------------------------------------- */
/* ALERT limit registers                                                     */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_SET 0x611D   /*!< เขียน High Set */
#define MASSMORE_SHT3X_CMD_WRITE_ALERT_HIGH_CLEAR 0x6116 /*!< เขียน High Clear */
#define MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_CLEAR 0x610B  /*!< เขียน Low Clear */
#define MASSMORE_SHT3X_CMD_WRITE_ALERT_LOW_SET 0x6100    /*!< เขียน Low Set */

#define MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_SET 0xE11F   /*!< อ่าน High Set */
#define MASSMORE_SHT3X_CMD_READ_ALERT_HIGH_CLEAR 0xE114 /*!< อ่าน High Clear */
#define MASSMORE_SHT3X_CMD_READ_ALERT_LOW_CLEAR 0xE109  /*!< อ่าน Low Clear */
#define MASSMORE_SHT3X_CMD_READ_ALERT_LOW_SET 0xE102    /*!< อ่าน Low Set */

/*!
 * รูปแบบการแพ็ก threshold เป็น 16 บิต (จาก Application Note Alert Mode)
 *   bit 15..9 = 7 บิตบนสุดของค่าดิบความชื้น   (ความละเอียดประมาณ 1 %RH)
 *   bit  8..0 = 9 บิตบนสุดของค่าดิบอุณหภูมิ   (ความละเอียดประมาณ 0.5 องศา)
 */
#define MASSMORE_SHT3X_ALERT_RH_MASK 0xFE00
#define MASSMORE_SHT3X_ALERT_T_MASK 0x01FF
#define MASSMORE_SHT3X_ALERT_T_SHIFT 7

/* ------------------------------------------------------------------------- */
/* Status register (16 บิต)                                                  */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_STATUS_ALERT_PENDING (1u << 15) /*!< มีอย่างน้อย 1 alert ค้าง */
#define MASSMORE_SHT3X_STATUS_HEATER_ON (1u << 13)     /*!< ฮีตเตอร์เปิดอยู่ */
#define MASSMORE_SHT3X_STATUS_RH_ALERT (1u << 11)      /*!< ความชื้นเลยขอบเขต */
#define MASSMORE_SHT3X_STATUS_T_ALERT (1u << 10)       /*!< อุณหภูมิเลยขอบเขต */
#define MASSMORE_SHT3X_STATUS_RESET_DETECTED (1u << 4) /*!< เพิ่งรีเซ็ต/เพิ่งจ่ายไฟ */
#define MASSMORE_SHT3X_STATUS_CMD_FAILED (1u << 1)     /*!< คำสั่งล่าสุดทำงานไม่สำเร็จ */
#define MASSMORE_SHT3X_STATUS_CRC_FAILED (1u << 0)     /*!< checksum ของข้อมูลที่เขียนผิด */

/*!
 * บิตที่ datasheet ระบุว่า reserved ต้องอ่านได้เป็น 0 เสมอ
 * ใช้เป็นหนึ่งในเงื่อนไขตรวจว่าเป็นชิป SHT3x จริง
 * bit 14, 12, 9..5, 3..2
 */
#define MASSMORE_SHT3X_STATUS_RESERVED_MASK 0x53ECu

/* ------------------------------------------------------------------------- */
/* CRC-8 ตาม datasheet ตาราง "CRC properties"                                */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_CRC8_POLYNOMIAL 0x31 /*!< x^8 + x^5 + x^4 + 1 */
#define MASSMORE_SHT3X_CRC8_INIT 0xFF       /*!< ค่าเริ่มต้น */
#define MASSMORE_SHT3X_CRC8_FINAL_XOR 0x00  /*!< ไม่ XOR ตอนจบ */

/* ------------------------------------------------------------------------- */
/* เวลาที่ต้องรอ (มิลลิวินาที) เผื่อค่าสูงสุดจาก datasheet ไว้แล้ว              */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_MEAS_DURATION_LOW_MS 5   /*!< สูงสุด 4 ms + เผื่อ */
#define MASSMORE_SHT3X_MEAS_DURATION_MED_MS 7   /*!< สูงสุด 6 ms + เผื่อ */
#define MASSMORE_SHT3X_MEAS_DURATION_HIGH_MS 16 /*!< สูงสุด 15 ms + เผื่อ */
#define MASSMORE_SHT3X_SOFT_RESET_MS 2          /*!< สูงสุด 1.5 ms + เผื่อ */
#define MASSMORE_SHT3X_POWER_UP_MS 2            /*!< สูงสุด 1.5 ms + เผื่อ */
#define MASSMORE_SHT3X_CMD_GAP_MS 1             /*!< เว้นระหว่างคำสั่งอย่างน้อย 1 ms */
#define MASSMORE_SHT3X_HARD_RESET_PULSE_US 20   /*!< ต่ำสุด 1 us ใช้ 20 us เผื่อสายยาว */

/* ------------------------------------------------------------------------- */
/* ค่าคงที่สำหรับการแปลงสัญญาณ (datasheet หัวข้อ 4.13)                         */
/* ------------------------------------------------------------------------- */

#define MASSMORE_SHT3X_RAW_FULL_SCALE 65535.0f /*!< 2^16 - 1 */
#define MASSMORE_SHT3X_T_C_OFFSET (-45.0f)
#define MASSMORE_SHT3X_T_C_SPAN 175.0f
#define MASSMORE_SHT3X_T_F_OFFSET (-49.0f)
#define MASSMORE_SHT3X_T_F_SPAN 315.0f
#define MASSMORE_SHT3X_RH_SPAN 100.0f

/*! ความยาวข้อมูลที่ชิปตอบกลับตอนอ่านผลวัด: T(2) + CRC(1) + RH(2) + CRC(1) */
#define MASSMORE_SHT3X_MEAS_FRAME_LEN 6
/*! ความยาวข้อมูลตอนอ่าน status หรือ alert limit: word(2) + CRC(1) */
#define MASSMORE_SHT3X_WORD_FRAME_LEN 3
/*! ความยาวข้อมูลตอนอ่านซีเรียล: word(2) + CRC(1) + word(2) + CRC(1) */
#define MASSMORE_SHT3X_SERIAL_FRAME_LEN 6

#endif /* MASSMORE_SHT3X_REGISTERS_H */
