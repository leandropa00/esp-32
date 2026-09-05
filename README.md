# esp-32

Repositorio con dos proyectos independientes de Arduino (`*.ino`) para la placa **ESP32**: control de un servo por web y un servidor de telemetría/SSE.

## Tabla de Contenidos

├── 1. Objetivos y Alcance
├── 2. Arquitectura y Componentes
├── 3. Diagrama de Bloques / Flujo
├── 4. Desarrollo e Implementación (Código/Configuración)
├── 5. Pruebas y Evidencias de Funcionamiento
└── 6. Registro de Incidencias, Análisis y Conclusiones

---

## 1. Objetivos y Alcance

### Objetivo general
Desarrollar dos firmwares autónomos para ESP32 que demuestren capacidades de **conectividad WiFi** y **servicios web embebidos** en la placa, sin depender de plataformas de nube externas.

### Objetivos específicos
- **`servo/`**: controlar la posición de un servo motor (0° a 180°) a través de una página web con un slider, con respuesta en tiempo real.
- **`wifi-capture/`**: publicar en tiempo real (vía SSE) toda la telemetría que la placa puede leer por sí misma (temperatura del chip, ADC, pines digitales, recursos del sistema), sin sensores externos.

### Alcance
- Firmware compilado y subido desde **Arduino IDE 2.x** con el núcleo `esp32`.
- Comunicación a través de una **red WiFi local** (modo estación). No se usan servicios en la nube.
- Fuera de alcance: internet/LAN remota, seguridad/auntenticación, persistencia de datos, y cualquier sensor o actuador externo que no sea el propio servo en el proyecto `servo`.

---

## 2. Arquitectura y Componentes

### Plataforma hardware
- Placa **ESP32** (chip **ESP32-D0WD** o compatible, devkit típico).
- **Servo** SG90/MG90S/MG996R (proyecto `servo`), señal en **GPIO 13**, alimentación +5V, GND común.
- Cable USB de datos para flasheo y alimentación.

### Plataforma software
- Arduino IDE 2.x con core `esp32`.
- Librerías:
  - `servo`: **`ESP32Servo`** + `WiFi.h` / `WebServer.h` (core).
  - `wifi-capture`: solo `WiFi.h` / `WebServer.h` del core (**sin librerías externas**).

### Componentes de red / software
- **Servidor HTTP** embebido (puerto 80) que sirve una página web embebida en `PROGMEM`.
- **Flujo Server-Sent Events (SSE)** (puerto 8888, solo `wifi-capture`) para transmitir telemetría en tiempo real al navegador.
- **Construcción de JSON** manual (sin librerías) en `wifi-capture` para el payload de datos.

| Componente | `servo` | `wifi-capture` |
|---|---|---|
| Servidor HTTP (puerto 80) | Sí | Sí |
| Página web embebida (HTML/CSS/JS) | Sí | Sí |
| Endpoint de control/estado | `/estado` (GET, JSON) | `/` |
| SSE en tiempo real (puerto 8888) | No | Sí |
| Librerías externas | `ESP32Servo` | Ninguna |

---

## 3. Diagrama de Bloques / Flujo

### `servo/`
```
[Slider web] --fetch /estado?dir=<0..180>--> [Servidor HTTP ESP32]
                                                    |
                                    [constrain(ANG_MIN..ANG_MAX)]
                                                    |
                                              servo.write(angulo)
                                                    |
                                             [Servo motor GPIO 13]
                                                    |
                                     devuelve JSON {"ang":<ángulo>}
```
Flujo del navegador: el slider dispara `mover(val)` → `fetch('/estado?dir='+val)` → el servidor valida y limita el ángulo, mueve el servo y responde el ángulo aplicado (que se refleja en pantalla). En el arranque se consulta `/estado` para sincronizar la UI con la posición actual.

### `wifi-capture/`
```
[ESP32] --lee datos internos--> buildPayload() -> JSON
   (temperatureRead, ADC, digitalRead, ESP.getFreeHeap/CpuFreq, millis, RSSI)
                                          |
                                    <-- SSE :8888 (data: {...})
                                          |
   [Navegador] --fetch :8888/stream--> EventSource --acumula--> actualiza DOM (250ms)
                                          |
                                    [Página HTTP :80]
```
`setup()` conecta a WiFi, levanta el servidor HTTP (puerto 80) y el SSE (puerto 8888). `loop()` atiende peticiones HTTP y, cada ~500 ms, emite una trama `data:` por SSE si hay un cliente conectado (`sseDead == false`).

---

## 4. Desarrollo e Implementación (Código/Configuración)

### Configuración común (arriba de cada `.ino`)
```cpp
const char* SSID     = "TuRed";
const char* PASSWORD = "TuClave";
```

### `servo/servo.ino`
```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// Configuración
const int PIN_SERVO = 13;   // Señal del servo
const int ANG_MIN   = 0;
const int ANG_MAX   = 180;

WebServer server(80);
Servo servo;
int angulo = 90;   // Posición inicial (centro)

void handleRoot() {
  if (server.hasArg("dir")) {
    int valor = server.arg("dir").toInt();
    angulo = constrain(valor, ANG_MIN, ANG_MAX);
    servo.write(angulo);
    server.send(200, "application/json", jsonEstado());
    return;
  }
  server.send(200, "application/json", jsonEstado());
}
```
- `setup()`: `servo.attach(PIN_SERVO)`, conexión WiFi (reintenta y, si falla, `ESP.restart()`), rutas `/` (HTML) y `/estado` (JSON), `server.begin()`.
- `loop()`: `server.handleClient()`.
- Enrutado: `/` → página HTML; `/estado` → control/consulta del ángulo; resto → 404.
- Endpoint `GET /estado?dir=<valor>`: limita con `constrain`, escribe en el servo y responde `{"ang":<ángulo>}`.

