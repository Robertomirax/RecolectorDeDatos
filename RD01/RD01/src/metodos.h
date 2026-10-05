#ifndef _METODOS_H_
#define _METODOS_H_

/*****************************************************************************************
 * Este archivo reúne la lógica de aplicación del recolector: configuración global,
 * comunicación con el servidor, pantallas, teclado táctil, lector de códigos y Wi-Fi.
 * Se incluye desde main.cpp; por eso aquí también se definen las variables compartidas
 * por esas rutinas.
 *****************************************************************************************/

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
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include "esp_system.h"
#include <string.h>

constexpr size_t MAX_SCANNER_CODE_LENGTH = 256;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 15000;

// Pines y buses conectados al hardware del recolector.
#define VOLTAJE 35 // Voltaje de la batería
#define ONOFF 26   // Control de apagado 1=encendido 0=apagado
#define RXD2 16    // RX para el lector de barras
#define TXD2 17    // TX para el lector de barras
#define INT_PIN 5  // INT del touch del display
#define RST_PIN 27 // Reset del touch del display
// Bus SPI y señales de control del display ILI9488.
#define display_SCK 18
#define display_MOSI 23
#define display_MISO 19
#define display_CS 15
#define display_DC 2
#define display_RESET 4
// fin pines display
// fin pines

// Parámetros operativos que se revisan al preparar una publicación.
#define FIRM_VERSION 16 // Versión del firmware actualmente instalado. Debe ser un número entero
#define APAGADO 240     // tiempo en segundos tras el cual se apaga si no se toca ningún botón

String servidor = "";
//String servidor = "192.168.101.64"; // newfac de pruebas
// String servidor = "192.168.2.3"; // newfac

// Valores de estado reconocidos localmente por la interfaz.
#define INICIO 0     // Estado inicial despues del encendido o reset.
#define APAGANDO 100 // Se presionó el botón de apagar y está esperando confirmación
// fin estados

// Periféricos y datos compartidos por las rutinas del firmware.
Preferences preferences; // objeto que maneja el almacenamiento en flash de los parámetros
DynamicJsonDocument doc(16384);
Goodix touch = Goodix();
uint32_t tiempoUltNum = xTaskGetTickCount(); // registra el momento en que se tocó el último número
String visor = "";
// El display se comunica por SPI; la clase Goodix usa Wire/I2C para el panel táctil.
Arduino_ESP32SPI bus = Arduino_ESP32SPI(display_DC, display_CS, display_SCK, display_MOSI, display_MISO); // objeto que maneja la conexión SPI con el display
Arduino_ILI9488_18bit display = Arduino_ILI9488_18bit(&bus, display_RESET, 0, false);                     // objeto que maneja el display ILI9488
// Credenciales se cargan de Preferences y palabra acumula bytes del lector hasta CR.
String ssid{""};
String password{""};
String palabra{""};
byte letra{0};
bool scannerInputOverflow = false;
bool wifiStarted = false;
bool wifiConnectionHandled = false;
bool firmwareCheckDone = false;
uint32_t lastWiFiReconnectAttempt = 0;
byte estado = 0;     // estado en el que se encuentra el recolector de datos
int codigo = 0;      // código del producto leido por el escaner
char ubicacion[100]; // ubicacion leida del producto para el inventario
int idIndice = 0;    // idIndice de la tabla transito_entrepiso u origen del llamado al teclado
int tiempo_encendido = 0;
bool escaner = true;             // escaner leyendo o no
char sucursalapu[20] = "inicio"; // si se encuentra en ventas o entrepiso en el apumanque
int voltaje = 0;                 // voltaje de la batería
int cantidad = 0;                // cantidad escrita en el teclado numérico
int maximo_subir = 0;            // cantidad máxima de unidades que se pueden subir al entrepiso en una operación

