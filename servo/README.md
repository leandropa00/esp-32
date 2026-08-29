# servo

Control de un **servo motor** desde una página web mediante un **slider** deslizante de **0° a 180°**.

## Requisitos

- Arduino IDE con núcleo **ESP32** instalado
- Librería **`ESP32Servo`** (menú *Tools → Manage Libraries → buscar "ESP32Servo" → Install*)
- Un servo compatible con 5V (ej. SG90, MG90S, MG996R, etc.)

## Configuración

En la parte superior de `servo.ino`:

```cpp
const char* SSID     = "El Pan";
const char* PASSWORD = "12345678";

const int PIN_SERVO = 13;   // Señal del servo
const int ANG_MIN   = 0;    // Ángulo mínimo
const int ANG_MAX   = 180;  // Ángulo máximo
```

## Conexión del servo

| Cable | A dónde |
|---|---|
| Señal (naranja/amarillo) | **GPIO 13** de la ESP32 |
| VCC (rojo) | **5V** de la ESP32 |
| GND (marrón/negro) | **GND** de la ESP32 |

Si el servo consume mucha corriente y la placa se reinicia, usa una **fuente externa de 5V**: conecta el VCC del servo a la fuente y une **GND de la fuente con GND de la ESP32**.

## Uso

1. Sube el firmware a la ESP32.
2. Abre el monitor serial (115200 baudios) para ver la IP asignada.
3. En el navegador abre `http://<IP-de-la-ESP32>`.
4. Arrastra el **slider** para girar el servo a la posición deseada.

## Notas

- El slider envía el ángulo al servidor (`/estado?dir=<valor>`), que mueve el servo y devuelve el ángulo aplicado.
- El resto de pines quedan libres.
