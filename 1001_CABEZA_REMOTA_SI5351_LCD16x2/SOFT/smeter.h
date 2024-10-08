int smeter_signal = 0;                // Nivel de señal inicial del S-Meter
#define T_REFRESH    100              // Tiempo de refresco en milisegundos para el S-Meter
#define T_PEAKHOLD   (3 * T_REFRESH)  // Timepo de refresco en milisegundos para el indicador de pico del S-Meter


/*
 * Caracteres especiales para dibujar la barra del S-Meter
 */
byte  fill[6] = {0x20, 0x00, 0x01, 0x02, 0x03, 0xFF};
byte  peak[7] = {0x20, 0x00, 0x04, 0x05, 0x06, 0x07, 0x20};
byte block[8][8] =
{
  {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10},
  {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18},
  {0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C},
  {0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E},

  {0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08},
  {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
  {0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02},
  {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
};

int lmax[2];
int dly[2];

long lastT = 0;

/*
 * S-Meter dibuja una barra en la línea inferior
 * de la pantalla para indicar la medición de RX
 * o modulación de TX. Se hace un cálculo de raíz
 * cuadrada para que la señal resultante sea
 * logarítmica.
 */
void smeter(int row, int lev) {
  lcd.setCursor(1, row);
  for (int i = 1; i < 13; i++) {
    int f = constrain(lev - i * 5, 0, 5);
    int p = constrain(lmax[row] - i * 5, 0, 6);
    if (f)
      lcd.write(fill[f]);
    else
      lcd.write(peak[p]);
  }
  if (lev > lmax[row]) {
    lmax[row] = lev;
    dly[row]  = -(T_PEAKHOLD) / T_REFRESH;
  }
  else {
    if (dly[row] > 0)
      lmax[row] -= dly[row];

    if (lmax[row] < 0)
      lmax[row] = 0;
    else
      dly[row]++;
  }
}
