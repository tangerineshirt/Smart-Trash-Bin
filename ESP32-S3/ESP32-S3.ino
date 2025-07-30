#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ====== KONFIGURASI PIN ======
#define SERVO_PIN 17
#define US_TRIGGER 5
#define US_ECHO 4
#define US_PENUH_KERTAS 12
#define US_PENUH_ECHO_KERTAS 13
#define US_PENUH_PLASTIK 41
#define US_PENUH_ECHO_PLASTIK 42

#define JARAK_SAMPAH_MIN_CM 10
#define WAKTU_PENUH_MS 10000  // 10 detik stabil di bawah threshold → penuh

// ====== VARIABEL ======
Servo servo;
String currentLabel = "";
bool labelBaruDiterima = false;

int jmlKertas = 0, jmlPlastik = 0;

typedef struct struct_message {
  char label[32];
} struct_message;

struct_message incomingDataStruct;

const float selisihTinggi = 21;
const float globalMaks = 27;

LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD initialization

// ====== FUNGSI ULTRASONIK ======
float ukurJarakCM(int trigger, int echo) {
  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);
  long duration = pulseIn(echo, HIGH, 30000);  // timeout 30ms
  long distance = duration * 0.034 / 2;
  return distance;
}

float hitungPersen(float jarak, float maks) {
  return 100.0 - (jarak / maks * 100.0);
}

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

  pinMode(US_TRIGGER, OUTPUT);
  pinMode(US_ECHO, INPUT);
  pinMode(US_PENUH_KERTAS, OUTPUT);
  pinMode(US_PENUH_ECHO_KERTAS, INPUT);
  pinMode(US_PENUH_PLASTIK, OUTPUT);
  pinMode(US_PENUH_ECHO_PLASTIK, INPUT);

  servo.attach(SERVO_PIN);
  servo.write(90);  // posisi netral

  lcd.init();
  lcd.backlight();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init gagal");
    return;
  }

  esp_now_register_recv_cb(onReceiveData);
}

// ====== LOOP UTAMA ======
void loop() {
  float jarakSampah = ukurJarakCM(US_TRIGGER, US_ECHO);
  Serial.print("Jarak sampah: ");
  Serial.print(jarakSampah);
  Serial.println(" cm");

  float penuhKertas = ukurJarakCM(US_PENUH_KERTAS, US_PENUH_ECHO_KERTAS) - selisihTinggi;
  float penuhPlastik = ukurJarakCM(US_PENUH_PLASTIK, US_PENUH_ECHO_PLASTIK) - selisihTinggi;

  float persenKertas = hitungPersen(penuhKertas, globalMaks);
  float persenPlastik = hitungPersen(penuhPlastik, globalMaks);

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

  Serial.print("\nKepenuhan Kertas: ");
  Serial.print(persenKertas);
  Serial.print("%");

  Serial.print("\nKepenuhan Plastik: ");
  Serial.print(persenPlastik);
  Serial.print("%");
  lcd.clear();
  lcd.setCursor(0, 0);
  if (penuhKertas <= 27) {
    lcd.print("Kertas: FULL");
  } else {
    lcd.print("Kertas: ");
    lcd.print(persenKertas);
    lcd.print("%");
  }
  lcd.setCursor(0, 1);

  if (penuhPlastik <= 27) {
    lcd.print("Kertas: FULL");
  } else {
    lcd.print("Plastik: ");
    lcd.print(persenPlastik);
    lcd.print("%");
  }

  delay(500);
}