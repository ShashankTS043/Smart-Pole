#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <MPU6050.h>

// GPS module on D4 (RX), D3 (TX)
static const int RXPin = 4, TXPin = 3;
static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;
SoftwareSerial gpsSerial(RXPin, TXPin);

// LoRa SX1278 module connections
#define LORA_CS   10
#define LORA_RST  9
#define LORA_IRQ  2

MPU6050 mpu;

const float ACC_THRESHOLD = 0.1;  // Threshold for motion detection

int serialNumber = 0;

void setup() {
  Serial.begin(9600);
  gpsSerial.begin(GPSBaud);

  Serial.println("Initializing MPU6050...");
  Wire.begin();
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while (1);
  }
  Serial.println("MPU6050 connected.");

  Serial.println("Initializing LoRa...");
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed.");
    while (1);
  }
  Serial.println("LoRa initialized.");

  Serial.println("Setup complete.");
}

void loop() {
  // Feed GPS
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // Read MPU6050 accel data
  int16_t ax_raw, ay_raw, az_raw;
  mpu.getAcceleration(&ax_raw, &ay_raw, &az_raw);

  float ax = ax_raw / 16384.0;
  float ay = ay_raw / 16384.0;
  float az = az_raw / 16384.0;

  float magnitude = sqrt(ax*ax + ay*ay + az*az);
  bool isMoving = (magnitude > (1.0 + ACC_THRESHOLD)) || (magnitude < (1.0 - ACC_THRESHOLD));

  // Print MPU6050 data
  Serial.println("----- MPU6050 Data -----");
  Serial.print("Accel X: "); Serial.print(ax, 3);
  Serial.print(" g\tY: "); Serial.print(ay, 3);
  Serial.print(" g\tZ: "); Serial.print(az, 3);
  Serial.print(" g\tMagnitude: "); Serial.print(magnitude, 3);
  Serial.print(" g\tMotion: ");
  Serial.println(isMoving ? "MOVING" : "STATIONARY");
  Serial.println("-----------------------");

  // Print GPS data
  if (gps.location.isValid()) {
    Serial.println("----- GPS Data -----");
    Serial.print("Latitude: "); Serial.println(gps.location.lat(), 6);
    Serial.print("Longitude: "); Serial.println(gps.location.lng(), 6);
    Serial.print("Speed (km/h): "); Serial.println(gps.speed.kmph());
    Serial.print("UTC Time: ");
    Serial.print(gps.time.hour()); Serial.print(":");
    Serial.print(gps.time.minute()); Serial.print(":");
    Serial.println(gps.time.second());
    Serial.println("--------------------");
  } else {
    Serial.println("[GPS] Waiting for GPS fix...");
  }

  // Send LoRa message depending on movement and GPS lock
  if (!isMoving) {
    if (gps.location.isValid()) {
      String data = "LoRa 1 | SN: " + String(serialNumber);
      data += " | Lat: " + String(gps.location.lat(), 6);
      data += " | Lon: " + String(gps.location.lng(), 6);
      data += " | Spd: " + String(gps.speed.kmph(), 2);
      data += " | UTC: ";
      data += gps.time.hour(); data += ":";
      data += gps.time.minute(); data += ":";
      data += gps.time.second();

      LoRa.beginPacket();
      LoRa.print(data);
      LoRa.endPacket();

      Serial.println("[LoRa] Sent data:");
      Serial.println(data);
    } else {
      String noFixMsg = "LoRa 1 | SN: " + String(serialNumber) + " | No GPS fix yet";
      LoRa.beginPacket();
      LoRa.print(noFixMsg);
      LoRa.endPacket();

      Serial.println("[LoRa] Sent no-fix message:");
      Serial.println(noFixMsg);
    }
  } else {
    String movingMsg = "LoRa 1 | SN: " + String(serialNumber) + " | Ball is moving";
    LoRa.beginPacket();
    LoRa.print(movingMsg);
    LoRa.endPacket();

    Serial.println("[LoRa] Sent moving message:");
    Serial.println(movingMsg);
  }

  serialNumber++;

  delay(3000);
}
