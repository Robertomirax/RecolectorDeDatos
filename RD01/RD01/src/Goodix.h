#ifndef _GOODIX_H_
#define _GOODIX_H_

// Interfaz de la capa de bajo nivel para el controlador táctil Goodix: configuración,
// acceso a registros I2C y notificación de coordenadas de hasta cinco contactos.
#include <Arduino.h>
#include "GoodixStructs.h"
#include "GoodixFW.h"

#define GOODIX_OK   0

// Direcciones I2C de 7 bits; la dirección se selecciona durante el pulso de reset.
#define GOODIX_I2C_ADDR_28  0x14
#define GOODIX_I2C_ADDR_BA  0x5D

// Cada contacto ocupa ocho bytes en el informe del sensor.
#define GOODIX_CONTACT_SIZE   8
#define GOODIX_MAX_CONTACTS   5

// Registros base para configuración, identificación e informe de coordenadas.
#define GT_REG_CFG  0x8047
#define GT_REG_DATA 0x8140

// Registro de comando de escritura del controlador.
#define GOODIX_REG_COMMAND        0x8040

// Inicio del bloque de configuración legible y escribible.
#define GOODIX_REG_CONFIG_DATA  0x8047

// Límites del bloque de configuración; el registro central guarda el checksum.
#define GOODIX_REG_CONFIG_MIDDLE	0x80A2
#define GOODIX_REG_CONFIG_END		0x80FE

// Registros de solo lectura: identificación del producto e informe de coordenadas.
#define GOODIX_REG_ID           0x8140

// Inicio del informe de contacto, incluida la bandera de disponibilidad.
#define GOODIX_READ_COORD_ADDR  0x814E


class Goodix {
  public:
    // Datos leídos del sensor y puntos decodificados disponibles para la aplicación.
    uint8_t i2cAddr;
    struct GTConfig config;
    struct GTInfo info;
    struct GTPoint points[GOODIX_MAX_CONTACTS]; //processed points

    // Crea el controlador; setHandler asocia el callback antes de inicializarlo.
    Goodix();

    // Registra el callback que recibe la cantidad de contactos y el arreglo de puntos.
    void setHandler(void (*handler)(int8_t, GTPoint*));

    // Inicializa pines, dirección y secuencia de reset. loop() procesa después las
    // interrupciones pendientes y entrega contactos mediante el callback registrado.
    bool begin(uint8_t interruptPin, uint8_t resetPin, uint8_t addr=GOODIX_I2C_ADDR_BA);
    // Ejecuta la secuencia eléctrica de reset para seleccionar dirección e iniciar sensor.
    bool reset();
    // Prueba la respuesta I2C leyendo un byte de configuración.
    uint8_t test();
    // Procesa fuera de la ISR la lectura del informe táctil pendiente.
    void loop();

    // Acceso genérico a registros por I2C; true indica que la transferencia terminó bien.
    bool write(uint16_t reg, uint8_t value);
    bool writeBytes(uint16_t reg, uint8_t *data, int nbytes);
    bool readBytes(uint16_t reg, uint8_t *data, int nbytes);

    // Utilidades para leer, comprobar o actualizar el bloque de configuración Goodix.
    uint8_t calcChecksum(uint8_t* buf, uint8_t len);
    uint8_t readChecksum();

    // Actualiza resolución/configuración y su checksum en el controlador.
    void fwResolution(uint16_t maxX, uint16_t maxY);
    void configUpdate();
    uint8_t configCheck(bool isLilyPi);
    
    // Lectura de identificación, configuración e informe bruto de contactos.
    GTConfig* readConfig();
    GTInfo* readInfo();

    // Escribe cuatro caracteres de ID y un terminador NUL en un buffer de al menos 5 bytes.
    uint8_t productID(char *buf);

    // Lee el informe; los valores negativos señalan error I2C o informe aún no disponible.
    int16_t readInput(uint8_t *data);

  //--- Private routines ---
  private:
    uint8_t intPin, rstPin;
    void (*touchHandler)(int8_t, GTPoint*);

    void onIRQ();

    //--- utils ---
    void usSleep(uint16_t microseconds);
    void msSleep(uint16_t milliseconds);

    // --- I2C helper ---
    void i2cStart(uint16_t reg);
};

#endif
