// Effect library, presets, and functions for running the motor.
// Nothing in here reads Serial input, so any mode can use it.

#include <Arduino.h>
#include "motor_init.h"
#include "motor_effects.h"

namespace {

uint8_t lastEffect = 0;
bool buzzing = false;   // true while in real-time mode

// How the chip is set up to drive the LRA right now
enum class Drive : uint8_t { ClosedLoop, OpenLoopLRA, Synth };
Drive drive = Drive::ClosedLoop;

float freqHz = 0;       // requested buzz frequency, 0 = resonance
uint8_t buzzStrength = 0; // last startBuzz() strength, 0-127

// Software sine (Synth drive)
constexpr uint32_t SYNTH_STEP_US = 400;   // shortest time between updates (~2.5 kHz)
uint8_t  synthAmp = 0;                    // sine peak, 0-127
uint32_t synthStartUs = 0;
uint32_t synthLastUs = 0;
int8_t   synthLast = 0;

void setDrive(Drive d) {
  if (d == drive) return;
  if (d == Drive::ClosedLoop) driveClosedLoopLRA();
  if (d == Drive::Synth)      driveOpenLoopDirect();
  // OpenLoopLRA is applied in startBuzz, since it needs the frequency
  drive = d;
}

// Names from TI's DRV2605L datasheet, "Waveform Library Effects List".
const char *const EFFECT_NAMES[NUM_EFFECTS + 1] = {
  "",
  "Strong Click - 100%",
  "Strong Click - 60%",
  "Strong Click - 30%",
  "Sharp Click - 100%",
  "Sharp Click - 60%",
  "Sharp Click - 30%",
  "Soft Bump - 100%",
  "Soft Bump - 60%",
  "Soft Bump - 30%",
  "Double Click - 100%",
  "Double Click - 60%",
  "Triple Click - 100%",
  "Soft Fuzz - 60%",
  "Strong Buzz - 100%",
  "750 ms Alert - 100%",
  "1000 ms Alert - 100%",
  "Strong Click 1 - 100%",
  "Strong Click 2 - 80%",
  "Strong Click 3 - 60%",
  "Strong Click 4 - 30%",
  "Medium Click 1 - 100%",
  "Medium Click 2 - 80%",
  "Medium Click 3 - 60%",
  "Sharp Tick 1 - 100%",
  "Sharp Tick 2 - 80%",
  "Sharp Tick 3 - 60%",
  "Short Double Click Strong 1 - 100%",
  "Short Double Click Strong 2 - 80%",
  "Short Double Click Strong 3 - 60%",
  "Short Double Click Strong 4 - 30%",
  "Short Double Click Medium 1 - 100%",
  "Short Double Click Medium 2 - 80%",
  "Short Double Click Medium 3 - 60%",
  "Short Double Sharp Tick 1 - 100%",
  "Short Double Sharp Tick 2 - 80%",
  "Short Double Sharp Tick 3 - 60%",
  "Long Double Sharp Click Strong 1 - 100%",
  "Long Double Sharp Click Strong 2 - 80%",
  "Long Double Sharp Click Strong 3 - 60%",
  "Long Double Sharp Click Strong 4 - 30%",
  "Long Double Sharp Click Medium 1 - 100%",
  "Long Double Sharp Click Medium 2 - 80%",
  "Long Double Sharp Click Medium 3 - 60%",
  "Long Double Sharp Tick 1 - 100%",
  "Long Double Sharp Tick 2 - 80%",
  "Long Double Sharp Tick 3 - 60%",
  "Buzz 1 - 100%",
  "Buzz 2 - 80%",
  "Buzz 3 - 60%",
  "Buzz 4 - 40%",
  "Buzz 5 - 20%",
  "Pulsing Strong 1 - 100%",
  "Pulsing Strong 2 - 60%",
  "Pulsing Medium 1 - 100%",
  "Pulsing Medium 2 - 60%",
  "Pulsing Sharp 1 - 100%",
  "Pulsing Sharp 2 - 60%",
  "Transition Click 1 - 100%",
  "Transition Click 2 - 80%",
  "Transition Click 3 - 60%",
  "Transition Click 4 - 40%",
  "Transition Click 5 - 20%",
  "Transition Click 6 - 10%",
  "Transition Hum 1 - 100%",
  "Transition Hum 2 - 80%",
  "Transition Hum 3 - 60%",
  "Transition Hum 4 - 40%",
  "Transition Hum 5 - 20%",
  "Transition Hum 6 - 10%",
  "Ramp Down Long Smooth 1 - 100 to 0%",
  "Ramp Down Long Smooth 2 - 100 to 0%",
  "Ramp Down Medium Smooth 1 - 100 to 0%",
  "Ramp Down Medium Smooth 2 - 100 to 0%",
  "Ramp Down Short Smooth 1 - 100 to 0%",
  "Ramp Down Short Smooth 2 - 100 to 0%",
  "Ramp Down Long Sharp 1 - 100 to 0%",
  "Ramp Down Long Sharp 2 - 100 to 0%",
  "Ramp Down Medium Sharp 1 - 100 to 0%",
  "Ramp Down Medium Sharp 2 - 100 to 0%",
  "Ramp Down Short Sharp 1 - 100 to 0%",
  "Ramp Down Short Sharp 2 - 100 to 0%",
  "Ramp Up Long Smooth 1 - 0 to 100%",
  "Ramp Up Long Smooth 2 - 0 to 100%",
  "Ramp Up Medium Smooth 1 - 0 to 100%",
  "Ramp Up Medium Smooth 2 - 0 to 100%",
  "Ramp Up Short Smooth 1 - 0 to 100%",
  "Ramp Up Short Smooth 2 - 0 to 100%",
  "Ramp Up Long Sharp 1 - 0 to 100%",
  "Ramp Up Long Sharp 2 - 0 to 100%",
  "Ramp Up Medium Sharp 1 - 0 to 100%",
  "Ramp Up Medium Sharp 2 - 0 to 100%",
  "Ramp Up Short Sharp 1 - 0 to 100%",
  "Ramp Up Short Sharp 2 - 0 to 100%",
  "Ramp Down Long Smooth 1 - 50 to 0%",
  "Ramp Down Long Smooth 2 - 50 to 0%",
  "Ramp Down Medium Smooth 1 - 50 to 0%",
  "Ramp Down Medium Smooth 2 - 50 to 0%",
  "Ramp Down Short Smooth 1 - 50 to 0%",
  "Ramp Down Short Smooth 2 - 50 to 0%",
  "Ramp Down Long Sharp 1 - 50 to 0%",
  "Ramp Down Long Sharp 2 - 50 to 0%",
  "Ramp Down Medium Sharp 1 - 50 to 0%",
  "Ramp Down Medium Sharp 2 - 50 to 0%",
  "Ramp Down Short Sharp 1 - 50 to 0%",
  "Ramp Down Short Sharp 2 - 50 to 0%",
  "Ramp Up Long Smooth 1 - 0 to 50%",
  "Ramp Up Long Smooth 2 - 0 to 50%",
  "Ramp Up Medium Smooth 1 - 0 to 50%",
  "Ramp Up Medium Smooth 2 - 0 to 50%",
  "Ramp Up Short Smooth 1 - 0 to 50%",
  "Ramp Up Short Smooth 2 - 0 to 50%",
  "Ramp Up Long Sharp 1 - 0 to 50%",
  "Ramp Up Long Sharp 2 - 0 to 50%",
  "Ramp Up Medium Sharp 1 - 0 to 50%",
  "Ramp Up Medium Sharp 2 - 0 to 50%",
  "Ramp Up Short Sharp 1 - 0 to 50%",
  "Ramp Up Short Sharp 2 - 0 to 50%",
  "Long Buzz (runs until stopped) - 100%",
  "Smooth Hum 1 (no kick or brake) - 50%",
  "Smooth Hum 2 (no kick or brake) - 40%",
  "Smooth Hum 3 (no kick or brake) - 30%",
  "Smooth Hum 4 (no kick or brake) - 20%",
  "Smooth Hum 5 (no kick or brake) - 10%",
};

// Leave real-time mode (if in it) and get ready to trigger library effects.
void prepareForEffects() {
  drv.stop();
  if (buzzing) {
    drv.setRealtimeValue(0);
    buzzing = false;
  }
  drv.setMode(DRV2605_MODE_INTTRIG);
  setDrive(Drive::ClosedLoop);   // library effects are tuned for resonance
}

}  // namespace

