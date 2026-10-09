# Montaje y pruebas de banco

## Antes de conectar

1. Confirmar que la fuente externa entrega **5 V regulados**.
2. Mantener desconectada la energía mientras se cambia el cableado.
3. Conectar el GND de la fuente externa al GND del Arduino.
4. Verificar que D6 llega a `DIN`, no a `DOUT`, de la tira.
5. Verificar polaridad del capacitor y de la tira.

## Alimentación recomendada

- Fuente regulada de 5 V y al menos 2 A para servo y tira, con margen para picos del servo.
- Servo y NeoPixels alimentados desde la fuente externa.
- Arduino alimentado por USB durante las pruebas.
- Sólo las masas deben quedar comunes entre ambas alimentaciones.
- Capacitor de 500–1000 µF, 6,3 V o superior, entre 5 V y GND cerca de la tira.

No colocar una resistencia en serie con la alimentación de 5 V. La resistencia serie pertenece únicamente a la señal entre D6 y `DIN`.

## Resistencia de datos

El montaje actual usa 1 kΩ en la línea de datos. Esa resistencia no reduce el brillo porque los NeoPixels reciben datos digitales y alimentación independiente. Para mayor margen de integridad de señal se recomienda 330–470 Ω cerca del primer LED. Si con 1 kΩ no aparecen parpadeos, colores incorrectos o reinicios, puede evaluarse temporalmente en esta maqueta de cable corto.

## Secuencia de validación

1. Cargar el firmware sin servo ni tira y solicitar `ESTADO` por USB.
2. Conectar sólo la tira; recorrer las animaciones 0–4 y confirmar que se controlan exactamente tres LEDs.
3. Confirmar el brillo configurado en `120/255` y observar que no existan parpadeos.
4. Desenergizar, conectar el servo en D8 y probar `DISPENSAR` sin alimento.
5. Ajustar ángulo y velocidad evitando topes mecánicos y zumbido permanente.
6. Probar un intervalo corto, mínimo cinco segundos.
7. Añadir alimento gradualmente y pesar varias porciones para estimar repetibilidad.
8. Ejecutar una prueba prolongada y registrar reinicios, atascos, temperatura y variación de porción.

## Criterios pendientes

- Corriente real del servo en movimiento y en bloqueo.
- Repetibilidad de la porción dispensada.
- Estabilidad del reloj interno después de varias horas.
- Robustez del enlace Bluetooth junto con las animaciones NeoPixel.
- Temperatura de fuente, servo y conexiones.
