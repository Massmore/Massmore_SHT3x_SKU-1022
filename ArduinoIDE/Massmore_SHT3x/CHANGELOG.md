# บันทึกการเปลี่ยนแปลง

รูปแบบเวอร์ชันตาม [Semantic Versioning](https://semver.org/lang/th/)

## [1.0.0] - 2026-09-02

เวอร์ชันแรก

### เพิ่ม

- ไดรเวอร์ SHT3x-DIS เขียนจาก datasheet โดยตรง ไม่พึ่งไลบรารีอื่นนอกจาก `Wire`
- รองรับ SHT30 / SHT31 / SHT35 ทั้งรุ่นปกติ (-B) และรุ่นกันฝุ่นกันน้ำ (-F)
- โหมดวัด: single shot (สามระดับความละเอียด, เปิด/ปิด clock stretching),
  periodic ครบทั้งห้าอัตรา และ ART
- โหมดไม่บล็อก `startMeasurement()` / `isMeasurementReady()` / `readMeasurement()`
  และ `update()` สำหรับ periodic
- ฮีตเตอร์ในตัวพร้อม `runHeaterSelfTest()`
- อ่านและถอด status register ทีละบิต, `clearStatus()`
- ALERT ครบ 4 threshold พร้อม `setAlertWindow()` ที่คำนวณ hysteresis ให้
- อ่านหมายเลขซีเรียลจากโรงงาน (คำสั่ง `0x3780`)
- `verifyChip()` ตรวจพฤติกรรม 10 ข้อว่าเป็นชิป Sensirion ของแท้หรือไม่
- soft reset, general call reset และ hard reset ผ่านขา RST
- ค่าที่คำนวณต่อ: จุดน้ำค้าง, ความชื้นสัมบูรณ์, ดัชนีความร้อน, ความดันไออิ่มตัว
- ค่าชดเชยอุณหภูมิและความชื้น
- รหัสข้อผิดพลาดพร้อมคำอธิบายภาษาไทย
- ตัวอย่าง 15 ชุด รวมชุดทดสอบโรงงาน `15_FactoryTest`
- ชุดทดสอบที่รันบนเครื่อง PC ได้ 186 ข้อ (`PlatformIO/test`)