### `wifi-capture/wifi-capture.ino`
```cpp
#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);
WiFiServer sseSrv(8888);      // flujo SSE
const unsigned long SEND_INTERVAL = 500;   // ~2 fps

void buildPayload(String& out) {
  out = String("{\"type\":\"data\"");
  out += String(",\"temp\":")     + String(temperatureRead(), 1);
  out += String(",\"freeHeap\":") + String(ESP.getFreeHeap());
  out += String(",\"heapPct\":")  + String(...);   // % uso de heap
  out += String(",\"cpuFreq\":")  + String(ESP.getCpuFreqMHz());
  out += String(",\"uptime\":")   + String(millis() / 1000);
  out += String(",\"rssi\":")     + String(WiFi.RSSI());
  // ... "adc":[] y "dig":[] con raw/voltaje y nivel ALTO/bajo
}
```
- Pines monitoreados: **ADC** (GPIO 32, 33, 34, 35, 36, 39) leyéndose con `analogRead` y convirtiendo a voltaje (raw/4095 × 3.3 V); **digitales** (GPIO 0..27 practicables) con `INPUT_PULLUP` y `digitalRead`.
- `SSE` manual: al aceptar un cliente se envía el encabezado `Content-Type: text/event-stream` y luego tramas `data: <json>` cada 500 ms; la página web embebida los consume con `EventSource('http://<ip>:8888/stream')`.

---

## 5. Pruebas y Evidencias de Funcionamiento

### `servo/`
1. Fly de flasheo OK desde Arduino IDE (board *ESP32 Dev Module*, 115200, port `/dev/ttyUSB0`).
2. Monitor serial muestra la IP y `Servidor listo. Abre http://<IP>`.
3. En el navegador, la página carga con el ángulo inicial (90°).
4. Al arrastrar el slider, el servo gira y el indicador numérico refleja el ángulo aplicado (0–180°).
5. Root del navegador devuelve JSON `{"ang":N}`.

### `wifi-capture/`
1. Flasheo OK y conexión WiFi con IP local asignada.
2. En `:80` carga la página con las tarjetas (Chip, Sistema, CPU, WiFi, Uptime) y se marcan "En línea" cuando conecta al SSE.
3. En `:8888` el navegador recibe tramas consecutivas `data:` cada ~500 ms.
4. Se observan valores reales: temperatura interna, heap/PSRAM, frecuencia de CPU, RSSI WiFi (dBm), voltajes ADC y estados de pines digitales.
5. Los valores se actualizan en vivo sin recargar la página (verificado por el indicador de estado y los cambios de los datos).

> Las capturas de pantalla no se incluyen en el repositorio; registrar aquí las evidencias al ejecutar cada prueba.

---

## 6. Registro de Incidencias, Análisis y Conclusiones

### Incidencias y soluciones (flasheo)
- **Síntoma**: upload cortado a mitad, `Failed to communicate with the flash chip` / `csum err`.
- **Causa** habitual: uso de cable solo de carga, hub/puerto frontal o velocidad alta.
- **Solución**: usar cable USB de datos corto y directo al PC, bajar la **Upload Speed** de `921600` a `115200`, y como último recurso *Tools → Erase Flash: All Flash Contents* antes de re-subir.

### Incidencias y soluciones (firmware)
- **`wifi-capture`**: los pines **GPIO 36 y 39** (ADC1) no soportan pull-up interno; se leen solo como entrada analógica. No colocarlos en `PIN_DIG`.
- **Servo reiniciándose** en `servo`: si el servo consume mucha corriente y la placa se reinicia, alimentar el VCC del servo con una **fuente de 5V externa** y unir el GND de la fuente con el GND de la ESP32.
- **Fallo de WiFi** (reintento): si no conecta en ~20 s, el firmware imprime el error y ejecuta `ESP.restart()`; verificar SSID/PASSWORD.

### Análisis
- Los dos proyectos demuestran patrones reutilizables: **servidor web embebido** con páginas en `PROGMEM`, **control REST** vía query-string y **streaming SSE** para telemetría en tiempo real.
- `wifi-capture` logra funcionar **sin ninguna librería externa**, construyendo el JSON de forma manual, lo que reduce dependencias y footprint.
- El diseño de un solo hilo (`loop()`) es adecuado para estas cargas: la página SSE se atiende por conexión única y las rutas HTTP son rápidas, por lo que no hay bloqueos apreciables.

### Conclusiones
- La ESP32 puede actuar como **servidor de borde autónomo** (HTTP + SSE) con muy pocas líneas de código y sin nube.
- El patrón SSE es eficaz y simple para actualizar el navegador en vivo, evitando polling desde la página.
- Se recomienda reforzar autenticación y manejo multi-cliente en el SSE si la solución escala y se expone en una red no confiable.
