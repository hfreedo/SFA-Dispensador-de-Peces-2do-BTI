# Contribuir

Los cambios deben mantener sincronizados el firmware, el pinout, el protocolo serial y la interfaz.

Antes de proponer una modificación:

1. Compilar el sketch para `arduino:avr:uno`.
2. Ejecutar `python -m py_compile server/server.py`.
3. Ejecutar `node tools/check_ui.js`.
4. Actualizar los esquemas si cambia cualquier pin o componente.
5. Separar claramente resultados de software y pruebas físicas.

No incorporar credenciales, puertos COM personales, bases de datos, fotografías identificables ni archivos generados de entornos virtuales.
