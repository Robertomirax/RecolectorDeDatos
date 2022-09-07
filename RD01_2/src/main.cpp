#include "metodos.h"

void setup()
{
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

  
  conectarWiFi();
  estado = INICIO;
  requiereServidor("0");
  poneNumeros();
}

void loop()
{
  // ver si se tocó el display
  touch.loop();
  delay(10);

  // Leer los datos provenientes del escaner si están disponibles
  while (Serial2.available())
  {
    leerEscaner();
  }
}