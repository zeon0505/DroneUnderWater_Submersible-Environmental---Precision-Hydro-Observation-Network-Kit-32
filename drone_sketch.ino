#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =============================================
// 1. KONFIGURASI PIN SENSOR
// =============================================
#define PH_PIN        35  // Sensor pH di GPIO 35 (ADC1)
#define TURBIDITY_PIN 34  // Sensor Turbidity di GPIO 34 (ADC1)

// =============================================
// 2. KONFIGURASI WIFI & SERVER (Update Manual)
// =============================================
// PENTING: Perhatikan spasi jika masih gagal konek!
const char* ssid     = "daffa";       
const char* password = "daffa123";               // Password kosong sesuai info terakhir
String serverName    = "http://daffa.underwaterdrone.my.id/api.php";

// =============================================
// 3. INISIALISASI OBJEK & VARIABEL
// =============================================
LiquidCrystal_I2C lcd(0x27, 16, 2);

float phValue      = 0.0;
float turbidityNTU = 0.0;
unsigned long lastTime   = 0;
unsigned long timerDelay = 5000; // Kirim data tiap 5 detik

void setup() {
  Serial.begin(115200);

  // Setup LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Drone System");
  lcd.setCursor(0, 1);
  lcd.print("Scanning WiFi...");

  // WiFi Mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("\n--- Memulai Sistem Drone ---");
  Serial.print("Mencoba konek ke: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected to WiFi!");
  Serial.println("IP Address: " + WiFi.localIP().toString());
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected!");
  delay(2000);
}

void loop() {

  // --- A. PEMBACAAN SENSOR PH ---
  int adcPH = 0;
  for(int i=0; i<10; i++) { adcPH += analogRead(PH_PIN); delay(10); } // Sampling agar stabil
  adcPH = adcPH / 10;
  
  float voltagePH = adcPH * (3.3 / 4095.0);
  
  // Rumus Kalibrasi (Coba putar baut biru di modul pH jika masih 14.0)
  // phValue = 3.5 * voltagePH + offset
  phValue = 3.5 * voltagePH - 1.1; 
  
  if (phValue > 14.0) phValue = 14.0;
  if (phValue < 0.0)  phValue = 0.0;

  // --- B. PEMBACAAN SENSOR TURBIDITY ---
  int adcTurb = analogRead(TURBIDITY_PIN);
  float voltTurb = adcTurb * (3.3 / 4095.0);
  
  // KALIBRASI KHUSUS: Hasil tes air galon Anda menunjukkan 1.59V = Jernih.
  // Kita sesuaikan multiplier-nya agar 1.59V terbaca sebagai 4.2V di rumus.
  float clearWaterVolt = 1.59; 
  float voltNormalized = voltTurb * (4.2 / clearWaterVolt); 

  // Rumus estimasi NTU (menggunakan volt yang sudah dinormalisasi)
  turbidityNTU = -1120.4 * (voltNormalized * voltNormalized) + 5742.3 * voltNormalized - 4352.9;
  
  // Pengaman: Jika hasil minus atau masih di air jernih (sekitar 1.59V), kita paksa ke 0
  if (turbidityNTU < 0 || voltTurb >= (clearWaterVolt - 0.1)) turbidityNTU = 0;

  // --- C. UPDATE TAMPILAN (SERIAL & LCD) ---
  Serial.println("\n--- Data Sensor ---");
  Serial.print("Raw ADC PH : "); Serial.print(adcPH);
  Serial.print(" | Volt PH : "); Serial.println(voltagePH);
  Serial.print("pH Value   : "); Serial.println(phValue, 2);
  Serial.print("Volt Turb  : "); Serial.print(voltTurb); Serial.println(" V");
  Serial.print("Turbidity  : "); Serial.print(turbidityNTU, 0); Serial.println(" NTU");

  lcd.setCursor(0, 0);
  lcd.print("pH: " + String(phValue, 2) + "        ");
  lcd.setCursor(0, 1);
  lcd.print("Trb:" + String(turbidityNTU, 0) + " NTU    ");

  // --- D. KIRIM DATA KE SERVER ---
  if ((millis() - lastTime) > timerDelay) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      
      // Susun URL
      String url = serverName
                   + "?kualitas_air=" + String(phValue, 2)
                   + "&tahan="        + String(turbidityNTU, 2)
                   + "&daya_listrik=100";

      Serial.println("Mengirim ke API...");
      http.begin(url.c_str());
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {
        Serial.print("HTTP Success: ");
        Serial.println(httpResponseCode);
      } else {
        Serial.print("HTTP Error: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    } else {
      WiFi.reconnect();
    }
    lastTime = millis();
  }

  delay(2000); // Tunggu 2 detik untuk pembacaan berikutnya
}