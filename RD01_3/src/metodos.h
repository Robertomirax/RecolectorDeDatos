#include "Arduino.h"
#include "Preferences.h"
#include <Wire.h>
#include "Goodix.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>                 // Include the graphics library (this includes the sprite functions)

// pines
#define VOLTAJE 35    // Pin para medir Voltaje de la batería
#define ONOFF 26   // Control de apagado 1=encendido 0=apagado
#define RXD2 16    // RX para el lector de barras
#define TXD2 17    // TX para el lector de barras
#define INT_PIN 5  // INT del touch del tft
#define RST_PIN 27 // Reset del touch del tft

// constantes
#define FIRM_VERSION 3 // Versión del firmware actualmente instalado. Debe ser un número entero

// modos
#define PANTALLA_1 1 // Estado inicial despues del encendido o reset.
// fin modos

// fin constantes

// Crear instancias y variables
TFT_eSPI    tft = TFT_eSPI();         // Create object "tft"
//TFT_eSprite img = TFT_eSprite(&tft);  // Create Sprite object "img" with pointer to "tft" object
//                                    // the pointer is used by pushSprite() to push it onto the TFT
Preferences preferences; // objeto que maneja el almacenamiento en flash de los parámetros
DynamicJsonDocument doc(2048);
// StaticJsonDocument<1024> doc;
Goodix touch = Goodix();
uint32_t tiempoUltNum = xTaskGetTickCount(); // registra el momento en que se tocó el último número

String ssid{""};
String password{""};
String palabra{""};
byte letra{0};
byte estado = 0; // estado en el que se encuentra el recolector de datos
// fin Crear instancias y variables

// declaración de funciones ---------------------------------------------------
void handleTouch(int8_t contacts, GTPoint *points);
void touchStart();
void teclado(int tecla);
int tocoPantalla(uint16_t x, uint16_t y);
bool conectarWiFi();
void leerEscaner();
String getStringPartByNr(String data, char separator, int index);
String requiereServidor(String c);
void ejecutaComandos(JsonArray arr);
void logowifi(int posx, int posy);
void logowifioff(int posxoff, int posyoff);
void teclaApagado(u16_t posx, u16_t posy);
void bateria(int volt2);
void cambiarSsid(String ssid, String password);
//void dibujaTeclado(char botx = 4, char boty = 4, uint color = TFT_GREEN, bool fondo = false);
// fin declaración de funciones -----------------------------------------------


/***************************************************************************************/
/*!
    @brief   Dibuja el teclado en la pantalla
    @param   botx cantidad de botones en la horizontal
    @param   boty cantidad de botones en la vertical
    @param   s separación horizontal entre botones
    @param   v separación vertical entre botones
    @param   color color del botón 16-bit 5-6-5
    @param   fondo fondo del botón vacío = false o lleno = true
*/
/***************************************************************************************/
void dibujaTeclado(char botx = 4, char boty = 4, uint color = TFT_GREEN, bool fondo = false)
{
    int16_t ancho = 320;
    int16_t alto = 480;
    char x = 8;
    char y = 92;
    char r = 10;
    char s = 4;
    char v = 4;

    int16_t d = (ancho - 2 * x - (botx - 1) * s) / botx; // ancho del botón
    Serial.print("ancho del boton ");
    Serial.println(d);
    x = (ancho - d * botx - s * (botx - 1)) / 2; // distancia desde el margen izquierdo

    int16_t h = (alto - x - y - (boty - 1) * v) / boty; // alto del botón
    Serial.print("alto del boton ");
    Serial.println(h);

    for (size_t i = 0; i < botx; i++)
    {
        for (size_t z = 0; z < boty; z++)
        {
            if (fondo)
            {
                tft.fillRoundRect(i * (d + s) + x, z * (h + v) + y, d, h, r, color);
            }
            else
            {
                tft.drawRoundRect(i * (d + s) + x, z * (h + v) + y, d, h, r, color);
            }
        }
    }
}

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

            int numero = tocoPantalla(320 - points[i].x, 480 - points[i].y);
            if (numero != -1)
            {
                teclado(numero);
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

    if (tecla == 12) // apagado
    {
        Serial.println("apagando");
        digitalWrite(ONOFF, LOW); // apagar
    }
    else
    {

        String tocado = "0&tecla=";
        Serial.println(tocado + tecla);
        //requiereServidor(tocado + tecla);
    }
}