// Declaraciones adelantadas: las implementaciones permanecen agrupadas en este archivo
// aunque algunas rutinas llamen a funciones que aparecen más adelante.
void verificaFirmware();
void actualizaFirmware(uint16_t version);
void handleTouch(int8_t contacts, GTPoint *points);
void touchStart();
void teclado(int tecla);
int tocoPantalla(uint16_t x, uint16_t y);
bool conectarWiFi();
void gestionarWiFi();
bool copiarTextoSeguro(char *destino, size_t capacidad, const char *origen);
String codificarValorFormulario(const String &valor, bool conservarSeparadores = false);
void leerEscaner();
String getStringPartByNr(const String &data, char separator, int index);
void requiereServidor(String c);
void ejecutaComandos(JsonArray arr);
void teclaApagado(int posx = 45, int posy = 422);
void panFondo(); // pantalla de fondo general
void apagando(); // pantalla de verificación de apagado
void escanerOn();
void escanerOff();
void configEscaner();
bool enviaComando(byte com[], int largo, byte maxIntentos = 5);
void teclaListo(int posx, int posy);
void teclaBasura(int posx, int posy);
void tecladoNumerico(int aux1, int aux2, int aux3, int aux4);
void teclaSuspender(int posx, int posy);
void dibujaTeclado(char botx, char boty, uint color, bool fondo);
void poneNumeros();

/**
 * Consulta firm.json en el servidor configurado y compara su versión con FIRM_VERSION.
 * Si hay una versión posterior inicia la instalación OTA; ante errores HTTP vuelve a
 * consultar un número limitado de veces.
 */
void verificaFirmware()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Se omite la comprobación OTA: Wi-Fi desconectado");
        return;
    }

    for (uint8_t intento = 1; intento <= 5; ++intento)
    {
        HTTPClient http;
        http.setConnectTimeout(5000);
        http.setTimeout(8000);

        // El valor aleatorio evita reutilizar un manifiesto almacenado en caché.
        String url = "http://" + servidor + "/newfac/RD01/firm.json?r=" + String(esp_random());
        if (!http.begin(url))
        {
            Serial.println("No se pudo inicializar la consulta del manifiesto OTA");
        }
        else
        {
            const int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK)
            {
                doc.clear();
                DeserializationError error = deserializeJson(doc, http.getStream());
                http.end();

                if (error || !doc.is<JsonObject>() || !doc["version"].is<uint16_t>())
                {
                    Serial.println("Manifiesto OTA inválido o incompleto");
                }
                else
                {
                    const uint16_t version = doc["version"].as<uint16_t>();
                    if (version > FIRM_VERSION)
                    {
                        actualizaFirmware(version);
                    }
                    else
                    {
                        Serial.println("El firmware actual es la última versión");
                    }
                    return;
                }
            }
            else
            {
                Serial.print("Error al consultar manifiesto OTA: ");
                Serial.println(httpCode);
                http.end();
            }
        }

        if (intento < 5)
        {
            delay(250);
        }
    }
    Serial.println("No se pudo verificar si hay una actualización OTA");
}

/**
 * Descarga la imagen firm<version>.bin por HTTP y muestra el estado en pantalla.
 * Si la actualización termina correctamente, guarda el motivo del reinicio.
 */
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

    Serial.println(servi);

    // HTTPUpdate descarga y escribe la imagen en la partición OTA inactiva.
    // Se desactiva el reinicio automático para guardar el motivo y reiniciar aquí.
    WiFiClient cliente;
    HTTPUpdate actualizador;
    actualizador.rebootOnUpdate(false);
    t_httpUpdate_return resultado = actualizador.update(cliente, servi);

    if (resultado == HTTP_UPDATE_OK)
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
        Serial.print("Error OTA HTTP: ");
        Serial.println(actualizador.getLastErrorString());
    }
}

/**
 * Callback de Goodix: filtra lecturas demasiado cercanas y convierte cada coordenada
 * recibida en el identificador de tecla usado por la máquina de estados.
 */
