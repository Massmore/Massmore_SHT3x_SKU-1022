/*
  ไฟล์นี้สร้างจากตัวอย่างชื่อเดียวกันในโฟลเดอร์ ArduinoIDE
  เนื้อหาเหมือนกันทุกบรรทัด ต่างแค่ #include <Arduino.h> ที่ PlatformIO ต้องการ
  วิธีใช้: คัดลอกไฟล์นี้ไปทับ PlatformIO/src/main.cpp แล้วกด Upload
*/

#include <Arduino.h>

/*
  10_DewPoint_Comfort - ค่าที่คำนวณต่อจากอุณหภูมิและความชื้น

  ไลบรารีมีสูตรมาตรฐานให้พร้อมใช้ ทุกตัวเป็น static เรียกได้โดยไม่ต้องมีอ็อบเจกต์

    dewPoint()             จุดน้ำค้าง (Magnus)  - บอกว่าที่อุณหภูมิเท่าไรน้ำจะเริ่มควบแน่น
    absoluteHumidity()     ความชื้นสัมบูรณ์ g/m3 - ปริมาณไอน้ำจริงในอากาศ
    heatIndex()            ดัชนีความร้อน (NOAA) - อุณหภูมิที่ร่างกายรู้สึก
    saturationVaporPressure()  ความดันไออิ่มตัว hPa

  ใช้ทำอะไรได้บ้าง
    - จุดน้ำค้าง: เตือนก่อนเกิดฝ้าที่กระจก หรือเชื้อราในห้อง
                  ถ้าอุณหภูมิผิวของวัตถุต่ำกว่าจุดน้ำค้าง น้ำจะเกาะแน่นอน
    - ความชื้นสัมบูรณ์: ใช้เทียบอากาศในกับนอก ว่าเปิดหน้าต่างแล้วจะแห้งขึ้นหรือชื้นขึ้น
                  (เทียบด้วย %RH อย่างเดียวจะผิด เพราะ %RH ขึ้นกับอุณหภูมิด้วย)
    - heat index: เตือนความเสี่ยงฮีตสโตรกในโรงงานหรือกลางแจ้ง

  by Massmore  |  MIT License
*/

#include <Massmore_SHT3x.h>

#define PIN_SDA 21
#define PIN_SCL 22

MassmoreSHT3x sht;

// ประเมินความสบายของอากาศจากจุดน้ำค้าง (เกณฑ์ที่ใช้กันในงานปรับอากาศ)
const __FlashStringHelper *comfortFromDewPoint(float dewPoint) {
  if (dewPoint < 10.0f) return F("แห้งมาก");
  if (dewPoint < 13.0f) return F("สบาย แห้ง");
  if (dewPoint < 16.0f) return F("สบาย");
  if (dewPoint < 18.0f) return F("เริ่มรู้สึกชื้น");
  if (dewPoint < 21.0f) return F("ชื้น ไม่ค่อยสบาย");
  if (dewPoint < 24.0f) return F("ชื้นมาก อึดอัด");
  return F("ชื้นจัด อันตรายถ้าออกแรง");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    ;
  }

  Serial.println();
  Serial.println(F("Massmore SHT3x - 10 ค่าที่คำนวณต่อ"));

  if (!sht.begin(MASSMORE_SHT3X_I2C_ADDR_A, MASSMORE_SHT3X_VARIANT_SHT31,
                 PIN_SDA, PIN_SCL)) {
    Serial.print(F("เริ่มต้นไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    while (true) {
      delay(1000);
    }
  }
}

void loop() {
  float t = 0.0f;
  float h = 0.0f;

  if (!sht.measure(&t, &h)) {
    Serial.print(F("อ่านค่าไม่สำเร็จ: "));
    Serial.println(sht.lastErrorString());
    delay(1000);
    return;
  }

  float dew = MassmoreSHT3x::dewPoint(t, h);
  float absolute = MassmoreSHT3x::absoluteHumidity(t, h);
  float feels = MassmoreSHT3x::heatIndex(t, h);
  float svp = MassmoreSHT3x::saturationVaporPressure(t);

  Serial.println(F("------------------------------"));
  Serial.print(F("อุณหภูมิ         "));
  Serial.print(t, 2);
  Serial.print(F(" C  ("));
  Serial.print(MassmoreSHT3x::celsiusToFahrenheit(t), 1);
  Serial.println(F(" F)"));

  Serial.print(F("ความชื้นสัมพัทธ์  "));
  Serial.print(h, 2);
  Serial.println(F(" %RH"));

  Serial.print(F("จุดน้ำค้าง       "));
  Serial.print(dew, 2);
  Serial.print(F(" C   -> "));
  Serial.println(comfortFromDewPoint(dew));

  Serial.print(F("ความชื้นสัมบูรณ์  "));
  Serial.print(absolute, 2);
  Serial.println(F(" g/m3"));

  Serial.print(F("รู้สึกเหมือน      "));
  Serial.print(feels, 2);
  Serial.print(F(" C  (ต่างจากจริง "));
  Serial.print(feels - t, 2);
  Serial.println(F(" องศา)"));

  Serial.print(F("ความดันไออิ่มตัว  "));
  Serial.print(svp, 2);
  Serial.println(F(" hPa"));

  // เตือนเรื่องการควบแน่น: ถ้าจุดน้ำค้างใกล้อุณหภูมิห้องมาก แปลว่าอากาศเกือบอิ่มตัว
  if (t - dew < 2.0f) {
    Serial.println(F(">> เตือน อากาศเกือบอิ่มตัว มีโอกาสเกิดหยดน้ำเกาะบนผิววัตถุ"));
  }
  if (feels > 40.0f) {
    Serial.println(F(">> เตือน ดัชนีความร้อนสูง เสี่ยงต่อการเป็นลมแดด"));
  }

  delay(2000);
}
