# esp-32

Repositorio con proyectos para la placa **ESP32**. Cada carpeta contiene un proyecto de Arduino (`*.ino`) independiente con su propia documentación.

## Proyectos

| Carpeta | Descripción |
|---|---|
| [`wifi-capture/`](wifi-capture/) | Servidor web + flujo SSE que publica en tiempo real lo que lee la ESP32 sin sensores externos (temperatura del chip, ADC, pines digitales, recursos del sistema). |
| [`servo/`](servo/) | Control de un servo motor desde una página web mediante un slider (0° a 180°). |

## Requisitos

- [Arduino IDE](https://www.arduino.cc/en/software) (versión 2.x recomendada)
- Núcleo **ESP32** instalado en el Gestor de placas: `esp32` ([guía de instalación](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html))
- Una placa **ESP32** con chip **ESP32-D0WD** o compatible
- Cable **USB de datos** (no de carga únicamente) para flashear

## Cómo usar un proyecto

1. Abre el `*.ino` correspondiente en el Arduino IDE.
2. Configura los datos de tu red WiFi en la parte superior del archivo:
   ```cpp
   const char* SSID     = "TuRed";
   const char* PASSWORD = "TuClave";
   ```
3. En **Tools → Board** selecciona tu placa (ej. *ESP32 Dev Module*).
4. En **Tools → Port** selecciona el puerto de la ESP32 (ej. `/dev/ttyUSB0`).
5. Haz clic en **Upload**.

## Solución de problemas de flasheo

Si el upload se corta a mitad o aparece `Failed to communicate with the flash chip` / `csum err`:

- Usa un **cable USB de datos corto** y conecta directo al PC (no a un hub ni puerto frontal).
- Baja la **Upload Speed** en *Tools* de `921600` a `115200`.
- Verifica que el cable transfiera datos y no sea solo de carga.
- Como último recurso, en *Tools → Erase Flash: All Flash Contents* y vuelve a subir.
