// Motor setup: I2C power, chip detection, closed-loop ERM configuration,
// and auto-calibration. An open-loop configure function would go here later.
//
// Wiring (Feather -> DRV2605L breakout):
//   3V  -> VIN
//   GND -> GND
//   SDA -> SDA
//   SCL -> SCL
//   Motor wires -> the two motor pads (+ and -)
//   Leave the IN pin unconnected.

#include <Arduino.h>
#include <Wire.h>
#include "motor_init.h"

Adafruit_DRV2605 drv;

// ---- Motor settings: check your motor's datasheet ----
// If the motor rattles or sounds harsh, lower ERM_RATED_V a little.
static const float ERM_RATED_V = 3.0f;   // rated (average) voltage, volts
static const float ERM_PEAK_V  = 3.7f;   // overdrive clamp for the startup kick, volts peak

namespace {

// DRV2605L register addresses (TI datasheet)
constexpr uint8_t REG_STATUS   = 0x00;
constexpr uint8_t REG_MODE     = 0x01;
constexpr uint8_t REG_GO       = 0x0C;
constexpr uint8_t REG_RATED_V  = 0x16;
constexpr uint8_t REG_CLAMP_V  = 0x17;
constexpr uint8_t REG_CAL_COMP = 0x18;
constexpr uint8_t REG_CAL_BEMF = 0x19;
constexpr uint8_t REG_FEEDBACK = 0x1A;
constexpr uint8_t REG_CONTROL1 = 0x1B;
constexpr uint8_t REG_CONTROL2 = 0x1C;
constexpr uint8_t REG_CONTROL3 = 0x1D;
constexpr uint8_t REG_CONTROL4 = 0x1E;

constexpr uint8_t MODE_AUTOCAL = 0x07;

// ERM-mode lookup for BLANKING_TIME and IDISS_TIME
constexpr float ERM_TIME_LUT[4] = {45e-6f, 75e-6f, 150e-6f, 225e-6f};

// Some Feathers switch power to the I2C pins on and off; make sure it's on.
void powerI2C() {
#if defined(PIN_I2C_POWER)
  pinMode(PIN_I2C_POWER, OUTPUT);
  digitalWrite(PIN_I2C_POWER, HIGH);
#endif
#if defined(TFT_I2C_POWER)
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
#endif
  delay(10);
}

void configureClosedLoopERM() {
  // FEEDBACK: ERM (bit 7 = 0), brake factor 3x, loop gain high, BEMF gain 2.
  // (Auto-calibration overwrites BEMF_GAIN with its own result.)
  drv.writeRegister8(REG_FEEDBACK, 0x2A);

  // CONTROL1: startup boost on, keep the existing DRIVE_TIME
  uint8_t c1 = drv.readRegister8(REG_CONTROL1) | 0x80;
  drv.writeRegister8(REG_CONTROL1, c1);

  // CONTROL2: bidirectional input, brake stabilizer, SAMPLE_TIME = 300 us,
  // BLANKING_TIME = 1, IDISS_TIME = 1.
  // Written BEFORE the clamp math below, because the clamp depends on these timings.
  const uint8_t c2 = 0xF5;
  drv.writeRegister8(REG_CONTROL2, c2);

  float tDrive = ((c1 & 0x1F) * 0.2f + 1.0f) * 1e-3f;   // ERM: 0.2 ms steps + 1 ms
  float tBlank = ERM_TIME_LUT[(c2 >> 2) & 0x03];
  float tIdiss = ERM_TIME_LUT[c2 & 0x03];

  // Closed-loop ERM formulas (300 us sample time)
  int rated = (int)(ERM_RATED_V / 21.18e-3f + 0.5f);
  int clamp = (int)(ERM_PEAK_V * (tDrive + tIdiss + tBlank) /
                    (21.64e-3f * (tDrive - 300e-6f)) + 0.5f);
  drv.writeRegister8(REG_RATED_V, constrain(rated, 0, 255));
  drv.writeRegister8(REG_CLAMP_V, constrain(clamp, 0, 255));

  // CONTROL3: closed loop (clear ERM_OPEN_LOOP, bit 5), keep the other bits
  uint8_t c3 = drv.readRegister8(REG_CONTROL3);
  drv.writeRegister8(REG_CONTROL3, c3 & ~0x20);
}

}  // namespace

bool calibrateMotor() {
  Serial.println("Calibrating to the motor (hold it still, about 1 s)...");
  configureClosedLoopERM();
  drv.writeRegister8(REG_CONTROL4, 0x30);    // longest, most accurate calibration
  drv.writeRegister8(REG_MODE, MODE_AUTOCAL);
  drv.writeRegister8(REG_GO, 0x01);

  unsigned long start = millis();
  while ((drv.readRegister8(REG_GO) & 0x01) && millis() - start < 3000) {
    delay(10);
  }

  bool timedOut = drv.readRegister8(REG_GO) & 0x01;
  bool ok = !timedOut && !(drv.readRegister8(REG_STATUS) & 0x08);
  drv.stop();
  drv.setMode(DRV2605_MODE_INTTRIG);

  if (ok) {
    Serial.print("Calibration OK. Compensation 0x");
    Serial.print(drv.readRegister8(REG_CAL_COMP), HEX);
    Serial.print(", back-EMF 0x");
    Serial.println(drv.readRegister8(REG_CAL_BEMF), HEX);
  } else {
    Serial.println(timedOut ? "Calibration timed out." : "Calibration FAILED.");
    Serial.println("Check the motor wires and that the motor is mounted, not loose.");
  }
  return ok;
}

bool initMotor() {
  powerI2C();

  if (!drv.begin()) {
    Serial.println("DRV2605L not found. Check the 4 wires (3V, GND, SDA, SCL).");
    return false;
  }

  drv.useERM();
  drv.selectLibrary(1);   // ERM library A (libraries 1-5 are ERM, 6 is LRA)
  calibrateMotor();
  drv.setMode(DRV2605_MODE_INTTRIG);
  return true;
}
