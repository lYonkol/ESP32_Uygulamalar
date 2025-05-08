#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include <Preferences.h>

#define DS1302_CLK 19
#define DS1302_IO  18
#define DS1302_CE  5
#define HIGROMETRE_PIN 34
#define LED_GREEN 33
#define LED_YELLOW 32
#define RELAY_PIN 16

const char* ssid = "Enes_Emek";
const char* password = "126985473Ee";

AsyncWebServer server(80);
ThreeWire myWire(DS1302_IO, DS1302_CLK, DS1302_CE);
RtcDS1302<ThreeWire> Rtc(myWire);
Adafruit_SHT31 sht31 = Adafruit_SHT31();
Preferences prefs;

float dirt_hum = 0;
float air_temperature = 0.0;
float air_humidity = 0.0;
uint8_t sempling_rate = 100;
uint16_t dry_value;
uint16_t wet_value;

String lastWateringTime = "Henüz Sulama Yok";
String systemStatus = "Normal";

bool calibrationMode = false;
uint8_t pressCount = 0;
bool isWatering = false;
bool alreadyWateredThisMinute = false;
unsigned long wateringStartTime = 0;

String autoWaterTime1 = "08:00";
String autoWaterTime2 = "18:00";

void Read_SHT31() {
  float temp = sht31.readTemperature();
  float hum = sht31.readHumidity();
  if (!isnan(temp)) air_temperature = temp;
  if (!isnan(hum)) air_humidity = hum;
}

float Read_Higometre() {
  long toplam = 0;
  for (int i = 0; i <= sempling_rate; i++) {
    toplam += analogRead(HIGROMETRE_PIN);
    delay(5);
  }
  return toplam / sempling_rate;
}

float CalculateDirtHumidity() {
  if (wet_value >= dry_value) {
    Serial.println("Kalibrasyon değerleri hatalı");
    return 0;
  }
  float percent = (dry_value - dirt_hum) * 100.0 / (dry_value - wet_value);
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  return percent;
}

String getFormattedTime() {
  RtcDateTime now = Rtc.GetDateTime();
  char buffer[25];
  snprintf(buffer, sizeof(buffer), "%02u/%02u/%04u %02u:%02u:%02u",
           now.Day(), now.Month(), now.Year(),
           now.Hour(), now.Minute(), now.Second());
  return String(buffer);
}

void startCalibration() {
  isWatering = false;
  calibrationMode = true;
  pressCount = 0;
  systemStatus = "Kalibrasyon Modu";
  Serial.println("🌱 Kalibrasyon Modu Başladı!");
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_YELLOW, LOW);
}

void handleCalibration() {
  pressCount++;
  if (pressCount == 1) {
    dirt_hum = Read_Higometre();
    dry_value = dirt_hum;
    Serial.print("Kuru Toprak Kalibrasyon Değeri: ");
    Serial.println(dry_value);
  } else if (pressCount == 2) {
    dirt_hum = Read_Higometre();
    wet_value = dirt_hum;
    Serial.print("Islak Toprak Kalibrasyon Değeri: ");
    Serial.println(wet_value);
    saveCalibration();
  }
}

void saveCalibration() {
  prefs.putUShort("dry", dry_value);
  prefs.putUShort("wet", wet_value);
  Serial.println("Kalibrasyon EEPROM'a kaydedildi");
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, HIGH);
  calibrationMode = false;
  systemStatus = "Normal";
}

void startWatering() {
  if (calibrationMode || air_humidity > 80.0) {
    Serial.println("Sulama koşulları uygun değil");
    return;
  }
  digitalWrite(RELAY_PIN, LOW);
  isWatering = true;
  wateringStartTime = millis();
  lastWateringTime = getFormattedTime();
  prefs.putString("lastWatering", lastWateringTime);
  systemStatus = "Sulama Yapılıyor";
  Serial.println("Sulama Başladı!");
}

