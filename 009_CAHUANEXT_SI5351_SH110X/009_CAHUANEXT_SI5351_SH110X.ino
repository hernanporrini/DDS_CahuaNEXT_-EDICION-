#include <SPI.h>
#include <EEPROM.h>               
#include <Wire.h>                 
#include <Rotary.h>               //https://github.com/brianlow/Rotary
#include <si5351.h>               //https://github.com/etherkit/Si5351Arduino
#include <Adafruit_GFX.h>         //https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SH110X.h>

#define IF         2000000      //el valor de la frecuencia del filtro a cristal
#define tunestep   4         //Este pin es el pulsador central del rotary, se usa para cambiar el paso
#define band80     12        //Este pin se usa para que el micro sepa que se selecciono la banda de 80
#define band40     11        //Este pin se usa para que el micro sepa que se selecciono la banda de 40
#define band20     10        //Este pin se usa para que el micro sepa que se selecciono la banda de 20
#define pinTRX     A0        //Este pin se usa para que el micro sepa si esta en TX o RX
#define meterRX    A1        //pin de entrada analogica del indicador de recepcion
#define meterTX    A2        //pin de entrada analogica del indicador de potencia de salida

Rotary r = Rotary(3, 2);     //definicion del rotary switch en los pines 2 y 3

Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);
Si5351 si5351(0x60);                                             //definicion y direccion i2c del Si5351 en 0x60

unsigned long freq=100000, freqmem, freqbanda=5000000, freqlcd, freqout, freqold=0, fstep;
unsigned long ultimoTiempoGuardado = 0;
const unsigned long intervaloGuardado = 30000;
long cal = 137416;
unsigned int smvalTX;
unsigned int smvalRX;
byte encoder = 1;
byte stp=2;
byte n = 1;
byte y, x;
bool modo = 0;
bool save = 1;
int banda=0;                 //banda 1=80m, 2=40m, 3=20m
int count=0;
ISR(PCINT2_vect) 
  {
  char result = r.process();
  if (result == DIR_CW) set_frequency(-1);
  else if (result == DIR_CCW) set_frequency(1);
  }

void set_frequency(short dir) 
  {
  if (encoder == 1) 
    {                         
    if (dir == 1) 
    {freq = freq + fstep; if (freq>=500000) freq=500000;}
    if (dir == -1) 
    {freq = freq - fstep; if (freq<=100000) freq=100000;}
    }
  }

void setup() 
  {
  delay(200); // 
  Wire.begin();
  delay(250); // espera a que el OLED encienda
  display.begin(0x3C, true); // direccion 0x3C por default
  display.setTextColor(SH110X_WHITE);
  display.clearDisplay();
  display.display();
  delay(200); // 
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(tunestep, INPUT_PULLUP);
  pinMode(pinTRX, INPUT_PULLUP);
  pinMode(band80, INPUT);
  pinMode(band40, INPUT);
  pinMode(band20, INPUT);
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
  si5351.output_enable(SI5351_CLK0, 0);                  //1 - HABILITADO / 0 - DESHABILITADO
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 1);
  delay(200);
  si5351.set_correction(cal, SI5351_PLL_INPUT_XO);
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  setstep();
  EEPROM.get(0,freq);
  freqold = freq;
  if (digitalRead(band80) == HIGH && banda!=1) {freqbanda= 3500000;banda=1;modo=0;}
  if (digitalRead(band40) == HIGH && banda!=2) {freqbanda= 6800000;banda=2;modo=0;}
  if (digitalRead(band20) == HIGH && banda!=3) {freqbanda=13800000;banda=3;modo=1;}
  tunegen();  
  }

void loop() 
  {
  unsigned long tiempoActual=millis();
  EEPROM.get(0,freqmem);
  if (freq != freqmem && tiempoActual-ultimoTiempoGuardado >= intervaloGuardado)
  {
    EEPROM.put(0,freq);
    ultimoTiempoGuardado=tiempoActual;
    save=1;
    count++;
  }
  if (freqold != freq) 
    {
    save=0;
    tunegen();
    freqold = freq;
    
    }
  if (digitalRead(tunestep) == LOW) 
    {
    delay(500);
    if (digitalRead(tunestep) == LOW) 
      {
      modo = !modo;
      tunegen();
      }
    else 
      setstep();
      
    displayfreq();
    delay(500);
    }

  if (digitalRead(band80) == HIGH && banda!=1) 
    {
    banda=1;
    freqbanda=3500000;
    delay(300);
    modo=0;
    tunegen();
    }
  if (digitalRead(band40) == HIGH && banda!=2) 
    {
    banda=2;
    freqbanda=6800000;
    delay(300);
    modo=0;
    tunegen();
    }
  if (digitalRead(band20) == HIGH && banda!=3) 
    {
    banda=3;
    freqbanda=13800000;
    delay(300);
    modo=1;
    tunegen();
    }
  displayfreq();
  sgnalread();
  }

