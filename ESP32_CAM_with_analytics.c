#include "esp_camera.h"
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ArduinoJson.h>

// WiFi and Firebase config - update your credentials
#define WIFI_SSID "iPhone (2)"
#define WIFI_PASSWORD "12345678"
#define API_KEY "AIzaSyCFQVZ5TNp-q6D1ClJEsWGmFhUF6anQS9w"
#define DATABASE_URL "smart-street-pole-default-rtdb.asia-southeast1.firebasedatabase.app"
#define STORAGE_BUCKET "smart-street-pole.appspot.com"  // Your Firebase Storage bucket

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String uid = "esp32cam001";

camera_fb_t *prevFrame = NULL;

// Camera config (AI Thinker ESP32-CAM)
camera_config_t camera_config = {
  .pin_pwdn       = 32,
  .pin_reset      = -1,
  .pin_xclk       = 0,
  .pin_sscb_sda   = 26,
  .pin_sscb_scl   = 27,
  .pin_d7         = 35,
  .pin_d6         = 34,
  .pin_d5         = 39,
  .pin_d4         = 36,
  .pin_d3         = 21,
  .pin_d2         = 19,
  .pin_d1         = 18,
  .pin_d0         = 5,
  .pin_vsync      = 25,
  .pin_href       = 23,
  .pin_pclk       = 22,
  .xclk_freq_hz   = 20000000,
  .ledc_timer     = LEDC_TIMER_0,
  .ledc_channel   = LEDC_CHANNEL_0,
  .pixel_format   = PIXFORMAT_GRAYSCALE,  // For analysis
  .frame_size     = FRAMESIZE_QQVGA,
  .jpeg_quality   = 12,
  .fb_count       = 1
};

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println(" connected.");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.storage_bucket = STORAGE_BUCKET;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (esp_camera_init(&camera_config) != ESP_OK) {
    Serial.println("Camera init failed");
    while (1);
  }

  Serial.println("ESP32-CAM ready");
}

void loop() {
  // Check Serial for incoming JSON data lines from Arduino (environmental or GPS)
  if (Serial.available()) {
    String received = Serial.readStringUntil('\n');
    received.trim();

    if (received.startsWith("ENV:")) {
      String envJson = received.substring(4);
      uploadJsonToFirebase(envJson, "/smartpole/" + uid + "/environmental");
    }
    else if (received.startsWith("GPS:")) {
      String gpsJson = received.substring(4);
      uploadJsonToFirebase(gpsJson, "/smartpole/" + uid + "/lora");

      // Trigger visual clog inspection on new detected clog GPS event
      camera_fb_t *fb = esp_camera_fb_get();
      if (fb != NULL) {
        if (analyzeImageForClog(fb)) {
          uploadImageToFirebase(fb);
        } else {
          Serial.println("No visual clog detected");
        }
        esp_camera_fb_return(fb);
      }
    }
  }
}

// Firebase JSON upload helper
void uploadJsonToFirebase(String jsonStr, String path) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    Serial.println("JSON parse error: " + String(err.c_str()));
    return;
  }

  FirebaseJson json;
  for (JsonPair kv : doc.as<JsonObject>()) {
    json.set(kv.key().c_str(), kv.value());
  }

  if (Firebase.setJSON(fbdo, path, json)) {
    Serial.println("Uploaded to " + path);
  } else {
    Serial.println("Upload failed: " + fbdo.errorReason());
  }
}

// Analyze grayscale image for clog indicators by counting dark pixels
bool analyzeImageForClog(camera_fb_t *fb) {
  const uint8_t darkThreshold = 50;
  int darkCount = 0;
  for (size_t i = 0; i < fb->len; i++) {
    if (fb->buf[i] < darkThreshold) {
      darkCount++;
    }
  }

  float darkRatio = (float)darkCount / fb->len;
  Serial.printf("Image dark pixel ratio: %.3f\n", darkRatio);

  return (darkRatio > 0.2); // Adjust threshold (20% pixels dark) empirically
}

// Upload JPEG image to Firebase Storage
void uploadImageToFirebase(camera_fb_t *fb) {
  String path = "/clog_images/img_" + String(millis()) + ".jpg";
  if (Firebase.Storage.upload(&fbdo, path.c_str(), fb->buf, fb->len, "image/jpeg")) {
    Serial.println("Clog image uploaded: " + path);
  } else {
    Serial.println("Image upload failed: " + fbdo.errorReason());
  }
}
