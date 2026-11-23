// LSM6DSOX vibration meter: frequency and amplitude of what the motor is doing.
// See accel_meter.h for wiring and the output format.

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_LSM6DSOX.h>
#include "accel_meter.h"
#include "motor_effects.h"   // motorService(): keeps a low-frequency sine running

namespace {

Adafruit_LSM6DSOX lsm;

// ---- Settings ----
constexpr uint16_t N           = 512;      // samples per reading (~0.3 s)
constexpr uint16_t WAVE_POINTS = 160;      // samples sent for the UI's waveform plot
constexpr float    MIN_FREQ_HZ = 20.0f;    // ignore anything slower than this
constexpr float    MIN_RMS_G   = 0.005f;   // below this there is no real vibration
constexpr float    G_PER_LSB   = 0.000488f;// +/-16 g range
constexpr uint8_t  MAX_WORDS_PER_CALL = 40;
constexpr uint8_t  RAW_BATCH   = 32;       // raw samples per @RAW line

// ---- LSM6DSOX registers ----
constexpr uint8_t REG_FIFO_CTRL3 = 0x09;   // batch data rates
constexpr uint8_t REG_FIFO_CTRL4 = 0x0A;   // FIFO mode
constexpr uint8_t REG_CTRL3_C    = 0x12;
constexpr uint8_t REG_FIFO_STAT1 = 0x3A;
constexpr uint8_t REG_FREQ_FINE  = 0x63;   // factory ODR trim
constexpr uint8_t REG_FIFO_OUT   = 0x78;   // tag + 6 data bytes
constexpr uint8_t FIFO_BYPASS     = 0x00;
constexpr uint8_t FIFO_CONTINUOUS = 0x06;
constexpr uint8_t BDR_XL_1667HZ   = 0x08;
constexpr uint8_t TAG_ACCEL       = 0x02;

uint8_t addr = 0x6A;
bool present = false;
bool enabled = false;         // FIFO running (measuring || streaming)
bool measuring = false;       // a mode is running
bool streaming = false;       // the control panel is recording
float fs = 1666.67f;          // actual sample rate, corrected with REG_FREQ_FINE

float ax[N], ay[N], az[N];    // acceleration, g
float re[N], im[N];           // FFT working space
uint16_t count = 0;

AccelReading latest = {0, 0, 0, 0, 0};

int16_t rawBuf[RAW_BATCH][3]; // raw samples waiting to be sent
uint8_t rawCount = 0;

// ---- I2C helpers ----
void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void readBytes(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (size_t)n);
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.available() ? Wire.read() : 0;
}

uint8_t readReg(uint8_t reg) {
  uint8_t v;
  readBytes(reg, &v, 1);
  return v;
}

// Empty the FIFO and start filling it again.
void restartFifo() {
  writeReg(REG_FIFO_CTRL4, FIFO_BYPASS);
  writeReg(REG_FIFO_CTRL3, BDR_XL_1667HZ);
  writeReg(REG_FIFO_CTRL4, FIFO_CONTINUOUS);
  count = 0;
}

// Turn the FIFO on or off to match what is wanted.
void updateEnabled() {
  bool want = measuring || streaming;
  if (want && !enabled) restartFifo();
  if (!want && enabled) { writeReg(REG_FIFO_CTRL4, FIFO_BYPASS); count = 0; }
  enabled = want;
}

// Send the waiting raw samples as one @RAW line.
void flushRaw() {
  if (!rawCount) return;
  static char line[8 + RAW_BATCH * 20];
  int p = snprintf(line, sizeof line, "@RAW ");
  for (uint8_t i = 0; i < rawCount; i++) {
    p += snprintf(line + p, sizeof line - p, i ? ";%d,%d,%d" : "%d,%d,%d",
                  rawBuf[i][0], rawBuf[i][1], rawBuf[i][2]);
  }
  Serial.write(line, p);
  Serial.write('\n');
  rawCount = 0;
}

