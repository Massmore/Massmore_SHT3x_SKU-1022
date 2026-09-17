<div align="center">

<img src="docs/images/01_massmore_sht3x_cover.png" alt="Massmore SHT3X Temperature & Humidity Sensor" width="520">

# Massmore_SHT3x

**Arduino IDE / PlatformIO driver for the Massmore SHT3X (SKU-1022) temperature & humidity sensor board**
Sensirion SHT30 · SHT31 · SHT35 — รุ่นปกติ (`-B`) และรุ่นกันฝุ่นกันน้ำ (`-F` Outdoor)

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-2.0.0-green.svg)](ArduinoIDE/Massmore_SHT3x/library.properties)
[![ESP32](https://img.shields.io/badge/Arduino--ESP32-Core_3.x-00979D.svg)](#mcu-compatibility--limitation-matrix)
[![AVR](https://img.shields.io/badge/AVR-ATmega328P-orange.svg)](#mcu-compatibility--limitation-matrix)

*Designed and Manufactured by Massmore*

</div>

---

## Table of Contents

1. [Product Overview](#product-overview)
2. [Pinout Table](#pinout-table)
3. [MCU Compatibility & Limitation Matrix](#mcu-compatibility--limitation-matrix)
4. [Installation](#installation)
5. [Quick Start Code](#quick-start-code)
6. [Pin Mapping Examples](#pin-mapping-examples)
7. [API Reference](#api-reference)
8. [Examples](#examples)
9. [Factory Test & Web Serial Monitor](#factory-test--web-serial-monitor)
10. [ข้อควรรู้จากการทดสอบบนฮาร์ดแวร์จริง](#ข้อควรรู้จากการทดสอบบนฮาร์ดแวร์จริง)
11. [Where to Buy](#where-to-buy)
12. [License](#license)

---

## Product Overview

บอร์ดเซ็นเซอร์อุณหภูมิและความชื้นแบบ Digital I2C ใช้ชิป **Sensirion SHT3x-DIS** ของแท้
ประกอบและทดสอบทุกบอร์ดในประเทศไทยด้วย Factory Test ก่อนส่ง

| Item | Specification |
|---|---|
| Sensor IC | Sensirion SHT30-DIS / SHT31-DIS / SHT35-DIS (ติ๊กรุ่นบน Silkscreen) |
| Interface | I2C, 0 – 1000 kHz (แนะนำ 100 kHz) |
| I2C Address | `0x44` (default) · `0x45` (บัดกรี Jumper `ADDR` ให้ปิด) |
| Supply / Logic | 3 – 5 V ทุกขา (บนบอร์ดมี 3.3 V LDO และ Bidirectional level shifter) |
| Temperature | −40 … +125 °C, accuracy ±0.2 °C (SHT30/31) · ±0.1 °C (SHT35) |
| Humidity | 0 … 100 %RH, accuracy ±2 %RH (SHT30/31) · ±1.5 %RH (SHT35) |
| Resolution | 16-bit T และ RH |
| Max sample rate | 10 Hz (Periodic Mode) |
| Extra pins | `ALRT` (Alert output, push-pull) · `RST` (nRESET, มี Pull-up บนบอร์ด) · `ADDR` |
| Connector | Qwiic-compatible 4-pin JST-SH ×2 (daisy-chain ได้) + 2.54 mm header 7 ขา |
| Board size | 25.40 × 20.32 mm, รูยึด M2 |
| Variants | `-B` ตัวถังเปิด ตอบสนองเร็ว · `-F` มี PTFE membrane กันฝุ่นกันละอองน้ำ (Outdoor) |

> **SHT30 / SHT31 / SHT35 คือ Silicon ตัวเดียวกัน** ต่างกันเฉพาะเกรดความแม่นยำที่โรงงานคัด (binning)
> ชิปไม่มี Register บอกรุ่น จึงอ่านแยกรุ่นผ่าน I2C ไม่ได้ ให้ดูช่องติ๊กบน Silkscreen ของบอร์ด

<div align="center">
<img src="docs/images/02_massmore_sht3x_variants.png" alt="SHT3X variants" width="480">
</div>

---

## Pinout Table

<div align="center">
<img src="docs/images/05_massmore_sht3x_b_pinmap.png" alt="Massmore SHT3X pin map" width="520">
</div>

| Pin | Function | Notes |
|---|---|---|
| `VCC` | Power input | 3 – 5 V (LDO + level shifter บนบอร์ด) |
| `GND` | Ground | — |
| `SCL` | I2C Clock | มี Pull-up บนบอร์ด · Level-shifted <!-- TODO: [MASSMORE_INPUT_REQUIRED: pull-up value] --> |
| `SDA` | I2C Data | มี Pull-up บนบอร์ด · Level-shifted <!-- TODO: [MASSMORE_INPUT_REQUIRED: pull-up value] --> |
| `ADDR` | Address select | ปล่อยลอย = `0x44` · Jumper ด้านหลังปิด = `0x45` |
| `RST` | nRESET (active LOW) | มี Pull-up บนบอร์ด · ต่อเมื่อต้องการ `hardReset()` |
| `ALRT` | Alert output | Push-pull, HIGH เมื่อค่าเกิน threshold (Periodic Mode เท่านั้น) |

<div align="center">
<img src="docs/images/04_massmore_sht3x_pinout_dimension.png" alt="Board dimensions" width="420">
</div>

---

## MCU Compatibility & Limitation Matrix

| MCU Platform | Tested Core / Toolchain | Bus Remapping Support | Limitations / Notes |
|---|---|---|---|
| **ESP32-S3** | Arduino-ESP32 v3.x+ (pioarduino) | Full GPIO Matrix (`Wire` / `Wire1`) | None. Recommended for high-rate data. |
| **ESP32 (Classic)** | Arduino-ESP32 v3.x+ (pioarduino) | Full GPIO Matrix (`Wire` / `Wire1`) | None. **Primary Factory Test target.** |
| **AVR — Arduino Nano (ATmega328P)** | Arduino AVR Core | Fixed Hardware Pins (I2C: A4/A5) | 2 KB SRAM / 32 KB Flash. Factory Test ใช้ RAM ~960 byte. 5 V logic — บอร์ด SHT3X รองรับ 5 V โดยตรง ไม่ต้องใช้ level shifter เพิ่ม |

ไลบรารีไม่มีโค้ดเฉพาะ Platform จึงคอมไพล์บน Core อื่น (RP2040, STM32) ได้ แต่ไม่ได้ทดสอบและไม่รับประกัน

---

## Installation

### (a) Arduino IDE

1. ดาวน์โหลด [`ArduinoIDE/Massmore_SHT3x.zip`](ArduinoIDE/Massmore_SHT3x.zip)
2. **Sketch → Include Library → Add .ZIP Library…** เลือกไฟล์ที่โหลดมา
3. ตัวอย่างจะอยู่ที่ **File → Examples → Massmore_SHT3x**

ต้องใช้ **ESP32 board package 3.x ขึ้นไป** (Boards Manager → esp32 by Espressif) หรือ Arduino AVR Boards สำหรับ Nano

### (b) PlatformIO

เปิดโฟลเดอร์ [`PlatformIO/`](PlatformIO/) ด้วย VS Code แล้วกด **Build** ได้เลย ไม่ต้องติดตั้งอะไรเพิ่ม

```bash
git clone https://github.com/Massmore/Massmore_SHT3x_SKU-1022.git
code Massmore_SHT3x_SKU-1022/PlatformIO
```

`platformio.ini` เตรียม environment ไว้ 3 ตัว: `esp32dev` (default) · `esp32-s3-devkitc-1` · `nano`
ESP32 ใช้ **pioarduino** platform pin ไว้ที่ `55.03.311` (= Arduino-ESP32 Core 3.3.11)
ซึ่งเป็นเวอร์ชันที่ทดสอบผ่านบนฮาร์ดแวร์จริงแล้ว
(platform `espressif32` ตัวทางการยังติดอยู่ที่ Core 2.x จึงใช้ไม่ได้)

```bash
pio run -e esp32dev -t upload -t monitor
```

`src/main.cpp` คือ `05_Factory_Test` — ต้องการรัน example อื่น ให้คัดลอก `.ino` จาก `lib/Massmore_SHT3x/examples/` ทับ `src/main.cpp` แล้วเติม `#include <Arduino.h>` บรรทัดแรก

---

## Quick Start Code

```cpp
#include <Massmore_SHT3x.h>
#include <Wire.h>

Massmore_SHT3x sht(Wire);        // ฉีด Bus เข้ามา ไลบรารีไม่เรียก Wire.begin() เอง

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);            // ESP32: กำหนดขา SDA / SCL เอง   (AVR ใช้ Wire.begin();)
  if (!sht.begin(0x44)) {
    Serial.println(sht.lastErrorString());
    while (true) delay(1000);
  }
}

void loop() {
  Massmore_SHT3x::Reading r;
  if (sht.readAll(r)) {
    Serial.print(r.temperature, 2); Serial.print(" C  ");
    Serial.print(r.humidity, 2);    Serial.println(" %RH");
  }
  delay(1000);
}
```

---

## Pin Mapping Examples

**ESP32 (Classic) — Arduino-ESP32 Core 3.x custom GPIO**

```cpp
Wire.begin(21, 22, 100000UL);          // SDA, SCL, frequency
Massmore_SHT3x sht(Wire);
```

**ESP32-S3 — custom GPIO บน Bus ที่สอง**

```cpp
Wire1.begin(8, 9, 400000UL);           // GPIO Matrix เลือกขาได้เกือบทุกขา
Massmore_SHT3x sht(Wire1);
```

**Arduino Nano (ATmega328P) — standard wiring**

```cpp
Wire.begin();                          // ขาตายตัว: SDA = A4, SCL = A5
Wire.setClock(100000UL);
Massmore_SHT3x sht(Wire);
```

---

## API Reference

### Setup

| Function | Description | Return |
|---|---|---|
| `Massmore_SHT3x(TwoWire &wire = Wire)` | สร้าง object ผูกกับ Bus ที่ Sketch เปิดไว้ | — |
| `begin(addr = 0x44, alertPin = -1, resetPin = -1)` | Break → Soft Reset → ตรวจ Status Register | `bool` |
| `isConnected()` | มีอุปกรณ์ตอบ ACK ที่ address หรือไม่ | `bool` |
| `setRepeatability(Repeatability)` | `RPT_LOW` (~4 ms) / `RPT_MEDIUM` (~6 ms) / `RPT_HIGH` (~15 ms, default) | — |
| `setClockStretching(bool)` | เปิด Clock Stretching ใน Blocking API (default ปิด) | — |
| `setTemperatureOffset(float)` / `setHumidityOffset(float)` | ชดเชยค่าที่อ่านได้ | — |

### Simple Blocking API

| Function | Description | Return |
|---|---|---|
| `readTemperature()` | อ่านอุณหภูมิ รอจน conversion เสร็จ | `float` °C หรือ `NAN` |
| `readHumidity()` | อ่านความชื้น | `float` %RH หรือ `NAN` |
| `readAll(Reading &r)` | อ่านทั้งคู่ใน Transaction เดียว (แนะนำ) | `bool` |

### Advanced Non-blocking FSM API

| Function | Description | Return |
|---|---|---|
| `requestConversion()` | สั่งเริ่มวัดแล้วคืนทันที | `bool` |
| `update()` | เรียกทุกรอบ `loop()` ขับ FSM / ดึงผล Periodic ตาม Rate | `bool` มีผลใหม่ |
| `isDataReady()` | ผลพร้อมหรือยัง | `bool` |
| `getReadings(Reading &r)` | ดึงผลและเคลียร์ flag | `bool` |
| `getFsmState()` | `IDLE` / `CONVERTING` / `READY` / `FAULT` | `FsmState` |

### Periodic / ART Mode

| Function | Description | Return |
|---|---|---|
| `startPeriodic(Rate, Repeatability)` | ชิปวัดเองที่ 0.5 / 1 / 2 / 4 / 10 Hz | `bool` |
| `startART()` | Periodic 4 Hz แบบ Accelerated Response Time | `bool` |
| `fetchData(Reading &r)` | ดึงผลล่าสุดทันที (`NOT_READY` ถ้ายังไม่มีผลใหม่) | `bool` |
| `stopPeriodic()` | ส่ง Break กลับ Single Shot | `bool` |

### Heater · Status · ALERT · Reset

| Function | Description | Return |
|---|---|---|
| `setHeater(bool)` / `isHeaterOn()` | เปิด/ปิด Heater ในตัว ไล่ความชื้นเกาะ | `bool` |
| `readStatus(StatusBits &)` / `clearStatus()` | อ่าน/เคลียร์ Status Register | `bool` |
| `setAlertWindow(lowT, highT, lowRH, highRH, hysT, hysRH)` | ตั้ง threshold แบบง่าย (Periodic Mode เท่านั้น) | `bool` |
| `setAlertLimits(AlertLimits)` / `getAlertLimits(AlertLimits &)` | ตั้ง/อ่าน threshold ครบ 4 ชุด | `bool` |
| `isAlertPinActive()` | ขา ALRT เป็น HIGH หรือไม่ | `bool` |
| `softReset()` / `generalCallReset()` / `hardReset()` | Reset 3 แบบ (`hardReset` ต้องส่ง `resetPin` ใน `begin()`) | `bool` |
| `recoverBus()` | กู้ I2C Bus ที่ค้าง ดู [ข้อควรรู้](#ข้อควรรู้จากการทดสอบบนฮาร์ดแวร์จริง) | `bool` |

### Chip Identity (ตรวจของแท้)

| Function | Description | Return |
|---|---|---|
| `verifyChipID()` | SHT3x ไม่มี CHIP_ID — ตรวจ Reserved bit ของ Status Register แทน | `bool` |
| `getSerialNumber()` | Serial Number 32-bit จากโรงงาน (Command `0x3780`) | `uint32_t` (0 = fail) |
| `isGenuine()` | Heuristic 9 ข้อ: Status / Clear / Serial / Heater / Alert R/W / CRC / Range | `bool` |
| `getGenuineVerdict()` / `getVerifyMask()` | ผลละเอียด `GENUINE` / `PARTIAL` / `SUSPECT` / `NOT_SHT3X` | `Genuine` / `uint16_t` |

### Error & utility

| Function | Description | Return |
|---|---|---|
| `lastError()` / `lastErrorString()` | `OK` `NOT_FOUND` `WRONG_ID` `TIMEOUT` `CRC_FAIL` `BUS_ERROR` `NOT_READY` `NOT_BEGUN` `WRONG_MODE` `BAD_ARG` | `ErrorCode` / `const char*` |
| `Massmore_SHT3x::dewPoint(t, rh)` | จุดน้ำค้าง (Magnus) | `float` °C |
| `Massmore_SHT3x::absoluteHumidity(t, rh)` | ความชื้นสัมบูรณ์ | `float` g/m³ |
| `Massmore_SHT3x::scan(Wire, found[2])` | หา SHT3x ที่ `0x44` / `0x45` | จำนวนที่พบ |

---

## Examples

| # | Example | Description |
|:-:|---|---|
| 01 | [`01_BasicRead`](ArduinoIDE/Massmore_SHT3x/examples/01_BasicRead) | Simple Blocking API อ่าน T / RH / Dew point ทุก 1 วินาที |
| 02 | [`02_CustomPins_BusRemap`](ArduinoIDE/Massmore_SHT3x/examples/02_CustomPins_BusRemap) | ESP32 / S3 ย้ายขาและใช้ `Wire1` · Nano fallback ขา A4/A5 |
| 03 | [`03_NonBlocking_Multitask`](ArduinoIDE/Massmore_SHT3x/examples/03_NonBlocking_Multitask) | FSM API + กะพริบ LED แสดงว่า `loop()` ไม่ถูก Block |
| 04 | [`04_Alert_Heater`](ArduinoIDE/Massmore_SHT3x/examples/04_Alert_Heater) | Periodic Mode + ALERT threshold ผ่าน Interrupt + Heater + Status Register |
| 05 | [`05_Factory_Test`](ArduinoIDE/Massmore_SHT3x/examples/05_Factory_Test) | **Outgoing QA/QC** พิมพ์ `#RESULT` / `#VERDICT` ให้ Web Serial Monitor อ่าน |

ทุก example คอมไพล์ผ่านบน `esp32dev` · `esp32-s3-devkitc-1` · `nano` แบบ 0 error / 0 warning จากโค้ดไลบรารี
และ **ทดสอบรันจริงครบทุกตัวบน ESP32-WROOM + Massmore SHT3X (SHT30)** แล้ว

---

## Factory Test & Web Serial Monitor

`05_Factory_Test` ใช้ตรวจบอร์ดก่อนส่ง (และลูกค้าใช้ยืนยันว่าบอร์ดปกติ) ผลออกทาง Serial 115200 baud
บรรทัดที่ขึ้นต้นด้วย `#` ถูก **Massmore Web Serial Monitor** parse อัตโนมัติ

```text
#MASSMORE_FACTORY_TEST v1.0
#PRODUCT Massmore_SHT3x
#MCU ESP32
#RESULT BUS_RECOVER PASS READY
#RESULT BUS_SCAN PASS 0x44
#RESULT CHIP_ID PASS 0x8010
#RESULT SERIAL PASS 0x2A124EE2
#RESULT AUTHENTICITY PASS GENUINE
#RESULT RANGE_TEMP PASS 26.6
#RESULT RANGE_HUMI PASS 57.0
#RESULT CONTINUOUS PASS 20/20
#VERDICT PASS
[PASS] SENSOR QA PASSED - READY TO SHIP
```

ไฟล์ `.bin` พร้อมแฟลชและคู่มือ: [`firmware/README.md`](firmware/README.md)

---

## ข้อควรรู้จากการทดสอบบนฮาร์ดแวร์จริง

ไลบรารีนี้ทดสอบบน ESP32-WROOM + Massmore SHT3X (SHT30) จริง ไม่ได้อ่านจาก Datasheet อย่างเดียว
สามเรื่องด้านล่างคือพฤติกรรมที่ Datasheet ไม่ได้บอก และมีผลกับโค้ดของผู้ใช้โดยตรง

### 1. NACK หนึ่งครั้งทำให้ I2C ของ ESP32 Core 3.x ค้างถาวร

เมื่อชิปตอบ NACK เช่นได้รับ Command ที่ไม่รู้จัก หรือ Write ที่ CRC ผิด
driver ของ Arduino-ESP32 Core 3.x จะคืน `ESP_ERR_INVALID_STATE` กับ **ทุก** Transaction ถัดไป
และไม่ฟื้นเอง แม้เรียก `Wire.end()` แล้ว `Wire.begin()` ใหม่ หรือกดปุ่ม reset ที่ MCU
(เพราะ MCU reset ไม่ได้ตัดไฟเลี้ยงเซ็นเซอร์)

วิธีกู้ที่ได้ผลคือส่ง General Call Reset ซึ่งไลบรารีห่อไว้ให้แล้ว

```cpp
if (sht.lastError() == Massmore_SHT3x::ErrorCode::BUS_ERROR) {
  sht.recoverBus();     // ส่ง General Call Reset แล้วตรวจว่าชิปกลับมาตอบ
}
```

`begin()` เรียกให้อัตโนมัติหนึ่งครั้งเมื่อไม่พบชิปในครั้งแรก จึงกู้จากการรันครั้งก่อนได้เอง
ข้อควรระวัง: General Call รีเซ็ตอุปกรณ์ I2C ตัวอื่นบน Bus เดียวกันด้วย

### 2. Soft Reset ไม่ตั้ง Reset-detected bit

`softReset()` ทำงานจริง (ยืนยันแล้วว่า Heater ถูกปิดและ Register กลับเป็นค่า default)
แต่ชิปไม่ตั้ง bit 4 ของ Status Register หลังคำสั่งนี้ ตั้งเฉพาะตอน power-up และ General Call Reset
จึง **ห้ามใช้ bit 4 ยืนยันว่า Soft Reset สำเร็จ** ไลบรารีและ Factory Test ไม่ใช้เกณฑ์นี้แล้ว

### 3. Reserved bit 6 และ 5 ไม่ได้เป็น 0 เสมอ

Datasheet ระบุ bit 9…5 เป็น reserved ที่ต้องอ่านได้ 0 แต่ขณะอยู่ใน Periodic Mode
บอร์ดจริงอ่าน Status ได้ `0x8C60` ซึ่ง bit 6 และ 5 เป็น 1
ไลบรารีจึงใช้ mask `0x538C` (ตัดสอง bit นี้ออก) ไม่เช่นนั้น `verifyChipID()` จะคืน `WRONG_ID`
ทั้งที่ชิปทำงานปกติ

---

## Where to Buy

| Channel | Link |
|---|---|
| massmore.shop | https://www.massmore.shop/products/f9e65fad-f86d-4ac3-9e95-6f575e1d33d3 |
| Shopee | https://shopee.co.th/%E0%B9%80%E0%B8%8B%E0%B9%87%E0%B8%99%E0%B9%80%E0%B8%8B%E0%B8%AD%E0%B8%A3%E0%B9%8C%E0%B8%A7%E0%B8%B1%E0%B8%94%E0%B8%AD%E0%B8%B8%E0%B8%93%E0%B8%AB%E0%B8%A0%E0%B8%B9%E0%B8%A1%E0%B8%B4-%E0%B8%84%E0%B8%A7%E0%B8%B2%E0%B8%A1%E0%B8%8A%E0%B8%B7%E0%B9%89%E0%B8%99-SHT30-SHT31-SHT35-Sensirion-%E0%B9%81%E0%B8%97%E0%B9%89-I2C-Digital-Humi-Temp-sensor-Massmore-i.5641091.43529658930 |
| Lazada | https://www.lazada.co.th/products/sht30-sht31-sht35-sensirion-i2c-digital-humi-temp-sensor-massmore-i16116238552.html |
| Product docs | https://www.massmore.shop/docs/44 |

---

## License

MIT License — Copyright (c) 2026 Massmore Biz Co., Ltd. ดู [LICENSE](LICENSE)

Datasheet: [Sensirion SHT3x-DIS](https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf)
