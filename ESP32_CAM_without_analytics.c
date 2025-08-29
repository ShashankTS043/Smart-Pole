#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ArduinoJson.h>

#define WIFI_SSID "iPhone (2)"
#define WIFI_PASSWORD "12345678"
#define API_KEY "AIzaSyCFQVZ5TNp-q6D1ClJEsWGmFhUF6anQS9w"
#define DATABASE_URL "smart-street-pole-default-rtdb.asia-southeast1.firebasedatabase.app"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String uid = "esp32cam001";
unsigned long lastUploadTime = 0;
unsigned long uploadInterval = 5000;

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
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Firebase initialized.");
}

void loop() {
  if (Serial.available()) {
    String received = Serial.readStringUntil('\n');
    received.trim();

    if (received.startsWith("ENV:")) {
      String envJson = received.substring(4);
      uploadJsonToFirebase(envJson, "/smartpole/" + uid + "/environmental");
    } else if (received.startsWith("GPS:")) {
      String gpsJson = received.substring(4);
      uploadJsonToFirebase(gpsJson, "/smartpole/" + uid + "/lora");
    }
  }
}

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
