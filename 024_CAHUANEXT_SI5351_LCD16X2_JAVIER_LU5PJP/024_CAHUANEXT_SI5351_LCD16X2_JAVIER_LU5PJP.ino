#include <LiquidCrystal.h>
LiquidCrystal lcd(10, 9, 5, 6, 7, 8);

#include "smeter.h"
#include <EEPROM.h>               
#include <Wire.h>                 
#include <Rotary.h>          //https://github.com/brianlow/Rotary
#include <si5351.h>          //https://github.com/etherkit/Si5351Arduino

#define IF         2000000      //el valor de la frecuencia del filtro a cristal
#define tunestep   4        //Este pin es el pulsador central del rotary, se usa para cambiar el paso
#define band80     13        //Este pin se usa para que el micro sepa que se selecciono la banda de 80
#define band40     12        //Este pin se usa para que el micro sepa que se selecciono la banda de 40
#define band20     11        //Este pin se usa para que el micro sepa que se selecciono la banda de 30
//#define meterVDC   A1        //pin de entrada analogica del indicador de tension de entrada
#define meterTX    A2        //pin de entrada analogica del indicador de recepcion
#define meterRX    A3        //pin de entrada analogica del indicador de potencia de salida

Rotary r = Rotary(3, 2);     //definicion del rotary switch en los pines 2 y 3
Si5351 si5351(0x60);         //definicion y direccion i2c del Si5351 en 0x60

unsigned long freq=100000, freqmem, freqbanda=5000000, freqlcd, freqout, freqold=0, fstep;
unsigned long ultimoTiempoGuardado = 0;
const unsigned long intervaloGuardado = 30000;
long cal = 127416;     //Si5351 factor de calibracion , ajustar exactamnente 10MHz. incrementando este valor se reduce la frecuencia generada y viceversa
byte encoder = 1;
byte stp=2;
bool save = 1;
bool modo = 1;
int banda=0;                 //banda 1=80m, 2=40m, 3=20m
int controlTX=0;
int controlRX=0;
//float volt=0;

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
    {freq = freq + fstep; if (freq>=1100000) freq=1100000;}
    if (dir == -1) 
    {freq = freq - fstep; if (freq<=100000) freq=100000;}
    }
  }

void setup() 
  {
  Wire.begin();
  lcd.begin(16, 2);
  byte fillbar[8] = {B11000,B01100,B00110,B00011,B00011,B00110,B01100,B11000};
  lcd.createChar(0, fillbar);
  byte mark[8] =    {B00100,B01010,B01010,B10001,B10001,B01010,B01010,B00100};
  lcd.createChar(1, mark);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(tunestep, INPUT_PULLUP);
  pinMode(band80, INPUT);
  pinMode(band40, INPUT);
  pinMode(band20, INPUT);
  lcd.setCursor(0, 0);
  lcd.print("CAHUANEXT LU5PJP");
  lcd.setCursor(0, 1);
  lcd.print("#24 by H.Porrini");
  delay(5000);
  lcd.clear();  
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.output_enable(SI5351_CLK0, 1);                  //1 - HABILITADO / 0 - DESHABILITADO
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
  if (digitalRead(band40) == HIGH && banda!=2) {freqbanda= 6500000;banda=2;modo=0;}
  if (digitalRead(band20) == HIGH && banda!=3) {freqbanda=13500000;banda=3;modo=1;}
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
    freqbanda=6500000;
    delay(300);
    modo=0;
    tunegen();
    }
  if (digitalRead(band20) == HIGH && banda!=3) 
    {
    banda=3;
    freqbanda=13500000;
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
  si5351.set_freq(freqout * 100ULL, SI5351_CLK0); 
  si5351.set_freq(1000000 * 100ULL, SI5351_CLK2); 
  }

void displayfreq() 
  {
  freqlcd= freqbanda + freq ;
  unsigned int m = freqlcd / 1000000;
  unsigned int k = (freqlcd % 1000000) / 1000;
  unsigned int h = (freqlcd % 1000) / 1;
  char buffer[15] = "";
  lcd.setCursor(0, 0); sprintf(buffer, "%2d.%003d.%003d ", m, k, h);
  lcd.print(buffer);
  lcd.setCursor(13, 1);
  if (stp == 1) lcd.print(" 10");
  if (stp == 2) lcd.print("100");
  if (stp == 3) lcd.print(" 1k");
  if (stp == 4) lcd.print("10k");
  lcd.setCursor(13, 0);
  if (modo) lcd.print("USB");
  else lcd.print("LSB"); 
  lcd.setCursor(11, 0); 
  if(save) lcd.print("s "); else lcd.print("  ");
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
  lcd.setCursor(0, 1); 
  controlTX=analogRead(meterTX);
  controlRX=analogRead(meterRX);
  if (controlTX > 50)
    {lcd.print("TX");smeter_signal = map(sqrt(analogRead(meterTX) * 16), 0, 128, 0, 80);}
  else if (controlRX > 50)
    {lcd.print("RX");smeter_signal = map(sqrt(analogRead(meterRX) * 16), 40, 128, 0, 100);}
  if (controlTX <= 50 && controlRX <= 50)
    {lcd.print("  ");}
  smeter(1, smeter_signal);
  //volt=(float(analogRead(meterVDC))/1024.0)*23.4;
  //if (volt<10) lcd.setCursor( 8, 1); else lcd.setCursor( 7, 1);
  //lcd.print(volt);
  //lcd.print("V");

  }
  
