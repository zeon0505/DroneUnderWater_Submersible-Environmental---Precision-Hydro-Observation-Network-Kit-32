// ============================================================
// 🌊 Underwater Drone - Monitoring Kualitas Air
// ============================================================
// Board       : ESP32 DevKit V1
// Framework   : Arduino
// Simulator   : Wokwi
// 
// Sensor yang digunakan:
// 1. pH Sensor      → Potentiometer di GPIO 34 (Analog)
// 2. Turbidity      → Potentiometer di GPIO 35 (Analog)
// 3. Suhu (DS18B20) → GPIO 4 (Digital, pull-up 4.7kΩ ke 3.3V)
//
// Catatan:
// - ADC ESP32 = 12-bit (0–4095)
// - Tegangan referensi = 3.3V
// - pH dikonversi ke skala 0–14
// - Turbidity dikonversi ke 0–1000 NTU
// - Suhu dalam °C dari DS18B20
// ============================================================

// ─────────────────────────────────────────────
// 📦 Library yang Dibutuhkan
// ─────────────────────────────────────────────
#include <WiFi.h>          // Library WiFi bawaan ESP32
#include <HTTPClient.h>    // Library HTTP Client untuk kirim data
#include <OneWire.h>       // Library protokol OneWire (untuk DS18B20)
#include <DallasTemperature.h>  // Library sensor suhu DS18B20

// ─────────────────────────────────────────────
// 📌 Definisi Pin Sensor
// ─────────────────────────────────────────────
#define PH_SENSOR_PIN        34   // Pin analog untuk sensor pH (potentiometer)
#define TURBIDITY_SENSOR_PIN 35   // Pin analog untuk sensor turbidity (potentiometer)
#define DS18B20_PIN          4    // Pin digital untuk sensor suhu DS18B20

// ─────────────────────────────────────────────
// 📡 Konfigurasi WiFi (Wokwi)
// ─────────────────────────────────────────────
const char WIFI_SSID[]     = "Wokwi-GUEST";   // SSID WiFi Wokwi (gratis)
const char WIFI_PASSWORD[] = "";                // Password kosong untuk Wokwi

// ─────────────────────────────────────────────
// 🌐 Konfigurasi Server API
// ─────────────────────────────────────────────
String HOST_NAME = "http://yoga.underwaterdrone.my.id/";  // Alamat server API
String PATH_NAME = "/api.php";                               // Path endpoint API

// ─────────────────────────────────────────────
// ⚙️ Konstanta ADC & Konversi
// ─────────────────────────────────────────────
const int   ADC_RESOLUTION   = 4095;    // Resolusi ADC 12-bit (0–4095)
const float V_REF            = 3.3;     // Tegangan referensi ESP32 (3.3V)

// Konstanta konversi pH
// Sensor pH analog umumnya menghasilkan tegangan ~0V (pH 0) sampai ~3.3V (pH 14)
// Rumus linier: pH = (voltage / V_REF) * 14.0
// Namun sensor pH real biasanya memiliki offset dan slope tertentu
// Untuk simulasi ini, kita gunakan formula linier sederhana
const float PH_OFFSET       = 0.0;     // Offset kalibrasi pH (sesuaikan jika perlu)
const float PH_SLOPE         = 14.0 / V_REF;  // Slope: 14 / 3.3 ≈ 4.2424

// Konstanta konversi Turbidity
// Turbidity sensor: tegangan tinggi = air jernih, tegangan rendah = air keruh
// Rentang: 0 NTU (jernih) sampai 1000 NTU (sangat keruh)
const float NTU_MAX          = 1000.0;  // Nilai NTU maksimum

// ─────────────────────────────────────────────
// ⏱️ Interval Pembacaan & Pengiriman
// ─────────────────────────────────────────────
const unsigned long SENSOR_READ_INTERVAL = 1000;    // Baca sensor setiap 1 detik
const unsigned long DATA_SEND_INTERVAL   = 5000;    // Kirim data ke server setiap 5 detik
unsigned long lastSensorRead = 0;   // Timestamp terakhir baca sensor
unsigned long lastDataSend   = 0;   // Timestamp terakhir kirim data

