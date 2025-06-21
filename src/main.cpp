#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "LocalTimeHelper.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Inisialisasi I2C dengan GPIO 8 (SDA) dan GPIO 9 (SCL)
TwoWire myWire = TwoWire(0);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &myWire, OLED_RESET);

const int totalFrames = 774;
const int frameSize = 1024;
const int lastFrameSubFrames = 75;
uint8_t buffer[frameSize];

WebServer server(80);

int currentScreen = 0;
int currentFrame = 0;
int currentSubFrame = 0;
unsigned long lastFrameTime = 0;
const unsigned long frameInterval = 30;
String weatherDescription = "";
float temperature = 0;

// Pin tombol & buzzer sesuai permintaan
const int BUTTON_CLOCK_PIN = 2;
const int BUTTON_WEATHER_PIN = 3;
const int BUZZER_PIN = 4;

unsigned long lastButtonClockPress = 0;
unsigned long lastButtonWeatherPress = 0;
const unsigned long debounceDelay = 50;
const unsigned long screenDuration = 2000;
unsigned long screenTimeout = 0;
bool screenTimeoutActive = false;

int alarmHour = -1;
int alarmMinute = -1;
bool alarmTriggeredToday = false;

// --- Deklarasi fungsi (biar lengkap) ---
void setupWebServer();
void saveWiFiCredentials(const String &ssid, const String &password);
void loadWiFiCredentials(String &ssid, String &password);
void fetchWeather();
void renderClock();
void renderWeather();
void handleButtonPress();
bool loadAndDecodeRLE(const char *path);
bool loadSubFrame(const char *path, int subFrameIndex);
void beep(int durationMs);
void checkAlarm();

