#include <SoftwareSerial.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <MPU6050.h>
#include <SPI.h>
#include <LoRa.h>

// GPS pins for Nano
SoftwareSerial gpsSerial(5, 6); // RX, TX
TinyGPSPlus gps;

// MPU6050 instance
MPU6050 mpu;

// LoRa pins
#define LORA_SS 10
#define LORA_RST 9
#define LORA_DIO0 8

uint16_t seqNumber = 0;
unsigned long stationaryStart = 0;
const unsigned long stationaryThreshold = 5000; // 5 seconds
bool isStationary = false;
const float accelThreshold = 0.05; // sensitivity to gravity
bool sentStopAlert = false;

void setup() {
  Serial.begin(9600);
  gpsSerial.begin(9600);

  Wire.begin();
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while (1);
  }

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed");
    while (1);
  }
  LoRa.setTxPower(20);

  Serial.println("Setup complete");
}

void loop() {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  
  float xg = ax / 16384.0;
  float yg = ay / 16384.0;
  float zg = az / 16384.0;

  float totalAccel = sqrt(xg * xg + yg * yg + zg * zg);

  if (abs(totalAccel - 1.0) < accelThreshold) {
    if (!isStationary) {
      stationaryStart = millis();
      isStationary = true;
    } else if (!sentStopAlert && (millis() - stationaryStart > stationaryThreshold)) {
      sendClogAlert();
      sentStopAlert = true;
    }
  } else {
    isStationary = false;
    sentStopAlert = false;
  }
}

void sendClogAlert() {
  if (gps.location.isValid()) {
    String packet = String(seqNumber) + "|";
    packet += String(millis()) + "|";
    packet += String(gps.location.lat(), 6) + "|";
    packet += String(gps.location.lng(), 6);

    LoRa.beginPacket();
    LoRa.print(packet);
    LoRa.endPacket();

    Serial.println("LoRa sent: " + packet);
    seqNumber++;
  } else {
    Serial.println("GPS location invalid, cannot send alert");
  }
}
