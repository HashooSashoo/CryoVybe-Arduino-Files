#pragma once
#include <Adafruit_DRV2605.h>

// The single shared driver object, defined in motor_init.cpp.
// Mode files should NOT use this directly; go through motor_effects.h.
extern Adafruit_DRV2605 drv;

// Powers the I2C bus, finds the DRV2605L, configures closed-loop ERM,
// and runs auto-calibration. Returns false if the chip isn't found.
// Call Serial.begin() before this if you want status messages.
bool initMotor();

// Re-runs auto-calibration (about 1 s, blocking). Returns true on success.
bool calibrateMotor();