void setup()
{
  Serial.begin(115200);

  // Inisialisasi Wire custom
  myWire.begin(8, 9); // SDA = 8, SCL = 9

  pinMode(BUTTON_CLOCK_PIN, INPUT_PULLUP);
  pinMode(BUTTON_WEATHER_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (!SPIFFS.begin(true))
  {
    Serial.println("❌ SPIFFS gagal dimount!");
    while (true)
      ;
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println("❌ OLED tidak terdeteksi!");
    while (true)
      ;
  }

  display.clearDisplay();
  display.display();

  beep(100);

  String ssid, password;
  loadWiFiCredentials(ssid, password);

  if (ssid.length() > 0)
  {
    Serial.printf("Mencoba konek ke WiFi: %s\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 20)
    {
      delay(500);
      Serial.print(".");
      tries++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("✅ WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      LocalTimeHelper::initNTP();
      fetchWeather();
    }
    else
    {
      Serial.println("❌ Gagal konek WiFi");
      for (int i = 0; i < 3; i++)
      {
        beep(100);
        delay(100);
      }
      WiFi.softAP("GantengEsp");
    }
  }
  else
  {
    WiFi.softAP("GantengEsp");
  }

  setupWebServer();
  Serial.println("Setup selesai");
}
void loop()
{
  Serial.begin(115200); // HARUS ada ini
  Serial.println("ESP32-C3 Siap");
  server.handleClient();
  handleButtonPress();
  checkAlarm();

  if (screenTimeoutActive && millis() > screenTimeout)
  {
    currentScreen = 0;
    screenTimeoutActive = false;
  }

  switch (currentScreen)
  {
  case 0:
  {
    unsigned long now = millis();
    if (now - lastFrameTime > frameInterval)
    {
      lastFrameTime = now;

      if (currentFrame < 773)
      {
        char path[32];
        sprintf(path, "/frame_%05d.bin", currentFrame);
        if (loadAndDecodeRLE(path))
        {
          display.clearDisplay();
          display.drawBitmap(0, 0, buffer, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
          display.display();
        }
        currentFrame++;
      }
      else
      {
        if (loadSubFrame("/frame_00773.bin", currentSubFrame))
        {
          display.clearDisplay();
          display.drawBitmap(0, 0, buffer, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
          display.display();
        }
        currentSubFrame++;
        if (currentSubFrame >= lastFrameSubFrames)
        {
          currentSubFrame = 0;
          currentFrame = 0;
        }
      }
    }
    break;
  }
  case 1:
    renderClock();
    break;
  case 2:
    renderWeather();
    break;
  }
}

bool loadSubFrame(const char *path, int subFrameIndex)
{
  File file = SPIFFS.open(path, "r");
  if (!file)
  {
    Serial.printf("⚠️ Gagal buka frame: %s\n", path);
    return false;
  }

  uint32_t offset = subFrameIndex * frameSize;
  if (!file.seek(offset, SeekSet))
  {
    Serial.println("⚠️ Gagal seek");
    file.close();
    return false;
  }

  int bytesRead = file.read(buffer, frameSize);
  file.close();

  if (bytesRead != frameSize)
  {
    Serial.printf("⚠️ Frame tidak lengkap: %d bytes\n", bytesRead);
    return false;
  }

  return true;
}

void beep(int durationMs)
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(BUZZER_PIN, LOW);
}

bool loadAndDecodeRLE(const char *path)
{
  File file = SPIFFS.open(path, "r");
  if (!file)
  {
    Serial.printf("⚠️ Gagal membuka frame: %s\n", path);
    return false;
  }

  int index = 0;
  while (file.available() && index < frameSize)
  {
    uint8_t count = file.read();
    int val = file.read();
    if (val < 0)
      break;

    for (int i = 0; i < count && index < frameSize; i++)
    {
      buffer[index++] = (uint8_t)val;
    }
  }
  file.close();

  if (index != frameSize)
  {
    Serial.printf("⚠️ Ukuran frame tidak cocok: %d bytes\n", index);
  }

  return true;
}

// -- fungsi lainnya tetap sama --

void handleButtonPress()
{
  if (digitalRead(BUTTON_CLOCK_PIN) == LOW)
  {
    if (millis() - lastButtonClockPress > debounceDelay)
    {
      lastButtonClockPress = millis();
      currentScreen = 1;
      screenTimeout = millis() + screenDuration;
      screenTimeoutActive = true;
      Serial.println("Tombol Jam ditekan");
    }
  }

  if (digitalRead(BUTTON_WEATHER_PIN) == LOW)
  {
    if (millis() - lastButtonWeatherPress > debounceDelay)
    {
      lastButtonWeatherPress = millis();
      currentScreen = 2;
      screenTimeout = millis() + screenDuration;
      screenTimeoutActive = true;
      Serial.println("Tombol Cuaca ditekan");
    }
  }
}

void checkAlarm()
{
  if (alarmHour < 0 || alarmMinute < 0)
    return;

  struct tm timeinfo;
  if (!LocalTimeHelper::getTimeInfo(timeinfo))
    return;

  if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute)
  {
    if (!alarmTriggeredToday)
    {
      Serial.println("🔔 Alarm triggered!");
      for (int i = 0; i < 5; i++)
      {
        beep(200);
        delay(200);
      }
      alarmTriggeredToday = true;
    }
  }
  else
  {
    alarmTriggeredToday = false;
  }
}

void renderClock()
{
  struct tm timeinfo;
  if (!LocalTimeHelper::getTimeInfo(timeinfo))
  {
    Serial.println("Gagal mendapatkan waktu lokal");
    return;
  }

  const char *hari[] = {"Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"};
  char tanggal[32];
  sprintf(tanggal, "%s, %02d-%02d-%04d", hari[timeinfo.tm_wday], timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
  char jam[16];
  sprintf(jam, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  int16_t x1, y1;
  uint16_t w, h;

  display.setTextSize(1);
  display.getTextBounds(tanggal, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 0);
  display.print(tanggal);

  display.setTextSize(2);
  display.getTextBounds(jam, 0, 0, &x1, &y1, &w, &h);
  int jamX = (SCREEN_WIDTH - w) / 2;
  int jamY = 30;

  int rectWidth = w + 12;
  int rectHeight = h + 10;
  int rectX = (SCREEN_WIDTH - rectWidth) / 2;
  int rectY = jamY - 5;

  display.drawRoundRect(rectX, rectY, rectWidth, rectHeight, 8, SSD1306_WHITE);
  display.setCursor(jamX, jamY);
  display.print(jam);
  display.display();
}
void playMusicMelody()
{
  // Contoh irama sederhana
  int melody[] = {262, 294, 330, 349, 392, 440, 494, 523};        // Nada C D E F G A B C
  int noteDurations[] = {250, 250, 250, 250, 250, 250, 250, 500}; // ms

  for (int i = 0; i < 8; i++)
  {
    tone(BUZZER_PIN, melody[i], noteDurations[i]);
    delay(noteDurations[i] * 1.3); // jeda antar nada
    noTone(BUZZER_PIN);
  }
}

void renderWeather()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor((display.width() - 8 * 6) / 2, 2);
  display.print("YOGYAKARTA");
  display.drawFastHLine(0, 12, display.width(), SSD1306_WHITE);

  display.setTextSize(1);
  int descWidth = weatherDescription.length() * 6;
  display.setCursor((display.width() - descWidth) / 2, 18);
  display.print(weatherDescription);

  display.setTextSize(2);
  String tempStr = String(temperature, 1);
  int tempWidth = tempStr.length() * 12 + 12;
  display.setCursor((display.width() - tempWidth) / 2, 32);
  display.print(tempStr);
  display.write(167);
  display.print("C");

  display.drawFastHLine(0, 52, display.width(), SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor((display.width() - 9 * 6) / 2, 56);
  display.print("openweather");
  display.display();
}

void fetchWeather()
{
  if (WiFi.status() != WL_CONNECTED)
    return;

  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=Yogyakarta&units=metric&lang=id&appid=71a068ac7dc5a8183446d4d8e028eb2c";

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    weatherDescription = doc["weather"][0]["description"].as<String>();
    temperature = doc["main"]["temp"].as<float>();
    Serial.printf("Cuaca: %s, Suhu: %.1f°C\n", weatherDescription.c_str(), temperature);
  }
  else
  {
    Serial.printf("Error fetch cuaca: %d\n", httpCode);
    weatherDescription = "Error";
    temperature = 0.0;
  }
  http.end();
}

void setupWebServer()
{
  server.on("/save", HTTP_POST, []()
            {
  if (server.hasArg("ssid") && server.hasArg("password"))
  {
    String ssid = server.arg("ssid");
    String pass = server.arg("password");
    saveWiFiCredentials(ssid, pass);

    // Koneksi ulang ke WiFi agar bisa ambil IP
    WiFi.begin(ssid.c_str(), pass.c_str());

    int maxAttempts = 10;
    while (WiFi.status() != WL_CONNECTED && maxAttempts-- > 0)
    {
      delay(500);
    }

    String ipAddress = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "Gagal Terhubung";

    String html = "<h3>Oke Ganteng. Reboot ESP...</h3>";
    html += "<p>Terhubung ke: <strong>" + ssid + "</strong></p>";
    html += "<p>IP ESP32: <strong>" + ipAddress + "</strong></p>";
    html += "<p>Rebooting dalam 3 detik...</p>";

    server.send(200, "text/html", html);
    delay(3000);
    ESP.restart();
  }
  else
  {
    server.send(400, "text/html", "<h3>Missing parameters</h3>");
  } });

  server.on("/", HTTP_GET, []()
            {
    File file = SPIFFS.open("/index.html", "r");
    if (!file)
    {
      server.send(500, "text/plain", "File not found");
      return;
    }
    server.streamFile(file, "text/html");
    file.close(); });

  server.on("/save", HTTP_POST, []()
            {
    if (server.hasArg("ssid") && server.hasArg("password"))
    {
      String ssid = server.arg("ssid");
      String pass = server.arg("password");
      saveWiFiCredentials(ssid, pass);
      server.send(200, "text/html", "<h3>Oke Ganteng. Reboot ESP...</h3>");
      delay(2000);
      ESP.restart();
    }
    else
    {
      server.send(400, "text/html", "<h3>Missing parameters</h3>");
    } });

  server.on("/toggleWeather", HTTP_GET, []()
            {
    currentScreen = 2;
    screenTimeout = millis() + screenDuration;
    screenTimeoutActive = true;
    server.send(200, "text/plain", "Cuaca ditampilkan"); });

  server.on("/toggleClock", HTTP_GET, []()
            {
    if (currentScreen == 1)
    {
      currentScreen = 0;
      server.send(200, "text/plain", "Jam disembunyikan");
    }
    else
    {
      currentScreen = 1;
      screenTimeout = millis() + screenDuration;
      screenTimeoutActive = true;
      server.send(200, "text/plain", "Jam ditampilkan");
    } });

  server.on("/setAlarm", HTTP_POST, []()
            {
    if (server.hasArg("hour") && server.hasArg("minute"))
    {
      alarmHour = server.arg("hour").toInt();
      alarmMinute = server.arg("minute").toInt();
      alarmTriggeredToday = false;
      server.send(200, "text/plain", "Alarm set");
      Serial.printf("Alarm set: %02d:%02d\n", alarmHour, alarmMinute);
    }
    else
    {
      server.send(400, "text/plain", "Missing hour or minute");
    } });
  server.on("/playMusicFrame", HTTP_GET, []()
            {
  currentFrame = 773;
  currentSubFrame = 0;
  currentScreen = 0; // Pastikan animasi ditampilkan di mode animasi utama
  screenTimeout = millis() + 5000; // Durasi tampilan (misal 5 detik)
  screenTimeoutActive = true;
  Serial.println("🎵 Menampilkan frame 773 dan memainkan musik");
  
  // Mainkan irama buzzer
  playMusicMelody();

  server.send(200, "text/plain", "Frame 773 dan musik diputar"); });

  server.on("/status", HTTP_GET, []()
            {
    DynamicJsonDocument doc(256);
    doc["weatherDesc"] = weatherDescription;
    doc["temp"] = temperature;

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStr[16];
      sprintf(timeStr, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      doc["time"] = timeStr;
    } else {
      doc["time"] = "N/A";
    }

    doc["ssid"] = WiFi.SSID();
    doc["ip"] = WiFi.localIP().toString();

    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out); });

  server.begin();
  Serial.println("Web server started");
}

void saveWiFiCredentials(const String &ssid, const String &password)
{
  File f = SPIFFS.open("/wifi.conf", "w");
  if (!f)
  {
    Serial.println("❌ Gagal simpan WiFi");
    return;
  }
  f.printf("%s\n%s\n", ssid.c_str(), password.c_str());
  f.close();
  Serial.println("✅ WiFi credentials disimpan");
}

void loadWiFiCredentials(String &ssid, String &password)
{
  File f = SPIFFS.open("/wifi.conf", "r");
  if (!f)
  {
    Serial.println("❌ WiFi credentials tidak ditemukan");
    ssid = "";
    password = "";
    return;
  }
  ssid = f.readStringUntil('\n');
  password = f.readStringUntil('\n');
  ssid.trim();
  password.trim();
  f.close();
  Serial.printf("Loaded WiFi: %s\n", ssid.c_str());
}