bool playEffect(uint8_t n) {
  if (n < 1 || n > NUM_EFFECTS) return false;
  prepareForEffects();
  drv.setWaveform(0, n);
  drv.setWaveform(1, 0);    // end of sequence
  drv.go();
  lastEffect = n;
  return true;
}

bool playSequence(const uint8_t *steps, uint8_t count) {
  if (count == 0 || count > 8) return false;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t s = steps[i];
    bool isWait = s & 0x80;
    if (!isWait && (s < 1 || s > NUM_EFFECTS)) return false;
  }
  prepareForEffects();
  for (uint8_t i = 0; i < count; i++) {
    drv.setWaveform(i, steps[i]);
    if (!(steps[i] & 0x80)) lastEffect = steps[i];
  }
  if (count < 8) drv.setWaveform(count, 0);   // end of sequence
  drv.go();
  return true;
}

float setDriveFrequency(float hz) {
  if (hz <= 0) freqHz = 0;
  else freqHz = constrain(hz, MIN_DRIVE_HZ, MAX_DRIVE_HZ);
  if (buzzing) startBuzz(buzzStrength);
  return driveFrequency();
}

float driveFrequency() {
  // Report what open-loop LRA mode really produces (its period is quantized)
  if (freqHz >= OPEN_LOOP_LRA_MIN_HZ) {
    int period = (int)(1e6f / (freqHz * 98.46f) + 0.5f);
    return 1e6f / (period * 98.46f);
  }
  return freqHz;
}

