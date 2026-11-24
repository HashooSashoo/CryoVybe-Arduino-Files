#pragma once

// Finds the drive that gives the most displacement on the device as it is
// actually mounted, using the accelerometer:
//   1. buzzes at full strength in open loop from SWEEP_MIN_HZ to
//      SWEEP_MAX_HZ, coarse steps then fine steps around the best one
//   2. buzzes at full strength in closed loop (the chip tracks resonance)
//   3. keeps closed loop unless open loop at the best frequency was clearly
//      stronger, since closed loop follows the resonance if it drifts
//      (warming up, cooling down, a different strap tension)
// Takes about 20 s, blocking. The motor is stopped afterwards.
// Returns the frequency to pass to setDriveFrequency(): 0 = closed loop.
// Without an accelerometer it returns 0 straight away.
float findBestDrive();

// Result of the last findBestDrive(), for the menu
float bestDriveHz();     // 0 = closed loop
float bestDispUm();      // main-frequency amplitude measured there, um peak-to-peak
float resonanceHz();     // frequency the motor actually vibrated at