// ─────────────────────────────────────────────
// 🌡️ Inisialisasi Sensor DS18B20
// ─────────────────────────────────────────────
OneWire oneWire(DS18B20_PIN);               // Buat objek OneWire pada pin DS18B20
DallasTemperature ds18b20Sensor(&oneWire);  // Buat objek DallasTemperature

// ─────────────────────────────────────────────
// 📊 Variabel Global Penyimpanan Data Sensor
// ─────────────────────────────────────────────
float nilaiPH          = 0.0;   // Nilai pH hasil konversi (0–14)
float nilaiTurbidity   = 0.0;   // Nilai Turbidity dalam NTU (0–1000)
float nilaiSuhu        = 0.0;   // Nilai Suhu dalam °C
float teganganPH       = 0.0;   // Tegangan mentah dari sensor pH
float teganganTurbidity= 0.0;   // Tegangan mentah dari sensor turbidity
int   rawPH            = 0;     // Nilai ADC mentah sensor pH
int   rawTurbidity     = 0;     // Nilai ADC mentah sensor turbidity

// ─────────────────────────────────────────────
// 📊 Variabel untuk Rata-rata (Averaging)
// ─────────────────────────────────────────────
const int JUMLAH_SAMPLING = 10;  // Jumlah sampel untuk rata-rata (noise filtering)


// ============================================================
// 🔧 FUNGSI: Baca nilai ADC dengan rata-rata (noise filtering)
// ============================================================
// Membaca pin analog sebanyak JUMLAH_SAMPLING kali,
// kemudian mengembalikan nilai rata-rata.
// Ini mengurangi noise pada pembacaan ADC.
// ============================================================
int bacaADCRataRata(int pin) {
  long total = 0;
  for (int i = 0; i < JUMLAH_SAMPLING; i++) {
    total += analogRead(pin);
    delay(2);  // Jeda kecil antar pembacaan (stabilitas ADC)
  }
  return (int)(total / JUMLAH_SAMPLING);
}


// ============================================================
// 🔧 FUNGSI: Konversi ADC ke Tegangan
// ============================================================
// Mengubah nilai mentah ADC (0–4095) menjadi tegangan (0–3.3V)
// Rumus: Tegangan = (nilaiADC / 4095) × 3.3V
// ============================================================
float adcKeVoltage(int nilaiADC) {
  return (float)nilaiADC * V_REF / (float)ADC_RESOLUTION;
}


// ============================================================
// 🔧 FUNGSI: Konversi Tegangan ke Nilai pH
// ============================================================
// Sensor pH analog umumnya menghasilkan tegangan linier
// yang berkorelasi dengan nilai pH.
//
// Pada simulasi ini (potentiometer 0–3.3V → pH 0–14):
//   - Tegangan 0V    → pH 0  (sangat asam)
//   - Tegangan 1.65V → pH 7  (netral)
//   - Tegangan 3.3V  → pH 14 (sangat basa)
//
// Rumus: pH = (tegangan × slope) + offset
//        pH = (tegangan × 4.2424) + 0.0
//
// Catatan: Pada sensor pH real (seperti SEN0161),
//          slope dan offset perlu dikalibrasi
//          menggunakan larutan buffer pH 4, 7, dan 10.
// ============================================================
float konversiKePH(float tegangan) {
  float ph = (tegangan * PH_SLOPE) + PH_OFFSET;
  
  // Batasi nilai pH dalam rentang valid (0–14)
  if (ph < 0.0)  ph = 0.0;
  if (ph > 14.0) ph = 14.0;
  
  return ph;
}


