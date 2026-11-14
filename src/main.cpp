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
