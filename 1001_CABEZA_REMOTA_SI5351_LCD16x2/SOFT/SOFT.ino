#include <LiquidCrystal.h>
LiquidCrystal lcd(4,5,6,7,8,9); 

#include "smeter.h"
#include <EEPROM.h>               
#include <Wire.h>                 
#include <Rotary.h>          //https://github.com/brianlow/Rotary
#include <si5351.h>          //https://github.com/etherkit/Si5351Arduino

#define ROT_A	      2 
#define ROT_B	      3 
#define IF          2000000      //el valor de la frecuencia del filtro a cristal
#define PTT         11        //Este pin se usa para que el micro sepa que se selecciono la banda de 80
#define port1       10        //Este pin se usa para que el micro sepa que se selecciono la banda de 30
#define BUZZER      12        //Este pin se usa para que el micro sepa que se selecciono la banda de 80
#define vfo_boton   13
#define modo_boton  A0        //Este pin se usa para que el micro sepa que se selecciono la banda de 80
#define meterVDC    A1        //pin de entrada analogica del indicador de tension de entrada
#define paso_boton  A2        //Este pin se usa para que el micro sepa que se selecciono la banda de 40
#define band_boton  A3        //Este pin se usa para que el micro sepa que se selecciono la banda de 30
#define meterRX     A6        //pin de entrada analogica del indicador de recepcion
#define meterTX     A7        //pin de entrada analogica del indicador de potencia de salida

Rotary r = Rotary(ROT_B, ROT_A);      //definicion del rotary switch en los pines 2 y 3
Si5351 si5351(0x60);          //definicion y direccion i2c del Si5351 en 0x60

long fbandamin=0, fbandamax=0, freqlcd=0, fpaso=0;
long freqA=0, freqmemA=0, freqoldA=0, freqoutA=0;
long freqB=0, freqmemB=0, freqoldB=0, freqoutB=0;
long SPLIT_IF=0;
unsigned long ultimoTiempoGuardado = 0;
//int ultimoTiempoGuardado = 0;
const unsigned long intervaloGuardado = 30000;
//int intervaloGuardado = 30000;
long cal = 127416;     //Si5351 factor de calibracion , ajustar exactamnente 10MHz. incrementando este valor se reduce la frecuencia generada y viceversa
byte encoder = 1;
byte vfo =0;
byte paso=5;
byte banda=3;
bool save = 1;
bool modo = HIGH;

byte fillbar[8] = {0b11000,0b01100,0b00110,0b00011,0b00011,0b00110,0b01100,0b11000};
byte mark[8]    = {0b00100,0b01010,0b01010,0b10001,0b10001,0b01010,0b01010,0b00100};
  
ISR(PCINT2_vect) 
  {
  char result = r.process();
  if (result == DIR_CW) setfreq(-1);
  else if (result == DIR_CCW) setfreq(1);
  }

void setfreq(short dir) 
  {
  if (encoder == 1) 
    switch (vfo)
    {
      case 0: 
      	if (dir ==  1) {freqA = freqA + fpaso; if (freqA > fbandamax-fbandamin) {freqA= fbandamax-fbandamin; tone(BUZZER, 300, 50);}}
      	if (dir == -1) {freqA = freqA - fpaso; if (freqA < 0)                   {freqA= 0;                   tone(BUZZER, 300, 50);}}
      break;
      case 1:
      	if (dir ==  1) {freqB = freqB + fpaso; if (freqB > fbandamax-fbandamin) {freqB= fbandamax-fbandamin; tone(BUZZER, 300, 50);}}
      	if (dir == -1) {freqB = freqB - fpaso; if (freqB < 0)                   {freqB= 0;                   tone(BUZZER, 300, 50);}}
      break;
      case 2:
      	if (dir ==  1) {freqB = freqB + fpaso; if (freqB > fbandamax-fbandamin) {freqB= fbandamax-fbandamin; tone(BUZZER, 300, 50);}}
      	if (dir == -1) {freqB = freqB - fpaso; if (freqB < 0)                   {freqB= 0;                   tone(BUZZER, 300, 50);}}
      break;
    }  
  }

