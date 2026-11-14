// DRV2605L effect browser: Adafruit Feather + Adafruit DRV2605L breakout
//
// Wiring (Feather -> DRV2605L breakout):
//   3V  -> VIN
//   GND -> GND
//   SDA -> SDA
//   SCL -> SCL
//   LRA motor wires -> the two motor pads (+ and -), either way round
//   (use USB for +, this provides 5V, and motor needs more than 3 V to properly calibrate)
//   Leave the IN pin unconnected.
//
// Open the Serial Monitor at 115200 baud, then type:
//   a number 1-123 and Enter   play that effect
//   n                          play the next effect
//   p                          play the previous effect
//   r                          replay the current effect
//   a                          play all 123 in order (any key stops)
//   l                          list all effect names
//   m                          maximum-strength buzz for 1 second
//   c                          re-run auto-calibration
//   s                          stop the motor
// On power-up it calibrates to your motor, then gives one strong buzz.

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_DRV2605.h>

Adafruit_DRV2605 drv;

const uint8_t NUM_EFFECTS = 123;

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
  "Long Buzz (runs until stopped, press s) - 100%",
  "Smooth Hum 1 (no kick or brake) - 50%",
  "Smooth Hum 2 (no kick or brake) - 40%",
  "Smooth Hum 3 (no kick or brake) - 30%",
  "Smooth Hum 4 (no kick or brake) - 20%",
  "Smooth Hum 5 (no kick or brake) - 10%",
};

// ---- Motor settings: typical coin LRA. Check your motor's datasheet. ----
// If the motor rattles or sounds harsh, lower LRA_RATED_VRMS a little.
const float ERM_RATED_V = 3.0;   // rated voltage, volts RMS (coin LRAs: usually 1.8-2.0)
const float ERM_PEAK_V  = 3.7;   // max voltage for short "kick" at start, volts peak

// DRV2605L register addresses (from the TI datasheet)
const uint8_t REG_STATUS   = 0x00;
const uint8_t REG_MODE     = 0x01;
const uint8_t REG_RTP      = 0x02;
const uint8_t REG_GO       = 0x0C;
const uint8_t REG_RATED_V  = 0x16;
const uint8_t REG_CLAMP_V  = 0x17;
const uint8_t REG_CAL_COMP = 0x18;
const uint8_t REG_CAL_BEMF = 0x19;
const uint8_t REG_FEEDBACK = 0x1A;
const uint8_t REG_CONTROL1 = 0x1B;
const uint8_t REG_CONTROL2 = 0x1C;
const uint8_t REG_CONTROL3 = 0x1D;
const uint8_t REG_CONTROL4 = 0x1E;

// ERM-mode lookup for BLANKING_TIME and IDISS_TIME (Table 25)
const float ERM_TIME_LUT[4] = {45e-6f, 75e-6f, 150e-6f, 225e-6f};

void configureMotor() {
  // ERM mode first (bit 7 = 0), 4x brake, medium loop gain, BEMF gain 2
  drv.writeRegister8(REG_FEEDBACK, 0x2A);

  // Read the timing fields straight from the chip
  uint8_t c1 = drv.readRegister8(REG_CONTROL1);
  uint8_t c2 = drv.readRegister8(REG_CONTROL2);

  uint8_t driveTime  = c1 & 0x1F;          // DRIVE_TIME[4:0]
  uint8_t blankIdx   = (c2 >> 2) & 0x03;   // BLANKING_TIME[1:0]
  uint8_t idissIdx   = c2 & 0x03;          // IDISS_TIME[1:0]

  float tDrive = (driveTime * 0.2f + 1.0f) * 1e-3f;   // ERM: 0.2 ms steps + 1 ms
  float tBlank = ERM_TIME_LUT[blankIdx];
  float tIdiss = ERM_TIME_LUT[idissIdx];

  // Rated voltage (datasheet formula for LRA, default 300 us sample time)
  int rated_voltage = (int)(ERM_RATED_V / 21.18e-3 + 0.5f);
  int clamp_value = (int)(ERM_PEAK_V * (tDrive + tIdiss + tBlank) / (21.64e-3f * (tDrive - 300e-6f)) + 0.5f);
  drv.writeRegister8(REG_RATED_V, constrain(rated_voltage, 0, 255));
  drv.writeRegister8(REG_CLAMP_V, constrain(clamp_value, 0, 255));

  // Startup boost on, keep whatever drive time is already set
  drv.writeRegister8(REG_CONTROL1, c1 | 0x80);

  // writing here to set sample time, blinking time, and IDSS time manually
   drv.writeRegister8(REG_CONTROL2, 0xF5);

  // Closed-loop ERM: clear ERM_OPEN_LOOP (bit 5), keep the other bits
  uint8_t c3 = drv.readRegister8(REG_CONTROL3);
  drv.writeRegister8(REG_CONTROL3, c3 & ~0x20);
}