/***************************************************************************************/
/*!
    @brief   Se ejecuta al tocar la pantalla
    @param   x coordenada x donde se tocó
    @param   y coordenada y donde se tocó
    @return  devuelve la tecla tocada
*/
/***************************************************************************************/
int tocoPantalla(uint16_t x, uint16_t y)
{
    int numero = -1;

    // primera fila
    if (x > 4 && x < 84 && y > 90 && y < 187)
    {
        numero = 0;
    }

    if (x > 83 && x < 161 && y > 90 && y < 187)
    {
        numero = 1;
    }

    if (x > 160 && x < 238 && y > 90 && y < 187)
    {
        numero = 2;
    }

    if (x > 237 && x < 316 && y > 90 && y < 187)
    {
        numero = 3;
    }

    // segunda fila
    if (x > 4 && x < 84 && y > 186 && y < 283)
    {
        numero = 4;
    }

    if (x > 83 && x < 161 && y > 186 && y < 283)
    {
        numero = 5;
    }

    if (x > 160 && x < 238 && y > 186 && y < 283)
    {
        numero = 6;
    }

    if (x > 237 && x < 316 && y > 186 && y < 283)
    {
        numero = 7;
    }

    // tercera fila
    if (x > 4 && x < 84 && y > 282 && y < 379)
    {
        numero = 8;
    }

    if (x > 83 && x < 161 && y > 282 && y < 379)
    {
        numero = 9;
    }

    if (x > 160 && x < 238 && y > 282 && y < 379)
    {
        numero = 10;
    }

    if (x > 237 && x < 316 && y > 282 && y < 379)
    {
        numero = 11;
    }

    // cuarta fila
    if (x > 4 && x < 84 && y > 378 && y < 476)
    {
        numero = 12;
    }

    if (x > 83 && x < 161 && y > 378 && y < 476)
    {
        numero = 13;
    }

    if (x > 160 && x < 238 && y > 378 && y < 476)
    {
        numero = 14;
    }

    if (x > 237 && x < 316 && y > 378 && y < 476)
    {
        numero = 15;
    }

    return numero;
}

void bateria(int volt2)
{
    int color = TFT_WHITE;
    int porciento = 0;
      if (volt2 > 408)
      {
        porciento = 100;
        color = TFT_GREEN;
      }
      else if (volt2 > 400)
      {
        porciento = 90;
        color = TFT_GREEN;
      }
      else if (volt2 > 393)
      {
        porciento = 80;
        color = TFT_GREEN;
      }
      else if (volt2 > 387)
      {
        porciento = 70;
        color = TFT_YELLOW;
      }
      else if (volt2 > 382)
      {
        porciento = 60;
        color = TFT_YELLOW;
      }
      else if (volt2 > 379)
      {
        porciento = 50;
        color = TFT_YELLOW;
      }
      else if (volt2 > 377)
      {
        porciento = 40;
        color = TFT_YELLOW;
      }
      else if (volt2 > 373)
      {
        porciento = 30;
        color = TFT_RED;
      }
      else if (volt2 > 370)
      {
        porciento = 20;
        color = TFT_RED;
      }
      else if (volt2 > 368)
      {
        porciento = 15;
        color = TFT_RED;
      }
      else if (volt2 > 350)
      {
        porciento = 10;
        color = TFT_RED;
      }
      else if (volt2 > 250)
      {
        porciento = 5;
        color = TFT_RED;
      }
      else
      {
        porciento = 0;
        color = TFT_RED;
      }

      Serial.print("voltaje: ");
      Serial.print(volt2);
      Serial.print("_____ ");
      Serial.print(porciento);
      Serial.println("%");


    tft.fillRoundRect(5,0,80,22,5,color);
    tft.fillRoundRect(85,6,5,9,0,color);
    tft.setTextFont(4);
    tft.setCursor(10, 0);
    tft.setTextColor(TFT_BLACK);
    tft.print(porciento);
    tft.print(" %");
}