void setup() 
  {
  tone(BUZZER, 1000, 100);
  Wire.begin();
  lcd.begin(16, 2);

  lcd.createChar(0, fillbar);
  lcd.createChar(1, mark);

  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_B, INPUT_PULLUP);
  pinMode(vfo_boton, INPUT_PULLUP);
  pinMode(paso_boton, INPUT_PULLUP);
  pinMode(modo_boton, INPUT_PULLUP);
  pinMode(band_boton, INPUT_PULLUP);
  pinMode(PTT,INPUT_PULLUP);
  pinMode(port1,OUTPUT);
  pinMode(BUZZER,OUTPUT);
  lcd.setCursor(0, 0);
  lcd.print("-- HEADREMOTE --");
  lcd.setCursor(0, 1);
  lcd.print("  by H.Porrini  ");
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
  setvfo();
  setpaso();
  setbanda();
  setmodo();
  EEPROM.get( 0,freqA);
  freqoldA = freqA;
  EEPROM.get(10,freqB);
  freqoldB = freqB;
  tunegen();  
  tone(BUZZER, 1000, 100);
  }

void loop() 
  {
  unsigned long tiempoActual=millis();
  EEPROM.get( 0,freqmemA);
  EEPROM.get(10,freqmemB);
  
  if ((freqA != freqmemA ||freqB != freqmemB) && tiempoActual-ultimoTiempoGuardado >= intervaloGuardado)
    {
    EEPROM.put( 0,freqA);
    EEPROM.put(10,freqB);
    ultimoTiempoGuardado=tiempoActual;
    save=1;
    }
  if (freqoldA != freqA || freqoldB != freqB) 
    {
    save=0;
    tunegen();
    freqoldA = freqA;
    freqoldB = freqB;
    }
  
  if (digitalRead(vfo_boton) == LOW) 
    {
    vfo++;
    setvfo();
    delay(1000);
    }
  if (digitalRead(band_boton) == LOW) 
    {
    tone(BUZZER, 300, 50);
    save=0;
    paso=5;
    setpaso();
    banda++;
    setbanda();
    tunegen();
    displayfreq();
    delay(1000);
    }
  if (digitalRead(paso_boton) == LOW) 
    {
    tone(BUZZER, 300, 50);
    paso++;
    setpaso();
    displayfreq();
    delay(500);
    }
  if (digitalRead(modo_boton) == LOW) 
    {
    tone(BUZZER, 300, 50);
    save=0;
    setmodo();
    tunegen();
    displayfreq();
    delay(1000);
    } 
  displayfreq();
  lcd.setCursor(0, 1); 
  if (digitalRead(PTT) == LOW)
    {lcd.print("T");smeter_signal = map(sqrt(analogRead(meterTX) * 16), 0, 10, 1, 10);}
  else
    {lcd.print("R");smeter_signal = map(sqrt(analogRead(meterRX) * 16), 0, 10, 1, 10);}
  smeter(1, smeter_signal);
  tunegen();
  }

void tunegen() 
  {
  if(modo)  {freqoutA= fbandamin + freqA + IF ; freqoutB= fbandamin + freqB + IF;}
  else      {freqoutA= fbandamin + freqA - IF ; freqoutB= fbandamin + freqB - IF;}
  switch(vfo)
  {
    case 0: si5351.set_freq(freqoutA * 100ULL, SI5351_CLK2); break;
    case 1: si5351.set_freq(freqoutB * 100ULL, SI5351_CLK2); break;
    case 2: if (digitalRead(PTT)==LOW) si5351.set_freq(freqoutA * 100ULL, SI5351_CLK2); else si5351.set_freq(freqoutB * 100ULL, SI5351_CLK2); break;
  }
  si5351.set_freq(IF * 100ULL, SI5351_CLK0); 
  }

