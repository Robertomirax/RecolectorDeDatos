# Recolector de datos RD01

Firmware para una terminal basada en ESP32 que lee códigos desde un escáner UART,
presenta una interfaz táctil en un display ILI9488 y envía las operaciones a un servidor
HTTP. El proyecto también permite configurar la red desde el lector y comprobar
actualizaciones de firmware durante el arranque.

## Hardware y conexiones

| Función | GPIO / interfaz |
| --- | --- |
| Control de alimentación | GPIO 26 |
| Escáner, recepción (RX2) | GPIO 16 |
| Escáner, transmisión (TX2) | GPIO 17 |
| Goodix, interrupción | GPIO 5 |
| Goodix, reset | GPIO 27 |
| Bus I2C | SDA GPIO 13, SCL GPIO 12 |
| Display ILI9488 por SPI | SCK 18, MOSI 23, MISO 19, CS 15, DC 2, RESET 4 |
| Medición de batería | GPIO 35 |

La asignación del bus I2C y los pines SPI se configura en `src/main.cpp` y
`src/metodos.h`. El escáner usa `Serial2` a 9600 baudios; el puerto de diagnóstico USB
usa `Serial` a 115200 baudios.

## Organización del proyecto

- `src/main.cpp`: secuencia de arranque y ciclo principal de atención.
- `src/metodos.h`: estado compartido y lógica de pantalla, teclado, escáner, Wi-Fi,
  peticiones al servidor y actualización OTA.
- `src/Goodix.h`, `src/Goodix.cpp`: interfaz e implementación del acceso I2C al
  digitalizador Goodix.
- `src/GoodixStructs.h`: estructuras que representan los informes y registros Goodix.
- `src/GoodixFW.h`: firmware/configuración binaria del controlador táctil; los valores
  dependen del hardware y no son texto editable.
- `platformio.ini`: placa, framework, librerías y opciones de compilación.
- `.editorconfig` y `.vscode/settings.json`: UTF-8 como codificación predeterminada.

## Requisitos y compilación

1. Instalar Visual Studio Code con PlatformIO IDE, o instalar PlatformIO Core.
2. Abrir en PlatformIO la carpeta que contiene `platformio.ini`.
3. Construir el firmware:

   ```powershell
   pio run
   ```

La configuración usa el entorno `esp32doit-devkit-v1`, plataforma `espressif32` y
framework Arduino. Las dependencias de gráficos, fuentes y JSON se declaran en
`platformio.ini`. Para subir por USB, conectar la placa y ejecutar:

```powershell
pio run --target upload
```

El monitor serie se puede iniciar con `pio device monitor`; su velocidad configurada es
115200 baudios.

## Puesta en marcha y comunicación

Al iniciar, el firmware inicializa alimentación, puertos serie, I2C, táctil y display.
Después recupera de NVS (`Preferences`) el servidor y las credenciales Wi-Fi, configura
el escáner, se conecta a la red y consulta si existe firmware nuevo.

Si no hay credenciales Wi-Fi guardadas, se pueden proporcionar escaneando un QR que el
lector entregue como texto separado por punto y coma. El parser busca el prefijo
`WIFI:T:nopass`, el campo `S:` para el SSID y el campo `P:` para la contraseña. Al
reconocerlo, guarda ambos campos bajo el espacio NVS `credenciales` y vuelve a intentar
la conexión.

Los códigos leídos terminan con retorno de carro (CR). Cada código ordinario se envía
como el parámetro `c` de un POST a:

```text
http://<servidor>/newfac/RD01/rd01.php
```

Junto con `c`, la petición transmite datos de estado como versión de firmware, código
del producto, sucursal, dirección MAC, voltaje, RSSI, índice y ubicación. Una respuesta
HTTP 200 debe contener JSON: una lista de comandos que el firmware interpreta para
dibujar la pantalla, habilitar el escáner y actualizar el estado de la operación.
Si el servidor devuelve HTTP 200 sin cuerpo, el firmware lo registra como respuesta
vacía y no intenta ejecutarla como comandos.
El campo `c` conserva `&` y `=` porque algunas acciones usan una subconsulta histórica,
por ejemplo `0&tecla=15`; los demás campos de texto se codifican como formulario URL.

