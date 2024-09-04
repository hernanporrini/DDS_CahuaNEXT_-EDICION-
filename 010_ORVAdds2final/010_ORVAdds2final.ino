


#include <EEPROM.h>               
#include <Wire.h>                 
#include <Rotary.h>               //https://github.com/brianlow/Rotary
#include <si5351.h>               //https://github.com/etherkit/Si5351Arduino
#include <Adafruit_GFX.h>         //https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h>     //https://github.com/adafruit/Adafruit_SSD1306

#define IF         2000000      //el valor de la frecuencia del filtro a cristal
#define tunestep   4         //Este pin es el pulsador central del rotary, se usa para cambiar el paso
#define band80     12        //Este pin se usa para que el micro sepa que se selecciono la banda de 80
#define band40     11        //Este pin se usa para que el micro sepa que se selecciono la banda de 40
#define band30     10        //Este pin se usa para que el micro sepa que se selecciono la banda de 30
#define band20      9        //Este pin se usa para que el micro sepa que se selecciono la banda de 20
//#define pinTRX     A0        //Este pin se usa para que el micro sepa si esta en TX o RX
//#define meterTX    A2        //pin de entrada analogica del indicador de recepcion
//#define meterRX    A3        //pin de entrada analogica del indicador de potencia de salida

Rotary r = Rotary(3, 2);     //definicion del rotary switch en los pines 2 y 3

Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);     //definicion del tamaño del display oled 
Si5351 si5351(0x60);                                             //definicion y direccion i2c del Si5351 en 0x60

unsigned long freq=100000, freqmem, freqbanda=5000000, freqlcd, freqout, freqold=0, fstep;
unsigned long ultimoTiempoGuardado = 0;
const unsigned long intervaloGuardado = 30000;
long cal = 145000;
unsigned int smval;
byte encoder = 1;
byte stp, n = 1;
byte count, y, x, xo;
bool sts = 0;
bool modo = 1;
unsigned int period = 100;
unsigned long time_now = 0;
int banda=4;                 //banda 0=80m, 1=40m, 2=30m, 3=20m
int save=0;
    
ISR(PCINT2_vect) {
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


void setup() {
  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();

  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  pinMode(tunestep, INPUT_PULLUP);
  //pinMode(pinTRX, INPUT_PULLUP);
  pinMode(band80, INPUT_PULLUP);
  pinMode(band40, INPUT_PULLUP);
  pinMode(band30, INPUT_PULLUP);
  pinMode(band20, INPUT_PULLUP);
  
  statup_text();  //SALUDO INICIAL
  
    
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
  si5351.set_correction(cal, SI5351_PLL_INPUT_XO);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);
  si5351.output_enable(SI5351_CLK0, 1);                  //1 - HABILITADO / 0 - DESHABILITADO
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 0);

  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
  sei();

  stp = 2;
  setstep();
  EEPROM.get(0,freq);
  freqold = freq;
  if (digitalRead(band80) == LOW && banda!=0) {freqbanda= 3500000;banda=0;modo=1;}
  if (digitalRead(band40) == LOW && banda!=1) {freqbanda= 6800000;banda=1;modo=1;}
  if (digitalRead(band30) == LOW && banda!=2) {freqbanda= 9800000;banda=2;modo=1;}
  if (digitalRead(band20) == LOW && banda!=3) {freqbanda=13800000;banda=3;modo=0;}
  tunegen();  
  
}

void loop() {
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
  

  if (digitalRead(tunestep) == LOW) {
    time_now = (millis() + 300);
    delay(500);
    if (digitalRead(tunestep) == LOW) 
    {
    modo = !modo;
    tunegen();
    }
    else setstep();
    delay(500);
    displayfreq();
    layout();
  }

  if (digitalRead(band80) == LOW && banda!=0) 
    {
    banda=0;
    freqbanda=3500000;
    delay(300);
    modo=1;
    tunegen();
    }
  if (digitalRead(band40) == LOW && banda!=1) 
    {
    banda=1;
    freqbanda=6800000;
    delay(300);
    modo=1;
    tunegen();
    }
  if (digitalRead(band30) == LOW && banda!=2) 
    {
    banda=2;
    freqbanda=9900000;
    delay(300);
    modo=1;
    tunegen();
    }
  if (digitalRead(band20) == LOW && banda!=3) 
    {
    banda=3;
    freqbanda=13800000;
    delay(300);
    modo=0;
    tunegen();
    }
  
  displayfreq();
  layout();
  
//  sgnalread();
}

