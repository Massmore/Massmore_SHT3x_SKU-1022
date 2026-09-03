# โฟลเดอร์สำหรับ VS Code + PlatformIO

เปิดโฟลเดอร์ **นี้** ด้วย VS Code ได้เลย (ไม่ใช่โฟลเดอร์แม่)

```bash
code PlatformIO
```

ไลบรารีอยู่ใน `lib/Massmore_SHT3x/` อยู่แล้ว PlatformIO จะหาเจอเองโดยไม่ต้องตั้งค่าเพิ่ม

## ใช้ตัวอย่าง

```bash
cp examples/01_BasicReading/main.cpp src/main.cpp
pio run -t upload -t monitor
```

## บอร์ดที่เตรียม env ไว้ให้

| env | บอร์ด |
|---|---|
| `esp32dev` | ESP32-WROOM-32 (ค่าเริ่มต้น) |
| `esp32-s3-devkitc-1` | ESP32-S3 |
| `esp32-s2-saola-1` | ESP32-S2 |
| `esp32-c3-devkitm-1` | ESP32-C3 |
| `esp32-c6-devkitc-1` | ESP32-C6 |

```bash
pio run -e esp32-s3-devkitc-1 -t upload
```

## รันชุดทดสอบบนเครื่อง PC

ไม่ต้องมีบอร์ด ไม่ต้องมี PlatformIO ใช้แค่ `g++`

```bash
cd test && make
```

## Arduino core เวอร์ชันไหน

`platformio.ini` pin ไปที่ pioarduino `55.03.311` ซึ่งให้ **Arduino ESP32 core 3.3.11**
ตรงกับที่ Arduino IDE รุ่นล่าสุดใช้ และทำให้ผลการ build ซ้ำได้เหมือนเดิมทุกครั้ง

---

**by Massmore** · [massmore.shop](https://www.massmore.shop)
