#include <Smart-Trash-Bin-9_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"
#include <esp_now.h>
#include <WiFi.h>

#define CAMERA_MODEL_AI_THINKER
#if defined(CAMERA_MODEL_AI_THINKER)
  #define PWDN_GPIO_NUM     32
  #define RESET_GPIO_NUM    -1
  #define XCLK_GPIO_NUM     0
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
#else
  #error "Camera model not selected"
#endif

#define EI_CAMERA_RAW_FRAME_BUFFER_COLS 320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS 240
#define EI_CAMERA_FRAME_BYTE_SIZE 3
#define CAPTURE_INTERVAL 2000  // 2 detik

// Struktur data yang dikirim via ESP-NOW
typedef struct struct_message {
  char label[32];
} struct_message;

struct_message myData;

// MAC ESP32-S3
uint8_t esp32s3Address[] = {0x94, 0xA9, 0x90, 0x2E, 0x1D, 0xEC};

// State
unsigned long lastCaptureTime = 0;
uint8_t *snapshot_buf;
static bool debug_nn = false;

// Callback saat data dikirim via ESP-NOW
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Status kirim ESP-NOW: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Sukses" : "Gagal");
}

// Setup ESP-NOW
void setupEspNow() {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init gagal");
    return;
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, esp32s3Address, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_add_peer(&peerInfo)) {
    Serial.println("Peer ditambahkan");
  } else {
    Serial.println("Gagal menambahkan peer");
  }
}

// Setup kamera pakai config default
bool ei_camera_init() {
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
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  return err == ESP_OK;
}

// Fungsi capture (mengisi snapshot_buf)
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    ei_printf("Gagal mendapatkan frame dari kamera!\n");
    return false;
  }

  if (fb->format != PIXFORMAT_JPEG) {
    ei_printf("Format frame bukan JPEG!\n");
    esp_camera_fb_return(fb);
    return false;
  }

  // Decode JPEG ke RGB
  bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, out_buf);
  esp_camera_fb_return(fb);

  if (!converted) {
    ei_printf("Konversi JPEG ke RGB888 gagal!\n");
    return false;
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  ei_printf("Smart Trash Bin - ESP-CAM with Edge Impulse\n");

  if (!ei_camera_init()) {
    ei_printf("Inisialisasi kamera gagal!\n");
    return;
  }

  setupEspNow();
  ei_printf("Inisialisasi selesai. Menunggu 10 detik...\n");
  lastCaptureTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastCaptureTime >= CAPTURE_INTERVAL) {
    lastCaptureTime = currentMillis;

    snapshot_buf = (uint8_t *)malloc(
      EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE
    );

    if (!snapshot_buf) {
      ei_printf("Gagal alokasi buffer!\n");
      return;
    }

    if (!ei_camera_capture(EI_CAMERA_RAW_FRAME_BUFFER_COLS, EI_CAMERA_RAW_FRAME_BUFFER_ROWS, snapshot_buf)) {
      ei_printf("Gagal capture!\n");
      free(snapshot_buf);
      return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CAMERA_RAW_FRAME_BUFFER_COLS *
                          EI_CAMERA_RAW_FRAME_BUFFER_ROWS *
                          EI_CAMERA_FRAME_BYTE_SIZE;
    signal.get_data = [](size_t offset, size_t length, float *out_ptr) -> int {
      memcpy(out_ptr, snapshot_buf + offset, length);
      return 0;
    };

    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, debug_nn);
    free(snapshot_buf);

    if (res != EI_IMPULSE_OK) {
      ei_printf("Inferensi gagal: %d\n", res);
      return;
    }

    // Ambil label terbaik
    float maxVal = 0;
    const char* topLabel = "";
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      if (result.classification[ix].value > maxVal) {
        maxVal = result.classification[ix].value;
        topLabel = result.classification[ix].label;
      }
    }

    ei_printf("Label terdeteksi: %s (%.2f)\n", topLabel, maxVal);
    strncpy(myData.label, topLabel, sizeof(myData.label));
    esp_now_send(esp32s3Address, (uint8_t *)&myData, sizeof(myData));
  }
}