// ============================================================
// 🔧 FUNGSI: Konversi Tegangan ke Nilai NTU (Turbidity)
// ============================================================
// Sensor turbidity mengukur kekeruhan air.
// Semakin keruh → semakin sedikit cahaya yang diterima
//              → tegangan semakin rendah.
//
// Pada simulasi ini (potentiometer 0–3.3V → NTU 1000–0):
//   - Tegangan 0V    → 1000 NTU (sangat keruh)
//   - Tegangan 1.65V → 500  NTU (sedang)
//   - Tegangan 3.3V  → 0    NTU (jernih)
//
// Rumus: NTU = NTU_MAX × (1 - (tegangan / V_REF))
//
// Hubungan TERBALIK: tegangan tinggi = air jernih
// Ini mensimulasikan perilaku sensor turbidity real
// yang outputnya menurun saat air semakin keruh.
// ============================================================
float konversiKeNTU(float tegangan) {
  // Hubungan terbalik: tegangan tinggi = NTU rendah (air jernih)
  float ntu = NTU_MAX * (1.0 - (tegangan / V_REF));
  
  // Batasi nilai NTU dalam rentang valid (0–1000)
  if (ntu < 0.0)    ntu = 0.0;
  if (ntu > 1000.0)  ntu = 1000.0;
  
  return ntu;
}


// ============================================================
// 🔧 FUNGSI: Baca Sensor Suhu DS18B20
// ============================================================
// DS18B20 adalah sensor suhu digital 1-Wire.
// Tidak perlu konversi manual, library DallasTemperature
// sudah menangani komunikasi dan konversi.
//
// Return: suhu dalam derajat Celsius
//         Mengembalikan -127 jika sensor error/tidak terhubung
// ============================================================
float bacaSuhuDS18B20() {
  ds18b20Sensor.requestTemperatures();       // Minta pembacaan suhu
  float suhu = ds18b20Sensor.getTempCByIndex(0);  // Ambil suhu sensor pertama
  return suhu;
}


// ============================================================
// 🔧 FUNGSI: Tentukan Kategori Kualitas Air berdasarkan pH
// ============================================================
String kategoriPH(float ph) {
  if (ph < 4.0)       return "Sangat Asam ⚠️";
  else if (ph < 6.5)  return "Asam";
  else if (ph <= 8.5) return "Normal ✅";
  else if (ph <= 11.0) return "Basa";
  else                 return "Sangat Basa ⚠️";
}


// ============================================================
// 🔧 FUNGSI: Tentukan Kategori Kekeruhan berdasarkan NTU
// ============================================================
String kategoriTurbidity(float ntu) {
  if (ntu < 5.0)        return "Jernih ✅";
  else if (ntu < 50.0)  return "Sedikit Keruh";
  else if (ntu < 200.0) return "Keruh";
  else if (ntu < 500.0) return "Sangat Keruh ⚠️";
  else                   return "Ekstrem Keruh 🚨";
}


// ============================================================
// 🔧 FUNGSI: Tentukan Kategori Suhu
// ============================================================
String kategoriSuhu(float suhu) {
  if (suhu < 0.0)       return "Beku ❄️";
  else if (suhu < 15.0) return "Dingin";
  else if (suhu < 25.0) return "Sejuk ✅";
  else if (suhu < 30.0) return "Hangat ✅";
  else if (suhu < 40.0) return "Panas";
  else                   return "Sangat Panas ⚠️";
}


