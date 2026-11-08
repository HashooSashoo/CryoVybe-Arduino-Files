#include <Arduino.h>

// Send any key (+ Enter) over serial to start logging; send again to stop.
// Output is CSV: copy it from the serial monitor into a .csv file.

// ---- Thermistor setup ----
const int   PIN       = A0;
const float R_FIXED   = 100.0;    // measure yours with a multimeter (see note: 100 ohms is very low)
const float R_NOMINAL = 13000.0;  // thermistor resistance at 25 °C
const float BETA      = 3892.0;   // from the thermistor's datasheet
const float T_NOMINAL = 298.15;   // 25 °C in Kelvin
const float ADC_MAX   = 4095.0;   // 12-bit ADC

// ---- Logging setup ----
const unsigned long INTERVAL_MS = 100;  // time between readings
const bool TELEPLOT = true;  // also print ">temp_F:" lines for Teleplot graphing;
                             // set false for clean copy-paste CSV

bool logging = false;
unsigned long startMs = 0;
unsigned long lastMs = 0;

float readThermistorF() {
  long sum = 0;
  for (int i = 0; i < 16; i++) sum += analogRead(PIN);  // average to reduce noise
  float raw = sum / 16.0;

  float r = R_FIXED * raw / (ADC_MAX - raw);            // thermistor resistance (thermistor on GND side)
  float invT = 1.0 / T_NOMINAL + log(r / R_NOMINAL) / BETA;
  float tempC = 1.0 / invT - 273.15;                     // Kelvin to °C
  return tempC * 9.0 / 5.0 + 32.0;                       // °C to °F
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  Serial.println("Send any key to start logging.");
}

void loop() {
  // Toggle logging on any incoming character, then clear the rest of the line
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    logging = !logging;
    if (logging) {
      startMs = millis();
      lastMs = startMs - INTERVAL_MS;  // take the first reading right away
      Serial.println("time_s,temp_F");  // CSV header
    } else {
      Serial.println("--- stopped. Send any key to start again. ---");
    }
  }

  if (logging && millis() - lastMs >= INTERVAL_MS) {
    lastMs += INTERVAL_MS;
    float tempF = readThermistorF();

    Serial.print((millis() - startMs) / 1000.0, 2);
    Serial.print(",");
    Serial.println(tempF, 2);

    if (TELEPLOT) {
      Serial.print(">temp_F:");
      Serial.println(tempF, 2);
    }
  }
}