void displayfreq() 
  {
  switch (vfo)
  {  
    case 0: freqlcd= fbandamin + freqA ; break;
    case 1: freqlcd= fbandamin + freqB ; break;
    case 2: if (digitalRead(PTT)==LOW) freqlcd= fbandamin + freqA ; else freqlcd= fbandamin + freqB ; break;
  }
  unsigned int m = freqlcd / 1000000;
  unsigned int k = (freqlcd % 1000000) / 1000;
  unsigned int h = (freqlcd % 1000) / 1;
  char buffer[15] = "";
  lcd.setCursor(3, 0); sprintf(buffer, "%2d,%003d.%003d", m, k, h);
  lcd.print(buffer);
  lcd.setCursor(13, 1);
  if (paso == 0) lcd.print("  5");
  if (paso == 1) lcd.print(" 10");
  if (paso == 2) lcd.print(" 50");
  if (paso == 3) lcd.print("100");
  if (paso == 4) lcd.print("500");
  if (paso == 5) lcd.print(" 1k");
  if (paso == 6) lcd.print("10k");

  lcd.setCursor(1, 0); 
  if(save) 	lcd.print("s");
  else 		lcd.print(" ");
  }

void setpaso() 
  {
  if (paso>6) paso=0;
  switch (paso) 
    {
    case 0: fpaso =     5; break;
    case 1: fpaso =    10; break;
    case 2: fpaso =    50; break;
    case 3: fpaso =   100; break;
    case 4: fpaso =   500; break;
    case 5: fpaso =  1000; break;
    case 6: fpaso = 10000; break;
    }
  }

void setmodo() 
  {
  modo=!modo;
  lcd.setCursor(0, 1); 
  if (modo)	lcd.print(" Modo USB       ");
  else		  lcd.print(" Modo LSB       ");
  lcd.setCursor(15, 0);
  if (modo) lcd.print("U");
  else      lcd.print("L");
  }
  
void setbanda() 
  {
  if (banda>9) banda=0;
  lcd.setCursor(0, 1); 
  switch (banda) 
    {
    case 0: lcd.print(" Banda 80m      "); modo=0; fbandamin =  3500000; fbandamax =  3800000;break;
    case 1: lcd.print(" Banda 60m      "); modo=0; fbandamin =  5350000; fbandamax =  5367000;break;
    case 2: lcd.print(" Banda 40m      "); modo=0; fbandamin =  7000000; fbandamax =  7300000;break;
    case 3: lcd.print(" Banda 30m      "); modo=0; fbandamin = 10100000; fbandamax = 10150000;break;
    case 4: lcd.print(" Banda 20m      "); modo=1; fbandamin = 14000000; fbandamax = 14350000;break;
    case 5: lcd.print(" Banda 17m      "); modo=1; fbandamin = 18068000; fbandamax = 18168000;break;
    case 6: lcd.print(" Banda 15m      "); modo=1; fbandamin = 21000000; fbandamax = 21450000;break;
    case 7: lcd.print(" Banda 12m      "); modo=1; fbandamin = 24890000; fbandamax = 24990000;break;
    case 8: lcd.print(" Banda 10m      "); modo=1; fbandamin = 28000000; fbandamax = 29700000;break;
    case 9: lcd.print(" Banda  6m      "); modo=1; fbandamin = 50000000; fbandamax = 54000000;break;
    }
  lcd.setCursor(15, 0);
  if (modo) lcd.print("U");
  else      lcd.print("L");
  }
  
void setvfo() 
  {
  if (vfo>2) vfo=0;
  lcd.setCursor(0, 0); 
  lcd.print(" ");
  lcd.setCursor(0, 0); 
  switch (vfo) 
    {
    case 0: lcd.print("A"); break;
    case 1: lcd.print("B"); break;
    case 2: lcd.print("X"); break;
    }
  lcd.setCursor(0, 1); 
  switch (vfo) 
    {
    case 0: lcd.print(" VFO = A        "); break;
    case 1: lcd.print(" VFO = B        "); break;
    case 2: lcd.print(" VFO Split A=Tx "); break;
    }
  tunegen();
  }


  
