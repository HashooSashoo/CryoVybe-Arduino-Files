#include <Arduino.h>

// ================== RTD HARDWARE SELECT ==================
// Uncomment to read the RTD through an Adafruit MAX31865 breakout (recommended).
// Leave commented to read the RTD through a voltage divider on RTD_PIN.
// #define USE_MAX31865

#ifdef USE_MAX31865
#include <Adafruit_MAX31865.h>   // PlatformIO: adafruit/Adafruit MAX31865 library
#endif

// ================== SHARED ==================
const float ADC_MAX   = 4095.0;  // 12-bit ADC
const int   N_SAMPLES = 64;      // readings averaged per measurement

// ================== NTC THERMISTOR ==================
const int   NTC_PIN       = A0;
const float NTC_R_FIXED   = 100.0;    // measure yours with a multimeter
const float NTC_R_NOMINAL = 13000.0;  // thermistor resistance at 25 °C
const float NTC_BETA      = 3892.0;   // from the thermistor's datasheet
const float NTC_T_NOMINAL = 298.15;   // 25 °C in Kelvin

// ================== RTD (YAGEO Nexensos M310) ==================
const float RTD_R0 = 1000.0;          // 1000 for Pt1000, 100 for Pt100 (resistance at 0 °C)
const float CVD_A  = 3.9083e-3;       // IEC 60751 Callendar-Van Dusen coefficients
const float CVD_B  = -5.775e-7;

#ifdef USE_MAX31865
const int   MAX_CS   = 10;            // chip-select pin, hardware SPI for the rest
const float MAX_RREF = 4300.0;        // reference resistor on the board: 4300 (Pt1000) or 430 (Pt100)
Adafruit_MAX31865 rtdAmp(MAX_CS);
#else
const int   RTD_PIN     = A0;          // same pin as the NTC: connect one sensor at a time
const float RTD_R_FIXED = 10000.0;    // measure yours. 10k keeps Pt1000 current ~0.3 mA at 3.3 V
                                      // (datasheet max for Pt1000) so self-heating stays < ~0.04 K
#endif

// ================== MENU STATE ==================
enum Mode { MODE_NTC, MODE_RTD, MODE_BOTH };
Mode mode = MODE_RTD;                 // starts in RTD mode
bool useFahrenheit = true;            // your original script printed °F
bool debugRaw = true;                 // print raw ADC + resistance (toggle with 'd')

// ---------------------------------------------------------------
float readAdcAvg(int pin) {
  long sum = 0;
  for (int i = 0; i < N_SAMPLES; i++) sum += analogRead(pin);
  return sum / (float)N_SAMPLES;
}

// Sensor on the GND side of the divider, fixed resistor to Vcc
float dividerResistance(float raw, float rFixed) {
  if (raw <= 0.5 || raw >= ADC_MAX - 0.5) return NAN;  // open or shorted
  return rFixed * raw / (ADC_MAX - raw);
}

// ---------------- NTC ----------------
float readNtcC() {
  float r = dividerResistance(readAdcAvg(NTC_PIN), NTC_R_FIXED);
  if (isnan(r)) return NAN;
  float invT = 1.0 / NTC_T_NOMINAL + log(r / NTC_R_NOMINAL) / NTC_BETA;
  return 1.0 / invT - 273.15;
}

// ---------------- RTD ----------------
// Inverse of R = R0 (1 + A t + B t^2). Exact for t >= 0 °C; below 0 the
// neglected C term contributes < 0.001 °C at -8 °C, so fine for your range.
float rtdResistanceToC(float r) {
  float ratio = r / RTD_R0;
  float disc  = CVD_A * CVD_A - 4.0 * CVD_B * (1.0 - ratio);
  if (disc < 0) return NAN;
  return (-CVD_A + sqrt(disc)) / (2.0 * CVD_B);
}

