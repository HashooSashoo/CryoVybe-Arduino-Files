#pragma once
#include <stdint.h>

// Live vibration measurement with an LSM6DSOX accelerometer.
//
// Wiring: the LSM6DSOX shares the I2C bus with the DRV2605L
// (3V, GND, SDA, SCL), or plugs into the Feather's STEMMA QT port.
// Mount it rigidly on the same object as the motor.
//
// While measuring, it samples at ~1.67 kHz using the sensor's FIFO and,
// every ~0.3 s, prints two lines over Serial:
//   @ACC f=203.4 pk=1.234 rms=0.872 max=1.402 disp=7.4 axis=Z fs=1667
//   @WAV 12,-34,56,...      (160 samples of the strongest axis, milli-g)
// The CryoVybe control panel (ui/cryovybe-control.html) reads these.

// Finds the sensor and sets it up. Call after initMotor().
// Returns false if no LSM6DSOX is found; everything else still works.
bool initAccel();

void accelStart();             // begin measuring (call when a mode starts)
void accelStop();              // stop measuring (call when a mode ends)
void accelService();           // collect samples; call as often as you can
void accelIdle(uint16_t ms);   // like delay(ms), but keeps measuring
