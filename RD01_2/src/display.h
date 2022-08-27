#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>

#define MODO_TECLADO_NUMEROS 1 // la pantalla está en modo teclado numérico 4x4
#define MODO_TECLADO_LETRAS 2  // la pantalla está en el modo teclado letras
#define BORRAR 10              // tecla borrar
#define LISTO 11               // Tecla listo

// pines display
#define TFT_SCK 18
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RESET 4
#define TFT_TIPO 5
// fin pines display

// declaración de funciones -------------------------------------

int tocoPantalla(uint16_t x, uint16_t y);
void pantalla_1();

// fin declaración de funciones ---------------------------------

// inicializaciones de objetos y variables -------------------------
Arduino_ESP32SPI bus = Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO); // objeto que maneja la conexión SPI con el display

Arduino_ILI9488_18bit display = Arduino_ILI9488_18bit(&bus, TFT_RESET, 0, false); // objeto que maneja el display ILI9488
byte modo = 0;
// fin inicializaciones de objetos y variables

/***************************************************************************************/
/*!
    @brief   Dibuja el teclado en la pantalla
    @param   ancho ancho del display
    @param   alto alto del display
    @param   x distancia a los bordes izquierdo derecho e inferior
    @param   y distancia desde el margen superior
    @param   r radio de las esquinas de los botones
    @param   botx cantidad de botones en la horizontal
    @param   boty cantidad de botones en la vertical
    @param   s separación horizontal entre botones
    @param   v separación vertical entre botones
    @param   color color del botón 16-bit 5-6-5
    @param   fondo fondo del botón vacío = false o lleno = true
*/
/***************************************************************************************/
void dibujaTeclado(int ancho = 320, int alto = 480, int x = 8, int y = 92, int r = 10, char botx = 4, char boty = 4, int s = 4, int v = 4, uint color = GREEN, bool fondo = false)
{
    int16_t d = (ancho - 2 * x - (botx - 1) * s) / botx; // ancho del botón
    Serial.print("ancho del boton ");
    Serial.println(d);
    x = (ancho - d * botx - s * (botx - 1)) / 2;         // distancia desde el margen izquierdo

    int16_t h = (alto - x - y - (boty - 1) * v) / boty; // alto del botón
    Serial.print("alto del boton ");
    Serial.println(h);

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
    int numero = -1;

    switch (modo)
    {
    case MODO_TECLADO_NUMEROS:

    //primera fila
        if (x > 8 && x < 81 && y > 92 && y < 184)
        {
            numero = 0;
        }

        if (x > 85 && x < 158 && y > 92 && y < 184)
        {
            numero = 1;
        }

        if (x > 162 && x < 235 && y > 92 && y < 184)
        {
            numero = 2;
        }

        if (x > 239 && x < 312 && y > 92 && y < 184)
        {
            numero = 3;
        }

        //segunda fila
        if (x > 8 && x < 81 && y > 188 && y < 280)
        {
            numero = 4;
        }

        if (x > 85 && x < 158 && y > 188 && y < 280)
        {
            numero = 5;
        }

        if (x > 162 && x < 235 && y > 188 && y < 280)
        {
            numero = 6;
        }

        if (x > 239 && x < 312 && y > 188 && y < 280)
        {
            numero = 7;
        }


        //tercera fila
        if (x > 8 && x < 81 && y > 284 && y < 376)
        {
            numero = 8;
        }

        if (x > 85 && x < 158 && y > 284 && y < 376)
        {
            numero = 9;
        }

        if (x > 162 && x < 235 && y > 284 && y < 376)
        {
            numero = 10;
        }

        if (x > 239 && x < 312 && y > 284 && y < 376)
        {
            numero = 11;
        }

        //cuarta fila
        if (x > 8 && x < 81 && y > 380 && y < 472)
        {
            numero = 12;
        }

        if (x > 85 && x < 158 && y > 380 && y < 472)
        {
            numero = 13;
        }

        if (x > 162 && x < 235 && y > 380 && y < 472)
        {
            numero = 14;
        }

        if (x > 239 && x < 312 && y > 380 && y < 472)
        {
            numero = 15;
        }


        break;

    default:
        numero = -1;
        break;
    }
    return numero;
}

/***************************************************************************************/
/*!
    @brief   Dibuja la pantalla número 1
*/
/***************************************************************************************/
void pantalla_1()
{
    modo = MODO_TECLADO_NUMEROS;

    display.fillScreen(BLACK);
    display.fillRoundRect(5, 5, 310, 50, 10, WHITE); // visor del número digitado

    display.drawFastHLine(0, 479, 319, WHITE);
    display.drawFastVLine(0, 0, 479, WHITE);
    display.drawFastHLine(0, 0, 319, WHITE);
    display.drawFastVLine(319, 0, 479, WHITE);

    // dibujaTeclado(320, 480, 8, 92, 10, 4, 3); // 12 TECLAS
    dibujaTeclado(); // 16 teclas

    // números
    display.setFont(u8g2_font_inb33_mf); // maniac_te);
    display.setTextSize(1);
    display.setTextColor(YELLOW);

    // primera fila
    display.setCursor(30, 155);
    display.println("0");

    display.setCursor(107, 155);
    display.println("1");

    display.setCursor(184, 155);
    display.println("2");

    display.setCursor(261, 155);
    display.println("3");

    // segunda fila
    display.setCursor(30, 249);
    display.println("4");

    display.setCursor(107, 249);
    display.println("5");

    display.setCursor(184, 249);
    display.println("6");

    display.setCursor(261, 249);
    display.println("7");

    //tercera fila
    display.setCursor(30, 343);
    display.println("8");

    display.setCursor(107, 343);
    display.println("9");

    display.setCursor(184, 343);
    display.println("A");

    display.setCursor(261, 343);
    display.println("B");

    //cuarta fila
    //display.setFont(u8g2_font_inb16_mf);
    display.setCursor(30, 437);
    display.setTextColor(WHITE);
    display.println("C");

    //display.setFont(u8g2_font_inb33_mf);
    display.setTextColor(YELLOW);
    display.setCursor(107, 437);
    display.println("D");

    //display.setFont(u8g2_font_inb16_mf);
    display.setTextColor(BLUE);
    display.setCursor(184, 437);
    display.println("E");

    display.setFont(u8g2_font_inb33_mf);
    display.setCursor(261, 437);
    display.println("F");
}