// ---- Analysis ----
void fft(float *r, float *i_, uint16_t n) {
  for (uint16_t i = 1, j = 0; i < n; i++) {
    uint16_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) {
      float t = r[i]; r[i] = r[j]; r[j] = t;
      t = i_[i]; i_[i] = i_[j]; i_[j] = t;
    }
  }
  for (uint16_t len = 2; len <= n; len <<= 1) {
    float ang = -2.0f * PI / len;
    float wr = cosf(ang), wi = sinf(ang);
    for (uint16_t s = 0; s < n; s += len) {
      float cr = 1, ci = 0;
      for (uint16_t k = 0; k < len / 2; k++) {
        uint16_t a = s + k, b = s + k + len / 2;
        float tr = r[b] * cr - i_[b] * ci;
        float ti = r[b] * ci + i_[b] * cr;
        r[b] = r[a] - tr;  i_[b] = i_[a] - ti;
        r[a] += tr;        i_[a] += ti;
        float ncr = cr * wr - ci * wi;
        ci = cr * wi + ci * wr;
        cr = ncr;
      }
    }
  }
}

float removeMeanRms(float *a) {
  float mean = 0;
  for (uint16_t i = 0; i < N; i++) mean += a[i];
  mean /= N;
  float sumSq = 0;
  for (uint16_t i = 0; i < N; i++) { a[i] -= mean; sumSq += a[i] * a[i]; }
  return sqrtf(sumSq / N);
}

void analyzeAndReport() {
  float rx = removeMeanRms(ax), ry = removeMeanRms(ay), rz = removeMeanRms(az);
  float *dom = ax; char axis = 'X'; float rms = rx;
  if (ry > rms) { dom = ay; axis = 'Y'; rms = ry; }
  if (rz > rms) { dom = az; axis = 'Z'; rms = rz; }

  // Main frequency: Hann window, FFT, peak bin with interpolation
  float freq = 0;
  if (rms >= MIN_RMS_G) {
    for (uint16_t i = 0; i < N; i++) {
      float w = 0.5f * (1 - cosf(2 * PI * i / (N - 1)));
      re[i] = dom[i] * w;
      im[i] = 0;
    }
    fft(re, im, N);
    for (uint16_t i = 0; i < N / 2; i++) re[i] = sqrtf(re[i] * re[i] + im[i] * im[i]);
    uint16_t kMin = (uint16_t)ceilf(MIN_FREQ_HZ * N / fs);
    if (kMin < 1) kMin = 1;
    uint16_t k = kMin;
    for (uint16_t i = kMin; i < N / 2 - 1; i++) if (re[i] > re[k]) k = i;
    float a = re[k - 1], b = re[k], c = re[k + 1];
    float denom = a - 2 * b + c;
    float shift = (denom != 0) ? 0.5f * (a - c) / denom : 0;
    freq = (k + shift) * fs / N;
  }

  float peakG = rms * sqrtf(2.0f);
  float maxAbs = 0;
  for (uint16_t i = 0; i < N; i++) if (fabsf(dom[i]) > maxAbs) maxAbs = fabsf(dom[i]);
  float w = 2 * PI * freq;
  float dispUm = (freq > 0) ? peakG * 9.80665f / (w * w) * 1e6f : 0;

  latest.freq = freq; latest.peakG = peakG; latest.maxG = maxAbs;
  latest.dispUm = dispUm; latest.seq++;

  Serial.print("@ACC f=");    Serial.print(freq, 1);
  Serial.print(" pk=");       Serial.print(peakG, 3);
  Serial.print(" rms=");      Serial.print(rms, 3);
  Serial.print(" max=");      Serial.print(maxAbs, 3);
  Serial.print(" disp=");     Serial.print(dispUm, 1);
  Serial.print(" axis=");     Serial.print(axis);
  Serial.print(" fs=");       Serial.println(fs, 0);

  Serial.print("@WAV ");
  for (uint16_t i = 0; i < WAVE_POINTS; i++) {
    if (i) Serial.print(',');
    Serial.print((int)lroundf(dom[i] * 1000));
  }
  Serial.println();
}

}  // namespace

