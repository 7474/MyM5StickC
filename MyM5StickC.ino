#include <M5StickC.h>
#include "esp_adc_cal.h"

#include <Wire.h>
#include "Adafruit_BMP280.h"
#include "ClosedCube_SHT31D.h"

#include <WiFi.h>
#include "env.h"

#include <EEPROM.h>
#include "MackerelClient.h"
MackerelHostMetric hostMetricsPool[10];
MackerelServiceMetric serviceMetricsPool[10];
MackerelClient mackerelClient(hostMetricsPool, 10, serviceMetricsPool, 10, mackerelApiKey);

#include "AkashiClient.h"
AkashiClient akashiClient;

#define VARSION = "MyM5StickC 0.0.3"
#define MY_SHT30_ADDRESS 0x44
#define MY_BMP280_ADDRESS 0x76

// EEPROM
const int eepromSize = 1000;
// Mackerel
const int hostIdVersionAddress = 0;
const char currentHostIdVersion[4] = "001";
char hostIdVersion[4];
const int hostIdAddress = 4;
// ホストIDは固定長だけれど何となく長めに取っておく。
char hostId[32];
// AKASHI
const int akashiTokenVersionAddress = 36;
const char currentAkashiTokenVersion[4] = "001";
char akashiTokenVersion[4];
const int akashiTokenValueAddress = 40;
// UUID文字列
char akashiTokenValue[50];
const int akashiTokenExpiredAtAddress = 90;
time_t akashiTokenExpiredAt;
// /EEPROM

RTC_TimeTypeDef RTCtime;
RTC_DateTypeDef RTCDate;

char flushStrBuf[64];

void putLog(char* message) {
  Serial.println(message);
}

void drawTime(int x, int y) {
  M5.Rtc.GetTime(&RTCtime);
  M5.Rtc.GetData(&RTCDate);

  sprintf(flushStrBuf, "%d/%02d/%02d %02d:%02d:%02d",
          RTCDate.Year, RTCDate.Month, RTCDate.Date,
          RTCtime.Hours, RTCtime.Minutes, RTCtime.Seconds);

  // XXX なんかLCDなんも出ない
  M5.Lcd.setCursor(x, y);
  M5.Lcd.println(flushStrBuf);
}

void drawLog(char* message) {
  drawTime(0, 0);
  
  // XXX なんかLCDなんも出ない
  M5.Lcd.setCursor(0, 15);
  M5.Lcd.println(message);
}

void flushTime() {
  drawTime(5, 10);
}

ClosedCube_SHT31D sht3xd;
SHT31D sht3xResult;

Adafruit_BMP280 bmp; // I2C
float temperature; // C
uint32_t pressure; // Pa
float altitude; // m
// TODO get current seaLevelhPa
// https://www.data.jma.go.jp/obd/stats/data/mdrr/synopday/data1s.html
// 毎日の全国データ一覧表（日別値詳細版:2020年11月14日）
// 地点 現地平均  海面平均  最低海面値 最低海面起時
// 東京 1023.0  1026.0  1022.5  02:51
float seaLevelhPa = 1026.0; // hPa

void flushEnv() {
  //  InkPageSprite.drawString(5, 30, "BMP280");
  //  sprintf(flushStrBuf, "%2.2fC %4.1fhPa %4.0fm",
  //          temperature, pressure / 100.0f, altitude);
  //  InkPageSprite.drawString(10, 45, flushStrBuf);
  //
  //  InkPageSprite.drawString(5, 60, "SHT30");
  //  if (sht3xResult.error == SHT3XD_NO_ERROR) {
  //    sprintf(flushStrBuf, "%2.2fC %2.1f%%",
  //            sht3xResult.t, sht3xResult.rh);
  //  } else {
  //    sprintf(flushStrBuf, "[ERROR] Code #%d", sht3xResult.error);
  //  }
  //  InkPageSprite.drawString(10, 75, flushStrBuf);
  //
  //  InkPageSprite.pushSprite();
}