float readRtdC() {
#ifdef USE_MAX31865
  float c = rtdAmp.temperature(RTD_R0, MAX_RREF);
  uint8_t fault = rtdAmp.readFault();
  if (fault) {
    rtdAmp.clearFault();
    return NAN;
  }
  return c;
#else
  float r = dividerResistance(readAdcAvg(RTD_PIN), RTD_R_FIXED);
  if (isnan(r)) return NAN;
  return rtdResistanceToC(r);
#endif
}

// ---------------- OUTPUT ----------------
void printReading(const char *label, float c) {
  if (isnan(c)) {
    Serial.print(label);
    Serial.println(": sensor error (open/short/fault)");
    return;
  }
  float value = useFahrenheit ? c * 9.0 / 5.0 + 32.0 : c;
  Serial.print('>');                  // Teleplot format, as in your original
  Serial.print(label);
  Serial.print(':');
  Serial.println(value, 2);
}

// Prints the raw ADC average, the voltage it implies (3.3 V ref), and the
// resistance the divider math gives. Use it to see what the pin actually sees.
void printDebug(const char *label, int pin, float rFixed) {
  float raw = readAdcAvg(pin);
  Serial.print(label);
  Serial.print(" raw=");
  Serial.print(raw, 1);
  Serial.print("  V=");
  Serial.print(raw / ADC_MAX * 3.3, 3);
  Serial.print("  R=");
  float r = dividerResistance(raw, rFixed);
  if (isnan(r)) Serial.println("out of range (pin at 0 V or 3.3 V)");
  else          Serial.println(r, 1);
}

void printMenu() {
  Serial.println();
  Serial.println(F("------ Temperature sensor menu ------"));
  Serial.println(F("  1  NTC thermistor"));
  Serial.println(F("  2  RTD (M310)"));
  Serial.println(F("  3  Both"));
  Serial.println(F("  c  Celsius     f  Fahrenheit"));
  Serial.println(F("  d  Debug on/off (raw ADC + resistance)"));
  Serial.println(F("  m  Show this menu"));
#ifdef USE_MAX31865
  Serial.println(F("  (RTD via MAX31865)"));
#else
  Serial.println(F("  (RTD via voltage divider)"));
#endif
  Serial.println(F("-------------------------------------"));
}

void handleSerial() {
  while (Serial.available()) {
    char ch = Serial.read();
    switch (ch) {
      case '1': mode = MODE_NTC;  Serial.println(F("Mode: NTC"));  break;
      case '2': mode = MODE_RTD;  Serial.println(F("Mode: RTD"));  break;
      case '3': mode = MODE_BOTH; Serial.println(F("Mode: both")); break;
      case 'c': case 'C': useFahrenheit = false; Serial.println(F("Units: °C")); break;
      case 'f': case 'F': useFahrenheit = true;  Serial.println(F("Units: °F")); break;
      case 'd': case 'D':
        debugRaw = !debugRaw;
        Serial.println(debugRaw ? F("Debug: on") : F("Debug: off"));
        break;
      case 'm': case 'M': case '?': printMenu(); break;
      default: break;                 // ignore newlines and anything else
    }
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
#ifdef USE_MAX31865
  rtdAmp.begin(MAX31865_2WIRE);       // M310 has 2 leads; use 3WIRE/4WIRE if you extend it that way
#endif
  delay(500);
  printMenu();
}

void loop() {
  handleSerial();

  if (mode == MODE_NTC || mode == MODE_BOTH) printReading("ntc", readNtcC());
  if (mode == MODE_RTD || mode == MODE_BOTH) printReading("rtd", readRtdC());

  if (debugRaw) {
    if (mode == MODE_NTC || mode == MODE_BOTH) printDebug("ntc", NTC_PIN, NTC_R_FIXED);
#ifndef USE_MAX31865
    if (mode == MODE_RTD || mode == MODE_BOTH) printDebug("rtd", RTD_PIN, RTD_R_FIXED);
#endif
  }

  delay(100);
}
