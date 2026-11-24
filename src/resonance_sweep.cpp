// Resonance sweep: find the drive that gives the most displacement.
// See resonance_sweep.h.

#include <Arduino.h>
#include "resonance_sweep.h"
#include "motor_init.h"
#include "motor_effects.h"
#include "accel_meter.h"

namespace {

constexpr float SWEEP_MIN_HZ   = 100.0f;  // LRAs resonate well above this
constexpr float SWEEP_MAX_HZ   = 300.0f;
constexpr float COARSE_STEP_HZ = 10.0f;
constexpr float FINE_SPAN_HZ   = 8.0f;    // fine sweep: best coarse +/- this
constexpr float FINE_STEP_HZ   = 2.0f;
// Closed loop wins unless open loop was more than this much stronger
constexpr float OPEN_LOOP_MARGIN = 1.05f;

float bestHz = 0, bestDisp = 0, resHz = 0;

// Buzz at full strength at hz (0 = closed loop) and return one clean
// accelerometer reading. The first reading after a change mixes old and
// new, so it is skipped.
bool measure(float hz, AccelReading &out) {
  setDriveFrequency(hz);
  startBuzz(127);
  AccelReading r;
  uint32_t startSeq = accelLatest(r) ? r.seq : 0;
  unsigned long t0 = millis();
  while (millis() - t0 < 2000) {
    accelIdle(5);
    if (accelLatest(r) && r.seq >= startSeq + 2) { out = r; return true; }
  }
  return false;
}

void printPoint(const char *label, float hz, const AccelReading &r) {
  Serial.print(label);
  if (hz > 0) { Serial.print(hz, 1); Serial.print(" Hz"); }
  Serial.print(" -> ");
  Serial.print(r.dispFUm, 1);
  Serial.print(" um at ");
  Serial.print(r.freq, 1);
  Serial.print(" Hz (");
  Serial.print(r.peakG, 2);
  Serial.println(" g)");
  Serial.print("@SWEEP ");               // for plotting: drive Hz, um, measured Hz
  Serial.print(hz, 1); Serial.print(' ');
  Serial.print(r.dispFUm, 2); Serial.print(' ');
  Serial.println(r.freq, 1);
}

}  // namespace

float findBestDrive() {
  AccelReading r;
  bestHz = 0; bestDisp = 0; resHz = 0;
  accelStart();
  if (!measure(0, r)) {
    stopMotor();
    accelStop();
    Serial.println("No accelerometer readings: using closed loop (chip tracks resonance).");
    return 0;
  }
  Serial.println("Finding the strongest drive (about 20 s; keep the device as it will be worn)...");

  // 1. Coarse, then fine, open-loop sweep
  float olHz = 0, olDisp = 0, olFreq = 0;
  bool saturated = false;
  for (float hz = SWEEP_MIN_HZ; hz <= SWEEP_MAX_HZ + 0.1f; hz += COARSE_STEP_HZ) {
    if (!measure(hz, r)) continue;
    printPoint("  open loop ", driveFrequency(), r);
    saturated |= r.maxG >= 15.5f;
    if (r.dispFUm > olDisp) { olDisp = r.dispFUm; olHz = hz; olFreq = r.freq; }
  }
  float centre = olHz;
  for (float hz = centre - FINE_SPAN_HZ; hz <= centre + FINE_SPAN_HZ + 0.1f; hz += FINE_STEP_HZ) {
    if (hz < SWEEP_MIN_HZ || hz > SWEEP_MAX_HZ || fabsf(hz - centre) < 0.1f) continue;
    if (!measure(hz, r)) continue;
    printPoint("  open loop ", driveFrequency(), r);
    saturated |= r.maxG >= 15.5f;
    if (r.dispFUm > olDisp) { olDisp = r.dispFUm; olHz = hz; olFreq = r.freq; }
  }

  // 2. Closed loop
  float clDisp = 0, clFreq = 0;
  if (measure(0, r)) {
    printPoint("  closed loop (tracks resonance)", 0, r);
    clDisp = r.dispFUm; clFreq = r.freq;
  }
  stopMotor();
  accelStop();

  // 3. Choose
  if (clDisp > 0 && clDisp * OPEN_LOOP_MARGIN >= olDisp) {
    bestHz = 0; bestDisp = clDisp; resHz = clFreq;
  } else {
    bestHz = olHz; bestDisp = olDisp; resHz = olFreq;
  }
  if (resHz > 0) setResonanceGuess(resHz);   // helps closed loop lock on quickly

  Serial.print("Best: ");
  if (bestHz == 0) Serial.print("closed loop");
  else { Serial.print("open loop at "); Serial.print(setDriveFrequency(bestHz), 1); Serial.print(" Hz"); }
  Serial.print(", ");
  Serial.print(bestDisp, 1);
  Serial.print(" um peak (");
  Serial.print(2 * bestDisp, 1);
  Serial.print(" um peak-to-peak) at ");
  Serial.print(resHz, 1);
  Serial.println(" Hz");
  if (saturated) Serial.println("Note: the accelerometer hit 16 g, so some readings are too low.");
  Serial.print("@BEST ");
  Serial.print(bestHz, 1); Serial.print(' ');
  Serial.print(bestDisp, 2); Serial.print(' ');
  Serial.println(resHz, 1);
  return bestHz;
}

float bestDriveHz() { return bestHz; }
float bestDispUm()  { return bestDisp; }
float resonanceHz() { return resHz; }
