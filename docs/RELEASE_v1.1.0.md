# Interfaz portable v1.1.0 para Windows x64

La interfaz ahora se puede ejecutar sin instalar Python ni pip.

## Descargar y abrir

1. Descargar `SFA_Dispensador_Peces_2BTI_v1.1.0_Windows_x64.zip` de los archivos adjuntos a esta versión.
2. Extraer todo el ZIP a una carpeta local.
3. Abrir `SFA_Dispensador_Peces_2BTI.exe` y mantener abierta su ventana mientras se usa el panel.
4. Elegir el puerto COM y conectar el Arduino. Para terminar, pulsar Ctrl+C en la ventana del servidor.

No descargar «Source code» para esta entrega: esos archivos necesitan Python.

## Compatibilidad

- Windows 10/11 de 64 bits; paquete construido con Python 3.13.12 y PyInstaller 6.19.0.
- Incluye el servidor, pyserial y la interfaz con sus recursos locales.
- El Arduino debe tener el firmware cargado. El paquete no instala firmware ni controladores USB.
- Si el Arduino no aparece como puerto COM, revisar cable de datos y controlador USB de la placa.
- Sin Internet, las animaciones decorativas usan imágenes locales de respaldo.

## Verificación del paquete

Prueba local del 9 de octubre de 2026 en Windows 11: ZIP extraído a una carpeta nueva con espacios; EXE iniciado desde otro directorio, con PATH limitado a System32 y sin variables PYTHONHOME, PYTHONPATH o VIRTUAL_ENV.

Se verificaron la identificación del servidor v1.1.0, el HTML, los recursos PNG y JSON, el listado de puertos y la respuesta `No conectado` al intentar enviar un comando sin conexión serial. También se ocupó el puerto inicial y se verificó el uso del siguiente puerto.

La prueba en otro equipo, la conexión serial real y la validación física no forman parte de esta comprobación.

SHA-256 del ZIP:

```text
4aff1d3683e635a614a15e6cf0336276629849fd8e9904f7fa4d89ffefd90065
```
