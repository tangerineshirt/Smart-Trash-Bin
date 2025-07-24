#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h>

// ====== KONFIGURASI PIN ======
#define SERVO_PIN 13
#define US_TRIGGER 5
#define US_ECHO 4
#define US_PENUH_KERTAS 18
#define US_PENUH_ECHO_KERTAS 19
#define US_PENUH_PLASTIK 21
#define US_PENUH_ECHO_PLASTIK 22

#define JARAK_SAMPAH_MIN_CM 10
#define WAKTU_PENUH_MS 10000  // 10 detik stabil di bawah threshold → penuh

// ====== VARIABEL ======
Servo servo;
String currentLabel = "";
bool labelBaruDiterima = false;

int jmlKertas = 0, jmlPlastik = 0;

unsigned long waktuAwalPenuhKertas = 0;
bool kertasPenuh = false;

unsigned long waktuAwalPenuhPlastik = 0;
bool plastikPenuh = false;

typedef struct struct_message {
  char label[32];
} struct_message;

struct_message incomingDataStruct;

// ====== FUNGSI ULTRASONIK ======
long ukurJarakCM(int trigger, int echo) {
  digitalWrite(trigger, LOW);
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);
  long duration = pulseIn(echo, HIGH, 30000);  // timeout 30ms
  long distance = duration * 0.034 / 2;
  return distance;
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

  pinMode(US_TRIGGER, OUTPUT);
  pinMode(US_ECHO, INPUT);
  pinMode(US_PENUH_KERTAS, OUTPUT);
  pinMode(US_PENUH_ECHO_KERTAS, INPUT);
  pinMode(US_PENUH_PLASTIK, OUTPUT);
  pinMode(US_PENUH_ECHO_PLASTIK, INPUT);

  servo.attach(SERVO_PIN);
  servo.write(90);  // posisi netral

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init gagal");
    return;
  }

  esp_now_register_recv_cb(onReceiveData);
}

// ====== LOOP UTAMA ======
void loop() {
  long jarakSampah = ukurJarakCM(US_TRIGGER, US_ECHO);
  Serial.print("Jarak sampah: ");
  Serial.print(jarakSampah);
  Serial.println(" cm");

  // Deteksi penuh tong kertas
  long jarakKertas = ukurJarakCM(US_PENUH_KERTAS, US_PENUH_ECHO_KERTAS);
  if (jarakKertas < 8) {
    if (waktuAwalPenuhKertas == 0) waktuAwalPenuhKertas = millis();
    else if (millis() - waktuAwalPenuhKertas > WAKTU_PENUH_MS) kertasPenuh = true;
  } else {
    waktuAwalPenuhKertas = 0;
    kertasPenuh = false;
  }

  // Deteksi penuh tong plastik
  long jarakPlastik = ukurJarakCM(US_PENUH_PLASTIK, US_PENUH_ECHO_PLASTIK);
  if (jarakPlastik < 8) {
    if (waktuAwalPenuhPlastik == 0) waktuAwalPenuhPlastik = millis();
    else if (millis() - waktuAwalPenuhPlastik > WAKTU_PENUH_MS) plastikPenuh = true;
  } else {
    waktuAwalPenuhPlastik = 0;
    plastikPenuh = false;
  }

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
      delay(1000);      // waktu gerakan
      servo.write(90);  // kembali ke netral
    } else if (jmlPlastik >= 5) {
      Serial.println("Sampah ke kanan");
      servo.write(170);
      jmlKertas = 0;
      jmlPlastik = 0;
      delay(1000);      // waktu gerakan
      servo.write(90);  // kembali ke netral
    }
  }

  // Cetak status penuh
  Serial.print("Kertas penuh? ");
  Serial.println(kertasPenuh ? "YA" : "TIDAK");
  Serial.print("Plastik penuh? ");
  Serial.println(plastikPenuh ? "YA" : "TIDAK");

  delay(500);
}