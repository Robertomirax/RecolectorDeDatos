
#include <Wire.h>
#include "Goodix.h"
#include "display.h"

#define INT_PIN 5
#define RST_PIN 27

// declaración de funciones ---------------------------------------------------

void handleTouch(int8_t contacts, GTPoint *points);
void touchStart();
void teclado(int tecla);

// fin declaración de funciones -----------------------------------------------
Goodix touch = Goodix();

uint32_t tiempoUltNum = xTaskGetTickCount(); // registra el momento en que se tocó el último número
String visor = "";

// rutina que se ejecuta al tocar la pantalla
void handleTouch(int8_t contacts, GTPoint *points)
{
  // verificar que se levantó el dedo
  uint32_t tiempoNum = xTaskGetTickCount();
  if (tiempoNum > tiempoUltNum + 50) // no hubo señal INT durante 50 ticks
  {

    // Serial.printf("Contacts: %d\n", contacts);
    for (uint8_t i = 0; i < contacts; i++)
    {
      // Serial.printf("C%d: %d %d \n", points[i].trackId, 320 - points[i].x, 480 - points[i].y);
      if (modo == MODO_TECLADO_NUMEROS)
      {
        int numero = tocoPantalla(320 - points[i].x, 480 - points[i].y);
        if (numero != -1)
        {
          teclado(numero);
        }
      }
    }
  }
  tiempoUltNum = tiempoNum;
}

void touchStart()
{
  unsigned short configInfo;
  touch.begin(INT_PIN, RST_PIN, GOODIX_I2C_ADDR_BA);
  Serial.print("Check ACK on addr request on 0x");
  Serial.print(touch.i2cAddr, HEX);
  Wire.beginTransmission(touch.i2cAddr);
  if (!Wire.endTransmission())
  {
    Serial.println(": SUCCESS");
  }
  else
  {
    Serial.print(": ERROR!");
  }
  configInfo = touch.configCheck(false);
  if (!configInfo)
  {
    Serial.println("Config is OK");
  }
  else
  {
    Serial.print("Config ERROR: ");
    Serial.println(configInfo);
  }
}

/***************************************************************************************/
/*!
    @brief   Se ejecuta al tocar una tecla en la pantalla
    @param   tecla número o letra tocada
*/
/***************************************************************************************/
void teclado(int tecla)
{
  Serial.print(tecla);
  
  if (tecla == 10)
  {
    display.fillRoundRect(5, 5, 310, 50, 10, WHITE);
    visor = "";
  }
  else if (visor.length() < 11)
  {
    visor += tecla;
        
    display.setCursor(10, 40);
    display.setFont(u8g2_font_inb30_mf);
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.print(visor);
  }
}