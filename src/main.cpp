// Mode menu over Serial (115200 baud).
//   1                    continuous buzz
//   2                    strong clicks in succession
//   3                    half-second buzz, then two clicks, repeating
//   4                    continuous buzz that adjusts its own strength to
//                        hold the target amplitude (needs the LSM6DSOX)
//   s                    re-run the resonance sweep (when nothing is running)
//   v + 0-100 + Enter    set strength in percent, e.g. v60 (works any time)
//   a + um + Enter       target amplitude for mode 4, micrometres
//                        peak-to-peak, e.g. a80
//   + / -                strength up / down by 10% (works any time)
//   r / e                start / end recording: streams every raw accelerometer
//                        sample as @RAW lines (works any time, motor on or off)
// Any other key ends the running mode and returns to this menu.
//
// Maximum-amplitude build: the frequency is not adjustable. At start-up a
// resonance sweep (resonance_sweep.cpp) measures, with the accelerometer,
// which drive moves the device the most, and every buzz uses that. Strength
// starts at 100%; the voltage limits in motor_init.cpp set what 100% means.
// While a mode runs, the LSM6DSOX (if connected) streams @ACC / @WAV readings.

#include <Arduino.h>
#include "motor_init.h"
#include "motor_effects.h"
#include "accel_meter.h"
#include "resonance_sweep.h"

// ---- Timing for the modes (tweak to taste) ----
const uint16_t CLICK_GAP_MS    = 150;   // mode 2: pause between clicks
const uint16_t PATTERN_BUZZ_MS = 500;   // mode 3: buzz length
const uint16_t DOUBLE_CLICK_MS = 100;   // mode 3: pause between the two clicks
const uint16_t PATTERN_GAP_MS  = 300;   // mode 3: pause before the pattern repeats

// ---- Amplitude ----
const uint16_t START_TARGET_UM = 80;    // mode 4 target, micrometres peak-to-peak
const float    SATURATION_G    = 15.5f; // accelerometer tops out at 16 g

uint16_t targetUm = START_TARGET_UM;

// ---- Strength ----
const uint8_t START_STRENGTH = 100;     // percent, at power-up
const uint8_t STRENGTH_STEP  = 10;      // percent, for + and -

uint8_t strengthPct = START_STRENGTH;
bool buzzActive = false;                // true while a mode wants a continuous buzz

void printMenu();

// ---- Strength helpers ----

// The driver's real-time scale runs 0-127.
uint8_t buzzLevel() {
  return (uint8_t)((strengthPct * 127 + 50) / 100);
}

// Clicks come from TI's effect library, which has them at fixed strengths,
// so pick the closest: Strong Click 100%, 80%, 60% or 30%.
uint8_t clickEffect() {
  if (strengthPct >= 90) return Effect::StrongClick;   // 1:  100%
  if (strengthPct >= 70) return 18;                    // 18: Strong Click 2 - 80%
  if (strengthPct >= 45) return 19;                    // 19: Strong Click 3 - 60%
  return 20;                                           // 20: Strong Click 4 - 30%
}

void buzzOn() {
  buzzActive = true;
  if (strengthPct > 0) startBuzz(buzzLevel());
  else stopMotor();
}

void buzzOff() {
  buzzActive = false;
  stopMotor();
}

void playClick() {
  if (strengthPct > 0) playEffect(clickEffect());
}

void playTwoClicks() {
  if (strengthPct == 0) return;
  const uint8_t c = clickEffect();
  const uint8_t twoClicks[] = {c, waitMs(DOUBLE_CLICK_MS), c};
  playSequence(twoClicks, 3);
}

void setStrength(int pct) {
  strengthPct = (uint8_t)constrain(pct, 0, 100);
  if (buzzActive) buzzOn();               // apply straight away if buzzing
  Serial.print("Strength: ");
  Serial.print(strengthPct);
  Serial.println("%");
  Serial.print("@STR ");                  // for the control panel
  Serial.println(strengthPct);
}

