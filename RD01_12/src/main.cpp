#include "metodos.h"

void setup()
{
  // inicializar pin on off con fuente encendida
  pinMode(ONOFF, OUTPUT);
  digitalWrite(ONOFF, HIGH);
  delay(200);

  // inicializa contador de tiempo encendido
  tiempo_encendido = esp_timer_get_time() / 1000000;

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
  display.setRotation(0); // 0, 1, 2, 3 "1"
  teclaApagado(45, 422);

  conectarWiFi();
  panFondo();
  configEscaner();
  verificaFirmware();
  escanerOff();
  
}

int vuelta = 0;
void loop()
{
  
  if (vuelta == 500)
  {
    vuelta = 0;

    panFondo();
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
