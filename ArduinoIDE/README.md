# โฟลเดอร์สำหรับ Arduino IDE

โฟลเดอร์ที่ต้องติดตั้งคือ **`Massmore_SHT3x/`** (โฟลเดอร์ที่มี `library.properties` อยู่ข้างใน)
ไม่ใช่โฟลเดอร์ `ArduinoIDE` นี้

## วิธีติดตั้ง

### วิธีที่ 1 — จากไฟล์ ZIP

1. บีบอัดโฟลเดอร์ `Massmore_SHT3x` ให้เป็น `Massmore_SHT3x.zip`
2. Arduino IDE → **Sketch → Include Library → Add .ZIP Library…**
3. เลือกไฟล์ ZIP ที่เพิ่งบีบอัด

### วิธีที่ 2 — คัดลอกโฟลเดอร์

คัดลอก `Massmore_SHT3x` ทั้งโฟลเดอร์ไปวางที่

| ระบบปฏิบัติการ | ตำแหน่ง |
|---|---|
| macOS | `~/Documents/Arduino/libraries/` |
| Windows | `Documents\Arduino\libraries\` |
| Linux | `~/Arduino/libraries/` |

ปิดเปิด Arduino IDE ใหม่ แล้วดูตัวอย่างที่ **File → Examples → Massmore_SHT3x**

## สิ่งที่ต้องมีก่อน

- Arduino IDE 2.x
- ESP32 board package **core 3.x** (ติดตั้งผ่าน Boards Manager)
  ใส่ URL นี้ใน Preferences → Additional Boards Manager URLs
  ```
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
  ```
- เลือกบอร์ดให้ตรงกับที่ใช้ เช่น **ESP32 Dev Module**
- ตั้ง Serial Monitor ที่ **115200 baud**

## ตัวอย่างทั้งหมด

ดูรายละเอียดของทั้ง 15 ตัวอย่างได้ที่ [README หลักของรีโป](../README.md#ตัวอย่างทั้งหมด)

---

**by Massmore** · [massmore.shop](https://www.massmore.shop)
