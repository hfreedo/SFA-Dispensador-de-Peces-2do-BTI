# SFA - Dispensador de Peces 2do. BTI

Versión alternativa del dispensador. El proyecto original `DispensadorPeces` no fue modificado.

## Contenido

- `SFA_Dispensador_Peces_2BTI/SFA_Dispensador_Peces_2BTI.ino`: firmware para Arduino UNO.
- `server/server.py`: puente local HTTP/serial a 9600 baudios.
- `server/static/index.html`: interfaz web con selección y vista previa de animaciones.
- `server/run.bat`: inicio rápido del servidor en Windows.

## Pinout confirmado por el firmware

| Función | Conexión Arduino UNO |
|---|---|
| Señal del servomotor SG90 | D8 |
| Datos DIN de tira WS2812B/NeoPixel | D6 |
| Buzzer pasivo | D12 |
| HC-06 TXD hacia Arduino | D2 (RX de SoftwareSerial) |
| Arduino hacia HC-06 RXD | D3 (TX, con adaptación de nivel) |
| LCD I2C SDA | A4 |
| LCD I2C SCL | A5 |

La tira está configurada para exactamente 3 LEDs. El nuevo comando serial es `SET ANIMACION X`, donde:

- `0`: apagada
- `1`: radar
- `2`: ola marina
- `3`: arcoíris
- `4`: pulso coral

La opción queda guardada en EEPROM y también aparece en la respuesta de `ESTADO`.

## Alimentación y cableado seguro

- El pin D6 va al terminal `DIN` de la tira, no a `DOUT`.
- Colocar preferentemente una resistencia de 330 a 470 ohmios en serie entre D6 y DIN. Una resistencia de 1 kohm en esta línea no limita el brillo y puede funcionar con cable corto, pero conviene cambiarla si aparecen parpadeos, colores erráticos o pérdida de comandos.
- Colocar un capacitor de aproximadamente 1000 uF entre 5 V y GND cerca de la tira, respetando polaridad.
- Alimentar el servo y la tira con una fuente regulada externa de 5 V. No cargar ambos desde el pin 5 V del UNO.
- Unir el GND de la fuente externa, Arduino, servo, tira y HC-06.
- La señal D3 hacia RXD del HC-06 debe bajar de 5 V a cerca de 3.3 V con divisor resistivo o adaptador de nivel.
- Para 3 NeoPixels, dimensionar la fuente con margen para el servo. Una fuente regulada de 5 V y al menos 2 A es una base prudente para esta maqueta.

## Librerías Arduino

- Servo
- LiquidCrystal I2C
- Adafruit NeoPixel
- EEPROM y SoftwareSerial (incluidas con el núcleo AVR)

## Uso

1. Abrir `SFA_Dispensador_Peces_2BTI.ino` en Arduino IDE y cargarlo en un Arduino UNO.
2. Cerrar el Monitor Serie para liberar el puerto COM.
3. Ejecutar `server/run.bat`.
4. Elegir el puerto del Arduino y pulsar **Conectar**.
5. En **Animación de luces**, elegir una opción y pulsar **Aplicar**.

## Protocolo de prueba física

1. Probar primero con servo y tira desconectados: cargar el firmware y comprobar LCD, puerto serial y comando `ESTADO`.
2. Conectar solamente la tira a la fuente externa con GND común. Probar las opciones 0 a 4 y confirmar que sólo encienden 3 LEDs.
3. Desenergizar, conectar el servo en D8 y volver a encender. Probar **Alimentar ahora** con el mecanismo sin carga.
4. Ajustar ángulo y velocidad desde el panel; verificar que la animación LED continúa sin congelarse durante el movimiento.
5. Probar un intervalo corto, mínimo 5 segundos, antes de usar horarios reales.
6. Repetir con alimento y medir que la porción no atasque el mecanismo.

La compilación confirma compatibilidad de software, pero no sustituye estas pruebas eléctricas y mecánicas.
