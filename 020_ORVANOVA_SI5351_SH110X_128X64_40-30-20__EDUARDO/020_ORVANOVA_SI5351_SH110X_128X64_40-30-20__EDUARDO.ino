#include <SPI.h>
#include <EEPROM.h>               
#include <Wire.h>                 
#include <Rotary.h>               //https://github.com/brianlow/Rotary
#include <si5351.h>               //https://github.com/etherkit/Si5351Arduino
#include <Adafruit_GFX.h>         //https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SH110X.h>

#define IF         2000000      //el valor de la frecuencia del filtro a cristal
#define tunestep   4         //Este pin es el pulsador central del rotary, se usa para cambiar el paso
#define band40     10        //Este pin se usa para que el micro sepa que se selecciono la banda de 40
#define band30     11        //Este pin se usa para que el micro sepa que se selecciono la banda de 30
#define band20      9        //Este pin se usa para que el micro sepa que se selecciono la banda de 20
#define pinTRX     A1        //Este pin se usa para que el micro sepa si esta en TX o RX
#define meterTX    A0        //pin de entrada analogica del indicador de potencia de salida
#define meterRX    A2        //pin de entrada analogica del indicador de recepcion
#define meterVDC   A3        //pin de entrada analogica del indicador de tension de entrada

Rotary r = Rotary(3, 2);     //definicion del rotary switch en los pines 2 y 3

Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);
Si5351 si5351(0x60);                                             //definicion y direccion i2c del Si5351 en 0x60

unsigned long freq=100000, freqmem, freqbanda=5000000, freqlcd, freqout, freqold=0, fstep;
unsigned long ultimoTiempoGuardado = 0;
const unsigned long intervaloGuardado = 30000;
long cal = 134416;
unsigned int smvalTX;
unsigned int smvalRX;
byte encoder = 1;
byte stp=2;
byte n = 1;
byte y, x;
bool modo = 0;
bool save = 1;
int banda=0;                 //banda 1=40m, 2=30m, 3=20m
int count=0;
float volt=0;
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
  delay(200); // 
  Wire.begin();

  delay(250); // espera a que el OLED encienda
  display.begin(0x3C, true); // direccion 0x3C por default
  display.setTextColor(SH110X_WHITE);
  display.clearDisplay();
  display.display();
  delay(200);  
  pinMode(2,INPUT_PULLUP);
  pinMode(3,INPUT_PULLUP);
  pinMode(tunestep,INPUT_PULLUP);
  pinMode(pinTRX, INPUT);
  pinMode(band40, INPUT);
  pinMode(band30, INPUT);
  pinMode(band20, INPUT);
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
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
  if (digitalRead(band40) == HIGH && banda!=1) {freqbanda= 6400000;banda=1;modo=1;}
  if (digitalRead(band30) == HIGH && banda!=2) {freqbanda= 9400000;banda=2;modo=0;}
  if (digitalRead(band20) == HIGH && banda!=3) {freqbanda=13400000;banda=3;modo=0;}
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

  if (digitalRead(band40) == HIGH && banda!=1) 
    {
    banda=1;
    freqbanda=6400000;
    delay(300);
    modo=1;
    tunegen();
    }
  if (digitalRead(band30) == HIGH && banda!=2) 
    {
    banda=2;
    freqbanda=9400000;
    delay(300);
    modo=0;
    tunegen();
    }
  if (digitalRead(band20) == HIGH && banda!=3) 
    {
    banda=3;
    freqbanda=13400000;
    delay(300);
    modo=0;
    tunegen();
    }
  displayfreq();
  sgnalread();
  }

