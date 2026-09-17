# Massmore_SHT3x — Factory Test Firmware

Pre-compiled `05_Factory_Test` สำหรับตรวจบอร์ด **Massmore SHT3X (SKU-1022)** โดยไม่ต้องคอมไพล์เอง
แฟลชแล้วเปิด Serial Monitor (115200 baud) จะรู้ผล PASS / FAIL ทันที

---

## Files

| File | Target | Flash offset | Size |
|---|---|---|---|
| `bin/Massmore_SHT3x_FactoryTest_ESP32.bin` | ESP32 (Classic) — merged image รวม bootloader + partition table + app | `0x0` | 384 768 bytes |
| `bin/SHA256SUMS.txt` | checksum ของไฟล์ข้างบน | — | — |

Build จาก Arduino-ESP32 Core 3.3.11 (pioarduino platform `55.03.311`) และ **ทดสอบแฟลชจริงแล้ว**
บน ESP32-WROOM + Massmore SHT3X (SHT30) ผลลัพธ์ `#VERDICT PASS`

ตรวจไฟล์ก่อนแฟลช

```bash
cd firmware/bin && shasum -a 256 -c SHA256SUMS.txt
```

---

## Wiring (Primary MCU: ESP32 Classic)

| SHT3X pin | ESP32 | Required |
|---|---|---|
| `VCC` | 3V3 (หรือ 5V) | Yes |
| `GND` | GND | Yes |
| `SDA` | GPIO 21 | Yes |
| `SCL` | GPIO 22 | Yes |
| `ADDR` | ปล่อยลอย (= `0x44`) | No |
| `ALRT` / `RST` | ไม่ต้องต่อ — Factory Test ไม่ใช้สองขานี้ | No |

หรือเสียบสาย Qwiic เข้าคอนเนกเตอร์บนบอร์ด Massmore ESP32 Breakout ได้โดยตรง

---

## Flashing

### esptool (คำสั่งที่ใช้ทดสอบจริง)

```bash
pip install esptool
esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX --baud 512000 \
  write_flash 0x0 bin/Massmore_SHT3x_FactoryTest_ESP32.bin
```

บน macOS ความเร็ว **512000 เสถียรที่สุด** ส่วน 921600 ทำให้เจอ `The chip stopped responding`
กับสาย USB บางเส้น ถ้ายังไม่ผ่านให้ลด `--baud` เป็น `460800` หรือ `115200`

Windows ใช้ `--port COM5` แทน ส่วน Linux ใช้ `--port /dev/ttyUSB0`

### PlatformIO (build จาก source)

```bash
cd PlatformIO
pio run -e esp32dev -t upload -t monitor
```

### Web flasher

เปิด **Massmore Web Serial Monitor** ในเบราว์เซอร์ที่รองรับ Web Serial (Chrome / Edge)
เลือกไฟล์ `.bin` แล้วกด Flash โดยตั้ง offset เป็น `0x0`

<!-- TODO: [MASSMORE_INPUT_REQUIRED: Massmore Web Serial Monitor URL] -->

---

## Serial Output Format (115200 baud)

บรรทัดที่ขึ้นต้นด้วย `#` ถูก parse โดยเครื่อง บรรทัดอื่นสำหรับคนอ่าน

```text
#RESULT <TEST_NAME> <PASS|FAIL> <value>
#VERDICT <PASS|FAIL> [<REASON>]
[PASS] SENSOR QA PASSED - READY TO SHIP      หรือ
[FAIL] QA CHECK FAILED: <REASON>
```

| Test | What it checks | FAIL reason |
|---|---|---|
| `BUS_RECOVER` | กู้ I2C Bus ที่อาจค้างจากการรันครั้งก่อน (ไม่ทำให้ตก QA) | — |
| `BUS_SCAN` | พบ SHT3x ที่ `0x44` หรือ `0x45` | `NO_DEVICE` |
| `CHIP_ID` | Status Register อ่านได้ CRC ถูก + Reserved bit = 0 + Clear Status ทำงาน | `CHIP_ID_MISMATCH` |
| `SERIAL` | Serial Number 32-bit ไม่ใช่ `0x00000000` / `0xFFFFFFFF` | `SERIAL_READ_FAIL` |
| `AUTHENTICITY` | Heuristic 9 ข้อ → `GENUINE` (9/9) หรือ `PARTIAL` (8/9) ผ่านทั้งคู่ | `AUTHENTICITY_FAIL` |
| `RANGE_TEMP` | −40 < T < 125 °C | `TEMP_OUT_OF_RANGE` |
| `RANGE_HUMI` | 0 ≤ RH ≤ 100 %RH | `HUMI_OUT_OF_RANGE` |
| `CONTINUOUS` | 20 samples ไม่มี NAN / TIMEOUT และ step ระหว่าง sample ≤ 1 °C / 3 %RH | `CONTINUOUS_READ_FAIL` / `NOISE_TOO_HIGH` |

