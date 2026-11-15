#pragma once
#include <stdint.h>

// Everything a mode file needs to drive the motor.
// Call initMotor() (motor_init.h) once in setup() before using any of this.

constexpr uint8_t NUM_EFFECTS = 123;

// Named presets from TI's effect library. Add more as you find ones you like.
namespace Effect {
  constexpr uint8_t StrongClick        = 1;
  constexpr uint8_t SharpClick         = 4;
  constexpr uint8_t SoftBump           = 7;
  constexpr uint8_t DoubleClick        = 10;
  constexpr uint8_t TripleClick        = 12;
  constexpr uint8_t StrongBuzz         = 14;
  constexpr uint8_t Alert750ms         = 15;
  constexpr uint8_t Alert1000ms        = 16;
  constexpr uint8_t Buzz100            = 47;
  constexpr uint8_t PulsingStrong      = 52;
  constexpr uint8_t RampDownLongSmooth = 70;
  constexpr uint8_t RampUpLongSmooth   = 82;
  constexpr uint8_t LongBuzz           = 118;   // runs until stopMotor()
}

// A pause inside a sequence, in 10 ms steps (max 1270 ms).
// Example: uint8_t seq[] = {Effect::StrongClick, waitMs(200), Effect::StrongClick};
constexpr uint8_t waitMs(uint16_t ms) {
  return 0x80 | (uint8_t)((ms > 1270 ? 1270 : ms) / 10);
}

// ---- Library effects (non-blocking: they start and return immediately) ----
bool playEffect(uint8_t n);                                // false if n isn't 1-123
bool playSequence(const uint8_t *steps, uint8_t count);    // up to 8 steps (effects or waitMs)

// ---- Direct strength control (real-time mode) ----
void startBuzz(uint8_t strength);                // 0-127, runs until stopMotor()
void buzzFor(uint16_t ms, uint8_t strength = 127); // blocking

// ---- Status and control ----
bool isPlaying(); // outputs if the motor is vibrating or not
void waitUntilDone(uint16_t timeoutMs = 3000); // goes in a loop until motor is done, so no work happens then
void stopMotor();

uint8_t currentEffect();                // last effect played, 0 if none yet
const char *effectName(uint8_t n);      // "" if out of range
