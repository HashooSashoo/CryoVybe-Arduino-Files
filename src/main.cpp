// Mode menu over Serial (115200 baud).
//   1  continuous buzz
//   2  strong clicks in succession
//   3  half-second buzz, then two clicks, repeating
// Any key ends the running mode and returns to this menu.

#include <Arduino.h>
#include "motor_init.h"
#include "motor_effects.h"

// ---- Timing for the modes (tweak to taste) ----
const uint16_t CLICK_GAP_MS    = 150;   // mode 2: pause between clicks
const uint16_t PATTERN_BUZZ_MS = 500;   // mode 3: buzz length
const uint16_t DOUBLE_CLICK_MS = 100;   // mode 3: pause between the two clicks
const uint16_t PATTERN_GAP_MS  = 300;   // mode 3: pause before the pattern repeats

// ---- Keystroke helpers ----

void flushInput() {
  delay(20);                                  // let the Enter after a menu key arrive
  while (Serial.available()) Serial.read();
}

bool keyPressed() {
  if (!Serial.available()) return false;
  flushInput();
  return true;
}

// Waits ms milliseconds. Returns false if a key was pressed.
bool waitOrAbort(uint16_t ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    if (keyPressed()) return false;
    delay(5);
  }
  return true;
}

// Waits for the current effect to finish. Returns false if a key was pressed.
bool waitForEffectOrAbort(uint16_t timeoutMs = 3000) {
  unsigned long start = millis();
  while (isPlaying() && millis() - start < timeoutMs) {
    if (keyPressed()) return false;
    delay(5);
  }
  return true;
}

// ---- Modes ----

void modeContinuousBuzz() {
  startBuzz(127);
  while (!keyPressed()) delay(5);
}

void modeStrongClicks() {
  while (true) {
    playEffect(Effect::StrongClick);
    if (!waitForEffectOrAbort()) return;
    if (!waitOrAbort(CLICK_GAP_MS)) return;
  }
}

void modeBuzzThenClicks() {
  const uint8_t twoClicks[] = {Effect::StrongClick, waitMs(DOUBLE_CLICK_MS), Effect::StrongClick};
  while (true) {
    startBuzz(127);
    if (!waitOrAbort(PATTERN_BUZZ_MS)) return;
    stopMotor();

    playSequence(twoClicks, 3);
    if (!waitForEffectOrAbort()) return;
    if (!waitOrAbort(PATTERN_GAP_MS)) return;
  }
}

// ---- Menu ----

void printMenu() {
  Serial.println();
  Serial.println("Select a mode (any key stops it):");
  Serial.println("  1  Continuous buzz");
  Serial.println("  2  Strong clicks");
  Serial.println("  3  Buzz, then two clicks (repeating)");
}

void runMode(char c) {
  flushInput();   // so the Enter after the number doesn't end the mode instantly
  Serial.print("Running mode ");
  Serial.print(c);
  Serial.println(". Press any key to stop.");

  switch (c) {
    case '1': modeContinuousBuzz(); break;
    case '2': modeStrongClicks();   break;
    case '3': modeBuzzThenClicks(); break;
  }

  stopMotor();
  Serial.println("Stopped.");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) delay(10);

  if (!initMotor()) {
    while (true) delay(1000);
  }
  Serial.println("DRV2605L ready.");
  printMenu();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();

  if (c == '\r' || c == '\n' || c == ' ') return;

  if (c >= '1' && c <= '3') {
    runMode(c);
  } else {
    Serial.print("Unknown mode: ");
    Serial.println(c);
  }
  printMenu();
}