พิมพ์ `r` ใน Serial Monitor เพื่อทดสอบซ้ำ

### รายการ 9 ข้อของ AUTHENTICITY

| Check | สิ่งที่ตรวจ |
|---|---|
| `ACK` | ชิปตอบ ACK ที่ address |
| `STATUS_CRC` | Status Register อ่านได้และ CRC-8 ถูก |
| `STATUS_RESERVED_ZERO` | Reserved bit (mask `0x538C`) อ่านได้เป็น 0 |
| `CLEAR_STATUS` | คำสั่ง Clear Status ลบ bit ที่ค้างได้จริง |
| `SERIAL_NUMBER` | อ่าน Serial Number ได้ และไม่ใช่ค่าตายตัว |
| `HEATER_BIT` | Heater bit 13 ขยับตามคำสั่งเปิด/ปิดจริง |
| `ALERT_REGISTER_RW` | เขียน Alert threshold แล้วอ่านกลับได้ค่าเดิม (คืนค่าเดิมให้หลังตรวจ) |
| `MEAS_CRC` | ผลวัดมี CRC ถูกทั้งสอง word |
| `MEAS_RANGE` | ผลวัดอยู่ในช่วง Physical range ของ Datasheet |

ทุกข้อเลือกมาแล้วว่าไม่ทำให้ชิป NACK เพราะ NACK หนึ่งครั้งทำให้ i2c driver ของ
Arduino-ESP32 Core 3.x ค้างถาวรจนกว่าจะกู้ด้วย General Call Reset

---

## Expected Report (ผลจริงจากบอร์ดที่ผ่าน)

รายงานด้านล่างคัดลอกมาจากบอร์ดจริงแบบคำต่อคำ (ESP32-WROOM + Massmore SHT3X รุ่น SHT30)

```text
#MASSMORE_FACTORY_TEST v1.0
#PRODUCT Massmore_SHT3x
#MCU ESP32
#LIB 2.0.0
#RESULT BUS_RECOVER PASS READY
#RESULT BUS_SCAN PASS 0x44
#RESULT CHIP_ID PASS 0x8010
#RESULT SERIAL PASS 0x2A124EE2
#RESULT AUTHENTICITY PASS GENUINE
  [ok]   ACK
  [ok]   STATUS_CRC
  [ok]   STATUS_RESERVED_ZERO
  [ok]   CLEAR_STATUS
  [ok]   SERIAL_NUMBER
  [ok]   HEATER_BIT
  [ok]   ALERT_REGISTER_RW
  [ok]   MEAS_CRC
  [ok]   MEAS_RANGE
#RESULT RANGE_TEMP PASS 26.6
#RESULT RANGE_HUMI PASS 57.0
#RESULT CONTINUOUS PASS 20/20
#VERDICT PASS
[PASS] SENSOR QA PASSED - READY TO SHIP
Press 'r' to run again
```

ค่า `SERIAL` ต่างกันทุกบอร์ด ส่วน `CHIP_ID` เป็น `0x8010` เมื่อชิปเพิ่งถูก reset
(bit 15 = alert pending จาก threshold ค่า default, bit 4 = reset detected)

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `#RESULT BUS_SCAN FAIL NONE` | สาย SDA/SCL สลับ, ไม่มีไฟ, สาย Qwiic เสียบไม่สุด | ตรวจสาย · วัด 3.3 V ที่ `VCC` |
| `CHIP_ID FAIL` | ชิปตอบ ACK แต่ Status Register ผิดรูปแบบ | ใช้สายสั้น < 30 cm · ลด I2C เป็น 100 kHz · ถ้ายังไม่ผ่านให้คัดบอร์ดออก |
| `AUTHENTICITY FAIL SUSPECT` | ชิปไม่ตอบ Command ของ Sensirion ครบ | บอร์ดต้องสงสัย คัดออก |
| `NOISE_TOO_HIGH` | มีลมร้อนหรือหายใจรดขณะทดสอบ, ไฟเลี้ยงไม่นิ่ง | ทดสอบในที่นิ่ง · เปลี่ยนสาย USB |
| `esp32-hal-i2c-ng.c ... ESP_ERR_INVALID_STATE` ตลอด | i2c driver ค้างหลังชิป NACK (MCU reset ไม่ช่วย เพราะเซ็นเซอร์ไม่ถูกตัดไฟ) | Factory Test กู้เองอัตโนมัติในขั้น `BUS_RECOVER` · ในโค้ดของคุณเรียก `recoverBus()` |
| อัปโหลดไม่ขึ้น `The chip stopped responding` | Baud สูงเกินไปสำหรับสาย USB เส้นนั้น | ลด baud เป็น 512000 หรือ 460800 · กดปุ่ม BOOT ค้างตอนเริ่มแฟลช |
