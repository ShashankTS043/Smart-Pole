#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

// --- Pin Definitions ---
#define DHTPIN A2
#define DHTTYPE DHT22
#define MQ135_PIN A0
#define NOISE_PIN A1
#define BUTTON_PIN 5

// LoRa Pins
#define LORA_SS 10
#define LORA_RST 9
#define LORA_DIO0 8

// Globals
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long lastSwitch = 0;
int displayState = 0;

// Noise Sampling
unsigned long lastNoiseSample = 0;
const int noiseSampleInterval = 100;
float noiseAverage = 0;
const float alphaNoise = 0.1;

// Air Quality Calibration
float mq135Baseline = 200;
unsigned long lastAirQualitySample = 0;
const int airQualitySampleInterval = 10000;
float calibratedAirQuality = 0;

// LoRa Receiving
String latestCoordinates = "";
uint16_t lastSequenceNumber = 0;

unsigned long lastEnvUpload = 0;
const unsigned long envUploadInterval = 5000;  // every 5 sec

// Environmental sensor data
float lastTemperature = 0;
float lastHumidity = 0;

void setup() {
  Serial.begin(115200);   // Serial to ESP32-CAM
  dht.begin();
  lcd.init();
  lcd.backlight();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setupLoRa();
}

void setupLoRa() {
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed");
    lcd.clear();
    lcd.print("LoRa Init Fail");
    while (1);
  }
  LoRa.receive();
  Serial.println("LoRa Receiver Ready");
  lcd.clear();
  lcd.print("LoRa Ready");
  delay(1000);
}

void loop() {
  receiveCoordinates();

  unsigned long now = millis();

  if (now - lastNoiseSample >= noiseSampleInterval) {
    lastNoiseSample = now;
    updateNoise();
  }

  if (now - lastAirQualitySample >= airQualitySampleInterval) {
    lastAirQualitySample = now;
    calibratedAirQuality = readCalibratedAirQuality();
  }

  if (now - lastEnvUpload >= envUploadInterval) {
    lastEnvUpload = now;
    sendEnvironmentalData();
  }

  if (now - lastSwitch >= 5000) {
    lastSwitch = now;
    displayState = (displayState + 1) % 5;
    lcd.clear();
    switch (displayState) {
      case 0: displayTempHumidity(); break;
      case 1: displayAirQuality(); break;
      case 2: displayNoiseLevel(); break;
      case 3: displayCoordinates(); break;
      case 4: displaySummary(); break;
    }
  }

  delay(100);
}

void receiveCoordinates() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String packet = "";
    while (LoRa.available()) {
      packet += (char)LoRa.read();
    }
    Serial.print("LoRa received: ");
    Serial.println(packet);

    // Handle "GPS data not available" messages
    if (packet.startsWith("GPS data not available")) {
      Serial.println("GPS data not available message received");
      Serial.println("GPS:{\"status\":\"no_data\"}");
      latestCoordinates = packet;
      return;
    }

    int sepIndex = packet.indexOf('|');
    if (sepIndex == -1) {
      Serial.println("Invalid packet format");
      latestCoordinates = packet; // display raw message anyway
      return;
    }

    uint16_t seqNum = packet.substring(0, sepIndex).toInt();

    LoRa.beginPacket();
    LoRa.print("ACK:" + String(seqNum));
    LoRa.endPacket();

    if (seqNum != lastSequenceNumber) {
      lastSequenceNumber = seqNum;
      latestCoordinates = packet;

      Serial.print("GPS:");
      Serial.println(createGpsJson(latestCoordinates));
    }
  }
}

String createGpsJson(String packet) {
  // Packet format: SEQ|TIMESTAMP|LATITUDE|LONGITUDE
  int first = packet.indexOf('|');
  int second = packet.indexOf('|', first + 1);
  int third = packet.indexOf('|', second + 1);

  if (first == -1 || second == -1 || third == -1) {
    return "{}"; // Invalid format
  }

  String latitude = packet.substring(second + 1, third);
  String longitude = packet.substring(third + 1);

  StaticJsonDocument<256> doc;
  doc["latitude"] = latitude.toFloat();
  doc["longitude"] = longitude.toFloat();

  String output;
  serializeJson(doc, output);
  return output;
}

void sendEnvironmentalData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Store values for display
  lastTemperature = temperature;
  lastHumidity = humidity;

  StaticJsonDocument<256> envDoc;
  envDoc["temperature"] = temperature;
  envDoc["humidity"] = humidity;
  envDoc["air_quality"] = calibratedAirQuality;
  envDoc["noise_level"] = noiseAverage;

  String envJson;
  serializeJson(envDoc, envJson);

  Serial.print("ENV:");
  Serial.println(envJson);
  Serial.println("Environmental data sent.");
}

float readCalibratedAirQuality() {
  int raw = analogRead(MQ135_PIN);
  float adjusted = raw - mq135Baseline;
  return (adjusted < 0) ? 0 : adjusted;
}

void updateNoise() {
  int noiseRaw = analogRead(NOISE_PIN);
  float dB = map(noiseRaw, 0, 1023, 30, 130);
  noiseAverage = alphaNoise * dB + (1 - alphaNoise) * noiseAverage;
}

void displayTempHumidity() {
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(lastTemperature, 1);
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print("Hum: ");
  lcd.print(lastHumidity, 1);
  lcd.print("%");
}

void displayAirQuality() {
  lcd.setCursor(0, 0);
  lcd.print("Air: ");
  lcd.print(calibratedAirQuality, 0);
  lcd.setCursor(0, 1);
  lcd.print(calibratedAirQuality < 50 ? "Good" : "Poor");
}

void displayNoiseLevel() {
  lcd.setCursor(0, 0);
  lcd.print("Noise: ");
  lcd.print((int)noiseAverage);
  lcd.print(" dB");
  lcd.setCursor(0, 1);
  lcd.print(noiseAverage > 70 ? "Loud" : "Quiet");
}

void displayCoordinates() {
  lcd.setCursor(0, 0);
  if (latestCoordinates.length() > 0) {
    lcd.print("Msg:");
    lcd.setCursor(0, 1);
    if (latestCoordinates.length() > 16) {
      lcd.print(latestCoordinates.substring(0, 16));
    } else {
      lcd.print(latestCoordinates);
    }
  } else {
    lcd.print("No LoRa Msg");
  }
}

void displaySummary() {
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(lastTemperature, 1);
  lcd.print("C AQ:");
  lcd.print(calibratedAirQuality, 0);
  lcd.setCursor(0, 1);
  lcd.print("N:");
  lcd.print((int)noiseAverage);
  lcd.print("dB");
}

