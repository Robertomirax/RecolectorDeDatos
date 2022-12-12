#ifndef _METODOS_H_
#define _METODOS_H_

/*****************************************************************************************
 * Antes de publicar verificar:
 * 1- Versión del firmware
 * 2- servidor
 * 3- tiempo de apagado
 *
 *
 * Para actualizar el firmware:
 * Colocar el número correspondiente a la nueva versión de firmware en FIRM_VERSION
 * Compilar el programa y subir al servidor el archivo firmware.bin con el nombre
 * firm(version).bin ejemplo: firm25.bin que se encuentra en
 * .pio\build\esp32doit-devkit-v1/firmware.bin
 * Modificar el archivo firm.json del servidor, cambiando el valor de version por la nueva versión
 * El firmware se actualizará automáticamente al encender la terminal
 *****************************************************************************************/

#include "Arduino.h"
#include "Preferences.h"
#include <Wire.h>
#include "Goodix.h"
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "certificado.h"

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
// fin pines display
// fin pines

// constantes
#define FIRM_VERSION 12 // Versión del firmware actualmente instalado. Debe ser un número entero
#define APAGADO 3600// 240    // tiempo en segundos tras el cual se apaga si no se toca ningún botón

String servidor = "192.168.101.64"; // newfac de pruebas
//String servidor = "192.168.2.3"; // newfac

// estados
#define INICIO 0     // Estado inicial despues del encendido o reset.
#define APAGANDO 100 // Se presionó el botón de apagar y está esperando confirmación
// fin estados

// fin constantes

// Crear instancias y variables
Preferences preferences; // objeto que maneja el almacenamiento en flash de los parámetros
DynamicJsonDocument doc(8192);
Goodix touch = Goodix();
uint32_t tiempoUltNum = xTaskGetTickCount(); // registra el momento en que se tocó el último número
String visor = "";
Arduino_ESP32SPI bus = Arduino_ESP32SPI(display_DC, display_CS, display_SCK, display_MOSI, display_MISO); // objeto que maneja la conexión SPI con el display
Arduino_ILI9488_18bit display = Arduino_ILI9488_18bit(&bus, display_RESET, 0, false);                     // objeto que maneja el display ILI9488
String ssid{""};
String password{""};
String palabra{""};
byte letra{0};
byte estado = 0;    // estado en el que se encuentra el recolector de datos
int codigo = 0;     // código del producto leido por el escaner
char ubicacion[40]; // ubicacion leida del producto para el inventario
int idIndice = 0;   // idIndice de la tabla transito_entrepiso u origen del llamado al teclado
int tiempo_encendido = 0;
bool escaner = true;             // escaner leyendo o no
char sucursalapu[20] = "inicio"; // si se encuentra en ventas o entrepiso en el apumanque
int voltaje = 0;                 // voltaje de la batería
int cantidad = 0;                // cantidad escrita en el teclado numérico
int maximo_subir = 0;            // cantidad máxima de unidades que se pueden subir al entrepiso en una operación

//  fin Crear instancias y variables

// declaración de funciones ---------------------------------------------------
void verificaFirmware();
void actualizaFirmware(uint16_t version);
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
void escanerOn();
void escanerOff();
void configEscaner();
bool enviaComando(byte com[], int largo);
void teclaListo(int posx, int posy);
void teclaBasura(int posx, int posy);
void tecladoNumerico(int aux1, int aux2, int aux3, int aux4);
// fin declaración de funciones -----------------------------------------------

