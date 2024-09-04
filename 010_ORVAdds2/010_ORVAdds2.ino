


#include <EEPROM.h>               
#include <Wire.h>                 
#include <Rotary.h>               //https://github.com/brianlow/Rotary
#include <si5351.h>               //https://github.com/etherkit/Si5351Arduino
#include <Adafruit_GFX.h>         //https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h>     //https://github.com/adafruit/Adafruit_SSD1306

#define IF         2000      //el valor de la frecuencia del filtro a cristal
#define XT_CAL_F   152000     //Si5351 factor de calibracion , ajustar exactamnente 10MHz. incrementando este valor se reduce la frecuencia generada y viceversa
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

unsigned long freq, freq80, freq40, freq30, freq20, freqold, fstep;
long interfreq = IF, interfreqold = 0;
long cal = XT_CAL_F;
unsigned int smval;
byte encoder = 1;
byte stp, n = 1;
byte count, y, x, xo;
bool sts = 0;
bool modo = 1;
unsigned int period = 100;
unsigned long time_now = 0;
int banda=0;                 //banda 0=80m, 1=40m, 2=30m, 3=20m

ISR(PCINT2_vect) {
  char result = r.process();
  if (result == DIR_CW) set_frequency(-1);
  else if (result == DIR_CCW) set_frequency(1);
}

void set_frequency(short dir) {
  if (encoder == 1) {                         
    if (banda==1)
      { if (dir == 1) freq = freq + fstep;
        if (freq >= 6000000) freq = 6000000;
        if (dir == -1) freq = freq - fstep;
        if (freq <= 3000000) freq = 3000000;
        freq80=freq; 
        EEPROM.put(0, freq80);
      }
    if (banda==2)
      { if (dir == 1) freq = freq + fstep;
        if (freq >= 8000000) freq = 8000000;
        if (dir == -1) freq = freq - fstep;
        if (freq <= 6000000) freq = 6000000;
        freq40=freq; 
        EEPROM.put(10, freq40);
      }
    if (banda==3)
      { if (dir == 1) freq = freq + fstep;
        if (freq >= 11000000) freq = 11000000;
        if (dir == -1) freq = freq - fstep;
        if (freq <= 9000000) freq = 9000000;
        freq30=freq; 
        EEPROM.put(20, freq30);
      }
    if (banda==4)
      { if (dir == 1) freq = freq + fstep;
        if (freq >= 15000000) freq = 15000000;
        if (dir == -1) freq = freq - fstep;
        if (freq <= 13000000) freq = 13000000;
        freq20=freq; 
        EEPROM.put(30, freq20);
      }
      
  
  }
  if (encoder == 1) {                       
    if (dir == 1) n = n + 1;
    if (n > 42) n = 1;
    if (dir == -1) n = n - 1;
    if (n < 1) n = 42;
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

  stp = 3;
  setstep();
}

void loop() {
  if (freqold != freq) {
    time_now = millis();
    tunegen();
    freqold = freq;
    }

  if (interfreqold != interfreq) {
    time_now = millis();
    tunegen();
    interfreqold = interfreq;
  }

  if (xo != x) {
    time_now = millis();
    xo = x;
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

  if (digitalRead(band80) == LOW && banda!=1) {
    time_now = (millis() + 300);
    EEPROM.get(0, freq80);             //RESTAURA FRECUENCIA DESDE LA EEPROM
    banda=1;
    freq =   freq80;
    delay(300);
  }
  if (digitalRead(band40) == LOW && banda!=2) {
    time_now = (millis() + 300);
    EEPROM.get(10, freq40);             //RESTAURA FRECUENCIA DESDE LA EEPROM
    banda=2;
    freq =   freq40;
    delay(300);
  }
  if (digitalRead(band30) == LOW && banda!=3) {
    time_now = (millis() + 300);
    EEPROM.get(20, freq30);             //RESTAURA FRECUENCIA DESDE LA EEPROM
    banda=3;
    freq =  freq30;
    delay(300);
  }
  if (digitalRead(band20) == LOW && banda!=4) {
    time_now = (millis() + 300);
    EEPROM.get(30, freq20);             //RESTAURA FRECUENCIA DESDE LA EEPROM
    banda=4;
    freq =  freq20;
    delay(300);
  }
  
  if ((time_now + period) > millis()) {
    displayfreq();
    layout();
  }
//  sgnalread();
}

void tunegen() {
  if(modo) {si5351.set_freq((freq + (interfreq * 1000ULL)) * 100ULL, SI5351_CLK0);}
  else {si5351.set_freq((freq - (interfreq * 1000ULL)) * 100ULL, SI5351_CLK0);}
}

void displayfreq() {
  unsigned int m = freq / 1000000;
  unsigned int k = (freq % 1000000) / 1000;
  unsigned int h = (freq % 1000) / 100;

  display.clearDisplay();
  display.setTextSize(3);

  char buffer[15] = "";
  display.setCursor(1, 30); sprintf(buffer, "%2d%003d.%1d", m, k, h);
  display.print(buffer);
}

void setstep() {
  switch (stp) {
    case 1: stp = 2; fstep = 100; break;
    case 2: stp = 3; fstep = 1000; break;
    case 3: stp = 4; fstep = 10000; break;
    case 4: stp = 1; fstep = 100000; break;
  }
}



void layout() {
  display.setTextColor(WHITE);
  display.drawLine(0, 17, 127, 17, WHITE);
  display.setTextSize(2);
  display.setCursor(4, 0);
  if (stp == 2) {display.print(" 100Hz");display.drawLine(109, 54, 123, 54, WHITE);display.drawLine(109, 55, 123, 55, WHITE);display.drawLine(109, 56, 123, 56, WHITE);} 
  if (stp == 3) {display.print("  1kHz");display.drawLine( 73, 54,  87, 54, WHITE);display.drawLine( 73, 55,  87, 55, WHITE);display.drawLine( 73, 56,  87, 56, WHITE);} 
  if (stp == 4) {display.print(" 10kHz");display.drawLine( 55, 54,  69, 54, WHITE);display.drawLine( 55, 55,  69, 55, WHITE);display.drawLine( 55, 56,  69, 56, WHITE);}
  if (stp == 1) {display.print("100kHz");display.drawLine( 37, 54,  51, 54, WHITE);display.drawLine( 37, 55,  51, 55, WHITE);display.drawLine( 37, 56,  51, 56, WHITE);} 
  display.setCursor(91, 0);
  if (modo) display.print("LSB");
  else display.print("USB"); 
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
  display.print("by H.Porrini");
  display.setTextSize(4);
  display.setCursor(102, 32);
  display.print("2");
  display.display(); 
  delay(5000);
}