void handleTouch(int8_t contacts, GTPoint *points)
{
    // verificar que se levantó el dedo
    uint32_t tiempoNum = xTaskGetTickCount();
    if (tiempoNum > tiempoUltNum + 50) // no hubo señal INT durante 50 ticks
    {
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
    // Inicializa el controlador con los pines y la dirección I2C del panel instalado.
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

/**
 * Despacha una tecla según el estado actual: confirmación de apagado, teclado numérico
 * de configuración/cantidad o acción solicitada al servidor.
 */
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
        else if (tecla == 4) // tecla de CONFIGURACIÓN
        {
            display.fillRoundRect(0, 23, 319, 450, 10, BLACK);
            dibujaTeclado(4, 4, DARKGREEN, false);
            poneNumeros();
            estado = 17; // teclado numérico
            maximo_subir = 10000000;
            idIndice = 1234;
        }
        else
        {
            // define servidor
            preferences.begin("credenciales", false);
            servidor = preferences.getString("servidor", "");
            preferences.end();

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

        if (tecla == 13 || tecla == 14 || tecla == 15) // tecla inicio
        {
            String tocado = "&tecla=";
            Serial.println(cantidad + tocado + tecla);
            requiereServidor(cantidad + tocado + tecla);
        }
        else if ((tecla == 3) && (idIndice != 1234)) // tecla listo uso general
        {
            String tocado = "&tecla=";
            Serial.println(cantidad + tocado + tecla);
            requiereServidor(cantidad + tocado + tecla);
        }
        else if ((tecla == 3) && (idIndice == 1234)) // tecla listo leyendo clave
        {
            Serial.println(cantidad);
            if (cantidad == 753064) // clave correcta cambia al servidor 101.64 (pruebas)
            {
                preferences.begin("credenciales", false);
                preferences.putString("servidor", "192.168.101.64");
                preferences.end();
            }
            else if (cantidad == 753003) // clave correcta cambia al servidor 2.3 (apumanque)
            {
                preferences.begin("credenciales", false);
                preferences.putString("servidor", "192.168.2.3");
                preferences.end();
            }
            else
            {
                cantidad = 0;
                display.print(cantidad);
            }
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
    // Las claves se guardan en la partición NVS bajo el espacio "credenciales".
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
        wifiStarted = false;
        wifiConnectionHandled = false;
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
        return false;
    }

    display.fillScreen(BLACK);
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

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), password.c_str());
    wifiStarted = true;
    wifiConnectionHandled = false;
    lastWiFiReconnectAttempt = millis();
    return WiFi.status() == WL_CONNECTED;
}

// Mantiene Wi-Fi sin bloquear el ciclo principal y realiza una acción una vez por conexión.
void gestionarWiFi()
{
    if (!wifiStarted)
    {
        return;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        if (!wifiConnectionHandled)
        {
            wifiConnectionHandled = true;
            display.fillScreen(BLACK);
            teclaApagado();
            estado = INICIO;
            Serial.println(WiFi.localIP());
            Serial.println("Conectado");
            requiereServidor("0&tecla=-1");

            if (!firmwareCheckDone)
            {
                firmwareCheckDone = true;
                verificaFirmware();
            }
        }
        return;
    }

    wifiConnectionHandled = false;
    const uint32_t now = millis();
    if (static_cast<uint32_t>(now - lastWiFiReconnectAttempt) >= WIFI_RECONNECT_INTERVAL_MS)
    {
        lastWiFiReconnectAttempt = now;
        Serial.println("Reintentando conexión Wi-Fi");
        WiFi.reconnect();
    }
}