// comprueba si hay actualizaciones del firmware y las instala
void verificaFirmware()
{
    // leer el archivo json del servidor donde indica cual es la última versión del firmware
    // si es distinta de la instalada, la actuliza con la rutina actualizarFirmware()

    int intento = 0;
    while (intento < 10) // hace 10 intentos de conectarse al servidor
    {
        HTTPClient http;
        ++intento;
        String rand = String(esp_random()); // número agregado para que el servidor no responda con datos viejos

        String servi = "http://" + servidor + "/newfac/RD01/firm.json?r=" + rand;
        Serial.println(servi);

        http.begin(servi);         // La url
        int httpCode = http.GET(); // Hacer el requerimiento

        if (httpCode == 200)
        {
            String payload = http.getString();
            Serial.println(payload);

            // decodifica el json --------------------------------------
            const char *respuesta = payload.c_str();
            deserializeJson(doc, F(respuesta));
            JsonObject obj = doc.as<JsonObject>();

            uint16_t version = obj[F("version")];

            if (version > FIRM_VERSION) // Si hay una versión con un número mas grande del firmware en el servidor actualizamos
            {
                actualizaFirmware(version);
            }
            else
            {
                Serial.println("El firmware actual es la ultima version");
                http.end(); // cierra la conexión con el servidor
            }
            break;
        }
        else // el servidor respondió con error
        {
            Serial.print("error de respuesta del servidor ");
            Serial.println(httpCode);
        }
        http.end(); // cierra la conexión con el servidor
    }
    if (intento == 10)
    {
        Serial.println("se intento 10 veces la conexion al servidor");
        // guardar motivo del reset (_3) en preferences
        preferences.begin("parametros", false);
        preferences.putString("reset", "_3");
        preferences.end();
        esp_restart(); // Resetea el esp32
    }
}

