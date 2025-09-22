# 📦 Smart Trash Classification System (ESP32-S3 + ESP-CAM)
This project is an IoT-based smart trash sorting system that uses an ESP32-S3 microcontroller, an ESP-CAM module, and a custom-trained Edge Impulse model to automatically classify and sort waste (Paper/Cardboard or Plastic). The system uses ESP-NOW for communication, ultrasonic sensors for fill level detection, and a servo motor for automated sorting.

# 🚀 Features
- Image Classification with Edge Impulse
- ESP-CAM captures an image every few seconds.
- Model predicts whether the trash is Paper/Cardboard or Plastic.
- Inference is done directly on ESP-CAM using a trained Edge Impulse model (RGB, 64x64 input).
- Wireless Communication (ESP-NOW)
- ESP-CAM sends the classification result (label) to the ESP32-S3 controller via ESP-NOW.
- Only sends the label if the detection confidence matches one of the target classes.
- Micro Servo Motor Control
- ESP32-S3 receives classification labels and controls the servo.
- Servo rotates right for Paper/Cardboard and left for Plastic.
- Ultrasonic Sensors for Fill Level
- Two ultrasonic sensors placed above the bins.
- Calculates real-time fill percentage (0–100%) based on a 27 cm bin depth.
- Displays fill percentage on an I²C LCD screen.

# Each folder description
ESP-CAM: Basic code for ESP-CAM AI Thinker to make sure the device is working properly
ESP32-S3: Complete code for ESP32-S3 that controls the sensors and actuators
