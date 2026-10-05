#include "Goodix.h"
#include "Wire.h"

// El controlador táctil puede interrumpir en cualquier momento. La ISR solo deja una
// marca; la lectura I2C y el callback se ejecutan luego desde Goodix::loop().
volatile bool goodixIRQ = false;

#if defined(ESP8266)
void ICACHE_RAM_ATTR _goodix_irq_handler() {
  noInterrupts();
  goodixIRQ = true;
  interrupts();
}
#elif defined(ESP32)
void IRAM_ATTR _goodix_irq_handler() {
  goodixIRQ = true;
}
#else
void _goodix_irq_handler() {
  noInterrupts();
  goodixIRQ = true;
  interrupts();
}
#endif


// Implementación del controlador Goodix por I2C.
Goodix::Goodix()
    : i2cAddr(GOODIX_I2C_ADDR_BA), config{}, info{}, points{},
      intPin(0), rstPin(0), touchHandler(nullptr) {}

// Registra la función de la aplicación que recibirá las coordenadas táctiles.
void Goodix::setHandler(void (*handler)(int8_t, GTPoint*)) {
  touchHandler = handler;
}

bool Goodix::begin(uint8_t interruptPin, uint8_t resetPin, uint8_t addr) {
  // Guarda pines/dirección y espera la estabilización del sensor antes de resetearlo.
  intPin = interruptPin;
  rstPin = resetPin;
  i2cAddr = addr;

  // El controlador necesita estabilizarse antes y después de la secuencia de reset.
  msSleep(300);
  bool result = reset();
  msSleep(200);

  return result;
}


bool Goodix::reset() {
  // La secuencia de niveles en INT durante RESET selecciona la dirección I2C del chip.
  msSleep(1);

  pinMode(intPin, OUTPUT);
  pinMode(rstPin, OUTPUT);

  digitalWrite(intPin, LOW);
  digitalWrite(rstPin, LOW);

  /* begin select I2C slave addr */

  /* T2: > 10ms */
  msSleep(11);

  /* HIGH: 0x28/0x29 (0x14 7bit), LOW: 0xBA/0xBB (0x5D 7bit) */
  digitalWrite(intPin, i2cAddr == GOODIX_I2C_ADDR_28);

  /* T3: > 100us */
  usSleep(110);
  pinMode(rstPin, INPUT);
  //if (!pinCheck(rstPin, HIGH))
  //  return false;

  /* T4: > 5ms */
  msSleep(6);
  digitalWrite(intPin, LOW);
  /* end select I2C slave addr */

  /* T5: 50ms */
  msSleep(51);
  pinMode(intPin, INPUT); // INT pin has no pullups so simple set to floating input

  attachInterrupt(intPin, _goodix_irq_handler, RISING);

  return true;
}

// Lee cuatro caracteres de identificación y añade el terminador NUL en el buffer destino.
uint8_t Goodix::productID(char *target) {
  // Lee una sola vez el bloque y copia el ID en el buffer del llamador.
  if (target == nullptr || readInfo() == nullptr) {
    return 1;
  }

  memcpy(target, info.productId, sizeof(info.productId));
  target[4] = 0;
  return 0;
}

// Comprueba que el sensor responde leyendo un byte de configuración por I2C.
uint8_t Goodix::test() {
  // Lee un byte del bloque de configuración para comprobar la comunicación I2C.
  uint8_t testByte;
  return readBytes(GOODIX_REG_CONFIG_DATA,  &testByte, 1);
}

uint8_t Goodix::calcChecksum(const uint8_t* buf, uint8_t len) {
  // El complemento a dos hace que la suma del bloque más este byte resulte cero
  // módulo 256, como espera el controlador Goodix.
  uint8_t ccsum = 0;
  for (uint8_t i = 0; i < len; i++) {
    ccsum += buf[i];
  }

  ccsum = (~ccsum) + 1;
  return ccsum;
}

uint8_t Goodix::readChecksum() {
	// El mapa de configuración se lee en dos tramos porque la dirección central tiene
	// un registro de checksum separado que no forma parte del bloque de datos.
	uint8_t len1 = GOODIX_REG_CONFIG_MIDDLE - GOODIX_REG_CONFIG_DATA +1;
	uint8_t len2 = GOODIX_REG_CONFIG_END - GOODIX_REG_CONFIG_MIDDLE;
	uint8_t buf1[len1];
	uint8_t buf2[len2];
	uint8_t buf[len1+len2];
	if (!readBytes(GOODIX_REG_CONFIG_DATA, buf1, len1) ||
        !readBytes(GOODIX_REG_CONFIG_MIDDLE+1, buf2, len2)) {
        return 0;
    }
    memcpy(buf, buf1, sizeof(buf1));
	memcpy(buf+sizeof(buf1), buf2, sizeof(buf2));	
  	
	return calcChecksum(buf, len1+len2);
}

