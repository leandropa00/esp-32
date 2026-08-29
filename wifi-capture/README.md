# wifi-capture

Servidor web + flujo SSE (Server-Sent Events) para **ESP32** que publica en tiempo real todo lo que la placa puede leer "tal cual", **sin sensores externos**:

- Temperatura interna del chip
- Voltaje de pines analógicos (ADC): GPIO 32, 33, 34, 35, 36, 39
- Estado de pines digitales de entrada
- Recursos del sistema (heap, PSRAM, CPU, uptime, RSSI WiFi)

## Requisitos

- Arduino IDE con núcleo **ESP32** instalado
- **Sin librerías externas**: solo usa `WiFi.h` y `WebServer.h` del core ESP32

## Configuración

En la parte superior de `wifi-capture.ino`:

```cpp
const char* SSID     = "El Pan";
const char* PASSWORD = "12345678";
```

Ajusta `PIN_ADC[]` y `PIN_DIG[]` si quieres monitorear otros pines.

## Uso

1. Sube el firmware a la ESP32.
2. Abre el monitor serial (115200 baudios) para ver la IP asignada.
3. En el navegador abre `http://<IP-de-la-ESP32>`.
   - Página web con los datos en tiempo real en `:80`
   - Flujo SSE de datos en `:8888`

## Conexión

No requiere hardware adicional. Los pines monitoreados son entradas internas del chip.

## Notas

- Los pines GPIO 36 y 39 (ADC) no soportan pull-up interno.
- Los datos se actualizan cada ~500 ms.
