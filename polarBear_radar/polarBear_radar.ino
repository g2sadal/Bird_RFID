#include <Arduino.h>
#include <LD2450.h>

// ESP32 Serial Pin Definitions
#define UART_RX_PIN 16  // GPIO17 for RX
#define UART_TX_PIN 17  // GPIO16 for TX

// LED Pin (Built-in or External)
#define LED_PIN 2  // ESP32 default built-in LED on GPIO2

// Sensor Instance
LD2450 ld2450;

// Detection thresholds (millimeters)
const unsigned long DETECTION_DISTANCE_MM = 7000UL;      // 7 m
const unsigned long MIN_DETECTION_DISTANCE_MM = 1000UL;  // ignore < 4 m and zero
const float MIN_DETECTION_SPEED_CMS = 16.0f;             // minimum 5 cm/s speed for valid detection

const int forwards = 32;
const int backwards = 33;//assign relay INx pin to arduino pin

bool retract = false;

// Debugging
// Set to true before uploading to see full LD2450 target fields
const bool LD2450_DEBUG = false;

void setup() {
  // Debugging Serial
  Serial.begin(115200);
  Serial.println("SETUP_STARTED");

  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(forwards, OUTPUT);//set relay as an output
  pinMode(backwards, OUTPUT);//set relay as an output

  // Initialize Sensor using Serial2
  Serial2.begin(25600, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);  // Set baud rate and pins for Serial2
  ld2450.begin(Serial2, false);

  // Test Sensor Connection
  if (!ld2450.waitForSensorMessage()) {
    Serial.println("LD2450: sensor connection OK");
  } else {
    Serial.println("LD2450: WARNING - no valid message yet");
  }

  // Fully extend actuator to start
  digitalWrite(forwards, HIGH);
  digitalWrite(backwards, LOW);
  delay(2000);
  digitalWrite(forwards, HIGH);
  digitalWrite(backwards, HIGH);

  Serial.println("RUN");

  delay(5000);


  Serial.println("SETUP_FINISHED");
}

void loop() {
  delay(1000);
  int got = ld2450.read();
  if (got > 0) {

    uint16_t maxTargets = ld2450.getSensorSupportedTargetCount();
    if (LD2450_DEBUG) {
      Serial.println("---- Detected Targets (raw) ----");
      // Serial.println(maxTargets);
    }

    for (uint16_t i = 0; i < maxTargets; ++i) {
      const LD2450::RadarTarget t = ld2450.getTarget(i);
      // Serial.println(ld2450.getLastTargetMessage());
      if (!t.valid) continue;

      if (LD2450_DEBUG) {
        Serial.print("Target ID: ");
        Serial.print(t.id);
        Serial.print(", X: ");
        Serial.print(t.x);
        Serial.print(" mm, Y: ");
        Serial.print(t.y);
        Serial.print(" mm, Speed: ");
        Serial.print(t.speed);
        Serial.print(" cm/s, Distance: ");
        Serial.print(t.distance);
        Serial.print(" mm, Resolution: ");
        Serial.print(t.resolution);
        Serial.println(" mm");
      }

      long dist = (long)t.distance;
      float speed = abs(t.speed);  // Use absolute value of speed

      if (dist <= 0 || t.y < 0 || t.x < 0) continue;                                        // ignore invalid
      //if ((unsigned long)dist < MIN_DETECTION_DISTANCE_MM) continue;  // ignore too-close noise

      // Check both distance AND speed thresholds
      if ((unsigned long)dist <= DETECTION_DISTANCE_MM ) {//&& speed >= MIN_DETECTION_SPEED_CMS
        
          Serial.print("=> VALID DETECTION - distance: ");
          Serial.print(dist);
          Serial.print("mm, speed: ");
          Serial.print(speed);
          Serial.println(" cm/s");

          // Retract actuator
          if (!retract && (unsigned long)dist <MIN_DETECTION_DISTANCE_MM) {
            Serial.println("RETRACT");
          digitalWrite(forwards, LOW);
          digitalWrite(backwards, HIGH);
          delay(2000);
          digitalWrite(forwards, HIGH);
          digitalWrite(backwards, HIGH);

          retract = true;
          }
        
      }
    }
  }
}