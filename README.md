# SFA · Dispensador de peces — 2.º BTI

![Arduino UNO](https://img.shields.io/badge/Arduino-UNO-00878F?logo=arduino&logoColor=white)
![Python](https://img.shields.io/badge/Python-3.13-3776AB?logo=python&logoColor=white)
![Interfaz](https://img.shields.io/badge/Interfaz-HTML%20%2B%20Serial-7AD4D6)
![Verificación](https://github.com/hfreedo/SFA-Dispensador-de-Peces-2do-BTI/actions/workflows/verificar.yml/badge.svg)

Prototipo educativo de alimentación programada para peces con **Arduino UNO**, servomotor, LCD, Bluetooth y una tira de tres NeoPixels. Incluye una interfaz web local para configurar horarios, intervalos, movimiento del servo y animaciones LED.

![Interfaz web del dispensador](docs/img/interfaz-web.png)

## Funciones principales

- Alimentación manual o automática por horario e intervalo.
- Servo SG90 en **D8**, con ángulo y velocidad configurables.
- Tira WS2812B/NeoPixel de **tres LEDs en D6**.
- Animaciones: apagada, radar, ola marina, arcoíris y pulso coral.
- Panel local con selección de puerto serial y registro de eventos.
- LCD 16×2 I2C, buzzer y enlace Bluetooth HC-06.
- Configuración persistente en EEPROM.

## Arquitectura

![Arquitectura del sistema](docs/arquitectura.svg)

La página no controla el Arduino directamente. El navegador envía comandos al servidor Python; el servidor los transmite por USB serial a 9600 baudios. El HC-06 acepta el mismo protocolo desde un dispositivo Bluetooth.

## Esquema de conexiones

![Esquema de conexiones](docs/esquema-conexiones.svg)

| Módulo | Conexión al Arduino UNO |
|---|---|
| Servo SG90, señal | D8 |
| Tira WS2812B, DIN | D6 mediante resistencia serie |
| Buzzer pasivo, positivo | D12 |
| HC-06 TXD → Arduino RX | D2 |
| Arduino TX → HC-06 RXD | D3 mediante divisor de tensión |
| LCD SDA / SCL | A4 / A5 |

El servo y la tira se alimentan con una **fuente externa regulada de 5 V**, con GND común al Arduino. La resistencia de 1 kΩ actualmente instalada está en la línea de datos: no regula el brillo. Puede funcionar con un conductor corto; 330–470 Ω es el rango preferido si aparecen parpadeos o colores erráticos.

Consulta [Montaje y pruebas](docs/MONTAJE_Y_PRUEBAS.md) antes de energizar el prototipo.

## Entrega para Windows sin Python

Descarga el **ZIP portable para Windows x64** desde [Releases](https://github.com/hfreedo/SFA-Dispensador-de-Peces-2do-BTI/releases/latest), extrae su contenido y abre `SFA_Dispensador_Peces_2BTI.exe`. Incluye Python, `pyserial` y los recursos de la interfaz. No requiere instalar Python, pip ni Arduino IDE para abrir el panel.

El navegador se abre automáticamente. Mantén abierta la ventana del servidor durante el uso y pulsa **Ctrl+C** allí para terminar. Consulta [la guía del portable](docs/USO_PORTABLE.txt).

Compatibilidad prevista: **Windows 10/11 de 64 bits**. Para controlar el prototipo, el Arduino debe tener el firmware cargado y Windows debe reconocer su puerto COM; algunas placas requieren el controlador CH340 u otro controlador USB. Sin Internet, las animaciones decorativas usan imágenes locales de respaldo.

**Para entregar a otra persona utiliza el ZIP de Releases**, no el botón «Code → Download ZIP», que descarga el código fuente.

## Software necesario para desarrollar o cargar el firmware

- Arduino IDE o Arduino CLI con `arduino:avr`.
- Bibliotecas `Servo`, `LiquidCrystal I2C` y `Adafruit NeoPixel`.
- Python 3.10 o superior con `pyserial`.

Instalación con Arduino CLI:

```sh
arduino-cli core update-index
arduino-cli core install arduino:avr
arduino-cli lib install "Servo" "LiquidCrystal I2C" "Adafruit NeoPixel"
```

## Compilar el firmware

```sh
arduino-cli compile --fqbn arduino:avr:uno SFA_Dispensador_Peces_2BTI
```

## Ejecutar la interfaz desde el código fuente

En Windows puede utilizarse `server/run.bat`. De forma manual:

```sh
python -m pip install -r server/requirements.txt
python server/server.py
```

El servidor abre `http://127.0.0.1:8765`. Si ese puerto está ocupado, busca automáticamente el siguiente disponible sin cerrar servidores ajenos.

## Construir otra entrega portable

En Windows con Python de 64 bits:

```sh
python -m pip install -r tools/requirements-build.txt
python tools/build_portable.py
python tools/verify_portable.py dist/SFA_Dispensador_Peces_2BTI_v1.1.0_Windows_x64.zip
```

El resultado y su SHA-256 quedan en `dist/`. La comprobación extrae el ZIP en una carpeta nueva con espacios y ejecuta el EXE con un PATH sin Python, desde otro directorio. Verifica el panel, los recursos, el listado COM, el puerto alternativo y el rechazo de comandos sin conexión. Es una prueba local del paquete; la prueba en un segundo equipo y con hardware real sigue pendiente.

## Comandos relevantes

```text
SET HORA HH:MM:SS
SET ANGULO 10..180
SET VELOCIDAD 5..200
SET MODO AUTO|MANUAL
SET TONO 1..3
SET ANIMACION 0..4
ADD HORA HH:MM
DEL HORA N
CLEAR HORAS
SET INTERVALO H:M:S
DISPENSAR
ESTADO
AYUDA
```

## Evidencia disponible

Comprobación local del **8 de octubre de 2026**:

| Evidencia | Resultado |
|---|---|
| Compilación Arduino UNO | Correcta |
| Flash | 20.070 B de 32.256 B · 62 % |
| RAM estática | 926 B de 2.048 B · 45 % |
| Sintaxis Python y JavaScript | Correcta |
| Servidor HTTP y listado de puertos | Verificados localmente |
| Renderizado de la interfaz | Verificado y capturado |
| Prueba física prolongada | Pendiente |

La compilación, el servidor y la captura verifican el software; **no prueban por sí solos** la estabilidad eléctrica, el caudal de alimento, el torque del servo ni el funcionamiento continuo del montaje real.

## Organización

```text
SFA_Dispensador_Peces_2BTI/   firmware principal
server/                       puente HTTP/serial e interfaz
docs/                         esquemas, montaje y pruebas
tools/                        comprobaciones de sintaxis
.github/workflows/            verificación automática
```

## Uso y autoría

Este repositorio se publica como demostración educativa y portafolio técnico. No se ha concedido una licencia abierta de redistribución, modificación o explotación comercial del código propio. Las dependencias conservan sus licencias originales.