// Runs the sweep and makes its result the buzz frequency.
void runSweep() {
  float hz = setDriveFrequency(findBestDrive());
  Serial.print("@FREQ ");                 // for the control panel (0 = closed loop)
  Serial.println(hz, 1);
}

void setTarget(int um) {
  targetUm = (uint16_t)constrain(um, 1, 2000);
  Serial.print("Target amplitude: ");
  Serial.print(targetUm);
  Serial.println(" um");
}

// ---- Keystrokes ----

char numberCmd = 0;                     // 'v' or 'a' while a number is typed
String numberDigits;

// Reads whatever has been typed. Strength commands are handled here and
// never interrupt a mode. Returns:
//   'x'        stop the running mode (any other key while running)
//   '1'-'4'    start that mode (only when nothing is running)
//   's'        re-run the resonance sweep (only when nothing is running)
//   0          nothing to do
char processInput(bool running) {
  while (Serial.available()) {
    char c = Serial.read();

    if (numberCmd) {
      if (c >= '0' && c <= '9') {
        if (numberDigits.length() < 4) numberDigits += c;
        continue;
      }
      char cmd = numberCmd;
      numberCmd = 0;
      if (c == '\r' || c == '\n') {
        if (numberDigits.length() == 0) {
          Serial.println("Type the letter, a number, then Enter (e.g. v60, a80).");
        } else {
          int n = numberDigits.toInt();
          if (cmd == 'v') setStrength(n);
          if (cmd == 'a') setTarget(n);
        }
        continue;
      }
      // anything else cancels the number entry and is handled below
    }

    if (c == '\r' || c == '\n') continue;
    if (c == 'v' || c == 'V') { numberCmd = 'v'; numberDigits = ""; continue; }
    if (c == 'a' || c == 'A') { numberCmd = 'a'; numberDigits = ""; continue; }
    if (c == '+' || c == '=') { setStrength(strengthPct + STRENGTH_STEP); continue; }
    if (c == '-' || c == '_') { setStrength((int)strengthPct - STRENGTH_STEP); continue; }
    if (c == 'r' || c == 'R') { accelStreamStart(); continue; }
    if (c == 'e' || c == 'E') { accelStreamStop(); continue; }

    if (running) return 'x';
    if (c >= '1' && c <= '4') return c;
    if (c == 's' || c == 'S') return 's';
    if (c == ' ') continue;

    Serial.print("Unknown command: ");
    Serial.println(c);
    printMenu();
  }
  return 0;
}

bool stopRequested() {
  return processInput(true) == 'x';
}

// Waits ms milliseconds. Returns false if the mode should stop.
bool waitOrAbort(uint16_t ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (stopRequested()) return false;
    accelIdle(5);
  }
  return true;
}

// Waits for the current effect to finish. Returns false if the mode should stop.
bool waitForEffectOrAbort(uint16_t timeoutMs = 3000) {
  unsigned long start = millis();
  while (isPlaying() && millis() - start < timeoutMs) {
    if (stopRequested()) return false;
    accelIdle(5);
  }
  return true;
}

// ---- Modes ----

void modeContinuousBuzz() {
  buzzOn();
  while (!stopRequested()) accelIdle(5);
}

void modeStrongClicks() {
  while (true) {
    playClick();
    if (!waitForEffectOrAbort()) return;
    if (!waitOrAbort(CLICK_GAP_MS)) return;
  }
}

void modeBuzzThenClicks() {
  while (true) {
    buzzOn();
    if (!waitOrAbort(PATTERN_BUZZ_MS)) return;
    buzzOff();

    playTwoClicks();
    if (!waitForEffectOrAbort()) return;
    if (!waitOrAbort(PATTERN_GAP_MS)) return;
  }
}