void Goodix::fwResolution(uint16_t maxX, uint16_t maxY) {
	// Cambia la resolución X/Y en la copia de la configuración y escribe su checksum.
	uint8_t len1 = GOODIX_REG_CONFIG_MIDDLE - GOODIX_REG_CONFIG_DATA +1;
	uint8_t len2 = GOODIX_REG_CONFIG_END - GOODIX_REG_CONFIG_MIDDLE;
	uint8_t buf1[len1];
	uint8_t buf2[len2];
	uint8_t buf3[2];
	uint8_t buf[len1+len2];
	
	if (!readBytes(GOODIX_REG_CONFIG_DATA, buf1, len1) ||
        !readBytes(GOODIX_REG_CONFIG_MIDDLE+1, buf2, len2)) {
        return;
    }
    memcpy(buf, buf1, sizeof(buf1));
	memcpy(buf+sizeof(buf1), buf2, sizeof(buf2));

  	buf[0]++;
	buf[1] = (maxX & 0xff);
	buf[2] = (maxX >> 8);
	buf[3] = (maxY & 0xff);
	buf[4] = (maxY >> 8);
	buf3[0] = calcChecksum(buf, len1+len2);
    buf3[1] = 0x01;

    if (writeBytes(GOODIX_REG_CONFIG_DATA, buf, len1+len2)) {
        writeBytes(GOODIX_REG_CONFIG_END+1, buf3, 2);
    }
}

uint8_t Goodix::configCheck(bool configVersion) {
	// Comprueba que responde un controlador compatible y valida checksum; cuando se
	// solicita, además compara la configuración con el perfil LilyPi incluido.
	uint8_t len1 = GOODIX_REG_CONFIG_MIDDLE - GOODIX_REG_CONFIG_DATA +1;
	uint8_t len2 = GOODIX_REG_CONFIG_END - GOODIX_REG_CONFIG_MIDDLE;
	uint8_t buf1[len1];
	uint8_t buf2[len2];
	uint8_t buf[len1+len2];
	uint8_t diff = 0;
	uint8_t calc_check_sum;
	uint8_t read_check_sum[1];
	char prodID[5] = {};
	
    if (!write(GOODIX_REG_COMMAND, 0)) {
        return 1;
    }
    
    if (productID(prodID) != 0)
    {
        return 1;
    }
    if (prodID[0] != '9')
    {
	    return (prodID[0]);
    }
    if (!readBytes(GOODIX_REG_CONFIG_DATA, buf1, len1) ||
        !readBytes(GOODIX_REG_CONFIG_MIDDLE+1, buf2, len2) ||
        !readBytes(GOODIX_REG_CONFIG_END+1, read_check_sum, 1)) {
        return 1;
    }
    memcpy(buf, buf1, sizeof(buf1));
	memcpy(buf+sizeof(buf1), buf2, sizeof(buf2));	
	calc_check_sum = calcChecksum(buf, len1+len2);
	
	if (configVersion)
	{
		
		for (uint8_t i=0; i<(len1+len2); i++) {
			if (LilyPi_config[i] != buf[i])
				{
					diff++;
				}
		}
	}
	if (read_check_sum[0] != calc_check_sum)
		{
			diff++;
		}
	return (diff);
}

void Goodix::configUpdate() {
	// Aplica el perfil LilyPi únicamente al modelo cuyo ID de producto empieza por '9'.
	uint8_t len1 = GOODIX_REG_CONFIG_MIDDLE - GOODIX_REG_CONFIG_DATA +1;
	uint8_t len2 = GOODIX_REG_CONFIG_END - GOODIX_REG_CONFIG_MIDDLE;
	uint8_t buf[2];
	char prodID[5] = {};

    buf[0] = calcChecksum(LilyPi_config, len1+len2);
    buf[1] = 0x01;
    if (!write(GOODIX_REG_COMMAND, 0) ||
        productID(prodID) != 0 || prodID[0] != '9')
    {
	    return;
    }
    if (writeBytes(GOODIX_REG_CONFIG_DATA, LilyPi_config, len1+len2)) {
        writeBytes(GOODIX_REG_CONFIG_END+1, buf, 2);
    }
}

GTConfig* Goodix::readConfig() {
  // Lee el bloque de configuración del sensor en la estructura pública config.
  return readBytes(GT_REG_CFG, (uint8_t *) &config, sizeof(config)) ? &config : nullptr;
}

GTInfo* Goodix::readInfo() {
  // Lee identificación, versión y resolución reportadas por el controlador.
  uint8_t rawInfo[11];
  if (!readBytes(GT_REG_DATA, rawInfo, sizeof(rawInfo))) {
    return nullptr;
  }
  memcpy(info.productId, rawInfo, sizeof(info.productId));
  info.fwId = static_cast<uint16_t>(rawInfo[4]) |
              (static_cast<uint16_t>(rawInfo[5]) << 8);
  info.xResolution = static_cast<uint16_t>(rawInfo[6]) |
                     (static_cast<uint16_t>(rawInfo[7]) << 8);
  info.yResolution = static_cast<uint16_t>(rawInfo[8]) |
                     (static_cast<uint16_t>(rawInfo[9]) << 8);
  info.vendorId = rawInfo[10];
  return &info;
}