// ============================================================
// 🔧 FUNGSI: Tampilkan Data Sensor ke Serial Monitor
// ============================================================
void tampilkanDataSerial() {
  Serial.println();
  Serial.println("╔══════════════════════════════════════════════════╗");
  Serial.println("║   🌊 UNDERWATER DRONE - WATER QUALITY MONITOR  ║");
  Serial.println("╠══════════════════════════════════════════════════╣");
  
  // --- Data pH ---
  Serial.println("║                                                  ║");
  Serial.println("║  🧪 SENSOR pH (GPIO 34)                         ║");
  Serial.print("║  ├─ ADC Raw      : ");
  Serial.print(rawPH);
  Serial.println(padRight("", 30 - String(rawPH).length()) + "║");
  
  Serial.print("║  ├─ Tegangan     : ");
  Serial.print(teganganPH, 2);
  Serial.println(" V" + padRight("", 27 - String(teganganPH, 2).length()) + "║");
  
  Serial.print("║  ├─ Nilai pH     : ");
  Serial.print(nilaiPH, 2);
  Serial.println(padRight("", 30 - String(nilaiPH, 2).length()) + "║");
  
  Serial.print("║  └─ Kategori     : ");
  String katPH = kategoriPH(nilaiPH);
  Serial.println(katPH + padRight("", 30 - katPH.length()) + "║");

  // --- Data Turbidity ---
  Serial.println("║                                                  ║");
  Serial.println("║  🌫️ SENSOR TURBIDITY (GPIO 35)                   ║");
  Serial.print("║  ├─ ADC Raw      : ");
  Serial.print(rawTurbidity);
  Serial.println(padRight("", 30 - String(rawTurbidity).length()) + "║");
  
  Serial.print("║  ├─ Tegangan     : ");
  Serial.print(teganganTurbidity, 2);
  Serial.println(" V" + padRight("", 27 - String(teganganTurbidity, 2).length()) + "║");
  
  Serial.print("║  ├─ Nilai NTU    : ");
  Serial.print(nilaiTurbidity, 1);
  Serial.println(" NTU" + padRight("", 25 - String(nilaiTurbidity, 1).length()) + "║");
  
  Serial.print("║  └─ Kategori     : ");
  String katNTU = kategoriTurbidity(nilaiTurbidity);
  Serial.println(katNTU + padRight("", 30 - katNTU.length()) + "║");

  // --- Data Suhu ---
  Serial.println("║                                                  ║");
  Serial.println("║  🌡️ SENSOR SUHU DS18B20 (GPIO 4)                 ║");
  Serial.print("║  ├─ Suhu         : ");
  Serial.print(nilaiSuhu, 2);
  Serial.println(" °C" + padRight("", 26 - String(nilaiSuhu, 2).length()) + "║");
  
  Serial.print("║  └─ Kategori     : ");
  String katSuhu = kategoriSuhu(nilaiSuhu);
  Serial.println(katSuhu + padRight("", 30 - katSuhu.length()) + "║");

  Serial.println("║                                                  ║");
  Serial.println("╚══════════════════════════════════════════════════╝");
}


// ============================================================
// 🔧 FUNGSI HELPER: Padding kanan untuk format tabel
// ============================================================
String padRight(String text, int totalLength) {
  String result = text;
  while ((int)result.length() < totalLength) {
    result += " ";
  }
  return result;
}


// ============================================================
// 🔧 FUNGSI: Kirim Data ke Server via HTTP GET
// ============================================================
void kirimDataKeServer() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Buat query string dengan data sensor
    String queryString = "?kualitas_air=" + String(nilaiPH, 2) 
                       + "&tahan="        + String(nilaiTurbidity, 1)
                       + "&udara="        + String(nilaiSuhu, 2)
                       + "&daya_listrik=" + String(100.0);  // Simulasi daya listrik

    // Gabungkan URL lengkap
    String serverPath = HOST_NAME + PATH_NAME + queryString;
    
    Serial.println();
    Serial.println("📡 Mengirim data ke server...");
    Serial.println("   URL: " + serverPath);

    http.begin(serverPath.c_str());
    
    // Kirim HTTP GET request
    int httpCode = http.GET();

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("   ✅ Berhasil! Response: " + payload);
      } else {
        Serial.printf("   ⚠️ HTTP Code: %d\n", httpCode);
      }
    } else {
      Serial.printf("   ❌ Gagal! Error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("❌ WiFi terputus! Tidak bisa mengirim data.");
  }
}