void tunegen() 
  {
  if(modo)  freqout= freqbanda + freq + IF ; 
  else      freqout= freqbanda + freq - IF ;
  si5351.set_freq(freqout * 100ULL, SI5351_CLK2); 
  //si5351.set_freq(1000000 * 100ULL, SI5351_CLK2); 
  }

void displayfreq() 
  {
  freqlcd= freqbanda + freq ;
  unsigned int m =  freqlcd / 1000000;
  unsigned int k = (freqlcd % 1000000) / 1000;
  unsigned int h = (freqlcd % 1000) / 1;
  display.clearDisplay();
  display.setTextSize(2);
  char buffer[15] = "";
  display.setCursor(5, 20); sprintf(buffer, "%2d,%003d.%003d", m, k, h);
  display.print(buffer);
  display.setTextColor(SH110X_WHITE);
  display.drawLine(0, 17, 127, 17, SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(59, 0);
  display.print("STEP");
  display.setCursor(50, 8);
  //if (stp == 5) {display.print("100kHz");display.drawLine( 41, 38,  51, 38, SH110X_WHITE); display.drawLine( 41, 37,  51, 37, SH110X_WHITE); display.drawLine( 41, 36,  51, 36, SH110X_WHITE);} 
  if (stp == 4) {display.print(" 10kHz");display.drawLine( 53, 38,  63, 38, SH110X_WHITE); display.drawLine( 53, 37,  63, 37, SH110X_WHITE); display.drawLine( 53, 36,  63, 36, SH110X_WHITE);} 
  if (stp == 3) {display.print("  1kHz");display.drawLine( 65, 38,  75, 38, SH110X_WHITE); display.drawLine( 65, 37,  75, 37, SH110X_WHITE); display.drawLine( 65, 36,  75, 36, SH110X_WHITE);} 
  if (stp == 2) {display.print("100 Hz");display.drawLine( 89, 38,  99, 38, SH110X_WHITE); display.drawLine( 89, 37,  99, 37, SH110X_WHITE); display.drawLine( 89, 36,  99, 36, SH110X_WHITE);}
  if (stp == 1) {display.print(" 10 Hz");display.drawLine(101, 38, 111, 38, SH110X_WHITE); display.drawLine(101, 37, 111, 37, SH110X_WHITE); display.drawLine(101, 36, 111, 36, SH110X_WHITE);}
  display.setTextSize(2);
  display.setCursor(91, 0);
  if (modo) display.print("USB");
  else display.print("LSB"); 
  if(digitalRead(pinTRX)==LOW)
    {
    display.setCursor(0, 0); 
    display.print("TX");
    }
  else
    {
    display.setCursor(0, 0); 
    display.print("RX");
    }
  display.setTextSize(1);
  //display.setCursor(28, 8);
  //display.print(count); 
  display.setCursor(28, 0); 
  if(save) display.print("SAVE"); else display.print("    ");
  display.display();
  }

void setstep() 
  {
  switch (stp) 
    {
    case 1: stp = 2; fstep = 100; break;
    case 2: stp = 3; fstep = 1000; break;
    case 3: stp = 4; fstep = 10000; break;
    case 4: stp = 1; fstep = 10; break;
    }
  }


void sgnalread() 
  {
  if(digitalRead(pinTRX)==HIGH)
    {
    smvalRX = analogRead(meterRX); 
     x = map(smvalRX, 0, 1024, 1, 26); 
    if (x > 26) x = 26;
    switch (x) 
      {
      case 26: display.fillRect(125, 38, 3, 26, SH110X_WHITE);
      case 25: display.fillRect(120, 39, 3, 25, SH110X_WHITE);
      case 24: display.fillRect(115, 40, 3, 24, SH110X_WHITE);
      case 23: display.fillRect(110, 41, 3, 23, SH110X_WHITE);
      case 22: display.fillRect(105, 42, 3, 22, SH110X_WHITE);
      case 21: display.fillRect(100, 43, 3, 21, SH110X_WHITE);
      case 20: display.fillRect(95, 44, 3, 20, SH110X_WHITE);
      case 19: display.fillRect(90, 45, 3, 19, SH110X_WHITE);
      case 18: display.fillRect(85, 46, 3, 18, SH110X_WHITE);
      case 17: display.fillRect(80, 47, 3, 17, SH110X_WHITE);
      case 16: display.fillRect(75, 48, 3, 16, SH110X_WHITE);
      case 15: display.fillRect(70, 49, 3, 15, SH110X_WHITE);
      case 14: display.fillRect(65, 50, 3, 14, SH110X_WHITE);
      case 13: display.fillRect(60, 51, 3, 13, SH110X_WHITE);
      case 12: display.fillRect(55, 52, 3, 12, SH110X_WHITE);
      case 11: display.fillRect(50, 53, 3, 11, SH110X_WHITE);
      case 10: display.fillRect(45, 54, 3, 10, SH110X_WHITE);
      case 9: display.fillRect(40, 55, 3, 9, SH110X_WHITE);
      case 8: display.fillRect(35, 56, 3, 8, SH110X_WHITE);
      case 7: display.fillRect(30, 57, 3, 7, SH110X_WHITE);
      case 6: display.fillRect(25, 58, 3, 6, SH110X_WHITE);
      case 5: display.fillRect(20, 59, 3, 5, SH110X_WHITE);
      case 4: display.fillRect(15, 60, 3, 4, SH110X_WHITE);
      case 3: display.fillRect(10, 61, 3, 4, SH110X_WHITE);
      case 2: display.fillRect(5, 62, 3, 2, SH110X_WHITE);
      case 1: display.fillRect(0, 63, 3, 1, SH110X_WHITE);
      display.display();
      }
    }
   else
    {
    smvalTX = analogRead(meterTX); 
    y = map(smvalTX, 10, 400, 1, 26); 
    if (y > 26) y = 26;
    switch (y) 
      {
      case 26: display.fillRect(125, 38, 4, 26, SH110X_WHITE);
      case 25: display.fillRect(120, 39, 5, 25, SH110X_WHITE);
      case 24: display.fillRect(115, 40, 4, 24, SH110X_WHITE);
      case 23: display.fillRect(110, 41, 5, 23, SH110X_WHITE);
      case 22: display.fillRect(105, 42, 4, 22, SH110X_WHITE);
      case 21: display.fillRect(100, 43, 5, 21, SH110X_WHITE);
      case 20: display.fillRect(95, 44, 4, 20, SH110X_WHITE);
      case 19: display.fillRect(90, 45, 5, 19, SH110X_WHITE);
      case 18: display.fillRect(85, 46, 4, 18, SH110X_WHITE);
      case 17: display.fillRect(80, 47, 5, 17, SH110X_WHITE);
      case 16: display.fillRect(75, 48, 4, 16, SH110X_WHITE);
      case 15: display.fillRect(70, 49, 5, 15, SH110X_WHITE);
      case 14: display.fillRect(65, 50, 4, 14, SH110X_WHITE);
      case 13: display.fillRect(60, 51, 5, 13, SH110X_WHITE);
      case 12: display.fillRect(55, 52, 4, 12, SH110X_WHITE);
      case 11: display.fillRect(50, 53, 5, 11, SH110X_WHITE);
      case 10: display.fillRect(45, 54, 4, 10, SH110X_WHITE);
      case 9: display.fillRect(40, 55, 5, 9, SH110X_WHITE);
      case 8: display.fillRect(35, 56, 4, 8, SH110X_WHITE);
      case 7: display.fillRect(30, 57, 5, 7, SH110X_WHITE);
      case 6: display.fillRect(25, 58, 4, 6, SH110X_WHITE);
      case 5: display.fillRect(20, 59, 5, 5, SH110X_WHITE);
      case 4: display.fillRect(15, 60, 4, 4, SH110X_WHITE);
      case 3: display.fillRect(10, 61, 5, 4, SH110X_WHITE);
      case 2: display.fillRect(5, 62, 4, 2, SH110X_WHITE);
      case 1: display.fillRect(0, 63, 5, 1, SH110X_WHITE);
      display.display();
      }
    }
  }
