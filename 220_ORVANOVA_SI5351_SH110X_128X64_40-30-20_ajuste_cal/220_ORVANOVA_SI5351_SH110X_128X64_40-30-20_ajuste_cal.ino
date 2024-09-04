#include <SPI.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Rotary.h>
#include <si5351.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define TUNESTEP_PIN 4
#define BAND40_PIN 10
#define BAND30_PIN 11
#define BAND20_PIN 9
#define PIN_TRX A0
#define METER_TX A1
#define METER_RX A2
#define METER_VDC A3

unsigned long IF = 2000000;  // Variable global
long CAL = 134416;  // Variable global

Rotary r = Rotary(3, 2);
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);
Si5351 si5351(0x60);

unsigned long freq = 100000, freqmem, freqbanda = 5000000, freqlcd, freqout, freqold = 0, fstep;
unsigned long ultimoTiempoGuardado = 0;
const unsigned long intervaloGuardado = 30000;
unsigned int smvalTX, smvalRX;
byte encoder = 1, stp = 2, n = 1, y, x;
bool modo = 0, save = 1, hiddenMode = false;
int banda = 0, count = 0;
float volt = 0;

ISR(PCINT2_vect) {
  char result = r.process();
  if (hiddenMode) {
    if (result == DIR_CW) {
      CAL = min(CAL + 1, 200000L);
    } else if (result == DIR_CCW) {
      CAL = max(CAL - 1, 1L);
    }
    displayCal();
  } else {
    if (result == DIR_CW) set_frequency(-1);
    else if (result == DIR_CCW) set_frequency(1);
  }
}

void set_frequency(short dir) {
  if (encoder == 1) {
    if (dir == 1) freq = min(freq + fstep, 1100000UL);
    if (dir == -1) freq = max(freq - fstep, 100000UL);
  }
}

void setup() {
  pinMode(TUNESTEP_PIN, INPUT_PULLUP);
  unsigned long startTime = millis();
  
  while (millis() - startTime < 5000) {
    if (digitalRead(TUNESTEP_PIN) != LOW) {
      break;
    }
  }
  
  if (millis() - startTime >= 5000 && digitalRead(TUNESTEP_PIN) == LOW) {
    hiddenMode = true;
    displayCal();
    while (digitalRead(TUNESTEP_PIN) == LOW); // Wait for button release
    while (true) {
      if (digitalRead(TUNESTEP_PIN) == LOW) {
        EEPROM.put(0, CAL);
        hiddenMode = false;
        break;
      }
    }
  }

  delay(200);
  Wire.begin();
  delay(250);
  display.begin(0x3C, true);
  display.setTextColor(SH110X_WHITE);
  display.clearDisplay();
  display.display();
  delay(200);

  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(PIN_TRX, INPUT);
  pinMode(BAND40_PIN, INPUT);
  pinMode(BAND30_PIN, INPUT);
  pinMode(BAND20_PIN, INPUT);

  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
  si5351.output_enable(SI5351_CLK0, 1);
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 1);
  delay(200);
  si5351.set_correction(CAL, SI5351_PLL_INPUT_XO);
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  setstep();
  EEPROM.get(0, freq);
  freqold = freq;
  check_band();
  tunegen();
}

void loop() {
  unsigned long tiempoActual = millis();
  EEPROM.get(0, freqmem);
  if (freq != freqmem && tiempoActual - ultimoTiempoGuardado >= intervaloGuardado) {
    EEPROM.put(0, freq);
    ultimoTiempoGuardado = tiempoActual;
    save = 1;
    count++;
  }

  if (freqold != freq) {
    save = 0;
    tunegen();
    freqold = freq;
  }

  if (digitalRead(TUNESTEP_PIN) == LOW) {
    delay(500);
    if (digitalRead(TUNESTEP_PIN) == LOW) {
      modo = !modo;
      tunegen();
    } else {
      setstep();
    }
    displayfreq();
    delay(500);
  }

  check_band();
  displayfreq();
  sgnalread();
}

void tunegen() {
  freqout = freqbanda + freq + (modo ? -IF : IF);
  si5351.set_freq(freqout * 100ULL, SI5351_CLK0);
  si5351.set_freq(1000000 * 100ULL, SI5351_CLK2);
}

void displayfreq() {
  freqlcd = freqbanda + freq;
  char buffer[15];
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(5, 19);
  sprintf(buffer, "%2lu,%03lu.%03lu", freqlcd / 1000000, (freqlcd % 1000000) / 1000, freqlcd % 1000);
  display.print(buffer);
  display.setTextColor(SH110X_WHITE);
  display.drawLine(1, 43, 126, 43, SH110X_WHITE);
  display.drawLine(12, 53, 126, 53, SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 48);
  display.print("S");

  for (int i = 1; i <= 12; i++) {
    if (i % 2 != 0) display.setCursor(i * 9 + 4, 55);
    display.drawPixel(i * 9 + 4, 48, SH110X_WHITE);
  }

  display.setCursor(5, 0);
  volt = (analogRead(METER_VDC) / 1024.0) * 20.25;
  if (volt < 10) display.setCursor(11, 0);
  else display.setCursor(5, 0);
  display.print(volt);
  display.print("V");

  display.setTextSize(2);
  display.setCursor(89, 0);
  display.print(modo ? "LSB" : "USB");
  display.setTextSize(1);
  display.setCursor(5, 9);
  display.print(save ? "SAVE" : "    ");
  display.display();
}

void displayCal() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("CAL Mode");
  display.setCursor(0, 20);
  display.print("CAL:");
  display.setCursor(50, 20);
  display.print(CAL);
  display.display();
}

void setstep() {
  switch (stp) {
    case 1: stp = 2; fstep = 100; break;
    case 2: stp = 3; fstep = 1000; break;
    case 3: stp = 4; fstep = 10000; break;
    case 4: stp = 5; fstep = 100000; break;
    case 5: stp = 1; fstep = 10; break;
  }
}

void sgnalread() {
  unsigned int val = digitalRead(PIN_TRX) == HIGH ? analogRead(METER_RX) : analogRead(METER_TX);
  x = map(val, 0, 724, 1, 12);
  if (x > 12) x = 12;
  for (int i = 1; i <= x; i++) {
    display.fillRect(i * 9 + 4, 44, 7, 9, SH110X_WHITE);
  }
  display.display();
}

void check_band() {
  if (digitalRead(BAND40_PIN) == HIGH && banda != 1) {
    banda = 1;
    freqbanda = 6400000;
    delay(300);
    modo = 1;
    tunegen();
  }
  if (digitalRead(BAND30_PIN) == HIGH && banda != 2) {
    banda = 2;
    freqbanda = 9400000;
    delay(300);
    modo = 0;
    tunegen();
  }
  if (digitalRead(BAND20_PIN) == HIGH && banda != 3) {
    banda = 3;
    freqbanda = 13400000;
    delay(300);
    modo = 0;
    tunegen();
  }
}