void stopWatering() {
  digitalWrite(RELAY_PIN, HIGH);
  delay(300);
  isWatering = false;
  systemStatus = "Normal";
  Serial.println("Sulama Bitti!");
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
  <meta charset="UTF-8">
  <title>🌱 Akıllı Sulama Sistemi</title>
  <style>
    body { font-family: Arial; background-color: #eef; text-align: center; padding: 20px; }
    h1 { color: #2a7; }
    button, input { padding: 10px; margin: 10px; font-size: 16px; }
  </style>
</head>
<body>
  <h1>🌿 Akıllı Sulama Paneli</h1>
  <div id="data">Veriler yükleniyor...</div>
  <br>
  <button onclick="fetch('/calibrate')">Kalibrasyon</button>
  <button onclick="fetch('/manualwater')">Manuel Sulama</button>
  <br><br>
  <label>Sulama Saati 1 (HH:MM):</label><br>
  <input type="time" id="time1" value="08:00"><br>
  <label>Sulama Saati 2 (HH:MM):</label><br>
  <input type="time" id="time2" value="18:00"><br>
  <button onclick="setTimes()">Zamanları Kaydet</button>
<script>
function fetchData() {
  fetch('/sensor')
    .then(response => response.text())
    .then(data => {
      document.getElementById('data').innerHTML = data;
    });
}
function setTimes() {
  const t1 = document.getElementById('time1').value;
  const t2 = document.getElementById('time2').value;
  fetch(`/settime?t1=${t1}&t2=${t2}`)
    .then(response => response.text())
    .then(alert);
}
setInterval(fetchData, 1000);
fetchData();
</script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, HIGH);
  digitalWrite(RELAY_PIN, HIGH);

  prefs.begin("calib", false);
  dry_value = prefs.getUShort("dry", 2500);
  wet_value = prefs.getUShort("wet", 1500);
  lastWateringTime = prefs.getString("lastWatering", "Henüz Sulama Yok");
  autoWaterTime1 = prefs.getString("autoTime1", "08:00");
  autoWaterTime2 = prefs.getString("autoTime2", "18:00");

  Wire.begin(21, 22);
  Rtc.Begin();

  if (!Rtc.IsDateTimeValid()) {
    Serial.println("RTC geçersiz, derleme zamanına ayarlanıyor.");
    Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
  }

  if (!sht31.begin(0x44)) {
    Serial.println("SHT31 bulunamadı!");
    while (1);
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi bağlandı. IP: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html; charset=utf-8", index_html);
  });

  server.on("/sensor", HTTP_GET, [](AsyncWebServerRequest *request){
    String nowTime = isWatering ? "Sulama Aktif" : getFormattedTime();
    float percent = CalculateDirtHumidity();
    String data = 
      "🕒 Saat: " + nowTime + "<br><br>" +
      "📶 Sistem Durumu: " + systemStatus + "<br><br>" +
      "🌡️ Hava Sıcaklığı: " + String(air_temperature, 1) + " °C<br>" +
      "💧 Hava Nemi: " + String(air_humidity, 1) + " %<br>" +
      "🌱 Toprak Nem: " + String(percent, 1) + " %<br><br>" +
      "💦 Son Sulama: " + lastWateringTime;
    request->send(200, "text/html", data);
  });

  server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!calibrationMode) {
      startCalibration();
    } else {
      handleCalibration();
    }
    request->send(200, "text/plain", "Kalibrasyon işlemi uygulandı.");
  });

  server.on("/manualwater", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!isWatering) {
      startWatering();
      request->send(200, "text/plain", "Manuel sulama başlatıldı.");
    } else {
      request->send(200, "text/plain", "Zaten sulama yapılıyor.");
    }
  });

  server.on("/settime", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("t1") && request->hasParam("t2")) {
      autoWaterTime1 = request->getParam("t1")->value();
      autoWaterTime2 = request->getParam("t2")->value();
      prefs.putString("autoTime1", autoWaterTime1);
      prefs.putString("autoTime2", autoWaterTime2);
      request->send(200, "text/plain", "Zamanlar kaydedildi.");
    } else {
      request->send(400, "text/plain", "Parametre eksik.");
    }
  });

  server.begin();
}

void loop() {
  if (!calibrationMode && !isWatering) {
    dirt_hum = Read_Higometre();
    Read_SHT31();
  }

  if (!calibrationMode && !isWatering) {
    float percent = CalculateDirtHumidity();
    if (percent < 30.0 && air_humidity < 80.0) {
      startWatering();
    }
  }

  if (isWatering && millis() - wateringStartTime > 2000) {
    stopWatering();
  }

if (!calibrationMode && !isWatering) {
  RtcDateTime now = Rtc.GetDateTime();
  char currentTime[6];
  snprintf(currentTime, sizeof(currentTime), "%02u:%02u", now.Hour(), now.Minute());

  if (!alreadyWateredThisMinute &&
      (String(currentTime) == autoWaterTime1 || String(currentTime) == autoWaterTime2)) {
    startWatering();
    alreadyWateredThisMinute = true;
  }

  static String lastMinute = "";
  String currentMinute = String(currentTime);
  if (currentMinute != lastMinute) {
    alreadyWateredThisMinute = false;
    lastMinute = currentMinute;
  }
}


  if (Serial.available()) {
    char command = Serial.read();
    if (command == 'k' || command == 'K') {
      if (!calibrationMode) startCalibration();
      else handleCalibration();
    }
  }

  delay(500);
}