void tunegen() 
  {
  if(modo)  freqout= freqbanda + freq - IF ; 
  else      freqout= freqbanda + freq + IF ;
  si5351.set_freq(freqout * 100ULL, SI5351_CLK0); 
  //  si5351.set_freq(1000000 * 100ULL, SI5351_CLK0); 
  }


void displayfreq() {
  freqlcd= freqbanda + freq ;
  unsigned int m = freqlcd / 1000000;
  unsigned int k = (freqlcd % 1000000) / 1000;
  unsigned int h = (freqlcd % 1000) / 10;

  display.clearDisplay();
  
  display.drawLine( 33, 52, 33, 53, WHITE);
  display.drawLine( 34, 51, 34, 54, WHITE);
  display.drawLine( 35, 50, 35, 55, WHITE);
  display.drawLine( 36, 51, 36, 54, WHITE);
  display.drawLine( 37, 52, 33, 53, WHITE);
  
  display.drawLine( 87, 52, 87, 53, WHITE);
  display.drawLine( 88, 51, 88, 54, WHITE);
  display.drawLine( 89, 50, 89, 55, WHITE);
  display.drawLine( 90, 51, 90, 54, WHITE);
  display.drawLine( 91, 52, 91, 53, WHITE);
  
  
  display.setTextSize(3);

  char buffer[15] = "";
  display.setCursor(1, 30); sprintf(buffer, "%2d%003d%02d", m, k, h);
  display.print(buffer);
  
}

void setstep() {
  switch (stp) {
    case 1: stp = 2; fstep = 100; break;
    case 2: stp = 3; fstep = 1000; break;
    case 3: stp = 4; fstep = 10000; break;
    case 4: stp = 1; fstep = 10; break;
  }
}



void layout() {
  display.setTextColor(WHITE);
  display.drawLine(0, 17, 127, 17, WHITE);
  display.setTextSize(2);
  display.setCursor(20, 0);
  if (stp == 1) {display.print(" 10Hz");display.drawLine(111, 56, 121, 56, WHITE);display.drawLine(110, 57, 122, 57, WHITE);display.drawLine(109, 58, 123, 58, WHITE);} 
  if (stp == 2) {display.print("100Hz");display.drawLine( 93, 56, 103, 56, WHITE);display.drawLine( 92, 57, 104, 57, WHITE);display.drawLine( 91, 58, 105, 58, WHITE);} 
  if (stp == 3) {display.print(" 1kHz");display.drawLine( 75, 56,  85, 56, WHITE);display.drawLine( 74, 57,  86, 57, WHITE);display.drawLine( 73, 58,  87, 58, WHITE);}
  if (stp == 4) {display.print("10kHz");display.drawLine( 57, 56,  67, 56, WHITE);display.drawLine( 56, 57,  68, 57, WHITE);display.drawLine( 55, 58,  69, 58, WHITE);} 
  display.setCursor(91, 0);
  if (modo) display.print("LSB");
  else display.print("USB"); 
  display.setTextSize(2);
  display.setCursor(5, 0); 
  if(save) display.print("S"); else display.print(" ");
  
//  drawbargraph();
  display.display();
}

//void sgnalread() {
//  smval = analogRead(meterRX); x = map(smval, 50, 512, 1, 26); if (x > 26) x = 26;
//  y = map(analogRead(meterTX), 100, 512, 1, 13); if (y > 13) y = 13;
//}

