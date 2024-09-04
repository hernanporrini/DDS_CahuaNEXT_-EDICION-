#include <SPI.h>
#include <EEPROM.h>               
#include <Wire.h>                 
#include <Rotary.h>               //https://github.com/brianlow/Rotary
#include <si5351.h>               //https://github.com/etherkit/Si5351Arduino
#include <Adafruit_GFX.h>         //https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h>     //Adafruit SSD1306 https://github.com/adafruit/Adafruit_SSD1306

#define IF         2000000      //el valor de la frecuencia del filtro a cristal
#define tunestep   4         //Este pin es el pulsador central del rotary, se usa para cambiar el paso
//#define band80     10        //Este pin se usa para que el micro sepa que se selecciono la banda de 80
//#define band40      9        //Este pin se usa para que el micro sepa que se selecciono la banda de 40
//#define band30      8        //Este pin se usa para que el micro sepa que se selecciono la banda de 20
#define pinTRX     A0        //Este pin se usa para que el micro sepa si esta en TX o RX
#define meterRX    A1        //pin de entrada analogica del indicador de recepcion
#define meterTX    A2        //pin de entrada analogica del indicador de potencia de salida

Rotary r = Rotary(2, 3);     //definicion del rotary switch en los pines 2 y 3

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);
Si5351 si5351(0x60);                                             //definicion y direccion i2c del Si5351 en 0x60

unsigned long freq=100000, freqmem, freqbanda=5000000, freqlcd, freqout, freqold=0, fstep;
unsigned long ultimoTiempoGuardado = 0;
const unsigned long intervaloGuardado = 30000;
long cal = 152416;
unsigned int smvalTX;
unsigned int smvalRX;
byte encoder = 1;
byte stp=2;
byte n = 1;
byte y, x;
bool modo = 0;
bool save = 1;
//int banda=0;                 //banda 1=80m, 2=40m, 3=20m
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
    {freq = freq + fstep; if (freq>=1000000) freq=1000000;}
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
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(tunestep, INPUT_PULLUP);
  pinMode(pinTRX, INPUT_PULLUP);
  //pinMode(band80, INPUT);
  //pinMode(band40, INPUT);
  //pinMode(band30, INPUT);
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
  //si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
  si5351.output_enable(SI5351_CLK0, 0);                  //1 - HABILITADO / 0 - DESHABILITADO
  si5351.output_enable(SI5351_CLK1, 1);
  si5351.output_enable(SI5351_CLK2, 0);
  delay(200);
  si5351.set_correction(cal, SI5351_PLL_INPUT_XO);
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();
  setstep();
  EEPROM.get(0,freq);
  freqold = freq;
  freqbanda= 6500000;
  modo=0;
  //if (digitalRead(band80) == HIGH && banda!=1) {freqbanda= 3000000;banda=1;modo=0;}
  //if (digitalRead(band40) == HIGH && banda!=2) {freqbanda= 6500000;banda=2;modo=0;}
  //if (digitalRead(band30) == HIGH && banda!=3) {freqbanda= 9500000;banda=3;modo=1;}
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
/*
  if (digitalRead(band80) == HIGH && banda!=1) 
    {
    banda=1;
    freqbanda=3000000;
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
  if (digitalRead(band30) == HIGH && banda!=3) 
    {
    banda=3;
    freqbanda= 9500000;
    delay(300);
    modo=1;
    tunegen();
    }
  */
  displayfreq();
  //sgnalread();
  }

void tunegen() 
  {
  if(modo)  freqout= freqbanda + freq + IF ; 
  else      freqout= freqbanda + freq - IF ;
  si5351.set_freq(freqout * 100ULL, SI5351_CLK1); 
  //si5351.set_freq(1000000 * 100ULL, SI5351_CLK2); 
  }