// ---------------------------------------------------------------------------

bool initAccel() {
  const uint8_t candidates[] = {0x6A, 0x6B};
  for (uint8_t a : candidates) {
    if (lsm.begin_I2C(a)) { addr = a; present = true; break; }
  }
  if (!present) {
    Serial.println("@ACCEL missing");
    Serial.println("LSM6DSOX not found: measurements off (the motor still works).");
    return false;
  }

  lsm.setAccelRange(LSM6DS_ACCEL_RANGE_16_G);
  lsm.setAccelDataRate(LSM6DS_RATE_1_66K_HZ);
  lsm.setGyroDataRate(LSM6DS_RATE_SHUTDOWN);
  writeReg(REG_CTRL3_C, readReg(REG_CTRL3_C) | 0x44);   // block data update + auto-increment
  writeReg(REG_FIFO_CTRL4, FIFO_BYPASS);
  Wire.setClock(400000);                                // fast I2C for both chips

  // Each chip's clock is slightly off; the factory trim tells us by how much.
  int8_t fine = (int8_t)readReg(REG_FREQ_FINE);
  fs = 1666.67f * (1.0f + 0.0015f * fine);

  Serial.print("@ACCEL ok fs=");
  Serial.println(fs, 1);
  return true;
}

void accelStart() {
  if (!present) return;
  measuring = true;
  updateEnabled();
}

void accelStop() {
  if (!present) return;
  measuring = false;
  updateEnabled();
}

void accelStreamStart() {
  if (!present) { Serial.println("@RAWERR no accelerometer"); return; }
  if (!streaming) rawCount = 0;
  streaming = true;
  Serial.print("@RAWSTART fs=");
  Serial.print(fs, 2);
  Serial.print(" lsb=");
  Serial.println(G_PER_LSB, 6);
  updateEnabled();
}

void accelStreamStop() {
  if (streaming) flushRaw();
  streaming = false;
  Serial.println("@RAWEND");
  if (present) updateEnabled();
}

void accelService() {
  if (!present || !enabled) return;

  uint8_t st[2];
  readBytes(REG_FIFO_STAT1, st, 2);
  uint16_t avail = ((uint16_t)(st[1] & 0x03) << 8) | st[0];
  bool overrun = st[1] & 0x48;          // FIFO_OVR_IA or FIFO_OVR_LATCHED
  if (overrun) {                        // samples were lost; start a clean reading
    restartFifo();
    if (streaming) { flushRaw(); Serial.println("@RAWGAP"); }
    return;
  }

  if (avail > MAX_WORDS_PER_CALL) avail = MAX_WORDS_PER_CALL;
  while (avail--) {
    motorService();
    uint8_t b[7];
    readBytes(REG_FIFO_OUT, b, 7);
    if ((b[0] >> 3) != TAG_ACCEL) continue;
    int16_t x = (int16_t)(b[2] << 8 | b[1]);
    int16_t y = (int16_t)(b[4] << 8 | b[3]);
    int16_t z = (int16_t)(b[6] << 8 | b[5]);
    if (streaming) {
      rawBuf[rawCount][0] = x; rawBuf[rawCount][1] = y; rawBuf[rawCount][2] = z;
      if (++rawCount == RAW_BATCH) flushRaw();
    }
    ax[count] = x * G_PER_LSB;
    ay[count] = y * G_PER_LSB;
    az[count] = z * G_PER_LSB;
    if (++count == N) {
      analyzeAndReport();
      count = 0;
    }
  }
}

void accelIdle(uint16_t ms) {
  unsigned long start = millis();
  uint32_t lastService = micros() - 1000;
  do {
    // Poll the sensor about once a millisecond, the motor as often as possible
    if (micros() - lastService >= 1000) { lastService = micros(); accelService(); }
    motorService();
  } while (millis() - start < ms);
}

bool accelLatest(AccelReading &r) {
  if (!present || latest.seq == 0) return false;
  r = latest;
  return true;
}