/*void drawbargraph() {
  display.setTextSize(2);

  if(digitalRead(pinTRX)==0)
  {
  display.setCursor(0, 0); 
  display.print("TX");
  switch (y) {
    
    case 13: display.fillRect( 0, 36, 2, 30, WHITE);display.fillRect(125, 36, 2, 30, WHITE);
    case 12: display.fillRect( 5, 37, 2, 28, WHITE);display.fillRect(120, 37, 2, 28, WHITE);
    case 11: display.fillRect(10, 38, 2, 26, WHITE);display.fillRect(115, 38, 2, 26, WHITE);
    case 10: display.fillRect(15, 39, 2, 24, WHITE);display.fillRect(110, 39, 2, 24, WHITE);
    case 9 : display.fillRect(20, 40, 2, 22, WHITE);display.fillRect(105, 40, 2, 22, WHITE);
    case 8 : display.fillRect(25, 41, 2, 20, WHITE);display.fillRect(100, 41, 2, 20, WHITE);
    case 7 : display.fillRect(30, 42, 2, 18, WHITE);display.fillRect(95, 42, 2, 18, WHITE);
    case 6 : display.fillRect(35, 43, 2, 16, WHITE);display.fillRect(90, 43, 2, 16, WHITE);
    case 5 : display.fillRect(40, 44, 2, 14, WHITE);display.fillRect(85, 44, 2, 14, WHITE);
    case 4 : display.fillRect(45, 45, 2, 12, WHITE);display.fillRect(80, 45, 2, 12, WHITE);
    case 3 : display.fillRect(50, 46, 2, 10, WHITE);display.fillRect(75, 46, 2, 10, WHITE);
    case 2 : display.fillRect(55, 47, 2, 8, WHITE);display.fillRect(70, 47, 2, 8, WHITE);
    case 1 : display.fillRect(60, 48, 2, 6, WHITE);display.fillRect(65, 48, 2, 6, WHITE);
  
  }}

  if(digitalRead(pinTRX)) 
  {
  display.setCursor(0, 0); 
  display.print("RX");
  switch (x) {
    case 26: display.fillRect(125, 38, 3, 26, WHITE);
    case 25: display.fillRect(120, 39, 3, 25, WHITE);
    case 24: display.fillRect(115, 40, 3, 24, WHITE);
    case 23: display.fillRect(110, 41, 3, 23, WHITE);
    case 22: display.fillRect(105, 42, 3, 22, WHITE);
    case 21: display.fillRect(100, 43, 3, 21, WHITE);
    case 20: display.fillRect(95, 44, 3, 20, WHITE);
    case 19: display.fillRect(90, 45, 3, 19, WHITE);
    case 18: display.fillRect(85, 46, 3, 18, WHITE);
    case 17: display.fillRect(80, 47, 3, 17, WHITE);
    case 16: display.fillRect(75, 48, 3, 16, WHITE);
    case 15: display.fillRect(70, 49, 3, 15, WHITE);
    case 14: display.fillRect(65, 50, 3, 14, WHITE);
    case 13: display.fillRect(60, 51, 3, 13, WHITE);
    case 12: display.fillRect(55, 52, 3, 12, WHITE);
    case 11: display.fillRect(50, 53, 3, 11, WHITE);
    case 10: display.fillRect(45, 54, 3, 10, WHITE);
    case 9: display.fillRect(40, 55, 3, 9, WHITE);
    case 8: display.fillRect(35, 56, 3, 8, WHITE);
    case 7: display.fillRect(30, 57, 3, 7, WHITE);
    case 6: display.fillRect(25, 58, 3, 6, WHITE);
    case 5: display.fillRect(20, 59, 3, 5, WHITE);
    case 4: display.fillRect(15, 60, 3, 4, WHITE);
    case 3: display.fillRect(10, 61, 3, 4, WHITE);
    case 2: display.fillRect(5, 62, 3, 2, WHITE);
    case 1: display.fillRect(0, 63, 3, 1, WHITE);
      }}
  
}
*/
void statup_text() {
  display.setTextSize(3);
  display.setCursor(1, 8);
  display.print("ORVAdds");
  display.setTextSize(1);
  display.setCursor(6, 35);
  display.print("Hector LU7 EHJ");
  display.setCursor(6, 50);
  display.print("by HernanLW1EHP");
  display.setTextSize(4);
  display.setCursor(102, 32);
  display.print("2");
  display.display(); 
  delay(5000);
}