void updateEnv() {
  sht3xResult = sht3xd.periodicFetchData();

  temperature =  bmp.readTemperature();
  pressure = bmp.readPressure();
  altitude = bmp.readAltitude(seaLevelhPa);
}

void sendEnvToMackerel() {
  // Host Metrics
  // M5StickC
  float batVoltage = M5.Axp.GetBatVoltage();
  float batCurrent =  M5.Axp.GetBatCurrent();
  mackerelClient.addHostMetric("custom.battery.voltage.lipo80mah", batVoltage);
  mackerelClient.addHostMetric("custom.battery.current.lipo80mah", batCurrent);
  // SHT30
  mackerelClient.addHostMetric("custom.sht30.t", sht3xResult.t);
  mackerelClient.addHostMetric("custom.sht30.rh", sht3xResult.rh);
  // BMP280
  // 自宅のRaspberry PiがBME280からの値を bme280.* で送っているのでそれに合わせている。
  // が、温度と気圧のセンサが同じものなのかは知らない。
  // https://github.com/7474/RealDiceBot/blob/master/IoTEdgeDevice/files/usr/local/bin/bme280/bme280.sh
  mackerelClient.addHostMetric("custom.bme280.temperature", temperature);
  mackerelClient.addHostMetric("custom.bme280.pressure", pressure / 100.0f);
  mackerelClient.postHostMetrics();
}

void updateMyM5StickC() {
  putLog("update proc start.");
  updateEnv();
  flushEnv();
  flushTime();
  sendEnvToMackerel();
  putLog("update proc end.");
}

void updateAkashiToken() {
  Serial.println("updateAkashiToken");
  bool needUpdate = false;
  EEPROM.get(akashiTokenVersionAddress, akashiTokenVersion);
  EEPROM.get(akashiTokenValueAddress, akashiTokenValue);
  EEPROM.get(akashiTokenExpiredAtAddress, akashiTokenExpiredAt);
  Serial.println(akashiTokenVersion);
  Serial.println(akashiTokenValue);
  Serial.println(akashiTokenExpiredAt);

  if (!strcmp(currentAkashiTokenVersion, akashiTokenVersion)) {
    akashiClient.setToken(akashiTokenValue);
    time_t now = time(NULL);
    Serial.println(now);
    Serial.println(now + 60 * 60 * 24 * 7);
    // 期限まで7週間くらいで更新しとく
    needUpdate = akashiTokenExpiredAt < now + 60 * 60 * 24 * 7;
  } else {
    needUpdate = true;
  }
  if (needUpdate) {
    int res = akashiClient.updateToken(akashiTokenValue, akashiTokenExpiredAt);
    Serial.print("updateToken #");
    Serial.println(res);
    Serial.println(akashiTokenValue);
    Serial.println(akashiTokenExpiredAt);
    if (!res) {
      akashiClient.setToken(akashiTokenValue);
      EEPROM.put(akashiTokenVersionAddress, currentAkashiTokenVersion);
      EEPROM.put(akashiTokenValueAddress, akashiTokenValue);
      EEPROM.put(akashiTokenExpiredAtAddress, akashiTokenExpiredAt);
      EEPROM.commit();
      drawLog("updateToken Seiko.");
    } else {
      // 更新失敗したら規定値に戻しておく
      EEPROM.put(akashiTokenVersionAddress, "");
      EEPROM.commit();
      akashiClient.setToken(akashiToken);
      drawLog("updateToken Shippai.");
    }
  }
}

void setupM5StickC() {
  M5.begin();

  // https://github.com/m5stack/m5-docs/blob/master/docs/en/api/lcd_m5stickc.md
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.fillScreen(BLUE);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.drawCentreString("My M5StickC", 80, 30, 1);
  delay(2000);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 0);

  // Grove for M5StickC
  Wire.begin(0, 26);

  // https://github.com/espressif/arduino-esp32/blob/master/libraries/EEPROM/examples/eeprom_write/eeprom_write.ino
  Serial.println("start...");
  if (!EEPROM.begin(eepromSize))
  {
    Serial.println("failed to initialise EEPROM"); delay(1000000);
  }
  Serial.println(" bytes read from Flash . Values are:");
  for (int i = 0; i < eepromSize; i++)
  {
    Serial.print(byte(EEPROM.read(i))); Serial.print(" ");
  }
  Serial.println();

  putLog("M5StickC initialized.");
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (WiFi.begin(ssid, password) != WL_DISCONNECTED) {
    ESP.restart();
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }

  putLog("Wi-Fi initialized.");
}

