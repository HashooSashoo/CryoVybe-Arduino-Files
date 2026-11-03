#include <Arduino.h>

const int   PIN       = A0;
const float R_FIXED   = 100.0;  // measure yours with a multimeter
const float R_NOMINAL = 13000.0;  // thermistor resistance at 25 °C
const float BETA      = 3892.0;   // from the thermistor's datasheet
const float T_NOMINAL = 298.15;   // 25 °C in Kelvin

float readThermistorC() {
  long sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(PIN);  // average to reduce noise
  float raw = sum / 16.0;

  float r = R_FIXED * raw / (4095.0 - raw);              // thermistor resistance
  float invT = 1.0 / T_NOMINAL + log(r / R_NOMINAL) / BETA;
  return 1.0 / invT - 273.15;                            // Kelvin → °C
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
}

void loop() {
  Serial.print(">thermistor:");
  Serial.println(readThermistorC());
  delay(100);
}
