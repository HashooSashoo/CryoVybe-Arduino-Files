// Motor setup: I2C power, chip detection, LRA configuration (closed loop,
// open loop at a set frequency, or direct drive), and auto-calibration.
//
// Wiring (Feather -> DRV2605L breakout):
//   3V  -> VIN      (or USB -> VIN for more voltage headroom; the breakout
//                    takes up to 5 V and the LRA can then be driven harder)
//   GND -> GND
//   SDA -> SDA
//   SCL -> SCL
//   LRA wires -> the two motor pads (an LRA has no polarity)
//   Leave the IN pin unconnected.

#include <Arduino.h>
#include <Wire.h>
#include "motor_init.h"

Adafruit_DRV2605 drv;

// ---- LRA settings: fill these in from YOUR motor's datasheet ----
// Resonant frequency: the frequency the LRA vibrates hardest at.
static const float LRA_RESONANT_HZ = 170.0f;
// Rated drive voltage, volts RMS.
static const float LRA_RATED_VRMS  = 2.0f;
// Maximum peak voltage the motor may see. This is also the full-scale
// output in the open-loop modes used for off-resonance frequencies, so it
// sets the most amplitude you can get there. Never set it above the
// motor's absolute maximum or above the supply voltage on VIN.
static const float LRA_PEAK_V      = 2.9f;

namespace {

// DRV2605L register addresses (TI datasheet)
constexpr uint8_t REG_STATUS    = 0x00;
constexpr uint8_t REG_MODE      = 0x01;
constexpr uint8_t REG_GO        = 0x0C;
constexpr uint8_t REG_RATED_V   = 0x16;
constexpr uint8_t REG_CLAMP_V   = 0x17;
constexpr uint8_t REG_CAL_COMP  = 0x18;
constexpr uint8_t REG_CAL_BEMF  = 0x19;
constexpr uint8_t REG_FEEDBACK  = 0x1A;
constexpr uint8_t REG_CONTROL1  = 0x1B;
constexpr uint8_t REG_CONTROL2  = 0x1C;
constexpr uint8_t REG_CONTROL3  = 0x1D;
constexpr uint8_t REG_CONTROL4  = 0x1E;
constexpr uint8_t REG_OL_PERIOD = 0x20;   // LRA open-loop period, 98.46 us steps
constexpr uint8_t REG_LRA_PER   = 0x22;   // measured LRA period (read only)

constexpr uint8_t MODE_AUTOCAL  = 0x07;

constexpr uint8_t FB_LRA        = 0x80;   // FEEDBACK: N_ERM_LRA
constexpr uint8_t C3_ERM_OL     = 0x20;   // CONTROL3: ERM_OPEN_LOOP
constexpr uint8_t C3_RTP_UNSIGN = 0x08;   // CONTROL3: DATA_FORMAT_RTP (0 = signed)
constexpr uint8_t C3_LRA_OL     = 0x01;   // CONTROL3: LRA_OPEN_LOOP

constexpr float PERIOD_STEP_US  = 98.46f;

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

void setBits(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t v = drv.readRegister8(reg);
  drv.writeRegister8(reg, (v & ~mask) | (value & mask));
}

// Full closed-loop LRA setup, written before auto-calibration.
void configureClosedLoopLRA() {
  // FEEDBACK: LRA (bit 7), brake factor 3x, loop gain high, BEMF gain 2.
  // (Auto-calibration overwrites BEMF_GAIN with its own result.)
  drv.writeRegister8(REG_FEEDBACK, 0xBA);

  // CONTROL1: startup boost on, DRIVE_TIME = half the resonant period
  // (LRA: 0.1 ms steps + 0.5 ms). This is the first guess the chip uses
  // to lock on to resonance.
  int driveTime = (int)((500.0f / LRA_RESONANT_HZ - 0.5f) / 0.1f + 0.5f);
  drv.writeRegister8(REG_CONTROL1, 0x80 | constrain(driveTime, 0, 31));

  // CONTROL2: bidirectional input, brake stabilizer, SAMPLE_TIME = 300 us,
  // BLANKING_TIME = 1, IDISS_TIME = 1.
  drv.writeRegister8(REG_CONTROL2, 0xF5);

  // Closed-loop LRA formulas (300 us sample time)
  float k = 1.0f - (4 * 300e-6f + 300e-6f) * LRA_RESONANT_HZ;
  int rated = (int)(LRA_RATED_VRMS * sqrtf(k > 0 ? k : 0) / 20.58e-3f + 0.5f);
  int clamp = (int)(LRA_PEAK_V / 21.96e-3f + 0.5f);
  drv.writeRegister8(REG_RATED_V, constrain(rated, 0, 255));
  drv.writeRegister8(REG_CLAMP_V, constrain(clamp, 0, 255));

  // CONTROL3: closed loop, signed real-time values, keep the other bits
  setBits(REG_CONTROL3, C3_LRA_OL | C3_ERM_OL | C3_RTP_UNSIGN, 0);
}

}  // namespace

// ---- Drive configurations ----
// These only flip mode bits, so the auto-calibration results are kept.

void driveClosedLoopLRA() {
  setBits(REG_FEEDBACK, FB_LRA, FB_LRA);
  setBits(REG_CONTROL3, C3_LRA_OL | C3_ERM_OL, 0);
}

float driveOpenLoopLRA(float hz) {
  int period = (int)(1e6f / (hz * PERIOD_STEP_US) + 0.5f);
  period = constrain(period, 1, 127);
  drv.writeRegister8(REG_OL_PERIOD, period);
  setBits(REG_FEEDBACK, FB_LRA, FB_LRA);
  setBits(REG_CONTROL3, C3_LRA_OL | C3_ERM_OL, C3_LRA_OL);
  return 1e6f / (period * PERIOD_STEP_US);
}

void driveOpenLoopDirect() {
  // ERM open loop with signed real-time values is a plain bidirectional
  // driver: the output voltage is (value / 127) x the clamp voltage, either
  // polarity. That lets us draw any waveform, including below 80 Hz.
  setBits(REG_FEEDBACK, FB_LRA, 0);
  setBits(REG_CONTROL3, C3_LRA_OL | C3_ERM_OL, C3_ERM_OL);
}

bool calibrateMotor() {
  Serial.println("Calibrating to the LRA (keep it mounted and still, about 1 s)...");
  configureClosedLoopLRA();
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
    uint8_t per = drv.readRegister8(REG_LRA_PER);
    if (per) {
      Serial.print("Measured LRA resonance: ~");
      Serial.print(1e6f / (per * PERIOD_STEP_US), 0);
      Serial.print(" Hz (LRA_RESONANT_HZ is set to ");
      Serial.print(LRA_RESONANT_HZ, 0);
      Serial.println(" Hz)");
    }
  } else {
    Serial.println(timedOut ? "Calibration timed out." : "Calibration FAILED.");
    Serial.println("Check the LRA wires, LRA_RESONANT_HZ, and that the motor is mounted, not loose.");
  }
  return ok;
}

bool initMotor() {
  powerI2C();

  if (!drv.begin()) {
    Serial.println("DRV2605L not found. Check the 4 wires (3V, GND, SDA, SCL).");
    return false;
  }

  drv.useLRA();
  drv.selectLibrary(6);   // library 6 is the LRA library (1-5 are ERM)
  calibrateMotor();
  drv.setMode(DRV2605_MODE_INTTRIG);
  return true;
}
