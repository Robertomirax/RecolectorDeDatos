
#include "metodos.h"

// -------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------
void setup(void)
{
  // inicializar pin onoff con fuente encendida
  pinMode(ONOFF, OUTPUT);
  digitalWrite(ONOFF, HIGH);
  delay(200);
  
  //Inicializar display
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  teclaApagado(45, 422);

  // iniciar la conexión serie con el lector de barras y el terminal de programación
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // serial2 pines 16 y 17 el tx del lector va al 16 (RX) del esp32

  // inicia Wire usado para el sensor tactil
  Wire.setPins(13, 12); // cambio de pin 21 al 13 para SDA y 22 al 12 para SCL por que el lolin no tiene pin 21 y el 22 tiene un led
  Wire.setClock(400000);
  Wire.begin();
  delay(300);

  // inicio del sensor tactil capacitivo
  touch.setHandler(handleTouch);
  touchStart();

  conectarWiFi();
}

// -------------------------------------------------------------------------
// Main loop
// -------------------------------------------------------------------------
int volts[10];
int i = 0;
int vuelta = 0;
void loop()
{
  if (vuelta == 100)
  {
    vuelta = 0;

    // medir voltaje de la batería
    if (i < 10)
    {
      volts[i] = analogReadMilliVolts(VOLTAJE);
      ++i;
    }
    else
    {
      i = 0;
      int volt1 = 0;
      for (size_t a = 0; a < 10; a++)
      {
        volt1 += volts[a];
      }
      int volt2 = round(1.754 * volt1 / 100);
      
      bateria(volt2);
    }
  }
  ++vuelta;

  // ver si se tocó el display
  touch.loop();
  delay(10);

  // Leer los datos provenientes del escaner si están disponibles
  while (Serial2.available())
  {
    leerEscaner();
  }
}