void tunegen() 
  {
  if(modo)  freqout= freqbanda + freq - IF ; 
  else      freqout= freqbanda + freq + IF ;
  si5351.set_freq(freqout * 100ULL, SI5351_CLK0); 
  si5351.set_freq(1000000 * 100ULL, SI5351_CLK2); 
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
  display.setCursor(5, 21); sprintf(buffer, "%2d,%003d.%003d", m, k, h);
  display.print(buffer);
  display.setTextColor(SH110X_WHITE);
  display.drawLine(1, 45, 126, 45, SH110X_WHITE);
  display.drawLine( 12, 55, 126, 55, SH110X_WHITE);
  display.setTextSize(2);
  display.setCursor( 0, 50);
  display.print("S");
  display.setTextSize(1);
  display.setCursor(14, 57);
  display.print("1");
  display.drawPixel(16, 48,SH110X_WHITE);
  display.drawPixel(25, 48,SH110X_WHITE);
  display.setCursor(32, 57);
  display.print("3");
  display.drawPixel(34, 48,SH110X_WHITE);
  display.drawPixel(43, 48,SH110X_WHITE);
  display.setCursor(50, 57);
  display.print("5");
  display.drawPixel(52, 48,SH110X_WHITE);
  display.drawPixel(61, 48,SH110X_WHITE);
  display.setCursor(68, 57);
  display.print("7");
  display.drawPixel(70, 48,SH110X_WHITE);
  display.drawPixel(79, 48,SH110X_WHITE);
  display.setCursor(86, 57);
  display.print("9");
  display.drawPixel(88, 48,SH110X_WHITE);
  display.setCursor( 95, 57);
  display.drawPixel(97, 48,SH110X_WHITE);
  display.setCursor(104, 57);
  display.drawPixel(106,48,SH110X_WHITE);
  display.setCursor(107, 57);
  display.print("+30");
  display.drawPixel(115,48,SH110X_WHITE);
  
  display.setCursor(45, 3);
  //if (stp == 4) {display.print(" 10kHz");display.drawLine( 53, 36,  63, 36, SH110X_WHITE);display.drawLine( 53, 35,  63, 35, SH110X_WHITE);} 
  if (stp == 5) {display.print("100kHz");display.drawTriangle( 41, 39,  51, 39,  46, 37, SH110X_WHITE);} 
  if (stp == 4) {display.print(" 10kHz");display.drawTriangle( 53, 39,  63, 39,  58, 37, SH110X_WHITE);} 
  if (stp == 3) {display.print("  1kHz");display.drawTriangle( 65, 39,  75, 39,  70, 37, SH110X_WHITE);} 
  if (stp == 2) {display.print("100 Hz");display.drawTriangle( 89, 39,  99, 39,  94, 37, SH110X_WHITE);}
  if (stp == 1) {display.print(" 10 Hz");display.drawTriangle(101, 39, 111, 39, 106, 37, SH110X_WHITE);}
  display.setCursor(51,12);
  display.print("VFO A");

  display.setCursor( 5, 3);
  volt=(float(analogRead(meterVDC))/1024.0)*20.25;
  if (volt<10) display.setCursor( 11, 3); else display.setCursor( 5, 3);
  display.print(volt);
  display.print("V");

  
  display.setTextSize(2);
  display.setCursor(89, 4);
  if (modo) display.print("LSB");
  else display.print("USB"); 
  display.setTextSize(1);
  display.setCursor(5,12); 
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
    case 4: stp = 5; fstep = 100000; break;
    case 5: stp = 1; fstep = 10; break;
    }
  }


void sgnalread() 
  {
  if(digitalRead(pinTRX)==HIGH)
    {
    smvalRX = analogRead(meterRX); 
    x = map(smvalRX, 0, 724, 1, 12); 
    if (x > 12) x = 12;
    switch (x) 
      {
      case 12: display.fillRect(112, 46, 7, 7 , SH110X_WHITE);
      case 11: display.fillRect(103, 46, 7, 7 , SH110X_WHITE);
      case 10: display.fillRect( 94, 46, 7, 7 , SH110X_WHITE);
      case 9:  display.fillRect( 85, 46, 7, 7 , SH110X_WHITE);
      case 8:  display.fillRect( 76, 47, 7, 7 , SH110X_WHITE);
      case 7:  display.fillRect( 67, 47, 7, 7 , SH110X_WHITE);
      case 6:  display.fillRect( 58, 47, 7, 7 , SH110X_WHITE);
      case 5:  display.fillRect( 49, 47, 7, 7 , SH110X_WHITE);
      case 4:  display.fillRect( 40, 47, 7, 7 , SH110X_WHITE);
      case 3:  display.fillRect( 31, 47, 7, 7 , SH110X_WHITE);
      case 2:  display.fillRect( 22, 47, 7, 7 , SH110X_WHITE);
      case 1:  display.fillRect( 13, 47, 7, 7 , SH110X_WHITE);
      display.display();
      }
    }
   else
    {
    smvalTX = analogRead(meterTX); 
    y = map(smvalTX, 256, 352, 1, 12); 
    
    if (y > 12) y = 12;
    switch (y) 
      {
      case 12: display.fillRect(112, 47, 7, 7 , SH110X_WHITE);
      case 11: display.fillRect(103, 47, 7, 7 , SH110X_WHITE);
      case 10: display.fillRect( 94, 47, 7, 7 , SH110X_WHITE);
      case 9:  display.fillRect( 85, 47, 7, 7 , SH110X_WHITE);
      case 8:  display.fillRect( 76, 47, 7, 7 , SH110X_WHITE);
      case 7:  display.fillRect( 67, 47, 7, 7 , SH110X_WHITE);
      case 6:  display.fillRect( 58, 47, 7, 7 , SH110X_WHITE);
      case 5:  display.fillRect( 49, 47, 7, 7 , SH110X_WHITE);
      case 4:  display.fillRect( 40, 47, 7, 7 , SH110X_WHITE);
      case 3:  display.fillRect( 31, 47, 7, 7 , SH110X_WHITE);
      case 2:  display.fillRect( 22, 47, 7, 7 , SH110X_WHITE);
      case 1:  display.fillRect( 13, 47, 7, 7 , SH110X_WHITE);
      display.display();
      }
    }
  }

 

  