void Goodix::onIRQ() {
  // Lee el informe de contactos, convierte los pares de bytes little-endian y entrega
  // hasta cinco puntos al callback registrado; al final libera el registro de datos.
  int16_t contacts;
  uint8_t rawdata[GOODIX_MAX_CONTACTS * GOODIX_CONTACT_SIZE]; //points buffer

  contacts = readInput(rawdata);
  
  	if (contacts < 0)
  	{
	  	return;
  	}
  	
    if (contacts > 0) {
    
	for (int8_t i = 0; i < contacts; ++i) {
	  const uint8_t offset = 1 + i * GOODIX_CONTACT_SIZE;
	  points[i].trackId = rawdata[offset];
	  points[i].x = ((uint16_t)rawdata[offset + 2] << 8) | rawdata[offset + 1];
	  points[i].y = ((uint16_t)rawdata[offset + 4] << 8) | rawdata[offset + 3];
	  points[i].area = ((uint16_t)rawdata[offset + 6] << 8) | rawdata[offset + 5];
	  points[i].reserved = 0;
	}

    if (touchHandler != nullptr) {
      touchHandler(contacts, points);
    }
	}
	write(GOODIX_READ_COORD_ADDR, 0);
}

void Goodix::loop() {
  // Copia y limpia la marca atómica con interrupciones deshabilitadas para evitar
  // perder una notificación mientras se procesa el informe anterior.
  noInterrupts();
  bool irq = goodixIRQ;
  goodixIRQ = false;
  interrupts();

  if (irq) {
    onIRQ();
  }
}

#define GOODIX_TRY_AGAIN 100
#define GOODIX_I2C_READ_ERROR 155

int16_t Goodix::readInput(uint8_t *regState) {
  // La bandera 0x80 indica que el controlador tiene un informe nuevo; los cuatro bits
  // inferiores contienen la cantidad de contactos activos.
  if (regState == nullptr ||
      !readBytes(GOODIX_READ_COORD_ADDR, regState, GOODIX_CONTACT_SIZE * GOODIX_MAX_CONTACTS)) {
    return -GOODIX_I2C_READ_ERROR;
  }

  if (!(regState[0] & 0x80)) {
    return -GOODIX_TRY_AGAIN;
  }

  const uint8_t touchCount = regState[0] & 0x0F;
  return touchCount > GOODIX_MAX_CONTACTS ? GOODIX_MAX_CONTACTS : touchCount;
}

//----- Utils -----
void Goodix::i2cStart(uint16_t reg) {
	// Inicia una transacción y coloca primero el byte alto de la dirección del registro.
	Wire.beginTransmission(i2cAddr);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
}

bool Goodix::write(uint16_t reg, uint8_t buf) {
  // Escribe un byte en el registro indicado.
  i2cStart(reg);
  const bool accepted = Wire.write(buf) == 1;
  const uint8_t result = Wire.endTransmission();
  return accepted && result == 0;
}

bool Goodix::writeBytes(uint16_t reg, const uint8_t *data, int nbytes)
{
	// Escribe de forma contigua un bloque en el registro indicado.
	if (data == nullptr || nbytes <= 0 ||
        nbytes > 0x10000L - static_cast<uint32_t>(reg) ||
        I2C_BUFFER_LENGTH <= 2) {
		return false;
	}

    const int maxPayload = I2C_BUFFER_LENGTH - 2;
    for (int offset = 0; offset < nbytes; ) {
        const int chunk = min(nbytes - offset, maxPayload);
        i2cStart(static_cast<uint16_t>(reg + offset));
        if (Wire.write(data + offset, chunk) != static_cast<size_t>(chunk)) {
            Wire.endTransmission();
            return false;
        }
        if (Wire.endTransmission() != 0) {
            return false;
        }
        offset += chunk;
    }
    return true;
}

bool Goodix::readBytes(uint16_t reg, uint8_t *data, int nbytes)
{
	// Selecciona el registro, solicita el bloque y devuelve true solo si llegaron todos
	// los bytes pedidos.
	if (data == nullptr || nbytes <= 0 ||
        nbytes > 0x10000L - static_cast<uint32_t>(reg) ||
        I2C_BUFFER_LENGTH <= 0) {
		return false;
	}

    for (int offset = 0; offset < nbytes; ) {
        const int chunk = min(nbytes - offset, I2C_BUFFER_LENGTH);
        i2cStart(static_cast<uint16_t>(reg + offset));
        if (Wire.endTransmission() != 0) {
            return false;
        }
        const uint8_t requested = Wire.requestFrom(i2cAddr, static_cast<uint8_t>(chunk));
        int received = 0;
        while (Wire.available() && received < chunk) {
            data[offset + received] = Wire.read();
            ++received;
        }
        if (requested != chunk || received != chunk) {
            while (Wire.available()) {
                Wire.read();
            }
            return false;
        }
        offset += chunk;
    }
    return true;
}

void Goodix::msSleep(uint16_t milliseconds) {
  // Abstracción de espera en milisegundos usada por la secuencia de reset.
  delay(milliseconds);
}

void Goodix::usSleep(uint16_t microseconds) {
  // Abstracción de espera corta en microsegundos usada por la secuencia de reset.
  delayMicroseconds(microseconds);
}