// ============================================================
// Alarm Gas Bocor + Kipas Buang Otomatis — ESP32 + MQ-2
// Kode dari YouTube Short "Alarm Gas Bocor ESP32"
// Repo: https://github.com/oerban77/alarm-gas-bocor-esp32
//
// Cara kerja: sensor MQ-2 membaca kadar gas. Kalau melewati
// BATAS, buzzer berbunyi, kipas buang menyala, dan HP kamu
// dapat notifikasi Telegram. Kalau aman, lampu hijau menyala.
//
// Wiring:
//   MQ-2 A0         -> GPIO34 (analog), VCC 5V, GND GND
//   Buzzer (+)      -> GPIO25 (aktif HIGH), (-) GND
//   Modul relay 5V  -> GPIO26 (IN), kipas 12V lewat kontak relay
//   LED hijau       -> GPIO27 (lewat resistor 220 ohm ke GND)
//   LED merah       -> GPIO32 (lewat resistor 220 ohm ke GND)
//
// Kalibrasi: panaskan MQ-2 minimal 5 menit di udara bersih,
// buka Serial Monitor (115200), catat nilai udara bersih,
// lalu set BATAS sedikit di atas nilai itu (default 400).
// ============================================================

#define USE_TELEGRAM 1   // 1 = kirim notifikasi Telegram, 0 = Serial saja

// ============ ISI BAGIAN INI (kalau USE_TELEGRAM = 1) ============
const char* WIFI_SSID     = "NAMA_WIFI_KAMU";
const char* WIFI_PASSWORD = "PASSWORD_WIFI_KAMU";
const String BOT_TOKEN    = "ISI_TOKEN_BOT_DARI_BOTFATHER";
const String CHAT_ID      = "ISI_CHAT_ID_KAMU";
// Cara dapat BOT_TOKEN: chat @BotFather di Telegram -> /newbot
// Cara dapat CHAT_ID: chat @userinfobot di Telegram, catat "Id" kamu.
// ================================================================

#if USE_TELEGRAM
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
WiFiClientSecure telegramClient;
UniversalTelegramBot bot(BOT_TOKEN, telegramClient);
#endif

const int PIN_SENSOR      = 34;   // MQ-2 A0
const int PIN_BUZZER      = 25;
const int PIN_RELAY_KIPAS = 26;
const int PIN_LED_HIJAU   = 27;
const int PIN_LED_MERAH   = 32;

const int BATAS = 400;            // sesuaikan setelah kalibrasi
const int BACA_TIAP_MS = 500;     // jeda baca sensor
const unsigned long BUNYI_MS = 400;  // buzzer bunyi putus-putus tiap 400 ms

bool sedangBahaya = false;
bool telegramAktif = false;
unsigned long bunyiTerakhir = 0;
bool buzzerNyala = false;

int bacaSensor() {
  int total = 0;
  for (int i = 0; i < 10; i++) { total += analogRead(PIN_SENSOR); delay(10); }
  return total / 10;
}

void setLampu(bool bahaya) {
  digitalWrite(PIN_LED_MERAH, bahaya ? HIGH : LOW);
  digitalWrite(PIN_LED_HIJAU, bahaya ? LOW : HIGH);
}

void kipasOn(bool nyala) { digitalWrite(PIN_RELAY_KIPAS, nyala ? HIGH : LOW); }

void kirimPesan(const String& teks) {
  Serial.println(teks);
#if USE_TELEGRAM
  if (telegramAktif) {
    bot.sendMessage(CHAT_ID, teks, "");
  }
#endif
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_RELAY_KIPAS, OUTPUT);
  pinMode(PIN_LED_HIJAU, OUTPUT);
  pinMode(PIN_LED_MERAH, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  kipasOn(false);
  setLampu(false);

#if USE_TELEGRAM
  if (BOT_TOKEN.startsWith("ISI_") || CHAT_ID.startsWith("ISI_")) {
    Serial.println("PERINGATAN: BOT_TOKEN / CHAT_ID belum diisi, notifikasi Telegram nonaktif.");
  } else {
    Serial.print("Menghubungkan WiFi ");
    Serial.print(WIFI_SSID);
    Serial.print(" ... ");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    unsigned long mulai = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - mulai < 20000) {
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(" terhubung!");
      telegramClient.setInsecure();  // tanpa verifikasi sertifikat (sesuai contoh UniversalTelegramBot)
      telegramAktif = true;
    } else {
      Serial.println(" GAGAL. Notifikasi Telegram nonaktif, alarm tetap jalan.");
    }
  }
#endif

  Serial.println("Alarm gas siap. Panaskan sensor 5 menit dulu sebelum kalibrasi.");
}

void loop() {
  int nilai = bacaSensor();
  Serial.print("Gas: ");
  Serial.println(nilai);

  bool bahaya = (nilai > BATAS);

  if (bahaya && !sedangBahaya) {
    // Baru saja melewati batas: nyalakan alarm + kipas, kirim 1 pesan.
    sedangBahaya = true;
    setLampu(true);
    kipasOn(true);
    kirimPesan("BAHAYA! Gas bocor terdeteksi (nilai " + String(nilai) + "). Kipas buang menyala.");
  } else if (!bahaya && sedangBahaya) {
    // Gas kembali aman.
    sedangBahaya = false;
    digitalWrite(PIN_BUZZER, LOW);
    buzzerNyala = false;
    setLampu(false);
    kipasOn(false);
    kirimPesan("Aman. Kadar gas kembali normal (nilai " + String(nilai) + ").");
  }

  // Buzzer bunyi putus-putus selama bahaya (non-blocking).
  if (sedangBahaya && millis() - bunyiTerakhir >= BUNYI_MS) {
    bunyiTerakhir = millis();
    buzzerNyala = !buzzerNyala;
    digitalWrite(PIN_BUZZER, buzzerNyala ? HIGH : LOW);
  }

  delay(BACA_TIAP_MS);
}
