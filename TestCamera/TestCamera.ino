#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ===== KONFIGURASI PIN UNTUK ESP32-CAM MODULE =====
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ===== CONFIGURASI ACCESS POINT =====
const char* ssid = "ESP-CAM";
const char* password = "esp12345";

WebServer server(80);

// ===== INISIALISASI KAMERA =====
void startCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // OUTPUT: JPEG - QQVGA or QVGA for low memory
  config.frame_size = FRAMESIZE_96X96;  // atau QQVGA jika mau lebih kecil
  config.jpeg_quality = 10;            // 0–63 (semakin kecil = kualitas tinggi)
  config.fb_count = 1;

  // Inisialisasi kamera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera gagal: 0x%x\n", err);
    while (1);
  }
}

// ===== HANDLER: /capture =====
void handleCapture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Gagal ambil gambar");
    return;
  }

  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");  // tanpa content body
  WiFiClient client = server.client();
  client.write(fb->buf, fb->len);

  esp_camera_fb_return(fb);
  Serial.println("Gambar berhasil dikirim via /capture");
}


void setup() {
  Serial.begin(115200);
  delay(500);

  startCamera();

  // Setup Access Point
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("ESP-CAM AP IP address: ");
  Serial.println(IP);  // default: 192.168.4.1

  server.on("/capture", HTTP_GET, handleCapture);
  server.begin();
  Serial.println("Server siap diakses di /capture");
}

void loop() {
  server.handleClient();
}
