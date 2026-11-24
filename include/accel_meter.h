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
//   @ACC f=203.4 pk=1.234 rms=0.872 max=1.402 disp=7.4 dispf=7.1 axis=Z fs=1667
//   disp  = peak displacement from x = a/(2 pi f)^2, with a = rms * sqrt(2)
//           of the whole signal (harmonics and noise push it up)
//   dispf = the same, but a is only the main frequency's amplitude (from
//           the FFT), so it ignores harmonics and noise. Both in um, peak.
//   @WAV 12,-34,56,...      (160 samples of the strongest axis, milli-g)
// The CryoVybe control panel (ui/cryovybe-control.html) reads these.
//
// Recording: accelStreamStart() additionally streams every raw sample
// (all three axes, raw counts; multiply by lsb for g) until accelStreamStop():
//   @RAWSTART fs=1666.7 lsb=0.000488
//   @RAW x,y,z;x,y,z;...    (up to 32 samples per line)
//   @RAWGAP                 (samples were lost here)
//   @RAWEND
// Streaming works whether or not a mode is running.

// Finds the sensor and sets it up. Call after initMotor().
// Returns false if no LSM6DSOX is found; everything else still works.
bool initAccel();

void accelStart();             // begin measuring (call when a mode starts)
void accelStop();              // stop measuring (call when a mode ends)
void accelService();           // collect samples; call as often as you can
void accelIdle(uint16_t ms);   // like delay(ms), but keeps measuring

void accelStreamStart();       // start streaming raw samples (recording)
void accelStreamStop();        // stop streaming raw samples

// The most recent @ACC reading. seq goes up by one with each new reading.
// dispUm / dispFUm: peak displacement (half of peak-to-peak), micrometres,
// from the whole signal / from the main frequency only (see @ACC above).
struct AccelReading {
  float freq, peakG, maxG, dispUm, dispFUm;
  uint32_t seq;
};
bool accelLatest(AccelReading &r);   // false if no sensor or no reading yet
