#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HCSR04.h>

// ====== KONFIGURASI PIN ======
#define SERVO_PIN 17
#define US_TRIGGER 5
#define US_ECHO 4
#define US_PENUH_KERTAS 9
#define US_PENUH_ECHO_KERTAS 10
#define US_PENUH_PLASTIK 41
#define US_PENUH_ECHO_PLASTIK 42

#define JARAK_SAMPAH_MIN_CM 12
// ====== VARIABEL ======
Servo servo;
String currentLabel = "";
bool labelBaruDiterima = false;

UltraSonicDistanceSensor sensorSampah(US_TRIGGER, US_ECHO);
UltraSonicDistanceSensor sensorKertas(US_PENUH_KERTAS, US_PENUH_ECHO_KERTAS);
UltraSonicDistanceSensor sensorPlastik(US_PENUH_PLASTIK, US_PENUH_ECHO_PLASTIK);

int jmlKertas = 0, jmlPlastik = 0;

typedef struct struct_message {
  char label[32];
} struct_message;

struct_message incomingDataStruct;

const float selisihTinggi = 21;
const float globalMaks = 27;

LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD initialization

// ====== CALLBACK ESP-NOW ======
void onReceiveData(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  memcpy(&incomingDataStruct, incomingData, sizeof(incomingDataStruct));
  currentLabel = String(incomingDataStruct.label);
  labelBaruDiterima = true;

  Serial.print("Label diterima: ");
  Serial.println(currentLabel);

  Serial.print("MAC pengirim: ");
  char macStr[18];
  snprintf(macStr, sizeof(macStr),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           info->src_addr[0], info->src_addr[1], info->src_addr[2],
           info->src_addr[3], info->src_addr[4], info->src_addr[5]);
  Serial.println(macStr);
}
// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Wire.begin(21, 20);

  servo.attach(SERVO_PIN);
  servo.write(90);  // posisi netral

  lcd.init();
  lcd.backlight();
  lcd.clear();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init gagal");
    return;
  }

  esp_now_register_recv_cb(onReceiveData);
}

// ====== LOOP UTAMA ======
void loop() {
  float jarakSampah = sensorSampah.measureDistanceCm();
  Serial.print("Jarak sampah: ");
  Serial.print(jarakSampah);
  Serial.println(" cm");

  // // Hitung persentase penuh tong kertas
  float jarakKertas = sensorKertas.measureDistanceCm();
  float persenKertas = 100 - (jarakKertas - 20)/27 * 100.0;

  // // Batasi nilai persen agar tidak negatif atau lebih dari 100%
  if (persenKertas < 0) persenKertas = 0;
  if (persenKertas > 100) persenKertas = 100;

  // // Hitung persentase penuh tong plastik
  float jarakPlastik = sensorPlastik.measureDistanceCm();
  float persenPlastik = 100 - (jarakPlastik - 20)/27 * 100.0;

  if (persenPlastik < 0) persenPlastik = 0;
  if (persenPlastik > 100) persenPlastik = 100;

  // Gerakkan servo jika menerima label dan ada sampah dekat
  if (labelBaruDiterima && jarakSampah < JARAK_SAMPAH_MIN_CM) {
    labelBaruDiterima = false;
    if (currentLabel == "Kertas" || currentLabel == "Kardus") {
      jmlKertas++;
    } else if (currentLabel == "Plastik") {
      jmlPlastik++;
    }
    if (jmlKertas >= 5) {
      Serial.println("Sampah ke kiri");
      servo.write(10);
      jmlKertas = 0;
      jmlPlastik = 0;
      delay(3000);      // waktu gerakan
      servo.write(90);  // kembali ke netral
    } else if (jmlPlastik >= 5) {
      Serial.println("Sampah ke kanan");
      servo.write(170);
      jmlKertas = 0;
      jmlPlastik = 0;
      delay(3000);      // waktu gerakan
      servo.write(90);  // kembali ke netral
    }
  }

  // Serial.print("\nKepenuhan Kertas: ");
  // Serial.print(persenKertas, 1);
  // Serial.print("%");

  // Serial.print("\nKepenuhan Plastik: ");
  // Serial.print(persenPlastik, 1);
  // Serial.print("%");

  Serial.println(jarakKertas);
  Serial.println(jarakPlastik);

  lcd.setCursor(0, 0);
  lcd.print("Kertas: ");
  lcd.print(persenKertas);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("Plastik: ");
  lcd.print(persenPlastik);
  lcd.print("%");

  delay(500);
}