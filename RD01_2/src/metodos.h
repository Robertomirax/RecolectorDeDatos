#ifndef _METODOS_H_
#define _METODOS_H_

#include "Arduino.h"
#include "Preferences.h"
#include <Wire.h>
#include "Goodix.h"
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// pines
#define VOLTAJE 35 // Voltaje de la batería
#define ONOFF 26   // Control de apagado 1=encendido 0=apagado
#define RXD2 16    // RX para el lector de barras
#define TXD2 17    // TX para el lector de barras
#define INT_PIN 5  // INT del touch del display
#define RST_PIN 27 // Reset del touch del display
// pines display
#define display_SCK 18
#define display_MOSI 23
#define display_MISO 19
#define display_CS 15
#define display_DC 2
#define display_RESET 4
#define display_TIPO 5
// fin pines display
// fin pines

// constantes
#define FIRM_VERSION 2 // Versión del firmware actualmente instalado. Debe ser un número entero

// estados
#define INICIO 0     // Estado inicial despues del encendido o reset.
#define PANTALLA_1 1 // Pantalla inicial con menú 1.
#define APAGANDO 100 // Se presionó el botón de apagar y está esperando confirmación
#define PANTALLA_2 2 // Pantalla de menú ubicaciones
#define PANTALLA_3 3 // Pantalla de menú apu/ventas/código del producto
// fin estados

// fin constantes

// Crear instancias y variables
Preferences preferences; // objeto que maneja el almacenamiento en flash de los parámetros
DynamicJsonDocument doc(4096);
// StaticJsonDocument<1024> doc;
Goodix touch = Goodix();
uint32_t tiempoUltNum = xTaskGetTickCount(); // registra el momento en que se tocó el último número
String visor = "";
Arduino_ESP32SPI bus = Arduino_ESP32SPI(display_DC, display_CS, display_SCK, display_MOSI, display_MISO); // objeto que maneja la conexión SPI con el display
Arduino_ILI9488_18bit display = Arduino_ILI9488_18bit(&bus, display_RESET, 0, false);                     // objeto que maneja el display ILI9488
// byte modo = 0;
String ssid{""};
String password{""};
String palabra{""};
byte letra{0};
byte estado = 0; // estado en el que se encuentra el recolector de datos
int codigo = 0;  // código del producto leido por el escaner
int tiempo_encendido = 0;
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
void teclaApagado(int posx = 45, int posy = 422);
void panFondo(); // pantalla de fondo general
void apagando(); // pantalla de verificación de apagado

