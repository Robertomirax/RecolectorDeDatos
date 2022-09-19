#include "metodos.h"

void setup()
{
  // inicializar pin onoff con fuente encendida
  pinMode(ONOFF, OUTPUT);
  digitalWrite(ONOFF, HIGH);
  delay(200);

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

  // inicializa el display
  display.begin();
  display.setRotation(2); // 0, 1, 2, 3 "1"
  teclaApagado(45, 422);

  conectarWiFi();
  estado = PANTALLA_1;
  // requiereServidor("0");
  poneNumeros();
  
}

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
      int porciento = 0;
      if (volt2 > 408)
      {
        porciento = 100;
      }
      else if (volt2 > 400)
      {
        porciento = 90;
      }
      else if (volt2 > 393)
      {
        porciento = 80;
      }
      else if (volt2 > 387)
      {
        porciento = 70;
      }
      else if (volt2 > 382)
      {
        porciento = 60;
      }
      else if (volt2 > 379)
      {
        porciento = 50;
      }
      else if (volt2 > 377)
      {
        porciento = 40;
      }
      else if (volt2 > 373)
      {
        porciento = 30;
      }
      else if (volt2 > 370)
      {
        porciento = 20;
      }
      else if (volt2 > 368)
      {
        porciento = 15;
      }
      else if (volt2 > 350)
      {
        porciento = 10;
      }
      else if (volt2 > 250)
      {
        porciento = 5;
      }
      else
      {
        porciento = 0;
      }

      Serial.print("voltaje: ");
      Serial.print(volt2);
      Serial.print("_____ ");
      Serial.print(porciento);
      Serial.println("%");
      bateria(porciento);
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