// Buzzes at the set frequency and nudges the strength after each
// accelerometer reading until the measured amplitude reaches the target.
void modeHoldAmplitude() {
  AccelReading r;
  buzzOn();
  uint32_t waitFor = 2;                   // readings to skip after a change
  uint32_t lastSeq = 0;
  bool warnedMax = false, warnedSat = false;

  while (!stopRequested()) {
    accelIdle(5);
    if (!accelLatest(r)) {
      if (lastSeq == 0 && millis() % 3000 < 5)
        Serial.println("Waiting for the accelerometer (mode 4 needs it)...");
      continue;
    }
    if (r.seq == lastSeq) continue;
    lastSeq = r.seq;
    if (waitFor) { waitFor--; continue; } // first reading after a change is mixed

    if (r.maxG >= SATURATION_G && !warnedSat) {
      Serial.println("Warning: accelerometer is at its 16 g limit, so the amplitude");
      Serial.println("reading is too low. Lower the frequency or the target.");
      warnedSat = true;
    }

    float disp = r.dispFUm;               // main frequency only: ignores rattle
    float err = (disp - targetUm) / targetUm;
    if (fabsf(err) < 0.05f) continue;       // within 5%: leave it alone

    int pct = strengthPct;
    if (disp < 1.0f) pct += 10;             // nothing measurable yet: step up
    else {
      // Amplitude is roughly proportional to drive; move 70% of the way there.
      float want = strengthPct * (1 + 0.7f * ((targetUm / disp) - 1));
      pct = (int)lroundf(want);
      if (pct == strengthPct) pct += (disp < targetUm) ? 1 : -1;
    }
    pct = constrain(pct, 1, 100);

    if (pct == strengthPct) {
      if (strengthPct == 100 && !warnedMax) {
        Serial.print("Full strength only reaches ");
        Serial.print(disp, 1);
        Serial.print(" um at ");
        Serial.print(r.freq, 0);
        Serial.println(" Hz. Try a frequency nearer resonance (f0), a higher");
        Serial.println("LRA_PEAK_V / supply voltage, or a lighter mount.");
        warnedMax = true;
      }
      continue;
    }
    warnedMax = false;
    setStrength(pct);
    waitFor = 1;
  }
}

// ---- Menu ----

void printMenu() {
  Serial.println();
  Serial.print("Strength: ");
  Serial.print(strengthPct);
  Serial.println("%   (v + number + Enter to set, e.g. v60; + / - to step)");
  float f = driveFrequency();
  Serial.print("Drive: ");
  if (f == 0) Serial.print("closed loop, tracking resonance");
  else { Serial.print("open loop at "); Serial.print(f, 1); Serial.print(" Hz"); }
  if (bestDispUm() > 0) {
    Serial.print(" (sweep: ");
    Serial.print(bestDispUm(), 1);
    Serial.print(" um p-p at ");
    Serial.print(resonanceHz(), 1);
    Serial.print(" Hz)");
  }
  Serial.println("   (s to re-run the sweep)");
  Serial.print("Target amplitude: ");
  Serial.print(targetUm);
  Serial.println(" um p-p   (a + number + Enter)");
  Serial.println("Select a mode (any other key stops it):");
  Serial.println("  1  Continuous buzz");
  Serial.println("  2  Strong clicks");
  Serial.println("  3  Buzz, then two clicks (repeating)");
  Serial.println("  4  Hold target amplitude (auto strength, needs accelerometer)");
  Serial.println("r / e  start / end recording the accelerometer");
}

void runMode(char c) {
  Serial.print("Running mode ");
  Serial.print(c);
  Serial.println(". Press any key to stop.");

  accelStart();
  switch (c) {
    case '1': modeContinuousBuzz(); break;
    case '2': modeStrongClicks();   break;
    case '3': modeBuzzThenClicks(); break;
    case '4': modeHoldAmplitude();  break;
  }
  accelStop();

  buzzOff();
  Serial.println("Stopped.");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) delay(10);

  if (!initMotor()) {
    while (true) delay(1000);
  }
  Serial.println("DRV2605L ready.");
  initAccel();
  runSweep();
  Serial.print("@STR ");
  Serial.println(strengthPct);
  printMenu();
}

void loop() {
  accelService();                       // keeps recording while idle
  char c = processInput(false);
  if (c >= '1' && c <= '4') {
    runMode(c);
    printMenu();
  } else if (c == 's') {
    runSweep();
    printMenu();
  }
}
