# Massmore_SHT3x — Factory Test Firmware

Pre-compiled `05_Factory_Test` สำหรับตรวจบอร์ด **Massmore SHT3X (SKU-1022)** โดยไม่ต้องคอมไพล์เอง
แฟลชแล้วเปิด Serial Monitor (115200 baud) จะรู้ผล PASS / FAIL ทันที

---

## Files

| File | Target | Flash offset |
|---|---|---|
| `bin/Massmore_SHT3x_FactoryTest_ESP32.bin` | ESP32 (Classic) — merged bootloader + partitions + app | `0x0` |

<!-- TODO: [MASSMORE_INPUT_REQUIRED: firmware/bin will be generated after a real hardware [PASS] per Standard §11.4] -->

---

## Wiring (Primary MCU: ESP32 Classic)

| SHT3X pin | ESP32 | Required |
|---|---|---|
| `VCC` | 3V3 (หรือ 5V) | Yes |
| `GND` | GND | Yes |
| `SDA` | GPIO 21 | Yes |
| `SCL` | GPIO 22 | Yes |
| `ADDR` | ปล่อยลอย (= `0x44`) | No |
| `ALRT` / `RST` | ไม่ต้องต่อ | No |

หรือเสียบสาย Qwiic เข้าคอนเนกเตอร์บนบอร์ด Massmore ESP32 Breakout ได้โดยตรง

---

## Flashing

### esptool (macOS / Linux / Windows)

```bash
pip install esptool
esptool.py --chip esp32 --port /dev/cu.usbserial-XXXX --baud 921600 \
  write_flash 0x0 bin/Massmore_SHT3x_FactoryTest_ESP32.bin
```

ถ้าอัปโหลดไม่นิ่ง ลด `--baud` เป็น `512000` หรือ `460800`

### PlatformIO (build จาก source)

```bash
cd PlatformIO
pio run -e esp32dev -t upload -t monitor
```

### Web flasher

เปิด **Massmore Web Serial Monitor** ในเบราว์เซอร์ที่รองรับ Web Serial (Chrome / Edge) เลือกไฟล์ `.bin` แล้วกด Flash

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
| `BUS_SCAN` | พบ SHT3x ที่ `0x44` หรือ `0x45` | `NO_DEVICE` |
| `CHIP_ID` | Status Register หลัง Soft Reset: Reserved bit = 0, Reset flag = 1 | `CHIP_ID_MISMATCH` |
| `SERIAL` | Serial Number 32-bit ไม่ใช่ `0x00000000` / `0xFFFFFFFF` | `SERIAL_READ_FAIL` |
| `AUTHENTICITY` | Heuristic 10 ข้อ → `GENUINE` (10/10) หรือ `PARTIAL` (≥ 8/10) ผ่าน | `AUTHENTICITY_FAIL` |
| `RANGE_TEMP` | −40 < T < 125 °C | `TEMP_OUT_OF_RANGE` |
| `RANGE_HUMI` | 0 ≤ RH ≤ 100 %RH | `HUMI_OUT_OF_RANGE` |
| `CONTINUOUS` | 20 samples ไม่มี NAN / TIMEOUT และ step ระหว่าง sample ≤ 1 °C / 3 %RH | `CONTINUOUS_READ_FAIL` / `NOISE_TOO_HIGH` |

พิมพ์ `r` ใน Serial Monitor เพื่อทดสอบซ้ำ

---

## Expected Report (passing board)

<!-- TODO: [MASSMORE_INPUT_REQUIRED: paste real report from a passing board after hardware test] -->

```text
#MASSMORE_FACTORY_TEST v1.0
#PRODUCT Massmore_SHT3x
#MCU ESP32
#LIB 2.0.0
#RESULT BUS_SCAN PASS 0x44
#RESULT CHIP_ID PASS 0x8010
#RESULT SERIAL PASS 0x........
#RESULT AUTHENTICITY PASS GENUINE
  [ok]   ACK
  [ok]   STATUS_CRC
  [ok]   STATUS_RESERVED_ZERO
  [ok]   RESET_FLAG
  [ok]   CLEAR_STATUS
  [ok]   SERIAL_NUMBER
  [ok]   HEATER_BIT
  [ok]   MEAS_CRC
  [ok]   MEAS_RANGE
  [ok]   CMD_FAILED_FLAG
#RESULT RANGE_TEMP PASS 26.4
#RESULT RANGE_HUMI PASS 61.2
#RESULT CONTINUOUS PASS 20/20
#VERDICT PASS
[PASS] SENSOR QA PASSED - READY TO SHIP
Press 'r' to run again
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `#RESULT BUS_SCAN FAIL NONE` | สาย SDA/SCL สลับ, ไม่มีไฟ, สาย Qwiic เสียบไม่สุด | ตรวจสาย · วัด 3.3 V ที่ `VCC` |
| `CHIP_ID FAIL` / `CRC_FAIL` | สายยาวเกิน, สัญญาณรบกวน, Pull-up ไม่พอ | ใช้สายสั้น < 30 cm · ลด I2C เป็น 100 kHz |
| `AUTHENTICITY FAIL SUSPECT` | ชิปไม่ตอบ Command ของ Sensirion ครบ | บอร์ดต้องสงสัย คัดออก |
| `NOISE_TOO_HIGH` | มีลมร้อน/หายใจรดขณะทดสอบ, ไฟเลี้ยงไม่นิ่ง | ทดสอบในที่นิ่ง · เปลี่ยนสาย USB |
| อัปโหลดไม่ขึ้น `Timed out waiting for packet header` | Baud สูงเกินสำหรับสาย USB นั้น | ลด baud เป็น 512000 / 460800 · กดปุ่ม BOOT ค้างตอนเริ่มแฟลช |