Los principales grupos de comandos de esa respuesta son:

| Códigos | Acción |
| --- | --- |
| 1–8 | Dibujar rectángulos, mover cursor, elegir fuente/tamaño/color, escribir texto, limpiar pantalla o dibujar la cuadrícula |
| 9–12 | Cambiar estado, imprimir una línea, actualizar código del producto o habilitar/deshabilitar el escáner |
| 13–18 | Dibujar botones, cambiar sucursal, dibujar arco/línea o mostrar el teclado numérico |
| 19–21 | Actualizar índice/ubicación o dibujar el botón de cancelación |

## Servidor y actualización OTA

Antes de publicar, comprobar en `src/metodos.h` la versión `FIRM_VERSION`, la dirección
del servidor y el tiempo `APAGADO`. El servidor de firmware publica un manifiesto JSON
`firm.json` con la propiedad numérica `version`. Si es superior a `FIRM_VERSION`, la
terminal busca la imagen:

```text
/newfac/RD01/firm<version>.bin
```

Después de compilar, la imagen generada por PlatformIO suele estar en
`.pio/build/esp32doit-devkit-v1/firmware.bin`; debe publicarse con el nombre que espera
la versión anunciada por el servidor.

La descarga OTA usa HTTP sin TLS y no requiere un certificado local. **Esto elimina el
cifrado y la autenticación del servidor:** en redes no confiables, un atacante con
capacidad de interceptar o modificar el tráfico podría sustituir el firmware. Usar este
método únicamente en una red controlada y proteger el servidor/red; para despliegues
expuestos a redes no confiables, HTTPS o una verificación criptográfica de firma del
firmware es necesaria. El servidor debe proporcionar la imagen con `Content-Length`
válido; el cliente `HTTPUpdate` usa además el encabezado `x-MD5` si el servidor lo envía.

## Interfaz y estados

El firmware mantiene un botón de apagado disponible y dibuja indicadores de Wi-Fi,
servidor, versión, intensidad de señal, batería y tiempo activo. Las coordenadas
Goodix se traducen a índices de tecla de una cuadrícula de cuatro por cuatro; el
despachador `teclado()` interpreta los índices según el estado actual. En modo de
captura numérica, el límite y el origen de la operación proceden del servidor. El fondo
se actualiza periódicamente y la pantalla de apagado permite apagar, mantener la
terminal activa o abrir la configuración.

## Codificación y diagnóstico

Los archivos de texto del proyecto se mantienen en UTF-8. `.editorconfig` fija
`charset = utf-8`, VS Code desactiva la detección automática y PlatformIO pasa
`-finput-charset=UTF-8` al compilador. Si aparecen caracteres corruptos en comentarios
o cadenas, comprobar que el editor haya abierto el archivo como UTF-8 antes de guardarlo.

Los mensajes de diagnóstico del arranque y de la comunicación con el escáner se envían
por USB a 115200 baudios. La carpeta `test/` contiene actualmente el README de plantilla
de PlatformIO; no hay pruebas automatizadas específicas del firmware.

## Observaciones de mantenimiento

- `verificaFirmware()` limita las consultas a cinco intentos, aunque conserva una
  pausa corta entre intentos. La verificación se ejecuta al establecer la conexión.
- La reconexión Wi-Fi es supervisada desde el ciclo principal y evita esperar decenas de
  segundos bloqueando el escáner y la interfaz.
- Los logs ya no imprimen contraseñas ni el cuerpo completo de los POST.
- Los códigos del escáner se limitan a 256 bytes; las entradas más largas se descartan.
- La actualización OTA viaja por HTTP sin cifrado ni autenticación TLS; restringirla a
  redes confiables o añadir verificación criptográfica de firmware antes de exponerla.
