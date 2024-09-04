#include <SPI.h>
#include <EEPROM.h>               
#include <Wire.h>                 
#include <Rotary.h>               //https://github.com/brianlow/Rotary
#include <si5351.h>               //https://github.com/etherkit/Si5351Arduino
#include <Adafruit_GFX.h>         //https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h>     //Adafruit SSD1306 https://github.com/adafruit/Adafruit_SSD1306

#define IF         2000000      //el valor de la frecuencia del filtro a cristal
#define tunestep   4         //Este pin es el pulsador central del rotary, se usa para cambiar el paso
#define band80     10        //Este pin se usa para que el micro sepa que se selecciono la banda de 80
#define band40     11        //Este pin se usa para que el micro sepa que se selecciono la banda de 40
#define band20     12        //Este pin se usa para que el micro sepa que se selecciono la banda de 20
//#define pinTRX     A0        //Este pin se usa para que el micro sepa si esta en TX o RX
//#define meterRX    A1        //pin de entrada analogica del indicador de recepcion
//#define meterTX    A2        //pin de entrada analogica del indicador de potencia de salida
#define meterVDC   A3        //pin de entrada analogica del indicador de tension de entrada

Rotary r = Rotary(2, 3);     //definicion del rotary switch en los pines 2 y 3

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);
Si5351 si5351(0x60);                                             //definicion y direccion i2c del Si5351 en 0x60

unsigned long freq=100000, freqmem, freqbanda=5000000, freqlcd, freqout, freqold=0, fstep;
unsigned long ultimoTiempoGuardado = 0;
const unsigned long intervaloGuardado = 30000;
long cal = 125416;
unsigned int smvalTX;
unsigned int smvalRX;
byte encoder = 1;
byte stp=2;
byte n = 1;
//byte y, x;
bool modo = 0;
bool save = 1;
int banda=0;                 //banda 1=80m, 2=40m, 3=20m
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
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();
  delay(200); // 
  pinMode(2, INPUT);
  pinMode(3, INPUT);
  pinMode(tunestep, INPUT);
//pinMode(pinTRX, INPUT_PULLUP);
  pinMode(band80, INPUT_PULLUP);
  pinMode(band40, INPUT_PULLUP);
  pinMode(band20, INPUT_PULLUP);
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_2MA);
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
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
  if (digitalRead(band80) == LOW && banda!=1) {freqbanda= 2900000;banda=1;modo=0;}
  if (digitalRead(band40) == LOW && banda!=2) {freqbanda= 6400000;banda=2;modo=0;}
  if (digitalRead(band20) == LOW && banda!=3) {freqbanda= 13400000;banda=3;modo=1;}
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
  if (digitalRead(band80) == LOW && banda!=1) 
    {
    banda=1;
    freqbanda=2900000;
    delay(300);
    modo=0;
    tunegen();
    }
  if (digitalRead(band40) == LOW && banda!=2) 
    {
    banda=2;
    freqbanda=6400000;
    delay(300);
    modo=0;
    tunegen();
    }
  if (digitalRead(band20) == LOW && banda!=3) 
    {
    banda=3;
    freqbanda= 13400000;
    delay(300);
    modo=1;
    tunegen();
    }
  displayfreq();
  //sgnalread();
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
  
  display.setTextSize(3);
  char buffer1[15] = "";
  display.setCursor(-3, 30); sprintf(buffer1, "%2d%003d", m, k);
  display.print(buffer1);
  display.drawTriangle(28,55,34,55,31,52, WHITE);
  display.setTextSize(2);
  char buffer2[15] = "";
  display.setCursor(92,30); sprintf(buffer2, "%003d", h);
  display.print(buffer2);
  
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(40, 0);
  if (stp == 5) {display.print("100kHz");display.drawTriangle( 33, 23, 47, 23, 40, 27, WHITE);} 
  if (stp == 4) {display.print(" 10kHz");display.drawTriangle( 51, 23, 65, 23, 58, 27, WHITE);} 
  if (stp == 3) {display.print("  1kHz");display.drawTriangle( 69, 23, 83, 23, 76, 27, WHITE);} 
  if (stp == 2) {display.print("100 Hz");display.drawTriangle( 92, 23,100, 23, 96, 27, WHITE);}
  if (stp == 1) {display.print(" 10 Hz");display.drawTriangle(104, 23,112, 23,108, 27, WHITE);} 
  display.setCursor(58, 56);
  display.print("BANDA "); 
  display.setTextSize(2);
  display.setCursor(92, 49);
  if (banda == 1) display.print("80m"); 
  if (banda == 2) display.print("40m"); 
  if (banda == 3) display.print("20m"); 
  
                                        
                                         
  display.setTextSize(1);
  
  display.setCursor(40, 9);
  display.print("VFO A");
  
  display.setCursor( 0, 0);
  volt=(float(analogRead(meterVDC))/1024.00)*20;
  display.print(volt);
  display.print("V");

  display.setTextSize(2);
  display.setCursor(92, 0);
  if (modo) display.print("USB");
  else display.print("LSB"); 
  display.setTextSize(1);
  display.setCursor(0, 9); 
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

/*
void sgnalread() 
  {
  if(digitalRead(pinTRX)==HIGH)
    {
    smvalRX = analogRead(meterRX); 
    x = map(smvalRX, 0, 1024, 1, 12); 
    if (x > 12) x = 12;
    switch (x) 
      {
      case 12: display.fillRect(112, 44, 7, 9 , WHITE);
      case 11: display.fillRect(103, 44, 7, 9 , WHITE);
      case 10: display.fillRect( 94, 44, 7, 9 , WHITE);
      case 9:  display.fillRect( 85, 44, 7, 9 , WHITE);
      case 8:  display.fillRect( 76, 44, 7, 9 , WHITE);
      case 7:  display.fillRect( 67, 44, 7, 9 , WHITE);
      case 6:  display.fillRect( 58, 44, 7, 9 , WHITE);
      case 5:  display.fillRect( 49, 44, 7, 9 , WHITE);
      case 4:  display.fillRect( 40, 44, 7, 9 , WHITE);
      case 3:  display.fillRect( 31, 44, 7, 9 , WHITE);
      case 2:  display.fillRect( 22, 44, 7, 9 , WHITE);
      case 1:  display.fillRect( 13, 44, 7, 9 , WHITE);
      display.display();
      }
    }
   else
    {
    smvalTX = analogRead(meterTX); 
    y = map(smvalTX, 0, 512, 1, 12); 
    if (y > 12) y = 12;
    switch (y) 
      {
      case 12: display.fillRect(112, 44, 7, 9 , WHITE);
      case 11: display.fillRect(103, 44, 7, 9 , WHITE);
      case 10: display.fillRect( 94, 44, 7, 9 , WHITE);
      case 9:  display.fillRect( 85, 44, 7, 9 , WHITE);
      case 8:  display.fillRect( 76, 44, 7, 9 , WHITE);
      case 7:  display.fillRect( 67, 44, 7, 9 , WHITE);
      case 6:  display.fillRect( 58, 44, 7, 9 , WHITE);
      case 5:  display.fillRect( 49, 44, 7, 9 , WHITE);
      case 4:  display.fillRect( 40, 44, 7, 9 , WHITE);
      case 3:  display.fillRect( 31, 44, 7, 9 , WHITE);
      case 2:  display.fillRect( 22, 44, 7, 9 , WHITE);
      case 1:  display.fillRect( 13, 44, 7, 9 , WHITE);
      display.display();
      }
    }
  }
*/  
