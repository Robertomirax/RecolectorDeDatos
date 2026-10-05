#include "metodos.h"

void setup()
{
  // inicializar pin on off con fuente encendida
  pinMode(ONOFF, OUTPUT);
  digitalWrite(ONOFF, HIGH);
  delay(200);

  // inicializa contador de tiempo encendido
  tiempo_encendido = esp_timer_get_time() / 1000000;

  // iniciar la conexi�n serie con el lector de barras y el terminal de programaci�n
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // serial2 pines 16 y 17 el tx del lector va al 16 (RX) del esp32
  Serial.println("DBG 1: serial ok");

  // inicia Wire usado para el sensor tactil
  Wire.setPins(13, 12); // cambio de pin 21 al 13 para SDA y 22 al 12 para SCL por que el lolin no tiene pin 21 y el 22 tiene un led
  Wire.begin();
  Wire.setClock(400000);
  delay(300);
  Serial.println("DBG 2: wire ok");

  // inicio del sensor tactil capacitivo
  touch.setHandler(handleTouch);

  touchStart();
  Serial.println("DBG 3: touch ok");

  // inicializa el display
  display.begin();
  Serial.println("DBG 4: display.begin ok");
  display.setRotation(0); // 0, 1, 2, 3 "1"
  teclaApagado(45, 422);
  Serial.println("DBG 5: teclaApagado ok");

  // define servidor
  preferences.begin("credenciales", false);
  servidor = preferences.getString("servidor", "");
  if (servidor == "")//Si no hay servidor definido asigna por defecto el newfac apumanque
  {
    preferences.begin("credenciales", false);
    preferences.putString("servidor", "192.168.2.3");
  }

  Serial.println("DBG 6: preferences ok");
  conectarWiFi();
  Serial.println("DBG 7: wifi ok");
  panFondo();
  configEscaner();
  Serial.println("DBG 8: escaner ok");
  verificaFirmware();
  escanerOff();
  Serial.println("DBG 9: setup fin");
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

  // ver si se toc� el display
  touch.loop();
  delay(10);

  // Leer los datos provenientes del escaner si est�n disponibles
  while (Serial2.available())
  {
    leerEscaner();
  }
}
