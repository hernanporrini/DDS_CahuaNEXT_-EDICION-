#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#include <EEPROM.h>               
#include <Wire.h>                 
#include <Rotary.h>          //https://github.com/brianlow/Rotary
#include <si5351.h>          //https://github.com/etherkit/Si5351Arduino

Rotary r = Rotary(2, 3);     //definicion del rotary switch en los pines 2 y 3
Si5351 si5351(0x60);         //definicion y direccion i2c del Si5351 en 0x60

unsigned long freq, freqold, fstep;
long IF = 2000000;      //el valor de la frecuencia del filtro a cristal
long cal = 147416;     //Si5351 factor de calibracion , ajustar exactamnente 10MHz. incrementando este valor se reduce la frecuencia generada y viceversa
byte encoder = 1;
byte stp=3;
int modo = 0;
unsigned int period = 100;
unsigned long time_now = 0;

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
      if (freq <= 3000000) freq = 3000000;
      EEPROM.put(0, freq);
    }
  }

void setup() 
  {
  Wire.begin();
  lcd.begin(16, 2);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(11, INPUT_PULLUP);
  lcd.setCursor(0, 0);
  lcd.print("Generador Si5351");
  lcd.setCursor(0, 1);
  lcd.print("#01 by H.Porrini");
  delay(5000);
  lcd.clear();  
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.output_enable(SI5351_CLK0, 0);                  //1 - HABILITADO / 0 - DESHABILITADO
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 1);
  delay(200);
  si5351.set_correction(cal, SI5351_PLL_INPUT_XO);
  //si5351.set_ms_source(SI5351_CLK0, SI5351_PLLB);
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  setstep();
  EEPROM.get(0, freq);             //RESTAURA FRECUENCIA DESDE LA EEPROM
  }

void loop() 
  {
  if (freqold != freq) 
    {
    tunegen();
    freqold = freq;
    }
  if (digitalRead(11) == LOW) 
    {
    delay(500);
    if (digitalRead(11) == LOW) 
      {
      modo = modo+1;
      if (modo>4) modo =0;
      tunegen();
      }
    else 
      setstep();
    displayfreq();
    delay(500);
    }
  displayfreq();
  }

void tunegen() 
  {
  si5351.set_freq((freq + IF) * 100ULL, SI5351_CLK2);
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
    case 0: IF=-2000000; lcd.print(IF/1000); break;
    case 1: IF=-1650000; lcd.print(IF/1000); break;
    case 2: IF= 0; lcd.print("    ");lcd.print(IF/1000); break;
    case 3: IF= 1650000; lcd.print(" ");lcd.print(IF/1000); break;
    case 4: IF= 2000000; lcd.print(" ");lcd.print(IF/1000); break;
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
  }


  
