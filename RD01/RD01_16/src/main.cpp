#include "metodos.h"

// Punto de entrada Arduino. setup() prepara los periféricos y servicios; loop()
// atiende el panel táctil, el UART del lector y la supervisión de la conexión Wi-Fi.
void setup()
{
  // Mantiene activa la alimentación externa del recolector desde el pin de control.
  pinMode(ONOFF, OUTPUT);
  digitalWrite(ONOFF, HIGH);
  delay(200);

  // Guarda el instante de inicio para mostrar el tiempo de actividad y gestionar el reposo.
  tiempo_encendido = esp_timer_get_time() / 1000000;

  // Serial sirve para diagnóstico USB; Serial2 enlaza el lector en UART a 9600 baudios.
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // serial2 pines 16 y 17 el tx del lector va al 16 (RX) del esp32
  palabra.reserve(MAX_SCANNER_CODE_LENGTH);
  Serial.println("DBG 1: serial ok");

  // I2C se comparte con el controlador Goodix; se asignan los pines cableados en la placa.
  Wire.setPins(13, 12); // cambio de pin 21 al 13 para SDA y 22 al 12 para SCL por que el lolin no tiene pin 21 y el 22 tiene un led
  Wire.begin();
  Wire.setClock(400000);
  delay(300);
  Serial.println("DBG 2: wire ok");

  // Registra el callback de toques y configura el controlador táctil.
  touch.setHandler(handleTouch);

  touchStart();
  Serial.println("DBG 3: touch ok");

  // Inicializa el display SPI y dibuja el botón persistente de apagado.
  display.begin();
  Serial.println("DBG 4: display.begin ok");
  display.setRotation(0); // 0, 1, 2, 3 "1"
  teclaApagado(45, 422);
  Serial.println("DBG 5: teclaApagado ok");

  // Abre NVS y recupera el servidor configurado; si no existe, guarda el servidor inicial.
  preferences.begin("credenciales", false);
  servidor = preferences.getString("servidor", "");
  if (servidor == "")//Si no hay servidor definido asigna por defecto el newfac apumanque
  {
    preferences.begin("credenciales", false);
    preferences.putString("servidor", "192.168.2.3");
  }

  // La conexión puede procesar las credenciales recibidas por el lector.
  Serial.println("DBG 6: preferences ok");
  conectarWiFi();
  Serial.println("DBG 7: wifi ok");
  // Dibuja indicadores, configura el lector e intenta actualizar el firmware al arrancar.
  panFondo();
  configEscaner();
  Serial.println("DBG 8: escaner ok");
  // Si aún no hay Wi-Fi, dejar el lector activo permite escanear el QR de configuración.
  Serial.println("DBG 9: setup fin");
}

int vuelta = 0;
void loop()
{
  // Refresca los indicadores cada 500 iteraciones del ciclo principal.
  if (vuelta == 500)
  {
    vuelta = 0;

    panFondo();
  }
  ++vuelta;

  // El controlador táctil consulta la marca puesta por la interrupción y despacha
  // coordenadas a handleTouch() cuando hay un nuevo informe.
  touch.loop();
  delay(10);

  // Drena los bytes UART disponibles; leerEscaner() procesa el código al recibir CR.
  while (Serial2.available())
  {
    leerEscaner();
  }

  gestionarWiFi();
}
