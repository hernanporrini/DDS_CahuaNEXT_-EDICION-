#include <SPI.h>
#include <EEPROM.h>               
#include <Wire.h>                 
#include <Rotary.h>               //https://github.com/brianlow/Rotary
#include <si5351.h>               //https://github.com/etherkit/Si5351Arduino
#include <Adafruit_GFX.h>         //https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h>     //Adafruit SSD1306 https://github.com/adafruit/Adafruit_SSD1306


#define IF         2000000      //el valor de la frecuencia del filtro a cristal
#define tunestep   4         //Este pin es el pulsador central del rotary, se usa para cambiar el paso
#define band80     12        //Este pin se usa para que el micro sepa que se selecciono la banda de 60
#define band40     11        //Este pin se usa para que el micro sepa que se selecciono la banda de 40
#define band20     10        //Este pin se usa para que el micro sepa que se selecciono la banda de 20
#define pinTRX     A2        //Este pin se usa para que el micro sepa si esta en TX o RX
#define meterVDC   A0        //pin de entrada analogica del indicador de tension de entrada
#define meterTX    A3        //pin de entrada analogica del indicador de potencia de salida
#define meterRX    A1        //pin de entrada analogica del indicador de recepcion

Rotary r = Rotary(2, 3);     //definicion del rotary switch en los pines 2 y 3

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);
Si5351 si5351(0x60);                                             //definicion y direccion i2c del Si5351 en 0x60

unsigned long freq=100000, freqmem, freqbanda=5000000, freqlcd, freqout, freqold=0, fstep;
unsigned long ultimoTiempoGuardado = 0;
const unsigned long intervaloGuardado = 30000;
long cal = 136816;
unsigned int smvalTX;
unsigned int smvalRX;
byte encoder = 1;
byte stp=2;
byte n = 1;
byte y, x;
bool modo = 0;
bool save = 1;
int banda=0;                 //banda 1=60m, 2=40m, 3=20m
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
    {freq = freq - fstep; if (freq<= 100000) freq= 100000;}
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
  pinMode(2,INPUT_PULLUP);
  pinMode(3,INPUT_PULLUP);
  pinMode(tunestep,INPUT_PULLUP);
  pinMode(pinTRX, INPUT);
  pinMode(band80, INPUT);
  pinMode(band40, INPUT);
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
  if (digitalRead(band80) == HIGH && banda!=1) {freqbanda= 3400000;banda=1;modo=0;}
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
    freqbanda=3400000;
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
  if(modo)  freqout= freqbanda + freq - IF ; 
  else      freqout= freqbanda + freq + IF ;
  si5351.set_freq(freqout * 100ULL, SI5351_CLK2); 
  si5351.set_freq(1000000 * 100ULL, SI5351_CLK0); 
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
  display.setCursor(5,10); sprintf(buffer, "%2d,%003d.%003d", m, k, h);
  display.print(buffer);
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(74, 0);
  if (stp == 4) {display.print("10kHz");display.drawLine( 53, 26,  63, 26, WHITE);display.drawLine( 53, 25,  63, 25, WHITE);} 
  if (stp == 3) {display.print(" 1kHz");display.drawLine( 65, 26,  75, 26, WHITE);display.drawLine( 65, 25,  75, 25, WHITE);} 
  if (stp == 2) {display.print("100Hz");display.drawLine( 89, 26,  99, 26, WHITE);display.drawLine( 89, 25,  99, 25, WHITE);}
  if (stp == 1) {display.print(" 10Hz");display.drawLine(101, 26, 111, 26, WHITE);display.drawLine(101, 25, 111, 25, WHITE);}
  display.setCursor(110, 0);
  if (modo) display.print("USB");
  else display.print("LSB"); 
  display.setTextSize(1);
  display.setCursor(0, 0); 
  if(save) display.print("SAVE"); else display.print("    ");

  display.drawPixel(16, 31,WHITE);
  display.drawPixel(25, 31,WHITE);
  display.drawPixel(34, 31,WHITE);
  display.drawPixel(43, 31,WHITE);
  display.drawPixel(52, 31,WHITE);
  display.drawPixel(61, 31,WHITE);
  display.drawPixel(70, 31,WHITE);
  display.drawPixel(79, 31,WHITE);
  display.drawPixel(88, 31,WHITE);
  display.drawPixel(97, 31,WHITE);
  display.drawPixel(106,31,WHITE);
  display.drawPixel(115,31,WHITE);
  
  
  //display.setCursor(20, 0);
  //display.print("VFO A");

  display.setCursor( 30, 0);
  volt=(float(analogRead(meterVDC))/1024.00)*28.00;
  display.print(volt);
  display.print("V");
  
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
    x = map(smvalRX, 200, 1024, 1, 12); 
    if (x > 12) x = 12;
    switch (x) 
      {
      case 12: display.fillRect(112, 29, 7, 3, WHITE);
      case 11: display.fillRect(103, 29, 7, 3, WHITE);
      case 10: display.fillRect( 94, 29, 7, 3, WHITE);
      case 9:  display.fillRect( 85, 29, 7, 3, WHITE);
      case 8:  display.fillRect( 76, 29, 7, 3, WHITE);
      case 7:  display.fillRect( 67, 29, 7, 3, WHITE);
      case 6:  display.fillRect( 58, 29, 7, 3, WHITE);
      case 5:  display.fillRect( 49, 29, 7, 3, WHITE);
      case 4:  display.fillRect( 40, 29, 7, 3, WHITE);
      case 3:  display.fillRect( 31, 29, 7, 3, WHITE);
      case 2:  display.fillRect( 22, 29, 7, 3, WHITE);
      case 1:  display.fillRect( 13, 29, 7, 3, WHITE);
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
      case 12: display.fillRect(112, 29, 7, 3, WHITE);
      case 11: display.fillRect(103, 29, 7, 3, WHITE);
      case 10: display.fillRect( 94, 29, 7, 3, WHITE);
      case 9:  display.fillRect( 85, 29, 7, 3, WHITE);
      case 8:  display.fillRect( 76, 29, 7, 3, WHITE);
      case 7:  display.fillRect( 67, 29, 7, 3, WHITE);
      case 6:  display.fillRect( 58, 29, 7, 3, WHITE);
      case 5:  display.fillRect( 49, 29, 7, 3, WHITE);
      case 4:  display.fillRect( 40, 29, 7, 3, WHITE);
      case 3:  display.fillRect( 31, 29, 7, 3, WHITE);
      case 2:  display.fillRect( 22, 29, 7, 3, WHITE);
      case 1:  display.fillRect( 13, 29, 7, 3, WHITE);
      display.display();
      }
    }
  }
