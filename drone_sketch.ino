// ============================================================
// 🌊 Underwater Drone - Monitoring Kualitas Air (Pro Version)
// ============================================================
// Board       : ESP32 DevKit V1
// Framework   : Arduino
// Display     : LCD I2C 16x2
// Sensor yang digunakan:
// 1. pH Sensor      → GPIO 34 (Analog) + Smoothing Logic
// 2. Turbidity      → GPIO 35 (Analog)
// 3. Suhu (DS18B20) → GPIO 4 (Digital)
// ============================================================

// ─────────────────────────────────────────────
// 📦 Library yang Dibutuhkan
// ─────────────────────────────────────────────
#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ─────────────────────────────────────────────
// 📌 Definisi Pin & Inisialisasi LCD
// ─────────────────────────────────────────────
#define PH_SENSOR_PIN        34   
#define TURBIDITY_SENSOR_PIN 35   
#define DS18B20_PIN          4    

// Inisialisasi LCD I2C (Alamat 0x27, 16 kolom, 2 baris)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ─────────────────────────────────────────────
// 📡 Konfigurasi WiFi & API
// ─────────────────────────────────────────────
const char WIFI_SSID[]     = "Rapip x Salman"; 
const char WIFI_PASSWORD[] = "ikanjawasumedang";               
String HOST_NAME = "http://yoga.underwaterdrone.my.id/";  
String PATH_NAME = "/api.php";                             

// ─────────────────────────────────────────────
// ⚙️ Konfigurasi pH (Smoothing & Kalibrasi)
// ─────────────────────────────────────────────
float calibration_value = 21.34 - 1.05 +3.10  ; // Nilai kalibrasi dari screenshot
unsigned long int avgval; 
int buffer_arr[10], temp;
float ph_act;

// ─────────────────────────────────────────────
// ⏱️ Interval
// ─────────────────────────────────────────────
const unsigned long SENSOR_READ_INTERVAL = 1000;
const unsigned long DATA_SEND_INTERVAL   = 5000;
unsigned long lastSensorRead = 0;
unsigned long lastDataSend   = 0;

// ─────────────────────────────────────────────
// 🌡️ Sensor Suhu
// ─────────────────────────────────────────────
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20Sensor(&oneWire);

// ─────────────────────────────────────────────
// 📊 Variabel Global
// ─────────────────────────────────────────────
float nilaiPH          = 0.0;
float nilaiTurbidity   = 0.0;
float nilaiSuhu        = 0.0;
int   rawTurbidity     = 0;

// ============================================================
// 🔧 FUNGSI: Baca pH dengan Filter Sorting (Smoothing)
// ============================================================
float bacaPHSmoothing() {
  // Ambil 10 sampel
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(PH_SENSOR_PIN);
    delay(30);
  }

  // Urutkan sampel (Bubble Sort) untuk mencari median/menghapus noise
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }

  // Ambil rata-rata dari 6 sampel tengah (buang 2 terendah & 2 tertinggi)
  avgval = 0;
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i];
  }
  
  float volt = (float)avgval * 3.3 / 4095.0 / 6.0; // Rata-rata tegangan
  float phVal = 3.5 * volt + calibration_value;   // Rumus pH berdasarkan tegangan
  
  return phVal;
}

// ============================================================
// 🔧 FUNGSI: Konversi Turbidity ke NTU
// ============================================================
float bacaNTU() {
  int raw = 0;
  for(int i=0; i<10; i++) { raw += analogRead(TURBIDITY_SENSOR_PIN); delay(5); }
  raw = raw / 10;
  float volt = (float)raw * 3.3 / 4095.0;
  float ntu = 1000.0 * (1.0 - (volt / 3.3));
  if (ntu < 0) ntu = 0;
  return ntu;
}

// ============================================================
// 🔧 FUNGSI: Tampilkan ke LCD
// ============================================================
void updateLCD() {
  lcd.clear();
  // Baris 1: pH dan Suhu
  lcd.setCursor(0, 0);
  lcd.print("pH:");
  lcd.print(nilaiPH, 1);
  lcd.setCursor(8, 0);
  lcd.print(" T:");
  lcd.print(nilaiSuhu, 1);
  lcd.print("C");

  // Baris 2: Turbidity
  lcd.setCursor(0, 1);
  lcd.print("Tur:");
  lcd.print(nilaiTurbidity, 0);
  lcd.print(" NTU");
  
  // Status WiFi
  lcd.setCursor(13, 1);
  if (WiFi.status() == WL_CONNECTED) lcd.print("OK!");
  else lcd.print("ERR");
}

// ============================================================
// 🔧 FUNGSI: Kirim Data ke Server
// ============================================================
void kirimDataKeServer() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Format URL GET harus persis dengan variabel di api.php
    String url = HOST_NAME + PATH_NAME + 
                 "?kualitas_air=" + String(nilaiPH, 2) + 
                 "&tahan=" + String(nilaiTurbidity, 1) + 
                 "&udara=" + String(nilaiSuhu, 2) + 
                 "&daya_listrik=100.0"; // Simulasi baterai 100%

    Serial.println("📡 Mencoba Kirim ke: " + url);
    
    http.begin(url.c_str()); 
    int httpCode = http.GET(); // Menggunakan GET sesuai api.php
    
    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("✅ Server Merespon: " + payload);
    } else {
      Serial.print("❌ Gagal Kirim, Error: ");
      Serial.println(http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("⚠️ WiFi Terputus, Gagal Kirim Data");
  }
}

// ============================================================
// 🚀 SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("SE-PHON 32");
  lcd.setCursor(0, 1);
  lcd.print("System Ready...");
  delay(2000);

  // Inisialisasi Sensor
  ds18b20Sensor.begin();
  analogSetAttenuation(ADC_11db);

  // WiFi
  Serial.print("📡 Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 15) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  Serial.println("\n✅ WiFi OK!");
}

// ============================================================
// 🔄 LOOP
// ============================================================
void loop() {
  unsigned long currentMillis = millis();

  // Baca Sensor (Setiap 1 Detik)
  if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = currentMillis;

    nilaiPH = bacaPHSmoothing();
    nilaiTurbidity = bacaNTU();
    
    ds18b20Sensor.requestTemperatures();
    nilaiSuhu = ds18b20Sensor.getTempCByIndex(0);
    if (nilaiSuhu == -127.0) nilaiSuhu = 0.0;

    // Tampilkan di LCD & Serial
    updateLCD();
    Serial.printf("pH: %.2f | NTU: %.1f | Temp: %.2fC\n", nilaiPH, nilaiTurbidity, nilaiSuhu);
  }

  // Kirim ke Cloud (Setiap 5 Detik)
  if (currentMillis - lastDataSend >= DATA_SEND_INTERVAL) {
    lastDataSend = currentMillis;
    kirimDataKeServer();
  }
}