/**************************************************************************************/
/*!
    @brief   actualiza el firmware
    @param   version número de la versión a actualizar
*/
/*************************************************************************************/
void actualizaFirmware(uint16_t version)
{
    // pantalla actualizando firmware
    // limpiar fondo
    display.fillRect(0, 30, 319, 479, BLACK);

    display.setFont(u8g2_font_inb30_mf);
    display.setTextSize(1);
    display.setTextColor(RED);
    display.setCursor(0, 155);
    display.println("ACTUALIZANDO");
    display.print("FIRMWARE");

    int azar = random(1, 100000);

    String servi = "http://" + servidor + "/newfac/RD01/firm" + version + ".bin?v=" + azar;

    const char *servi2 = servi.c_str();

    esp_http_client_config_t ota_client_config = {
        .url = servi2,
        .cert_pem = root_ca,
    };
    Serial.println(ota_client_config.url);

    esp_err_t ret = esp_https_ota(&ota_client_config); // Actualiza el firmware
    if (ret == ESP_OK)
    {
        printf("ACTUALIZACION OTA OK, reseteando...\n");
        configEscaner();
        // guardar motivo del reset (_2) en preferences
        preferences.begin("parametros", false);
        preferences.putString("reset", "_2");
        preferences.end();
        esp_restart(); // Resetea el esp32
    }
    else
    {
        printf("FALLO en la actualizacion OTA ...\n");
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
            delay(5000);
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

    case 17:                                 // teclado numérico
        display.setFont(u8g2_font_inb33_mf); // maniac_te);
        display.setTextColor(YELLOW);
        display.setCursor(30, 75);
        display.fillRect(30, 35, 200, 50, BLACK);

        if (tecla == 13 || tecla == 14 || tecla == 15 || tecla == 3) // tecla inicio o listo
        {
            String tocado = "&tecla=";
            Serial.println(cantidad + tocado + tecla);
            requiereServidor(cantidad + tocado + tecla);
        }
        else if (tecla == 7) // tecla de borrado
        {
            cantidad = 0;
            display.print(cantidad);
        }
        else
        {
            int cantidad2 = cantidad * 10;

            if (tecla == 0) // número 7
            {
                cantidad2 += 7;
            }
            else if (tecla == 1) // número 8
            {
                cantidad2 += 8;
            }
            else if (tecla == 2) // número 9
            {
                cantidad2 += 9;
            }
            else if (tecla == 4) // número 4
            {
                cantidad2 += 4;
            }
            else if (tecla == 5) // número 5
            {
                cantidad2 += 5;
            }
            else if (tecla == 6) // número 6
            {
                cantidad2 += 6;
            }
            else if (tecla == 8) // número 1
            {
                cantidad2 += 1;
            }
            else if (tecla == 9) // número 2
            {
                cantidad2 += 2;
            }
            else if (tecla == 10) // número 3
            {
                cantidad2 += 3;
            }

            if (cantidad2 <= maximo_subir)
            {
                cantidad = cantidad2;
                display.print(cantidad);
            }
            else
            {
                display.print(cantidad);
            }
        }

        if (tecla == 12) // tecla de apagado
        {
            apagando();
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
    // ssid = "ASUS";
    // password = "rdepmgdm";

    WiFi.useStaticBuffers(true);
    WiFi.setAutoReconnect(true);

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

        do
        {
            WiFi.reconnect();
            Serial.println("reconectando");
            escanerOn();
            for (size_t i = 0; i < 300; i++)
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
            intentos += 1;

        } while ((WiFi.status() != WL_CONNECTED) and (intentos < 20));

        if ((WiFi.status() == WL_CONNECTED))
        {
            ok = true;
            display.fillScreen(BLACK);
            teclaApagado();
            estado = INICIO;
            requiereServidor("0&tecla=-1");
            Serial.println();
            Serial.println(WiFi.localIP());
            Serial.println("Conectado");
        }
        else
        {
            ok = false;
            Serial.print("Error, no es posible conectarse al wifi ");
            Serial.println(ssid.c_str());
            touch.loop();
            delay(10);
            // guardar motivo del reset (_4) en preferences
            preferences.begin("parametros", false);
            preferences.putString("reset", "_4");
            preferences.end();
            // esp_restart(); // Resetea el esp32
            digitalWrite(ONOFF, LOW); // apagar
            delay(5000);
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
    tiempo_encendido = esp_timer_get_time() / 1000000;
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
    display.print("7");

    display.setCursor(107, 155);
    display.print("8");

    display.setCursor(184, 155);
    display.print("9");

    // display.setCursor(261, 155);
    // display.print("#");
    teclaListo(276, 134);

    // segunda fila
    display.setCursor(30, 249);
    display.print("4");

    display.setCursor(107, 249);
    display.print("5");

    display.setCursor(184, 249);
    display.print("6");

    // display.setCursor(261, 249);
    // display.print("#");
    teclaBasura(239, 188);

    // tercera fila
    display.setCursor(30, 343);
    display.print("1");

    display.setCursor(107, 343);
    display.print("2");

    display.setCursor(184, 343);
    display.print("3");

    display.setCursor(261, 343);
    display.print("0");

    // cuarta fila
    // display.setFont(u8g2_font_inb16_mf);
    // display.setCursor(30, 437);
    // display.setTextColor(WHITE);
    // display.print("C");

    // display.setFont(u8g2_font_inb33_mf);
    // display.setTextColor(YELLOW);
    // display.setCursor(107, 437);
    // display.print("0");

    // display.setFont(u8g2_font_inb16_mf);
    // display.setTextColor(BLUE);
    // display.setCursor(184, 437);
    // display.print("#");

    // display.setFont(u8g2_font_inb33_mf);
    // display.setCursor(261, 437);
    // display.print("#");
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
        escanerOff();

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
            //  mandar el código del producto a la web

            requiereServidor(palabra);

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

    int intento = 0;
    while (intento < 5) // hace 5 intentos de conectarse al servidor
    {
        HTTPClient http;
        ++intento;
        String rand = String(esp_random()); // número agregado para que el servidor no responda con datos viejos
        // agregar el número de intento en el requerimiento
        Serial.println("requiriendo al servidor 2: ");
        Serial.println(intento);

        String servi = "http://" + servidor + "/newfac/RD01/rd01.php?c=" + c + "&r=" + rand + "&estado=" + estado + "&codigo=" + codigo + "&sucursalapu=" + sucursalapu + "&mac=" + WiFi.macAddress() + "&voltaje=" + voltaje + "&RSSI=" + WiFi.RSSI() + "&ver=" + FIRM_VERSION + "&idIndice=" + idIndice + "&ubicacion=" + ubicacion;

        http.begin(servi);
        httpCode = http.GET(); // Hacer el requerimiento
        Serial.println(servi);
        Serial.print("httpCode: ");
        Serial.println(httpCode);

        if (httpCode == 200) // Si el servidor respondió ok
        {
            String respuesta = http.getString();
            Serial.println(respuesta);
            http.end(); // libera los recursos

            // decodifica el json --------------------------------------
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

            escanerOn();
            for (size_t i = 0; i < 500; i++)
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
    // esp_restart(); // Resetea el esp32
    digitalWrite(ONOFF, LOW); // apagar
    delay(5000);
    return "0";
}

void ejecutaComandos(JsonArray arr)
{
    int count = arr.size();
    Serial.print("longitud de arr ");
    Serial.println(count);

    const char *texto = "";
    int8_t tipografia = 0;
    int8_t habi = 0;

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
            break;

        case 12: // habilitación escaner 0 = deshabilita 1 = habilita

            habi = arr[i][1];
            Serial.print("escaner: ");
            Serial.println(habi);

            if (habi == 1)
            {
                escanerOn();
            }
            else
            {
                escanerOff();
            }
            break;

        case 13: // dibuja la tecla listo
            teclaListo(arr[i][1], arr[i][2]);
            break;

        case 14: // dibuja la tecla basura
            teclaBasura(arr[i][1], arr[i][2]);
            break;

        case 15: // sucursal en la que se encuentra el terminal
            texto = arr[i][1];
            strcpy(sucursalapu, texto);
            break;

        case 16: // dibuja arco lleno
            display.fillArc(arr[i][1], arr[i][2], arr[i][3], arr[i][4], arr[i][5], arr[i][6], arr[i][7]);
            break;

        case 17: // dibuja linea recta
            display.drawLine(arr[i][1], arr[i][2], arr[i][3], arr[i][4], arr[i][5]);
            break;

        case 18: // tecladoNumerico
            tecladoNumerico(arr[i][1], arr[i][2], arr[i][3], arr[i][4]);
            break;

        case 19: // idIndice de la tabla transito_entrepiso
            idIndice = arr[i][1];
            break;

        case 20: // ubicacion del producto
            texto = arr[i][1];
            strcpy(ubicacion, texto);
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

void teclaListo(int posx, int posy)
{

    display.fillRoundRect(posx - 37, posy - 42, 73, 92, 10, GREEN);
    display.fillArc(posx + 80, posy + 40, 87, 80, 190, 230, WHITE);
    display.fillArc(posx - 85, posy + 25, 85, 78, 340, 0, WHITE);
    display.fillArc(posx - 1, posy + 5, 30, 25, 340, 290, WHITE);
}

void teclaBasura(int posx, int posy)
{

    display.fillRoundRect(posx, posy, 73, 92, 10, RED);
    display.fillRoundRect(posx + 15, posy + 16, 43, 60, 8, WHITE);
    display.fillRoundRect(posx + 12, posy + 16, 48, 10, 0, RED);
    display.fillRoundRect(posx + 7, posy + 18, 58, 5, 2, WHITE);
    display.fillRoundRect(posx + 32, posy + 14, 10, 5, 2, WHITE);
    display.fillRoundRect(posx + 22, posy + 34, 5, 35, 2, RED);
    display.fillRoundRect(posx + 47, posy + 34, 5, 35, 2, RED);
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
    voltaje = volt2;
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
        color = GREEN;
    }
    else if (volt2 > 382)
    {
        porciento = 60;
        color = GREEN;
    }
    else if (volt2 > 379)
    {
        porciento = 50;
        color = GREEN;
    }
    else if (volt2 > 377)
    {
        porciento = 40;
        color = GREEN;
    }
    else if (volt2 > 373)
    {
        porciento = 30;
        color = GREEN;
    }
    else if (volt2 > 370)
    {
        porciento = 20;
        color = YELLOW;
    }
    else if (volt2 > 368)
    {
        porciento = 15;
        color = YELLOW;
    }
    else if (volt2 > 350)
    {
        porciento = 10;
        color = YELLOW;
    }
    else if (volt2 > 280)
    {
        porciento = 5;
        color = RED;
    }
    else
    {
        // apagar recolector
        // limpiar fondo
        display.fillRect(0, 0, 319, 480, BLACK);
        display.setFont(u8g2_font_inb33_mf);
        display.setCursor(0, 200);
        display.println("BATERÍA");
        display.print("BAJA");
        delay(5000);

        digitalWrite(ONOFF, LOW); // apagar
        delay(5000);
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

    if ((esp_timer_get_time() / 1000000 - tiempo_encendido) > APAGADO)
    {
        Serial.println("apagando por tiempo inactivo");
        digitalWrite(ONOFF, LOW); // apagar
        delay(5000);
    }
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
}

// habilita el escaner, pone la variable pública escaner en true
void escanerOn()
{

    byte buf88[] = {0x04, 0xE9, 0x04, 0x00, 0xFF, 0x0F}; // SCAN_ENABLE
    byte largo8 = sizeof(buf88);
    enviaComando(buf88, largo8);
    escaner = true;
}

// Deshabilita el escaner y pone la variable pública escaner en false
void escanerOff()
{

    byte buf6[] = {0x04, 0xEA, 0x04, 0x00, 0xFF, 0x0E}; // SCAN_DISABLE
    byte largo = sizeof(buf6);
    enviaComando(buf6, largo);
    escaner = false;
}

// Inicializa el escaner
void configEscaner()
{
    Serial.println("configurando escaner");

    int largo = 0;

    // reset del escaner
    Serial.println("reset 04 FA 04 00 FE FE");
    byte buf9[] = {0x04, 0xFA, 0x04, 0x00, 0xFE, 0xFE};
    largo = sizeof(buf9);
    Serial.println(enviaComando(buf9, largo));
    Serial.println();
    delay(500);
    /*
        Serial.println("NO Allows scan configuration");
        byte buf[] = {0x07, 0xC6, 0x04, 0x08, 0x00, 0xEC, 0x00, 0xFE, 0x3B}; // NO Allows scan configuration bar code
        largo = sizeof(buf);                                                 // Serial2.write(buf,sizeof(buf)); // manda al escaner el comando
        Serial.println(enviaComando(buf, largo));
        Serial.println();
    */
    Serial.println("Automatic induction");
    byte buf4[] = {0x07, 0xC6, 0x04, 0x08, 0x00, 0x8A, 0x09, 0xFE, 0x94}; // Automatic induction
    largo = sizeof(buf4);
    Serial.println(enviaComando(buf4, largo));
    Serial.println();

    Serial.println("Retorno de carro 13 despues del codigo leido");
    byte buf3[] = {0x08, 0xC6, 0x04, 0x08, 0x00, 0xF2, 0x05, 0x02, 0xFE, 0x2D}; // envia retorno de carro 13 después de leer el código
    largo = sizeof(buf3);
    Serial.println(enviaComando(buf3, largo));
    Serial.println();

    Serial.println("SCAN_ENABLE");
    byte buf8[] = {0x04, 0xE9, 0x04, 0x00, 0xFF, 0x0F}; // SCAN_ENABLE
    largo = sizeof(buf8);
    Serial.println(enviaComando(buf8, largo));
    Serial.println();
}

/**************************************************************************************/
/*!
    @brief   Envia comando al escaner y espera el ACK
    @param   com array que se enviará al escaner
    @param   largo longitud del array com
    @return  true si hubo ACK, false si no
*/
/*************************************************************************************/
bool enviaComando(byte com[], int largo)
{
    byte buf[] = {0x00}; // Wake up
    Serial2.write(buf, sizeof(buf));
    delay(50);

    byte intentos = 0;
    bool ack = false;
    String recibido = "";

    while (intentos < 5 && !ack)
    {
        Serial2.write(com, largo);            // envía el comando al escaner
        uint32_t start = xTaskGetTickCount(); // registra el inicio del tiempo para ver si se produce timeout

        while (!Serial2.available() && ((xTaskGetTickCount() - start) < 500)) // espera que llegue la respuesta del escaner
        {
        }

        if ((xTaskGetTickCount() - start) < 500) // si no hubo timeout
        {
            recibido = "";
            while (Serial2.available() && ((xTaskGetTickCount() - start) < 500)) // lee el comando recibido
            {
                letra = Serial2.read();
                recibido += letra;
            }

            if ((xTaskGetTickCount() - start) < 500) // si no hubo timeout
            {
                Serial.println(recibido);

                if (recibido == "42080025544" || recibido.substring(0, 10) == "8716400801") // llegó ack o versión
                {
                    ack = true;
                }
                else
                {
                    Serial.println("ACK MAL");
                    ack = false;
                    intentos += 1;
                }
            }
            else // hubo timeout
            {
                Serial.println("Timeout 2");
                intentos += 1;
                ack = false;
            }
        }
        else // hubo timeout
        {
            Serial.println("Timeout 1");
            intentos += 1;
            ack = false;
        }
    }
    return ack;
}

/**************************************************************************************/
/*!
    @brief   Teclado numérico
    @param   codigo código del producto
    @param   max cantidad máxima de undidades
    @param   aux3
    @param   aux4
    @return  nada
*/
/*************************************************************************************/
void tecladoNumerico(int codigo, int max, int origen, int aux4)
{
    cantidad = 0;
    maximo_subir = max;
    idIndice = origen;
    dibujaTeclado(4, 4, DARKGREEN, false);
    poneNumeros();
}

#endif