void cambiarSsid(String ssid, String password)
{
    // Reemplaza las credenciales guardadas y vuelve a iniciar la conexión.
    preferences.begin("credenciales", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    Serial.println("Se guardaron las Credenciales\n.");
    conectarWiFi();
    escanerOn();
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
// Calcula el tamaño de las teclas para repartir una cuadrícula centrada en el display.
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
// Traduce coordenadas (x,y) del digitalizador a índices 0..15 de la cuadrícula táctil.
int tocoPantalla(uint16_t x, uint16_t y)
{
    tiempo_encendido = esp_timer_get_time() / 1000000; // reseteo el tiempo de inactividad
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
// Dibuja los dígitos y las teclas de acción sobre la cuadrícula numérica.
void poneNumeros()
{

    // números
    display.setFont(u8g2_font_inb33_mf);
    display.setTextSize(1);
    display.setTextColor(YELLOW);

    // primera fila
    display.setCursor(30, 155);
    display.print("7");

    display.setCursor(107, 155);
    display.print("8");

    display.setCursor(184, 155);
    display.print("9");

    teclaListo(276, 134);

    // segunda fila
    display.setCursor(30, 249);
    display.print("4");

    display.setCursor(107, 249);
    display.print("5");

    display.setCursor(184, 249);
    display.print("6");

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
}

/**************************************************************************************/
/*!
    @brief   Lee los datos recibidos desde el escaner
*/
/*************************************************************************************/
// Acumula bytes UART hasta CR; reconoce códigos QR de Wi-Fi o envía el dato escaneado.
void leerEscaner()
{
    const int byteRecibido = Serial2.read();
    if (byteRecibido < 0)
    {
        return;
    }
    letra = static_cast<byte>(byteRecibido);

    if (letra == 13)
    {
        // deshabilita el escaner
        escanerOff();

        if (scannerInputOverflow)
        {
            Serial.println("Código del escáner descartado: excede el tamaño máximo");
            palabra = "";
            scannerInputOverflow = false;
            while (Serial2.available() > 0)
            {
                Serial2.read();
            }
            escanerOn();
            return;
        }

        //  comprobar si es un comando el código leido

        if (getStringPartByNr(palabra, ';', 0) == "WIFI:T:nopass")
        {
            ssid = getStringPartByNr(palabra, ';', 1);
            ssid = ssid.substring(2); // rescata el ssid

            password = getStringPartByNr(palabra, ';', 2);
            password = password.substring(2); // rescata el password

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
    else if (letra < 0x20 || letra > 0x7E)
    {
        // Los códigos son ASCII imprimible; los demás bytes son restos de ACKs del lector.
        return;
    }
    else if (!scannerInputOverflow)
    {
        if (palabra.length() < MAX_SCANNER_CODE_LENGTH)
        {
            palabra += static_cast<char>(letra);
        }
        else
        {
            scannerInputOverflow = true;
            palabra = "";
        }
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
// Extrae un campo separado por un delimitador; se usa para interpretar el QR de Wi-Fi.
String getStringPartByNr(const String &data, char separator, int index)
{
    if (index < 0)
    {
        return "";
    }
    int stringData = 0;
    String dataPart;
    dataPart.reserve(data.length());
    for (size_t i = 0; i < data.length(); i++)
    {
        if (data[i] == separator)
        {
            if (stringData == index)
            {
                return dataPart;
            }
            stringData++;
        }
        else if (stringData == index)
        {
            dataPart.concat(data[i]);
        }
    }
    return stringData == index ? dataPart : String();
}

bool copiarTextoSeguro(char *destino, size_t capacidad, const char *origen)
{
    if (destino == nullptr || origen == nullptr || capacidad == 0)
    {
        return false;
    }
    const size_t longitud = strlen(origen);
    if (longitud >= capacidad)
    {
        return false;
    }
    memcpy(destino, origen, longitud + 1);
    return true;
}

String codificarValorFormulario(const String &valor, bool conservarSeparadores)
{
    static const char hex[] = "0123456789ABCDEF";
    String codificado;
    codificado.reserve(valor.length() * 3);
    for (size_t i = 0; i < valor.length(); ++i)
    {
        const uint8_t byteValor = static_cast<uint8_t>(valor[i]);
        if ((byteValor >= 'a' && byteValor <= 'z') ||
            (byteValor >= 'A' && byteValor <= 'Z') ||
            (byteValor >= '0' && byteValor <= '9') ||
            byteValor == '-' || byteValor == '_' || byteValor == '.' || byteValor == '*' ||
            (conservarSeparadores && (byteValor == '&' || byteValor == '=')))
        {
            codificado += static_cast<char>(byteValor);
        }
        else if (byteValor == ' ')
        {
            codificado += '+';
        }
        else
        {
            codificado += '%';
            codificado += hex[byteValor >> 4];
            codificado += hex[byteValor & 0x0F];
        }
    }
    return codificado;
}

/**************************************************************************************/
/*!
    @brief   Hace el requerimiento al servidor
    @param   c contenido del parámetro c del requerimiento
    @return  retorna string con la información recibida del servidor
*/
/*************************************************************************************/
// Envía los datos de la terminal por POST y ejecuta los comandos de pantalla recibidos
// en la respuesta JSON. También presenta un mensaje cuando falla la comunicación.
void requiereServidor(String c)
{

    HTTPClient http;
    http.setTimeout(30000);        // tiempo de timeout en milisegundos para recibir respuesta del servidor
    http.setConnectTimeout(15000); // tiempo de timeout en milisegundos para conectarse al servidor
    String servi = "http://" + servidor + "/newfac/RD01/rd01.php";
    const char *serverName = servi.c_str();
    if (!http.begin(serverName))
    {
        Serial.println("No se pudo inicializar la petición HTTP al servidor");
        return;
    }

    // If you need Node-RED/server authentication, insert user and password below
    // http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");

    // Specify content-type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // Codifica los campos de texto para que caracteres reservados no cambien la
    // estructura application/x-www-form-urlencoded.
    String httpRequestData;
    httpRequestData.reserve(c.length() * 3 + strlen(sucursalapu) * 3 + strlen(ubicacion) * 3 + 160);
    httpRequestData = "c=";
    // c contiene una subconsulta histórica como "0&tecla=15": sus separadores deben
    // seguir siendo visibles para que PHP reciba tecla como campo del formulario.
    httpRequestData += codificarValorFormulario(c, true);
    httpRequestData += "&r=" + String(esp_random());
    httpRequestData += "&estado=" + String(estado);
    httpRequestData += "&codigo=" + String(codigo);
    httpRequestData += "&sucursalapu=" + codificarValorFormulario(String(sucursalapu));
    httpRequestData += "&mac=" + WiFi.macAddress();
    httpRequestData += "&voltaje=" + String(voltaje);
    httpRequestData += "&RSSI=" + String(WiFi.RSSI());
    httpRequestData += "&ver=" + String(FIRM_VERSION);
    httpRequestData += "&idIndice=" + String(idIndice);
    httpRequestData += "&ubicacion=" + codificarValorFormulario(String(ubicacion));

    Serial.println(serverName);
    Serial.println("Enviando operación al servidor");

    // Send HTTP POST request
    int httpResponseCode = http.POST(httpRequestData);

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode == 200) // Si el servidor respondió ok
    {
        // El servidor puede responder a algunas acciones con HTTP 200 y sin cuerpo.
        // Leer primero la respuesta completa evita interpretar el stream HTTP agotado
        // como EmptyInput y permite distinguir ese caso de un JSON malformado.
        const int responseLength = http.getSize();
        String respuesta = http.getString();
        String respuestaUtil = respuesta;
        respuestaUtil.trim();
        const bool respuestaVacia = respuestaUtil.length() == 0;
        http.end();

        if (respuestaVacia)
        {
            Serial.print("HTTP 200 sin comandos JSON; Content-Length: ");
            Serial.println(responseLength);
            Serial.print("Bytes recibidos (hex):");
            const size_t bytesAMostrar = min(static_cast<size_t>(respuesta.length()), static_cast<size_t>(16));
            for (size_t i = 0; i < bytesAMostrar; ++i)
            {
                Serial.print(' ');
                if (static_cast<uint8_t>(respuesta[i]) < 0x10)
                {
                    Serial.print('0');
                }
                Serial.print(static_cast<uint8_t>(respuesta[i]), HEX);
            }
            Serial.println();
            return;
        }

        doc.clear();
        // El PHP genera la lista añadiendo "," tras cada comando, dejando una coma
        // final antes de "]" que no es JSON válido; se elimina antes de parsear.
        if (respuestaUtil.endsWith(",]"))
        {
            respuestaUtil.remove(respuestaUtil.length() - 2, 1);
        }
        DeserializationError error = deserializeJson(doc, respuestaUtil);

        if (error)
        {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            // Mostrar el inicio del cuerpo ayuda a detectar avisos/errores PHP o HTML
            // que el servidor antepone al JSON.
            Serial.print("Cuerpo recibido (");
            Serial.print(respuesta.length());
            Serial.print(" bytes): ");
            Serial.println(respuesta);
            Serial.print("Hex de bytes no ASCII:");
            for (size_t i = 0; i < respuesta.length(); ++i)
            {
                const uint8_t b = static_cast<uint8_t>(respuesta[i]);
                if (b < 0x20 || b > 0x7E)
                {
                    Serial.printf(" [%u]=%02X", static_cast<unsigned>(i), b);
                }
            }
            Serial.println();
            return;
        }

        // El servidor responde con una lista de instrucciones que actualiza la UI y
        // ciertos datos de estado que se enviarán en la siguiente petición.
        JsonArray arr = doc.as<JsonArray>();
        if (arr.isNull())
        {
            Serial.println("La respuesta del servidor no es una lista de comandos");
            return;
        }
        ejecutaComandos(arr);
    }
    else
    {
        Serial.print("Error en el requerimiento HTTPs ");
        Serial.println(httpResponseCode);
        http.end();

        // mostrar en pantalla el código de respuesta del servidor
        display.fillScreen(BLACK);
        display.setCursor(0, 50);
        display.setFont(u8g2_font_maniac_te);
        display.setTextSize(1);
        display.setTextColor(YELLOW);
        display.print("Error de respuesta del servidor ");
        display.setTextColor(RED);
        display.println(httpResponseCode);
        display.println();

        if (httpResponseCode == -11 && estado != 200)
        {
            display.setTextColor(WHITE);
            display.println("EL SERVIDOR TARDA");
            display.println("DEMASIADO EN");
            display.println("RESPONDER");
            display.println("");
            display.setFont(u8g2_font_10x20_mf);
            display.println("PROBABLEMENTE HIZO");
            display.println("LA OPERACIÓN IGUAL");
            display.println("Verifica con el supervisor");
            estado = 0;
            // mandar log al servidor
            estado = 200; // indica que la info es un log
            httpRequestData.replace('&', ' ');
            c = "[" + httpRequestData + "]";
            requiereServidor(c);
            estado = 0;
        }

        if (httpResponseCode == -1)
        {
            display.setTextColor(WHITE);
            display.println("No pudo establecer");
            display.println("conexión con el");
            display.println("servidor");
            display.println("");
            display.setFont(u8g2_font_10x20_mf);
            display.println("PROBABLEMENTE HIZO");
            display.println("LA OPERACIÓN IGUAL");
            display.println("Verifica con el supervisor");
            estado = 0;
        }

        display.fillRoundRect(85, 380, 227, 92, 10, BLUE);
        display.setFont(u8g2_font_inb30_mf);
        display.setTextColor(WHITE);
        display.setCursor(116, 439);
        display.print("INICIO");
        teclaApagado(45, 422);
    }
}

void ejecutaComandos(JsonArray arr)
{
    // Cada elemento es un arreglo cuyo primer valor identifica una operación y los
    // siguientes valores son sus argumentos. Los casos forman el protocolo UI servidor.
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
        // Las operaciones 1..10 dibujan la interfaz o cambian el estado de la terminal.
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
            if (texto != nullptr)
            {
                display.print(texto);
            }
            else
            {
                Serial.println("Comando de texto sin contenido");
            }
            break;

        case 7:
            display.fillScreen(arr[i][1]);
            break;

        case 8:
            dibujaTeclado((char)arr[i][1].as<int>(), (char)arr[i][2].as<int>(), arr[i][3].as<uint>(), arr[i][4].as<bool>());
            break;

        case 9:
            estado = arr[i][1];
            break;

        case 10:
            texto = arr[i][1];
            if (texto != nullptr)
            {
                display.println(texto);
            }
            else
            {
                Serial.println("Comando de línea sin contenido");
            }
            break;

        // Las operaciones restantes actualizan datos de negocio o dibujan controles.
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
            texto = arr[i][1].as<const char *>();
            if (!copiarTextoSeguro(sucursalapu, sizeof(sucursalapu), texto))
            {
                Serial.println("Nombre de sucursal inválido o demasiado largo");
            }
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
            texto = arr[i][1].as<const char *>();
            if (!copiarTextoSeguro(ubicacion, sizeof(ubicacion), texto))
            {
                Serial.println("Ubicación inválida o demasiado larga");
            }
            break;

        case 21: // dibuja la tecla cancelar
            teclaSuspender(arr[i][1], arr[i][2]);
            break;

        default:
            break;
        }
    }
    // La tecla de apagado se mantiene visible después de cada lote de instrucciones.
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
    // Construye el botón de encendido/apagado alrededor del centro indicado.
    display.fillRoundRect(posx - 37, posy - 42, 73, 92, 10, RED);
    display.fillArc(posx, posy, 25, 15, 320, 220, WHITE);
    display.fillRect(posx - 5, posy - 25, 11, 30, WHITE);
}

void teclaListo(int posx, int posy)
{
    // Botón verde con marca de confirmación.
    display.fillRoundRect(posx - 37, posy - 42, 73, 92, 10, GREEN);
    display.fillArc(posx + 80, posy + 40, 87, 80, 190, 230, WHITE);
    display.fillArc(posx - 85, posy + 25, 85, 78, 340, 0, WHITE);
    display.fillArc(posx - 1, posy + 5, 30, 25, 340, 290, WHITE);
}

void teclaSuspender(int posx, int posy)
{
    // Botón naranja de cancelación/suspensión, usado cuando lo solicita el servidor.
    display.fillRoundRect(posx - 37, posy - 42, 73, 92, 10, ORANGE);
    display.fillRoundRect(posx - 20, posy - 2, 10, 10, 5, BLACK);
    display.fillRoundRect(posx - 6, posy - 2, 10, 10, 5, BLACK);
    display.fillRoundRect(posx + 8, posy - 2, 10, 10, 5, BLACK);

    display.fillArc(posx - 1, posy + 3, 32, 26, 0, 360, RED);
}

void teclaBasura(int posx, int posy)
{
    // Botón de borrado del teclado numérico.
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
    // Actualiza los indicadores de red, servidor, versión, batería y tiempo encendido.
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

        // La reconexión no se inicia desde el repintado; gestionarWiFi() la supervisa
        // con un intervalo para mantener el ciclo de interfaz disponible.
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

    // servidor:
    display.fillRect(94, 0, 24, 19, BLACK);
    display.setFont(u8g2_font_mozart_nbp_tf);
    display.setCursor(94, 8);
    display.setTextColor(MAGENTA);
    display.print("SERV");
    display.setCursor(94, 18);
    if (servidor == "192.168.101.64")
    {
        display.print("64");
    }
    else if (servidor == "192.168.2.3")
    {
        display.print("03");
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
    // Promedia diez lecturas ADC en milivoltios y aplica el factor del divisor resistivo.
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

    // APAGADO es el umbral de inactividad; el toque actualiza tiempo_encendido.
    if ((esp_timer_get_time() / 1000000 - tiempo_encendido) > APAGADO)
    {
        Serial.println("apagando por tiempo inactivo");
        digitalWrite(ONOFF, LOW); // apagar
        delay(5000);
    }
}

void apagando()
{
    // Presenta las opciones de apagar, mantener encendida la terminal o configurar la red.
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
    display.setTextColor(BLUE);
    display.setCursor(120, 420);
    display.println("MANTENER");
    display.setCursor(112, 450);
    display.print("ENCENDIDO");

    // dibujaTeclado(4, 4, CYAN, false);

    // tecla configurar
    display.fillRoundRect(8, 188, 74, 92, 10, DARKCYAN);
    display.setFont(u8g2_font_inb21_mf);
    display.setTextColor(WHITE);
    display.setCursor(8, 245);
    display.println("CONF");

    display.fillArc(posx, posy, 25, 15, 320, 220, WHITE);
    display.fillRect(posx - 5, posy - 25, 11, 30, WHITE);

    estado = APAGANDO;
}

// Habilita el escáner con su comando UART y marca el estado local como activo.
void escanerOn()
{

    byte buf88[] = {0x04, 0xE9, 0x04, 0x00, 0xFF, 0x0F}; // SCAN_ENABLE
    byte largo8 = sizeof(buf88);
    enviaComando(buf88, largo8, 1); // un solo intento para no bloquear la UI
    escaner = true;
}

// Deshabilita el escáner con su comando UART y marca el estado local como inactivo.
void escanerOff()
{

    byte buf6[] = {0x04, 0xEA, 0x04, 0x00, 0xFF, 0x0E}; // SCAN_DISABLE
    byte largo = sizeof(buf6);
    enviaComando(buf6, largo, 1); // un solo intento para no bloquear la UI
    escaner = false;
}

// Restablece el lector y configura inducción automática, terminador CR y lectura activa.
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
// Envía primero el byte de activación y reintenta el comando hasta reconocer ACK,
// respuesta de versión o agotar cinco esperas de 500 ticks.
bool enviaComando(byte com[], int largo, byte maxIntentos)
{
    byte buf[] = {0x00}; // Wake up
    Serial2.write(buf, sizeof(buf));
    delay(50);

    byte intentos = 0;
    bool ack = false;
    String recibido = "";

    while (intentos < maxIntentos && !ack)
    {
        // Descarta restos de respuestas anteriores para no desfasar la lectura del ACK.
        while (Serial2.available() > 0)
        {
            Serial2.read();
        }
        Serial2.write(com, largo);            // envía el comando al escaner
        const uint32_t start = millis();

        // Cede tiempo al planificador mientras espera el ACK para no ocupar CPU en vacío.
        while (!Serial2.available() && static_cast<uint32_t>(millis() - start) < 500)
        {
            delay(1);
        }

        if (static_cast<uint32_t>(millis() - start) < 500)
        {
            // La respuesta llega fragmentada: se acumula hasta 20 ms sin bytes nuevos.
            recibido = "";
            uint32_t ultimoByte = millis();
            while (static_cast<uint32_t>(millis() - ultimoByte) < 20 &&
                   static_cast<uint32_t>(millis() - start) < 500)
            {
                if (Serial2.available())
                {
                    letra = Serial2.read();
                    ultimoByte = millis();
                    if (recibido.length() < 64)
                    {
                        recibido += letra; // cada byte se agrega en decimal: ACK = "42080025544"
                    }
                }
                else
                {
                    delay(1);
                }
            }

            if (static_cast<uint32_t>(millis() - start) < 500)
            {
                Serial.println(recibido);

                if (recibido.indexOf("42080025544") >= 0 || recibido.startsWith("8716400801")) // llegó ack o versión
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
// Inicializa el modo de captura de cantidades y dibuja sus controles; el servidor
// proporciona el máximo permitido y el origen asociado a la operación.
void tecladoNumerico(int codigo, int max, int origen, int aux4)
{
    cantidad = 0;
    maximo_subir = max;
    idIndice = origen;
    dibujaTeclado(4, 4, DARKGREEN, false);
    poneNumeros();
}

#endif