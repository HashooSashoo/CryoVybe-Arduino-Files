#include <Arduino.h>
#include "motor_init.h"
#include "motor_effects.h"

void setup() {
  Serial.begin(115200);

  // Wait up to 3 seconds for the Serial Monitor.
  while (!Serial && millis() < 3000) {
    delay(10);
  }

  // Initialize and calibrate the DRV2605L motor driver.
  if (!initMotor()) {
    Serial.println("Motor initialization failed.");

    // Stop here if the motor driver was not found.
    while (true) {
      delay(1000);
    }
  }

  Serial.println("DRV2605L ready.");
  Serial.println("Starting continuous vibration.");

  // Start continuous vibration.
  // Strength range: 0–127.
  startBuzz(127);
}

void loop() {
  // Nothing is required here.
  // The motor continues vibrating until power is removed.
}