// ============================================================
// 🚀 SETUP - Inisialisasi Awal
// ============================================================
// Fungsi ini dijalankan SEKALI saat ESP32 dinyalakan/reset.
// Berisi konfigurasi pin, koneksi WiFi, dan inisialisasi sensor.
// ============================================================
void setup() {
  // --- Inisialisasi Serial Monitor ---
  Serial.begin(115200);  // Baudrate 115200 (standar untuk ESP32)
  delay(1000);           // Tunggu Serial stabil
  
  // --- Tampilan Header ---
  Serial.println();
  Serial.println("╔══════════════════════════════════════════════════╗");
  Serial.println("║   🌊 UNDERWATER DRONE - WATER QUALITY MONITOR  ║");
  Serial.println("║   📋 Sistem Monitoring Kualitas Air            ║");
  Serial.println("║   🔧 ESP32 DevKit V1 | Wokwi Simulator        ║");
  Serial.println("╚══════════════════════════════════════════════════╝");
  Serial.println();

  // --- Konfigurasi Pin Analog ---
  // ESP32 ADC menggunakan resolusi 12-bit secara default
  // analogSetAttenuation(ADC_11db) → rentang input 0–3.3V
  analogSetAttenuation(ADC_11db);
  Serial.println("✅ ADC dikonfigurasi (12-bit, 0-3.3V)");

  // --- Inisialisasi Sensor DS18B20 ---
  ds18b20Sensor.begin();
  Serial.print("✅ DS18B20 ditemukan: ");
  Serial.print(ds18b20Sensor.getDeviceCount());
  Serial.println(" perangkat");

  // --- Koneksi WiFi ---
  Serial.println();
  Serial.print("📡 Menghubungkan ke WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }
  
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✅ Terhubung! IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("⚠️ Gagal terhubung ke WiFi. Data hanya ditampilkan di Serial.");
  }

  Serial.println();
  Serial.println("🚀 Sistem monitoring dimulai!");
  Serial.println("   📊 Pembacaan sensor setiap 1 detik");
  Serial.println("   📡 Pengiriman data setiap 5 detik");
  Serial.println();
}


// ============================================================
// 🔄 LOOP - Program Utama (Berjalan Berulang)
// ============================================================
// Fungsi ini berjalan terus-menerus setelah setup().
// Membaca semua sensor, menampilkan data, dan mengirim ke server.
// ============================================================
void loop() {
  unsigned long currentMillis = millis();

  // ─────────────────────────────────────────────
  // 📊 Pembacaan Sensor (Setiap 1 Detik)
  // ─────────────────────────────────────────────
  if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = currentMillis;

    // === 1. Baca Sensor pH (Potentiometer pada GPIO 34) ===
    rawPH       = bacaADCRataRata(PH_SENSOR_PIN);     // Baca ADC dengan rata-rata
    teganganPH  = adcKeVoltage(rawPH);                 // Konversi ke tegangan
    nilaiPH     = konversiKePH(teganganPH);            // Konversi ke skala pH

    // === 2. Baca Sensor Turbidity (Potentiometer pada GPIO 35) ===
    rawTurbidity      = bacaADCRataRata(TURBIDITY_SENSOR_PIN);  // Baca ADC
    teganganTurbidity = adcKeVoltage(rawTurbidity);              // Konversi ke tegangan
    nilaiTurbidity    = konversiKeNTU(teganganTurbidity);        // Konversi ke NTU

    // === 3. Baca Sensor Suhu (DS18B20 pada GPIO 4) ===
    nilaiSuhu = bacaSuhuDS18B20();

    // Cek apakah sensor suhu error
    if (nilaiSuhu == DEVICE_DISCONNECTED_C) {
      Serial.println("⚠️ Sensor DS18B20 tidak terdeteksi!");
      nilaiSuhu = 0.0;  // Set default jika error
    }

    // === 4. Tampilkan Data ke Serial Monitor ===
    tampilkanDataSerial();
  }

  // ─────────────────────────────────────────────
  // 📡 Pengiriman Data ke Server (Setiap 5 Detik)
  // ─────────────────────────────────────────────
  if (currentMillis - lastDataSend >= DATA_SEND_INTERVAL) {
    lastDataSend = currentMillis;
    kirimDataKeServer();
  }
}