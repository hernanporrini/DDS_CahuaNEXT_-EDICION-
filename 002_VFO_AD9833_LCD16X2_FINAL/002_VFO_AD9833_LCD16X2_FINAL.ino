#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#include <EEPROM.h>               
#include <Rotary.h>          
#include <AD9833.h>          
#define FNC_PIN       0       // Any digital pin. Used to enable SPI transfers (active LO  

Rotary r = Rotary(2, 3);     //definicion del rotary switch en los pines 2 y 3
//--------------- Create an AD9833 object ---------------- 
// Note, SCK and MOSI must be connected to CLK and DAT pins on the AD9833 for SPI
// -----      AD9833 ( FNCpin, referenceFrequency = 25000000UL )
AD9833 gen(FNC_PIN);       // Defaults to 25MHz internal reference frequency

unsigned long freq, freqold, fstep;
long IF = 0;      //el valor de la frecuencia del filtro a cristal
byte encoder = 1;
byte stp=0;
int modo = 0;

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
      if (dir == 1) freq = freq + fstep;
      if (freq >= 10000000) freq = 10000000;
      if (dir == -1) freq = freq - fstep;
      if (freq <= 1000000) freq = 1000000;
      EEPROM.put(3, freq);
      tunegen();
      
    }
  }

void setup() 
  {
  //Wire.begin();
  lcd.begin(16, 2);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);
  lcd.setCursor(0, 0);
  lcd.print("Generador AD9833");
  lcd.setCursor(0, 1);
  lcd.print("#02 by H.Porrini");
  delay(5000);
  lcd.clear();  
  gen.Begin();              // The loaded defaults are 1000 Hz SINE_WAVE using REG0
                            // The output is OFF, Sleep mode is disabled
                            // Turn ON the output
  EEPROM.get(0, stp);             //RESTAURA FRECUENCIA DESDE LA EEPROM
  EEPROM.get(1, modo);             //RESTAURA FRECUENCIA DESDE LA EEPROM
  EEPROM.get(3, freq);             //RESTAURA FRECUENCIA DESDE LA EEPROM
  gen.ApplySignal(SINE_WAVE,REG0,freq);
  gen.EnableOutput(true);
  delay(200);
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  setstep();
  }

void loop() 
  {
  if (freqold != freq) 
    {
    tunegen();
    freqold = freq;
    }
  if (digitalRead(A3) == LOW) 
    {
    delay(500);
    if (digitalRead(A3) == LOW) 
      {
      modo = modo+1;
      EEPROM.put(1, modo);
      if (modo>4) modo =0;
      switch (modo) 
        {
        case 0: IF=-2000000; break;
        case 1: IF=-1650000; break;
        case 2: IF= 0; break;
        case 3: IF= 1650000; break;
        case 4: IF= 2000000; break;
        }
      }
      else 
      setstep();
    tunegen();
    displayfreq();
    delay(500);
    }
    displayfreq();
  }

void tunegen() 
  {
  gen.ApplySignal(SINE_WAVE,REG0,freq+IF);
  }

void displayfreq() 
  {
  unsigned int m = freq / 1000000;
  unsigned int k = (freq % 1000000) / 1000;
  unsigned int h = (freq % 1000) / 1;
  char buffer[15] = "";
  lcd.setCursor(0, 0); lcd.print("VFO");
  lcd.setCursor(3, 0); sprintf(buffer, "%2d%003d.%003d   ", m, k, h);
  lcd.print(buffer);
  lcd.setCursor(10, 1);
  if (stp == 1) lcd.print("  1 ");
  if (stp == 2) lcd.print(" 10 ");
  if (stp == 3) lcd.print("100 ");
  if (stp == 4) lcd.print("  1k");
  if (stp == 5) lcd.print(" 10k");
  if (stp == 6) lcd.print("100k");
  if (stp == 7) lcd.print("  1M");
  lcd.print("Hz");
  lcd.setCursor(0, 1);
  lcd.print("IF=");
  switch (modo) 
    {
    case 0: lcd.print(IF/1000); break;
    case 1: lcd.print(IF/1000); break;
    case 2: lcd.print("    ");lcd.print(IF/1000); break;
    case 3: lcd.print(" ");lcd.print(IF/1000); break;
    case 4: lcd.print(" ");lcd.print(IF/1000); break;
    }
  
  }

void setstep() 
  {
  switch (stp) 
    {
    case 1: stp = 2; fstep = 10; break;
    case 2: stp = 3; fstep = 100; break;
    case 3: stp = 4; fstep = 1000; break;
    case 4: stp = 5; fstep = 10000; break;
    case 5: stp = 6; fstep = 100000; break;
    case 6: stp = 7; fstep = 1000000; break;
    case 7: stp = 1; fstep = 1; break;
    }
  EEPROM.put(0, stp);
  }


  
