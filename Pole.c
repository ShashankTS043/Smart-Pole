#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <MPU6050.h>

// --- GPS module on D4 (RX), D3 (TX) ---
static const int RXPin = 4, TXPin = 3;
static const uint32_t GPSBaud = 9600;

TinyGPSPlus gps;
SoftwareSerial gpsSerial(RXPin, TXPin);

// --- LoRa SX1278 pins ---
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
  // Keep feeding GPS parser
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // --- MPU6050 (optional motion debug) ---
  int16_t ax_raw, ay_raw, az_raw;
  mpu.getAcceleration(&ax_raw, &ay_raw, &az_raw);

  float ax = ax_raw / 16384.0;
  float ay = ay_raw / 16384.0;
  float az = az_raw / 16384.0;

  float magnitude = sqrt(ax*ax + ay*ay + az*az);
  bool isMoving = (magnitude > (1.0 + ACC_THRESHOLD)) || (magnitude < (1.0 - ACC_THRESHOLD));

  Serial.println("----- MPU6050 Data -----");
  Serial.print("Accel X: "); Serial.print(ax, 3);
  Serial.print(" g\tY: "); Serial.print(ay, 3);
  Serial.print(" g\tZ: "); Serial.print(az, 3);
  Serial.print(" g\tMagnitude: "); Serial.print(magnitude, 3);
  Serial.print(" g\tMotion: ");
  Serial.println(isMoving ? "MOVING" : "STATIONARY");
  Serial.println("-----------------------");

  // --- GPS + LoRa Transmission ---
  String data;

  if (gps.location.isValid()) {
    // Build GPS data string
    data = "LoRa 1 | SN:" + String(serialNumber);
    data += " | Lat:" + String(gps.location.lat(), 6);
    data += " | Lon:" + String(gps.location.lng(), 6);
    data += " | Spd:" + String(gps.speed.kmph(), 2);
    data += " | UTC:" + String(gps.time.hour()) + ":" +
            String(gps.time.minute()) + ":" + String(gps.time.second());

    Serial.println("----- GPS Data -----");
    Serial.println(data);
    Serial.println("--------------------");
  } else {
    // GPS not available
    data = "GPS not available";
    Serial.println("[GPS] No fix. Sending fallback message.");
  }

  // Transmit over LoRa
  LoRa.beginPacket();
  LoRa.print(data);
  LoRa.endPacket();

  Serial.println("[LoRa] Sent:");
  Serial.println(data);
  Serial.println();

  serialNumber++;
  delay(3000);
}