void startBuzz(uint8_t strength) {
  if (strength > 127) strength = 127;
  buzzStrength = strength;
  drv.stop();
  drv.setRealtimeValue(0);

  if (freqHz == 0) {
    setDrive(Drive::ClosedLoop);                // chip tracks resonance
  } else if (freqHz >= OPEN_LOOP_LRA_MIN_HZ) {
    driveOpenLoopLRA(freqHz);                   // chip makes the frequency
    drive = Drive::OpenLoopLRA;
  } else {
    setDrive(Drive::Synth);                     // we draw the sine ourselves
  }

  drv.setMode(DRV2605_MODE_REALTIME);
  if (drive == Drive::Synth) {
    synthAmp = strength;
    synthStartUs = micros();
    synthLastUs = synthStartUs - SYNTH_STEP_US;
    synthLast = 0;
    buzzing = true;
    motorService();
  } else {
    drv.setRealtimeValue(strength);
    buzzing = true;
  }
}

void motorService() {
  if (!buzzing || drive != Drive::Synth) return;
  uint32_t now = micros();
  if (now - synthLastUs < SYNTH_STEP_US) return;
  synthLastUs = now;
  // Phase comes from the clock, so late updates never shift the frequency.
  float t = (now - synthStartUs) * 1e-6f;
  float ph = t * freqHz;
  ph -= (uint32_t)ph;
  int8_t v = (int8_t)lroundf(synthAmp * sinf(2 * PI * ph));
  if (v != synthLast) {
    drv.setRealtimeValue((uint8_t)v);           // signed: negative = reverse
    synthLast = v;
  }
}

void buzzFor(uint16_t ms, uint8_t strength) {
  startBuzz(strength);
  delay(ms);
  stopMotor();
}

bool isPlaying() {
  return buzzing || (drv.readRegister8(DRV2605_REG_GO) & 0x01);
}

void waitUntilDone(uint16_t timeoutMs) {
  unsigned long start = millis();
  while (isPlaying() && millis() - start < timeoutMs) delay(5);
}

void stopMotor() {
  prepareForEffects();
}

uint8_t currentEffect() {
  return lastEffect;
}

const char *effectName(uint8_t n) {
  return (n >= 1 && n <= NUM_EFFECTS) ? EFFECT_NAMES[n] : "";
}
