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

Al iniciar, el firmware habilita la alimentación y prepara los puertos serie, I2C, táctil
y display. Recupera de NVS (`Preferences`) el servidor y las credenciales Wi-Fi; si no
existe una dirección guardada, usa `192.168.2.3`. Inicia la conexión Wi-Fi sin esperar de
forma bloqueante, configura el escáner y mantiene el ciclo principal activo. Cuando se
conecta, envía la acción inicial al servidor y consulta si hay firmware nuevo. La consulta
OTA puede reintentarse hasta cinco veces si falla la lectura del manifiesto.

Si no hay credenciales Wi-Fi guardadas, se pueden proporcionar escaneando un QR cuyo
contenido tenga este formato:

```text
WIFI:T:nopass;S:<SSID>;P:<contraseña>;;
```

El lector debe enviar ese texto y terminarlo con retorno de carro (CR). El parser
reconoce el primer campo `WIFI:T:nopass`, toma el SSID del campo `S:` y la contraseña
del campo `P:`. Aunque el prefijo diga `nopass`, el código actual espera que exista el
campo `P:`; no es el formato genérico para redes abiertas. Al reconocer el QR, guarda
ambos valores en NVS bajo el espacio `credenciales` e inicia/reintenta la conexión.
El parser actual no procesa escapes ni campos reordenados: mantener el orden mostrado.

### Cambiar el servidor desde la pantalla

1. Tocar el botón de apagado y luego **CONF**.
2. Introducir el código numérico y tocar **Listo** (tecla superior derecha).
3. Usar `753064` para guardar `192.168.101.64` (pruebas) o `753003` para guardar
   `192.168.2.3` (servidor principal).
4. Salir del teclado con su botón de apagado; en la pantalla de apagado, tocar
   **MANTENER ENCENDIDO** para volver al inicio. Al regresar se vuelve a leer el servidor
   guardado y se usa en las siguientes peticiones.

La dirección se persiste en NVS en el espacio `credenciales`; no se escribe directamente
en el firmware. Los códigos numéricos de acceso están definidos en `teclado()` de
`src/metodos.h`.

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

Cada elemento de la respuesta es un arreglo `[comando, argumento...]`. Los números y
argumentos son un protocolo compartido con `rd01.php`; al cambiarlo, actualizar ambos
lados. Los argumentos de dibujo se expresan en píxeles y los colores en RGB565.

| Código | Argumentos, en orden | Acción |
| --- | --- | --- |
| 1 | `x, y, ancho, alto, radio, color` | Rectángulo redondeado relleno |
| 2 | `x, y` | Posición del cursor |
| 3 | `índiceFuente` (1–7) | Elegir una de las fuentes incluidas |
| 4 | `tamaño` | Escala del texto |
| 5 | `color` | Color del texto |
| 6 | `texto` | Imprimir texto sin salto de línea |
| 7 | `color` | Limpiar la pantalla |
| 8 | `columnas, filas, color, relleno` | Dibujar la cuadrícula del teclado |
| 9 | `estado` | Actualizar el estado de la terminal |
| 10 | `texto` | Imprimir texto y terminar la línea |
| 11 | `código` | Actualizar el código del producto |
| 12 | `habilitado` | Habilitar el escáner solo si el valor es `1`; otros valores lo deshabilitan |
| 13–14 | `x, y` | Dibujar los botones Listo o Borrar |
| 15 | `sucursal` | Actualizar la sucursal (máximo 19 bytes más NUL) |
| 16 | `centroX, centroY, radioX, radioY, ánguloInicio, ánguloFin, color` | Dibujar arco relleno |
| 17 | `x1, y1, x2, y2, color` | Dibujar una línea |
| 18 | `código, máximo, origen, auxiliar` | Mostrar teclado numérico; actualmente solo se usan `máximo` y `origen` |
| 19 | `índice` | Actualizar el identificador de origen |
| 20 | `ubicación` | Actualizar ubicación (máximo 99 bytes más NUL) |
| 21 | `x, y` | Dibujar el botón de cancelar/suspender |

Al terminar cada lista, el firmware vuelve a dibujar el botón de apagado. Los textos de
sucursal y ubicación que exceden sus buffers se rechazan y se registran por Serial.

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
por USB a 115200 baudios. La petición POST también se imprime completa para diagnóstico;
incluye el código recibido y datos como dirección MAC y ubicación, por lo que debe
considerarse al compartir capturas del monitor serie. La carpeta `test/` contiene
actualmente el README de plantilla de PlatformIO; no hay pruebas automatizadas específicas
del firmware.

## Observaciones de mantenimiento

- `verificaFirmware()` limita las consultas a cinco intentos, aunque conserva una
  pausa corta entre intentos. La verificación se ejecuta al establecer la conexión.
- Los intentos de reconexión Wi-Fi se supervisan desde el ciclo principal cada 15
  segundos, sin una espera dedicada que pause la interfaz. Las peticiones HTTP siguen
  siendo síncronas y pueden bloquear durante sus tiempos de espera configurados.
- Los logs no imprimen la contraseña Wi-Fi; el cuerpo completo del POST sí se muestra
  para diagnóstico.
- Los códigos del escáner se limitan a 256 bytes; las entradas más largas se descartan.
- La actualización OTA viaja por HTTP sin cifrado ni autenticación TLS; restringirla a
  redes confiables o añadir verificación criptográfica de firmware antes de exponerla.