void displayfreq() 
  {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, WHITE);
  /*
  display.drawLine( 15, 52, 15, 53, WHITE);
  display.drawLine( 16, 51, 16, 54, WHITE);
  display.drawLine( 17, 50, 17, 55, WHITE);
  display.drawLine( 18, 51, 18, 54, WHITE);
  display.drawLine( 19, 52, 19, 53, WHITE);
  
  display.drawLine( 69, 52, 69, 53, WHITE);
  display.drawLine( 70, 51, 70, 54, WHITE);
  display.drawLine( 71, 50, 71, 55, WHITE);
  display.drawLine( 72, 51, 72, 54, WHITE);
  display.drawLine( 73, 52, 73, 53, WHITE);
  */
  
  freqlcd= freqbanda + freq ;
  unsigned int m =  freqlcd / 1000000;
  unsigned int k = (freqlcd % 1000000) / 1000;
  unsigned int h = (freqlcd % 1000) / 1;
  
  display.setTextSize(3);
  char buffer1[15] = "";
  display.setCursor(3, 30); sprintf(buffer1, "%1d.%003d", m, k);
  display.print(buffer1);
  
  display.setTextSize(2);
  char buffer2[15] = "";
  display.setCursor(91, 30); sprintf(buffer2, "%003d", h);
  display.print(buffer2);
  
  display.setTextColor(WHITE);
  display.drawLine(0, 16, 127, 16, WHITE);
  display.setTextSize(2);
  display.setCursor(20, 2);
  if (stp == 1) {display.print(" 10Hz");display.drawLine(108, 47, 108, 47, WHITE);
                                        display.drawLine(107, 48, 109, 48, WHITE);
                                        display.drawLine(106, 49, 110, 49, WHITE);
                                        display.drawLine(105, 50, 111, 50, WHITE);} 
                                        
  if (stp == 2) {display.print("100Hz");display.drawLine( 96, 47,  96, 47, WHITE);
                                        display.drawLine( 95, 48,  97, 48, WHITE);
                                        display.drawLine( 94, 49,  98, 49, WHITE);
                                        display.drawLine( 93, 50,  99, 50, WHITE);}
                                         
  if (stp == 3) {display.print(" 1kHz");display.drawLine( 76, 53,  84, 53, WHITE);
                                        display.drawLine( 75, 54,  85, 54, WHITE);
                                        display.drawLine( 74, 55,  86, 55, WHITE);
                                        display.drawLine( 73, 56,  87, 56, WHITE);}
                                        
  if (stp == 4) {display.print("10kHz");display.drawLine( 58, 53,  66, 53, WHITE);
                                        display.drawLine( 57, 54,  67, 54, WHITE);
                                        display.drawLine( 56, 55,  68, 55, WHITE);
                                        display.drawLine( 55, 56,  69, 56, WHITE);}
  display.setCursor(91, 2);
  if (modo) display.print("USB");
  else display.print("LSB"); 
  display.setTextSize(2);
  display.setCursor(5, 2); 
  if(save) display.print("S"); else display.print(" ");
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
      case 26: display.fillRect(125, 29, 3, 3, WHITE);
      case 25: display.fillRect(120, 29, 3, 3, WHITE);
      case 24: display.fillRect(115, 29, 3, 3, WHITE);
      case 23: display.fillRect(110, 29, 3, 3, WHITE);
      case 22: display.fillRect(105, 29, 3, 3, WHITE);
      case 21: display.fillRect(100, 29, 3, 3, WHITE);
      case 20: display.fillRect(95, 29, 3, 3, WHITE);
      case 19: display.fillRect(90, 29, 3, 3, WHITE);
      case 18: display.fillRect(85, 29, 3, 3, WHITE);
      case 17: display.fillRect(80, 29, 3, 3, WHITE);
      case 16: display.fillRect(75, 29, 3, 3, WHITE);
      case 15: display.fillRect(70, 29, 3, 3, WHITE);
      case 14: display.fillRect(65, 29, 3, 3, WHITE);
      case 13: display.fillRect(60, 29, 3, 3, WHITE);
      case 12: display.fillRect(55, 29, 3, 3, WHITE);
      case 11: display.fillRect(50, 29, 3, 3, WHITE);
      case 10: display.fillRect(45, 29, 3, 3, WHITE);
      case 9: display.fillRect(40, 29, 3, 3, WHITE);
      case 8: display.fillRect(35, 29, 3, 3, WHITE);
      case 7: display.fillRect(30, 29, 3, 3, WHITE);
      case 6: display.fillRect(25, 29, 3, 3, WHITE);
      case 5: display.fillRect(20, 29, 3, 3, WHITE);
      case 4: display.fillRect(15, 29, 3, 3, WHITE);
      case 3: display.fillRect(10, 29, 3, 3, WHITE);
      case 2: display.fillRect(5, 29, 3, 3, WHITE);
      case 1: display.fillRect(0, 29, 3, 3, WHITE);
      display.display();
      }
    }
   else
    {
    smvalTX = analogRead(meterTX); 
    y = map(smvalTX, 0, 512, 1, 26); 
    if (y > 26) y = 26;
    switch (y) 
      {
      case 26: display.fillRect(125, 29, 3, 3, WHITE);
      case 25: display.fillRect(120, 29, 3, 3, WHITE);
      case 24: display.fillRect(115, 29, 3, 3, WHITE);
      case 23: display.fillRect(110, 29, 3, 3, WHITE);
      case 22: display.fillRect(105, 29, 3, 3, WHITE);
      case 21: display.fillRect(100, 29, 3, 3, WHITE);
      case 20: display.fillRect(95, 29, 3, 3, WHITE);
      case 19: display.fillRect(90, 29, 3, 3, WHITE);
      case 18: display.fillRect(85, 29, 3, 3, WHITE);
      case 17: display.fillRect(80, 29, 3, 3, WHITE);
      case 16: display.fillRect(75, 29, 3, 3, WHITE);
      case 15: display.fillRect(70, 29, 3, 3, WHITE);
      case 14: display.fillRect(65, 29, 3, 3, WHITE);
      case 13: display.fillRect(60, 29, 3, 3, WHITE);
      case 12: display.fillRect(55, 29, 3, 3, WHITE);
      case 11: display.fillRect(50, 29, 3, 3, WHITE);
      case 10: display.fillRect(45, 29, 3, 3, WHITE);
      case 9: display.fillRect(40, 29, 3, 3, WHITE);
      case 8: display.fillRect(35, 29, 3, 3, WHITE);
      case 7: display.fillRect(30, 29, 3, 3, WHITE);
      case 6: display.fillRect(25, 29, 3, 3, WHITE);
      case 5: display.fillRect(20, 29, 3, 3, WHITE);
      case 4: display.fillRect(15, 29, 3, 3, WHITE);
      case 3: display.fillRect(10, 29, 3, 3, WHITE);
      case 2: display.fillRect(5, 29, 3, 3, WHITE);
      case 1: display.fillRect(0, 29, 3, 3, WHITE);
      display.display();
      }
    }
  }