/***************************************************************************************/
/*!
    @brief   dibuja la tecla apagado
    @param   posx coordenada x donde se ubica
    @param   posy coordenada y donde se ubica
*/
/***************************************************************************************/
void teclaApagado(u16_t posx, u16_t posy)
{
    tft.fillRoundRect(posx - 37, posy - 42, 73, 92, 10, TFT_RED);
    tft.fillCircle(posx, posy+5, 25, TFT_WHITE);
    tft.fillCircle(posx, posy+5, 15, TFT_RED);
    tft.fillRect(posx - 8, posy - 20, 17, 30, TFT_RED);
    tft.fillRect(posx - 5, posy - 25, 11, 35, TFT_WHITE);
}


/***************************************************************************************/
/*!
    @brief   Conecta a WiFi
    @return  true si la conexión es exitosa, false si no
*/
/***************************************************************************************/
bool conectarWiFi()
{
    bool ok = false;
    preferences.begin("credenciales", false);
    ssid = preferences.getString("ssid", "");
    password = preferences.getString("password", "");
    preferences.end();
    //ssid = "ASUS";
    //password = "rdepmgdm";

    if (ssid == "" || password == "")
    {
        Serial.println("No existen Credenciales WiFi guardadas!");
        // mostrar mensaje en pantalla indicando que faltan credenciales de red
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(20, 60);
        tft.setTextColor(TFT_RED);
        tft.println("FALTAN LAS");
        tft.println("CREDENCIALES");
        tft.println("DE RED");
        logowifioff(290, 15);
        ok = false;
        // Leer los datos provenientes del escaner si están disponibles
        while (Serial2.available())
        {
            leerEscaner();
        }
    }
    else
    {
        // Conectarse
        //tft.fillScreen(BLACK);
        logowifioff(290, 15);
        byte intentos = 0;
        WiFi.mode(WIFI_STA);
        Serial.println(ssid.c_str());
        Serial.println(password.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.println("Credenciales cargadas de memoria!\nConectando al WiFi");

        tft.setCursor(0, 60);
        tft.setTextFont(4);
        //tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE);
        tft.println();
        tft.println("CONECTANDO");
        tft.print("A ");
        tft.setTextColor(TFT_YELLOW);
        tft.println(ssid);

        while ((WiFi.status() != WL_CONNECTED) and (intentos < 5))
        {
            delay(1000);
            intentos += 1;
        }

        if ((WiFi.status() == WL_CONNECTED))
        {
            ok = true;
            //tft.fillScreen(BLACK);
            logowifi(290, 15);
            Serial.println();
            Serial.println(WiFi.localIP());
            Serial.println("leyendo codigo");
        }
        else
        {
            ok = false;
            Serial.print("Error, no es posible conectarse al wifi ");
            Serial.println(ssid.c_str());
            delay(3000);
            // guardar motivo del reset (_4) en preferences
            preferences.begin("parametros", false);
            preferences.putString("reset", "_4");
            preferences.end();
            esp_restart(); // Resetea el esp32
        }
    }
    return ok;
}

void cambiarSsid(String ssid, String password)
{
    // Guardar/reemplazar namespace
    preferences.begin("credenciales", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    Serial.println("Se guardaron las Credenciales\n.");
    conectarWiFi();
}

/**************************************************************************************/
/*!
    @brief   Lee los datos recibidos desde el escaner
*/
/*************************************************************************************/
void leerEscaner()
{
    letra = Serial2.read();

    if (letra == 13)
    {
        Serial.println(palabra);

        // deshabilita el escaner
        // escanerOff();

        // mensaje de espera en pantalla
        /*
        tft.fillScreen(BLACK);
        tft.setCursor(30, 140);
        tft.setFont(u8g2_font_maniac_te);
        tft.setTextSize(1);
        tft.setTextColor(YELLOW);
        tft.println("ESPERA POR FAVOR");
*/
        //  comprobar si es un comando el código leido

        if (getStringPartByNr(palabra, ';', 0) == "WIFI:T:nopass")
        {
            ssid = getStringPartByNr(palabra, ';', 1);
            ssid = ssid.substring(2); // rescata el ssid

            password = getStringPartByNr(palabra, ';', 2);
            password = password.substring(2); // rescata el password

            Serial.println(ssid);
            Serial.println(password);

            palabra = "";
            cambiarSsid(ssid, password);
        }

        else
        {
            // modo = LEYENDO;
            //  mandar el código del producto a la web
            if ((WiFi.status() == WL_CONNECTED)) // Verificar el estado de la conexión
            {
                requiereServidor(palabra);

                // fin de decodificación del json--------------------------

                // ejecución de comandos

                // verificaFirmware(); // verifica la actualización del firmware

                // pantalla_1();
            }
            else
            {
                //tft.fillScreen(BLACK);
                tft.setCursor(0, 50);
                //tft.setFont(u8g2_font_maniac_te);
                tft.setTextSize(1);
                tft.setTextColor(TFT_YELLOW);
                tft.println("No hay conexión");
                tft.println("WiFi");

                conectarWiFi();
            }

            palabra = "";
        }
        while (Serial2.available() > 0) // termina de vaciar el buffer
            Serial2.read();
        palabra = "";
    }
    else
    {
        palabra = palabra + char(letra);
    }
}

/**************************************************************************************/
/*!
    @brief   obtiene una parte de una cadena
    @param   data nombre del string
    @param   separator caracter que separa las partes de la cadena
    @param   index índice de la parte que se quiere obtener
*/
/*************************************************************************************/
String getStringPartByNr(String data, char separator, int index)
{
    int stringData = 0;   // variable para contar el número de la parte
    String dataPart = ""; // variable para almacenar el texto retornado

    for (int i = 0; i < data.length() - 1; i++)
    { // recorre el texto de a una letra por vez

        if (data[i] == separator)
        {
            // Cuenta el número de veces que aparece el separador en el texto
            stringData++;
        }
        else if (stringData == index)
        {
            // Obtiene el texto cuando el separador es el correcto
            dataPart.concat(data[i]);
        }
        else if (stringData > index)
        {
            // retorna el texto y se detiene ya llegamos al índice buscado
            return dataPart;
            break;
        }
    }
    // retorna el texto si es la última parte
    return dataPart;
}

/**************************************************************************************/
/*!
    @brief   Hace el requerimiento al servidor
    @param   c contenido del parámetro c del requerimiento
    @return  retorna string con la información recibida del servidor
*/
/*************************************************************************************/
String requiereServidor(String c)
{
    int httpCode = 0;

    // HTTPClient http;
    int intento = 0;
    while (intento < 10) // hace 10 intentos de conectarse al servidor
    {
        HTTPClient http;
        ++intento;
        String rand = String(esp_random()); // número agregado para que el servidor no responda con datos viejos
        // agregar el número de intento en el requerimiento
        Serial.println("requiriendo al servidor");

        String servi = "http://192.168.101.64/newfac/RD01/rd01.php?c=" + c + "&r=" + rand + "&estado=" + estado;

        http.begin(servi);
        httpCode = http.GET(); // Hacer el requerimiento
        Serial.println(servi);
        Serial.println(httpCode);

        if (httpCode == 200) // Si el servidor respondió ok
        {
            String respuesta = http.getString();
            Serial.println(respuesta);
            http.end(); // libera los recursos
            intento = 10;

            // decodifica el json --------------------------------------
            // const char *respuesta = payload.c_str();

            DeserializationError error = deserializeJson(doc, respuesta);

            if (error)
            {
                Serial.print("deserializeJson() failed: ");
                Serial.println(error.c_str());
            }

            JsonArray arr = doc.as<JsonArray>();

            ejecutaComandos(arr);

            return respuesta;
        }
        else
        {
            Serial.print("Error en el requerimiento HTTPs ");
            Serial.println(httpCode);
            delay(500);
        }
    }
    // mostrar en pantalla el código de respuesta del servidor
    //tft.fillScreen(BLACK);
    tft.setCursor(0, 50);
    //tft.setFont(u8g2_font_maniac_te);
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW);
    tft.println("Error de respuesta del servidor");
    tft.setTextColor(TFT_WHITE);
    tft.println(httpCode);
    delay(2000);
    // guardar motivo del reset (_1) en preferences
    preferences.begin("parametros", false);
    preferences.putString("reset", "_1");
    preferences.end();
    esp_restart(); // Resetea el esp32
}

void ejecutaComandos(JsonArray arr)
{
    int count = arr.size();
    Serial.print("longitud de arr ");
    Serial.println(count);

    const char *texto = "";
    int8_t tipografia = 0;

    for (size_t i = 0; i < count; i++)
    {
        u16_t com = arr[i][0];

        switch (com)
        {
        case 1:
            tft.fillRoundRect(arr[i][1], arr[i][2], arr[i][3], arr[i][4], arr[i][5], arr[i][6]);
            break;

        case 2:
            tft.setCursor(arr[i][1], arr[i][2]);
            break;

        case 3:
            tipografia = arr[i][1];

            if (tipografia == 2)
            {
                //tft.setFont(u8g2_font_inb30_mf);
            }
            else if (tipografia == 1)
            {
                //tft.setFont(u8g2_font_inb16_mf);
            }
            else if (tipografia == 3)
            {
                //tft.setFont(u8g2_font_inb33_mf);
            }
            /*
            else if (tipografia == 4)
            {
                tft.setFont(u8g2_font_iconquadpix_m_all);
            }
            break;
            */

        case 4:
            tft.setTextSize(arr[i][1]);
            break;

        case 5:
            tft.setTextColor(arr[i][1]);
            break;

        case 6:
            texto = arr[i][1];
            tft.print(texto);
            break;

        case 7:
            tft.fillScreen(arr[i][1]);
            break;

        case 8:
            dibujaTeclado(arr[i][1], arr[i][2], arr[i][3], arr[i][4]);
            break;

        case 9:
            estado = arr[i][1];
            break;

        default:
            break;
        }
    }
    teclaApagado(45, 422);
}

/***************************************************************************************/
/*!
    @brief   genera el logo de wifi activo
    @param   posx coordenada x donde se ubica
    @param   posy coordenada y donde se ubica
*/
/***************************************************************************************/
void logowifi(int posx, int posy)
{
    tft.fillRect(posx - 8, posy - 16, 25, 19, TFT_BLACK);
    //tft.drawCircle(posx, posy - 3, 9, 9, 220, 320,TFT_WHITE);
    //tft.drawArc(posx, posy, 8, 8, 230, 310, WHITE);
    //tft.drawArc(posx, posy + 3, 8, 8, 240, 300, WHITE);
    tft.fillCircle(posx, posy, 2, TFT_WHITE);
    // número de versión de firmware
    //tft.setFont(u8g2_font_mozart_nbp_tn);
    tft.setTextSize(1);
    tft.setCursor(posx + 15, posy - 5);
    tft.setTextColor(TFT_RED);
    tft.print(FIRM_VERSION);
    // npumero de version de spiffs
    // extraer de preferences la versión actual de archivos SPIFFS
    
    preferences.begin("parametros", false);
    int spiffs_version = preferences.getInt("fs_ver", 0);
    preferences.end();
    //tft.setFont(u8g2_font_mozart_nbp_tn);
    tft.setTextSize(1);
    tft.setCursor(posx + 15, posy + 5);
    tft.setTextColor(TFT_MAGENTA);
    tft.print(spiffs_version);
    
}

/***************************************************************************************/
/*!
    @brief   genera el logo de wifi sin señal
    @param   posxoff coordenada x donde se ubica
    @param   posyoff coordenada y donde se ubica
*/
/***************************************************************************************/
void logowifioff(int posxoff, int posyoff)
{
    tft.fillRect(posxoff - 8, posyoff - 16, 25, 19, TFT_BLACK);

    //tft.drawArc(posxoff, posyoff - 3, 9, 9, 220, 320, WHITE);
    //tft.drawArc(posxoff, posyoff, 8, 8, 230, 285, WHITE);
    //tft.drawArc(posxoff, posyoff + 3, 8, 8, 240, 300, WHITE);
    tft.fillCircle(posxoff, posyoff, 2, TFT_WHITE);

    tft.fillCircle(posxoff + 10, posyoff - 10, 6, TFT_RED);
    tft.drawLine(posxoff + 7, posyoff - 10, posxoff + 13, posyoff - 10, TFT_WHITE);
    tft.drawLine(posxoff + 7, posyoff - 9, posxoff + 13, posyoff - 9, TFT_WHITE);
}
