// Estructuras que describen los registros del controlador Goodix. Los tipos y el orden
// corresponden al mapa de registros documentado por el fabricante del sensor.
struct GTInfo
{
  // Identificación del chip, revisión de firmware y resolución reportada.
  char productId[4];
  uint16_t fwId;
  uint16_t xResolution;
  uint16_t yResolution;
  uint8_t vendorId; // Identificador del fabricante del sensor.
};

struct GTPoint
{
  // Un punto de contacto decodificado: ID de seguimiento, coordenadas y área.
  uint8_t trackId;
  uint16_t x;
  uint16_t y;
  uint16_t area;
  uint8_t reserved; // Byte reservado por el formato del informe.
};

struct GTLevelConfig
{
  // Umbral para iniciar y finalizar la detección de un toque.
  uint8_t touch; // Umbral para iniciar la detección de contacto.
  uint8_t leave; // Umbral para considerar que el contacto terminó.
};

struct GTStylusConfig
{
  // Ganancias de transmisión/recepción y desplazamiento de lectura del lápiz.
  uint8_t txGain;
  uint8_t rxGain;
  uint8_t dumpShift;
  GTLevelConfig level;
  uint8_t control; // Tiempo de salida del modo lápiz, en segundos.
};

struct GTFreqHoppingConfig
{
  // Frecuencia y factor de cada segmento de salto de frecuencia contra interferencia.
  uint16_t hoppingBitFreq;
  uint8_t hoppingFactor;
};

struct GTKeyConfig
{
  // Posiciones de las teclas auxiliares (0 indica que no hay contacto en esa tecla).
  uint8_t pos1;
  uint8_t pos2;
  uint8_t pos3;
  uint8_t pos4;
  uint8_t area;
  GTLevelConfig level;
  // Sensibilidad de los pares de teclas y restricción de detección.
  uint8_t sens12;
  uint8_t sens34;
  uint8_t restrain;
};

struct GTConfig
{
  // Imagen del bloque que comienza en 0x8047; los comentarios de dirección indican
  // las posiciones del mapa de registros y los campos NC reservan bytes no interpretados.
  uint8_t configVersion;
  uint16_t xResolution;
  uint16_t yResolution;
  // 0x804C
  uint8_t touchNumber; // 3:0 Touch No.: 1~10

  // Opciones del módulo; los bits bajos seleccionan el tipo de disparo de INT.
  uint8_t moduleSwitch1;
  uint8_t moduleSwitch2; // Opciones adicionales; bit 0 habilita teclas táctiles.
  uint8_t shakeCount;    // Filtro contra variaciones rápidas de posición.
  // 0x8050
  // Filtros de coordenadas, reducción de ruido y umbrales de inicio/fin de toque.
  uint8_t filter;
  uint8_t largeTouch;
  uint8_t noiseReduction;
  GTLevelConfig screenLevel;

  uint8_t lowPowerControl; // Tiempo antes de pasar a bajo consumo.
  uint8_t refreshRate;     // Periodo del informe: 5 + N milisegundos.
  uint8_t xThreshold;      // Umbral de movimiento en X.
  uint8_t yThreshold;      // Umbral de movimiento en Y.
  uint8_t xSpeedLimit;     // Límite de velocidad reportada en X.
  uint8_t ySpeedLimit;     // Límite de velocidad reportada en Y.
  uint8_t vSpace;          // Separación vertical superior/inferior codificada en nibbles.
  uint8_t hSpace;          // Separación horizontal izquierda/derecha codificada en nibbles.
  // 0x805D-0x8061
  uint8_t stretchRate; // Compensación de estiramiento débil.
  uint8_t stretchR0;   // Coeficiente del intervalo 1.
  uint8_t stretchR1;   // Coeficiente del intervalo 2.
  uint8_t stretchR2;   // Coeficiente del intervalo 3.
  uint8_t stretchRM;   // Valor base común de los intervalos.

  // Número de canales del panel y factores de frecuencia del escaneo.
  uint8_t drvGroupANum;
  uint8_t drvGroupBNum;
  uint8_t sensorNum;
  uint8_t freqAFactor;
  uint8_t freqBFactor;
  // 0x8067
  uint16_t pannelBitFreq;    // Frecuencia base de los grupos de drivers A/B.
  uint16_t pannelSensorTime; // Tiempo de integración/medición del sensor.
  // Ganancias analógicas, desplazamiento de lectura y control de trama.
  uint8_t pannelTxGain;
  uint8_t pannelRxGain;
  uint8_t pannelDumpShift;
  uint8_t drvFrameControl;
  // 0x806F - 0x8071
  uint8_t NC_2[3]; // Bytes reservados para conservar el mapa de registros.
  GTStylusConfig stylusConfig;
  // 0x8078-0x8079
  uint8_t NC_3[2]; // Bytes reservados.
  uint8_t freqHoppingStart; // Inicio del salto de frecuencia, en unidades de 2 kHz.
  uint8_t freqHoppingEnd;   // Fin del salto de frecuencia, en unidades de 2 kHz.
  uint8_t noiseDetectTims;  // Duración de detección de ruido.
  uint8_t hoppingFlag;      // Habilitación/estado del salto de frecuencia.
  uint8_t hoppingThreshold; // Umbral para activar el salto.

  uint8_t noiseThreshold; // Umbral de ruido del panel.
  uint8_t NC_4[2];         // Bytes reservados.
  // 0x8082
  GTFreqHoppingConfig hoppingSegments[5];
  // 0x8091
  uint8_t NC_5[2]; // Bytes reservados.
  GTKeyConfig keys;
};