void setupTime() {
  // https://wak-tech.com/archives/833
  configTime(9 * 3600L, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");//NTPの設定
  struct tm timeInfo;
  getLocalTime(&timeInfo);

  RTCtime.Hours = timeInfo.tm_hour;
  RTCtime.Minutes = timeInfo.tm_min;
  RTCtime.Seconds = timeInfo.tm_sec;
  M5.Rtc.SetTime(&RTCtime);

  RTCDate.Year = timeInfo.tm_year + 1900;
  RTCDate.Month = timeInfo.tm_mon + 1;
  RTCDate.Date = timeInfo.tm_mday;
  M5.Rtc.SetData(&RTCDate);

  putLog("Time synced.");
}

void setupEnv() {
  // https://github.com/closedcube/ClosedCube_SHT31D_Arduino/blob/master/examples/periodicmode/periodicmode.ino
  sht3xd.begin(MY_SHT30_ADDRESS);
  Serial.print("Serial #");
  Serial.println(sht3xd.readSerialNumber());
  if (sht3xd.periodicStart(SHT3XD_REPEATABILITY_HIGH, SHT3XD_FREQUENCY_10HZ) != SHT3XD_NO_ERROR)
    Serial.println("[ERROR] Cannot start periodic mode");
  putLog("SHT30 initialized.");

  // https://github.com/adafruit/Adafruit_BMP280_Library/blob/master/examples/bmp280test/bmp280test.ino
  if (!bmp.begin(MY_BMP280_ADDRESS)) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  }

  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
  putLog("BMP280 initialized.");

  putLog("Env initialized.");
}

void setupMackerel() {
  EEPROM.get(hostIdVersionAddress, hostIdVersion);
  if (!strcmp(currentHostIdVersion, hostIdVersion)) {
    // 永続化されたホストIDを読む
    EEPROM.get(hostIdAddress, hostId);
    Serial.print("loadedHost: ");
    Serial.println(hostId);
  } else {
    // ホストIDがなければ登録する
    mackerelClient.registerHost("register-by-m5", hostId);
    Serial.print("registerHost: ");
    Serial.println(hostId);
    EEPROM.put(hostIdVersionAddress, currentHostIdVersion);
    EEPROM.put(hostIdAddress, hostId);
    EEPROM.commit();
  }

  mackerelClient.setHostId(hostId);

  putLog("Mackerel initialized.");
}

void setupAkashi() {
  akashiClient.setCompanyCode(akashiCompanyCode);
  akashiClient.setToken(akashiToken);

  putLog("Akashi initialized.");
}

void setup() {
  setupM5StickC();
  setupWiFi();
  setupTime();
  setupEnv();
  setupMackerel();
  setupAkashi();

  drawLog("Initialized.");

  updateMyM5StickC();
}

void onBtnA() {
  updateAkashiToken();
  int res = akashiClient.stamp(AkashiStampTypeAuto);
  Serial.print("AkashiStampTypeAuto #");
  Serial.println(res);
  if (!res) {
    drawLog("Kintai Seiko.");
  } else {
    drawLog("Kintai Shippai.");
  }
}

void onBtnB() {
  // T.B.D.
}

unsigned long  lastUpdateMillis = 0;
void loop() {
  // 60秒毎にもろもろ処理する。
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdateMillis > 60 * 1000) {
    lastUpdateMillis = currentMillis;
    updateMyM5StickC();
  }
  // ボタンハンドリング。
  M5.update();
  if (M5.BtnA.wasPressed()) {
    onBtnA();
  }
  if (M5.BtnB.wasPressed()) {
    onBtnB();
  }
}
