#pragma once
#include <Adafruit_DRV2605.h>

// The single shared driver object, defined in motor_init.cpp.
// Mode files should NOT use this directly; go through motor_effects.h.
extern Adafruit_DRV2605 drv;

// Powers the I2C bus, finds the DRV2605L, configures closed-loop LRA,
// and runs auto-calibration. Returns false if the chip isn't found.
// Call Serial.begin() before this if you want status messages.
bool initMotor();

// Re-runs auto-calibration (about 1 s, blocking). Returns true on success.
bool calibrateMotor();

// ---- Drive configurations (used by motor_effects.cpp) ----
// LRA closed loop: the chip tracks the motor's resonance (strongest, and
// what the click/buzz library effects are tuned for).
void driveClosedLoopLRA();
// LRA open loop at a fixed frequency. The chip can only do periods up to
// 127 x 98.46 us, so this works from about 80 Hz up. Returns the actual Hz.
float driveOpenLoopLRA(float hz);
// Lowest drive frequency driveOpenLoopLRA() can produce.
constexpr float OPEN_LOOP_LRA_MIN_HZ = 1e6f / (127 * 98.46f);   // ~80 Hz
// Bidirectional open loop with no waveform of its own: the output simply
// follows the signed real-time value, so the sketch synthesizes the sine
// itself (used below ~80 Hz).
void driveOpenLoopDirect();