// Let the chip measure the motor and tune itself to it (about 1 second).
bool calibrate() {
  Serial.println("Calibrating to your motor (hold it still, about 1 s)...");
  configureMotor();
  drv.writeRegister8(REG_CONTROL4, 0x30);    // longest, most accurate calibration
  drv.writeRegister8(REG_MODE, 0x07);        // auto-calibration mode
  drv.writeRegister8(REG_GO, 0x01);
  unsigned long start = millis();
  while ((drv.readRegister8(REG_GO) & 0x01) && (millis() - start) < 3000) {

    Serial.println(millis() - start);
    Serial.println(drv.readRegister8(REG_GO));

    delay(10);
  }

  Serial.println(drv.readRegister8(REG_GO));

  bool ok = !(drv.readRegister8(REG_STATUS) & 0x08);
  drv.setMode(DRV2605_MODE_INTTRIG);
  if (ok) {
    Serial.print("Calibration OK. Compensation 0x");
    Serial.print(drv.readRegister8(REG_CAL_COMP), HEX);
    Serial.print(", back-EMF 0x");
    Serial.println(drv.readRegister8(REG_CAL_BEMF), HEX);
  } else {
    Serial.println("Calibration FAILED. Check the motor wires, and that the motor");
    Serial.println("is attached to something rather than loose. Effects will still");
    Serial.println("play, but may be weaker. Type c to try again.");
  }
  return ok;
}

// Strongest possible output: real-time mode at full scale.
void maxBuzz(uint16_t ms) {
  Serial.println("Maximum-strength buzz");
  drv.stop();
  drv.setMode(DRV2605_MODE_REALTIME);
  drv.setRealtimeValue(127);
  delay(ms);
  drv.setRealtimeValue(0);
  drv.setMode(DRV2605_MODE_INTTRIG);
}

uint8_t current = 0;     // last effect played
String numberBuffer;     // digits typed so far

// Start one effect. Returns straight away so the motor can be stopped mid-effect.
void playEffect(uint8_t n) {
  if (n < 1 || n > NUM_EFFECTS) {
    Serial.println("Effects go from 1 to 123.");
    return;
  }
  current = n;
  drv.stop();
  drv.setWaveform(0, n);
  drv.setWaveform(1, 0);    // end of sequence
  drv.go();
  Serial.print("Effect ");
  Serial.print(n);
  Serial.print(": ");
  Serial.println(EFFECT_NAMES[n]);
}

bool effectRunning() {
  return drv.readRegister8(DRV2605_REG_GO) & 0x01;
}

void stopMotor() {
  drv.stop();
  Serial.println("Stopped.");
}

void listEffects() {
  for (uint8_t i = 1; i <= NUM_EFFECTS; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.println(EFFECT_NAMES[i]);
  }
}

// Play every effect in order. Any key stops it.
void playAll() {
  Serial.println("Playing all 123 effects. Press any key to stop.");
  for (uint8_t n = 1; n <= NUM_EFFECTS; n++) {
    playEffect(n);
    unsigned long start = millis();
    // Wait for the effect to finish (max 3 s, so effect 118 doesn't run forever)
    while (effectRunning() && millis() - start < 3000) {
      if (Serial.available()) { while (Serial.available()) Serial.read(); stopMotor(); return; }
      delay(5);
    }
    drv.stop();
    // Short gap between effects so you can tell them apart
    start = millis();
    while (millis() - start < 800) {
      if (Serial.available()) { while (Serial.available()) Serial.read(); stopMotor(); return; }
      delay(5);
    }
  }
  Serial.println("Done.");
}

void printMenu() {
  Serial.println();
  Serial.println("Type 1-123 + Enter to play an effect | n next | p previous | r replay");
  Serial.println("a play all | l list names | m max buzz | c calibrate | s stop");
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) delay(10);

  // Some Feathers switch power to the I2C pins on and off; make sure it's on.
#if defined(PIN_I2C_POWER)
  pinMode(PIN_I2C_POWER, OUTPUT);
  digitalWrite(PIN_I2C_POWER, HIGH);
#endif
#if defined(TFT_I2C_POWER)
  pinMode(TFT_I2C_POWER, OUTPUT);
  digitalWrite(TFT_I2C_POWER, HIGH);
#endif
  delay(10);

  if (!drv.begin()) {
    Serial.println("DRV2605L not found. Check the 4 wires (3V, GND, SDA, SCL).");
    while (true) delay(1000);
  }

  drv.useERM();                       // driving an LRA, not a coin motor
  drv.selectLibrary(1);               // library 6 = effects tuned for LRAs
  calibrate();                        // full-strength settings + tune to the motor
  drv.setMode(DRV2605_MODE_INTTRIG);  // play effects when we call go()
  Serial.println("DRV2605L ready.");

  playEffect(14);                     // one strong buzz so you know it's working
  printMenu();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();

  if (c >= '0' && c <= '9') {               // collect digits until Enter
    if (numberBuffer.length() < 3) numberBuffer += c;
    return;
  }
  if (c == '\r' || c == '\n') {             // Enter: play the typed number
    if (numberBuffer.length() > 0) {
      playEffect(numberBuffer.toInt());
      numberBuffer = "";
    }
    return;
  }

  numberBuffer = "";                        // any letter cancels a half-typed number
  switch (c) {
    case 'n': case 'N': playEffect(current >= NUM_EFFECTS ? 1 : current + 1); break;
    case 'p': case 'P': playEffect(current <= 1 ? NUM_EFFECTS : current - 1); break;
    case 'r': case 'R': if (current) playEffect(current); else Serial.println("Nothing played yet."); break;
    case 'a': case 'A': playAll(); printMenu(); break;
    case 'l': case 'L': listEffects(); printMenu(); break;
    case 'm': case 'M': maxBuzz(1000); break;
    case 'c': case 'C': calibrate(); break;
    case 's': case 'S': stopMotor(); break;
    case ' ': break;
    default:
      Serial.print("Unknown key: ");
      Serial.println(c);
      printMenu();
  }
}