// fin declaración de funciones -----------------------------------------------

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

            // int numero = tocoPantalla(320 - points[i].x, 480 - points[i].y);
            int numero = tocoPantalla(points[i].x, points[i].y);
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
    Serial.print("tecla: ");
    Serial.println(tecla);
    Serial.print("estado: ");
    Serial.println(estado);
    switch (estado)
    {
    case APAGANDO:
        if (tecla == 3) // apagado
        {
            Serial.println("apagando");
            digitalWrite(ONOFF, LOW); // apagar
        }
        else
        {
            estado = INICIO;
            display.fillScreen(BLACK);
            panFondo();
            teclaApagado();
            requiereServidor("0&tecla=-1");
        }

        break;

    default:
        if (tecla == 12) // tecla de apagado
        {
            apagando();
        }
        else
        {

            String tocado = "0&tecla=";
            Serial.println(tocado + tecla);
            requiereServidor(tocado + tecla);
        }
        break;
    }
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
    ssid = "ASUS";
    password = "rdepmgdm";

    if (ssid == "" || password == "")
    {
        Serial.println("No existen Credenciales WiFi guardadas!");
        // mostrar mensaje en pantalla indicando que faltan credenciales de red
        display.fillScreen(BLACK);
        display.setCursor(0, 60);
        display.setFont(u8g2_font_maniac_te);
        display.setTextSize(1);
        display.setTextColor(RED);
        display.println();
        display.println("FALTAN LAS");
        display.println("CREDENCIALES");
        display.println("DE RED");
        ok = false;
        // Leer los datos provenientes del escaner si están disponibles
        while (Serial2.available())
        {
            leerEscaner();
        }
        touch.loop();
    }
    else
    {
        // Conectarse
        display.fillScreen(BLACK);
        byte intentos = 0;
        WiFi.mode(WIFI_STA);
        Serial.println(ssid.c_str());
        Serial.println(password.c_str());
        WiFi.begin(ssid.c_str(), password.c_str());
        Serial.println("Credenciales cargadas de memoria!\nConectando al WiFi");

        display.setCursor(0, 60);
        display.setFont(u8g2_font_maniac_te);
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.println();
        display.println("CONECTANDO");
        display.print("A ");
        display.setTextColor(YELLOW);
        display.println(ssid);
        teclaApagado(45, 422);

        while ((WiFi.status() != WL_CONNECTED) and (intentos < 5))
        {
            touch.loop();
            delay(1000);
            intentos += 1;
        }

        if ((WiFi.status() == WL_CONNECTED))
        {
            ok = true;
            display.fillScreen(BLACK);
            teclaApagado();
            estado = INICIO;
            requiereServidor("0&tecla=-1");
            Serial.println();
            Serial.println(WiFi.localIP());
            Serial.println("leyendo codigo");
        }
        else
        {
            ok = false;
            Serial.print("Error, no es posible conectarse al wifi ");
            Serial.println(ssid.c_str());
            touch.loop();
            delay(3000);
            touch.loop();
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
void dibujaTeclado(char botx = 4, char boty = 4, uint color = GREEN, bool fondo = false)
{
    int16_t ancho = 320;
    int16_t alto = 480;
    char x = 8;
    char y = 92;
    char r = 10;
    char s = 4;
    char v = 4;

    int16_t d = (ancho - 2 * x - (botx - 1) * s) / botx; // ancho del botón
    x = (ancho - d * botx - s * (botx - 1)) / 2;         // distancia desde el margen izquierdo

    int16_t h = (alto - x - y - (boty - 1) * v) / boty; // alto del botón

    for (size_t i = 0; i < botx; i++)
    {
        for (size_t z = 0; z < boty; z++)
        {
            if (fondo)
            {
                display.fillRoundRect(i * (d + s) + x, z * (h + v) + y, d, h, r, color);
            }
            else
            {
                display.drawRoundRect(i * (d + s) + x, z * (h + v) + y, d, h, r, color);
            }
        }
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
    tiempo_encendido = esp_timer_get_time()/1000000;
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

/***************************************************************************************/
/*!
    @brief   Dibuja la pantalla número 1
*/
/***************************************************************************************/
void poneNumeros()
{

    // números
    display.setFont(u8g2_font_inb33_mf); // maniac_te);
    display.setTextSize(1);
    display.setTextColor(YELLOW);

    // primera fila
    display.setCursor(30, 155);
    display.print("0");

    display.setCursor(107, 155);
    display.print("1");

    display.setCursor(184, 155);
    display.print("2");

    display.setCursor(261, 155);
    display.print("3");

    // segunda fila
    display.setCursor(30, 249);
    display.print("4");

    display.setCursor(107, 249);
    display.print("5");

    display.setCursor(184, 249);
    display.print("6");

    display.setCursor(261, 249);
    display.print("7");

    // tercera fila
    display.setCursor(30, 343);
    display.print("8");

    display.setCursor(107, 343);
    display.print("9");

    display.setCursor(184, 343);
    display.print("A");

    display.setCursor(261, 343);
    display.print("B");

    // cuarta fila
    // display.setFont(u8g2_font_inb16_mf);
    display.setCursor(30, 437);
    display.setTextColor(WHITE);
    display.print("C");

    // display.setFont(u8g2_font_inb33_mf);
    display.setTextColor(YELLOW);
    display.setCursor(107, 437);
    display.print("D");

    // display.setFont(u8g2_font_inb16_mf);
    display.setTextColor(BLUE);
    display.setCursor(184, 437);
    display.print("E");

    display.setFont(u8g2_font_inb33_mf);
    display.setCursor(261, 437);
    display.print("F");
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
        display.fillScreen(BLACK);
        display.setCursor(30, 140);
        display.setFont(u8g2_font_maniac_te);
        display.setTextSize(1);
        display.setTextColor(YELLOW);
        display.println("ESPERA POR FAVOR");
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
                display.fillScreen(BLACK);
                display.setCursor(0, 50);
                display.setFont(u8g2_font_maniac_te);
                display.setTextSize(1);
                display.setTextColor(YELLOW);
                display.println("No hay conexión");
                display.println("WiFi");

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

        String servi = "http://192.168.101.64/newfac/RD01/rd01.php?c=" + c + "&r=" + rand + "&estado=" + estado + "&codigo=" + codigo;

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
    display.fillScreen(BLACK);
    display.setCursor(0, 50);
    display.setFont(u8g2_font_maniac_te);
    display.setTextSize(1);
    display.setTextColor(YELLOW);
    display.println("Error de respuesta del servidor");
    display.setTextColor(WHITE);
    display.println(httpCode);
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
            display.fillRoundRect(arr[i][1], arr[i][2], arr[i][3], arr[i][4], arr[i][5], arr[i][6]);
            break;

        case 2:
            display.setCursor(arr[i][1], arr[i][2]);
            break;

        case 3:
            tipografia = arr[i][1];

            if (tipografia == 2)
            {
                display.setFont(u8g2_font_inb30_mf);
            }
            else if (tipografia == 1)
            {
                display.setFont(u8g2_font_inb16_mf);
            }
            else if (tipografia == 3)
            {
                display.setFont(u8g2_font_inb33_mf);
            }
            else if (tipografia == 4)
            {
                display.setFont(u8g2_font_6x10_mf);
            }
            else if (tipografia == 5)
            {
                display.setFont(u8g2_font_6x12_mf);
            }
            else if (tipografia == 6)
            {
                display.setFont(u8g2_font_t0_11_mf);
            }
            else if (tipografia == 7)
            {
                display.setFont(u8g2_font_10x20_mf);
            }
            break;

        case 4:
            display.setTextSize(arr[i][1]);
            break;

        case 5:
            display.setTextColor(arr[i][1]);
            break;

        case 6:
            texto = arr[i][1];
            display.print(texto);
            break;

        case 7:
            display.fillScreen(arr[i][1]);
            break;

        case 8:
            dibujaTeclado(arr[i][1], arr[i][2], arr[i][3], arr[i][4]);
            break;

        case 9:
            estado = arr[i][1];
            break;

        case 10:
            texto = arr[i][1];
            display.println(texto);
            break;

        case 11: // código del producto leido por el escaner
            codigo = arr[i][1];
            // display.println(codigo);
            break;

        default:
            break;
        }
    }
    teclaApagado();
}

/***************************************************************************************/
/*!
    @brief   dibuja la tecla apagado
    @param   posx coordenada x donde se ubica
    @param   posy coordenada y donde se ubica
*/
/***************************************************************************************/
void teclaApagado(int posx, int posy)
{
    display.fillRoundRect(posx - 37, posy - 42, 73, 92, 10, RED);
    display.fillArc(posx, posy, 25, 15, 320, 220, WHITE);
    display.fillRect(posx - 5, posy - 25, 11, 30, WHITE);
}

void panFondo()
{
    // logo wifi
    int posx = 275;
    int posy = 15;
    if ((WiFi.status() != WL_CONNECTED))
    {

        // logo wifi desconectado
        display.fillRect(posx - 8, posy - 16, 25, 19, BLUE);
        display.drawArc(posx, posy - 3, 9, 9, 220, 320, WHITE);
        display.drawArc(posx, posy, 8, 8, 230, 285, WHITE);
        display.drawArc(posx, posy + 3, 8, 8, 240, 300, WHITE);
        display.fillCircle(posx, posy, 2, WHITE);

        display.fillCircle(posx + 10, posy - 10, 6, RED);
        display.drawLine(posx + 7, posy - 10, posx + 13, posy - 10, WHITE);
        display.drawLine(posx + 7, posy - 9, posx + 13, posy - 9, WHITE);

        // RECONECTAR wifi
        conectarWiFi();
    }
    else
    {
        // logo wifi conectado
        display.fillRect(posx - 8, posy - 16, 25, 19, BLACK);
        display.drawArc(posx, posy - 3, 9, 9, 220, 320, WHITE);
        display.drawArc(posx, posy, 8, 8, 230, 310, WHITE);
        display.drawArc(posx, posy + 3, 8, 8, 240, 300, WHITE);
        display.fillCircle(posx, posy, 2, WHITE);
    }

    // número de versión de firmware
    display.fillRect(295, 0, 24, 19, BLACK);
    display.setFont(u8g2_font_mozart_nbp_tf);
    display.setTextSize(1);
    display.setCursor(295, 8);
    display.setTextColor(MAGENTA);
    display.print("FIRM");
    display.setCursor(295, 18);
    display.print(FIRM_VERSION);

    // nivel de señal wifi RSSI
    display.fillRect(240, 0, 24, 19, BLACK);
    display.setFont(u8g2_font_mozart_nbp_tf);
    display.setTextSize(1);
    display.setCursor(240, 8);
    display.setTextColor(WHITE);
    display.print("RSSI");
    display.setCursor(240, 18);
    display.print(WiFi.RSSI());

    // medir voltaje de la batería
    int sumVolts = 0;
    for (size_t a = 0; a < 10; a++)
    {
        sumVolts += analogReadMilliVolts(VOLTAJE);
    }
    int volt2 = round(1.754 * sumVolts / 100);
    int color = WHITE;
    int porciento = 0;
    if (volt2 > 408)
    {
        porciento = 100;
        color = GREEN;
    }
    else if (volt2 > 400)
    {
        porciento = 90;
        color = GREEN;
    }
    else if (volt2 > 393)
    {
        porciento = 80;
        color = GREEN;
    }
    else if (volt2 > 387)
    {
        porciento = 70;
        color = YELLOW;
    }
    else if (volt2 > 382)
    {
        porciento = 60;
        color = YELLOW;
    }
    else if (volt2 > 379)
    {
        porciento = 50;
        color = YELLOW;
    }
    else if (volt2 > 377)
    {
        porciento = 40;
        color = YELLOW;
    }
    else if (volt2 > 373)
    {
        porciento = 30;
        color = RED;
    }
    else if (volt2 > 370)
    {
        porciento = 20;
        color = RED;
    }
    else if (volt2 > 368)
    {
        porciento = 15;
        color = RED;
    }
    else if (volt2 > 350)
    {
        porciento = 10;
        color = RED;
    }
    else if (volt2 > 250)
    {
        porciento = 5;
        color = RED;
    }
    else
    {
        porciento = 0;
        color = RED;
    }

    display.fillRoundRect(5, 0, 80, 22, 5, color);
    display.fillRoundRect(85, 6, 5, 9, 0, color);
    display.setFont(u8g2_font_inb16_mf);
    display.setCursor(10, 19);
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.print(porciento);
    display.print(" %");

    // verifica el tiempo desde el encendido
    display.fillRoundRect(150, 0, 70, 25, 2, GREENYELLOW);
    display.setFont(u8g2_font_10x20_mf);
    display.setTextColor(BLUE);
    display.setCursor(155, 18);
    display.print(esp_timer_get_time() / 1000000 - tiempo_encendido);
    /*
    if ((esp_timer_get_time() / 1000000 - tiempo_encendido) > 120)
    {
        Serial.println("apagando");
        //digitalWrite(ONOFF, LOW); // apagar
        // Serial.println("tiempo encendido");
        // tiempo_encendido = esp_timer_get_time()/1000000;
    }
    */
}

void apagando()
{
    // limpiar fondo
    display.fillRect(0, 30, 319, 479, BLACK);

    int posx = 276;
    int posy = 134;
    display.fillRoundRect(posx - 37, posy - 42, 73, 92, 10, RED);
    display.fillArc(posx, posy, 25, 15, 320, 220, WHITE);
    display.fillRect(posx - 5, posy - 25, 11, 30, WHITE);

    display.setFont(u8g2_font_inb33_mf);
    display.setTextSize(1);
    display.setTextColor(RED);
    display.setCursor(30, 155);
    display.print("APAGAR");

    // tecla mantener encendido
    posx = 45;
    posy = 422;
    display.fillRoundRect(posx - 37, posy - 42, 73, 92, 10, BLUE);
    display.fillArc(posx, posy, 25, 15, 320, 220, WHITE);
    display.fillRect(posx - 5, posy - 25, 11, 30, WHITE);

    display.setFont(u8g2_font_inb21_mf);
    display.setTextSize(1);
    display.setTextColor(BLUE);
    display.setCursor(120, 420);
    display.println("MANTENER");
    display.setCursor(112, 450);
    display.print("ENCENDIDO");

    estado = APAGANDO;

    //    Serial.println("apagando");
    //    digitalWrite(ONOFF, LOW); // apagar
}

#endif