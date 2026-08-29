/*
 * servo.ino
 * ESP32: controla un servo motor con dos botones (izquierda / derecha).
 *
 * Dónde conectar:
 *   - Servo: señal (naranja/amarillo) -> GPIO 13
 *            VCC (rojo)              -> 5V (o 3.3V si el servo es pequeño)
 *            GND (marrón)            -> GND de la placa
 *   - Botón izquierdo: un pin -> GPIO 14 (usa INPUT_PULLUP)
 *   - Botón derecho  : un pin -> GPIO 12 (usa INPUT_PULLUP)
 *
 * Un botón presionado hace girar el servo un paso, el otro lo hace girar
 * en la dirección contraria. El ángulo actual se muestra por el monitor serie.
 */

#include <ESP32Servo.h>

// ==================== CONFIGURACIÓN ====================
const int PIN_SERVO = 13;   // Señal del servo
const int PIN_BTN_L = 14;   // Botón izquierda
const int PIN_BTN_R = 12;   // Botón derecha

const int ANG_MIN   = 0;    // Ángulo mínimo (0°)
const int ANG_MAX   = 180;  // Ángulo máximo (180°)
const int STEP      = 5;    // Pasos por pulsación (grados)
// =======================================================

Servo servo;
int angulo = 90;                   // Posición inicial (centro)

// Debounce de botones
unsigned long lastDebounceL = 0;
unsigned long lastDebounceR = 0;
const unsigned long DEBOUNCE_MS = 50;

void setup() {
  Serial.begin(115200);

  servo.attach(PIN_SERVO);
  servo.write(angulo);

  pinMode(PIN_BTN_L, INPUT_PULLUP);
  pinMode(PIN_BTN_R, INPUT_PULLUP);

  Serial.println("Servo listo. Usa los botones para girar.");
  Serial.print("Posición inicial: ");
  Serial.println(angulo);
}

void loop() {
  // Botón izquierda: disminuye el ángulo
  if (digitalRead(PIN_BTN_L) == LOW &&
      millis() - lastDebounceL > DEBOUNCE_MS) {
    lastDebounceL = millis();
    angulo -= STEP;
    if (angulo < ANG_MIN) angulo = ANG_MIN;
    servo.write(angulo);
    Serial.print("Izquierda -> ");
    Serial.println(angulo);
  }

  // Botón derecha: aumenta el ángulo
  if (digitalRead(PIN_BTN_R) == LOW &&
      millis() - lastDebounceR > DEBOUNCE_MS) {
    lastDebounceR = millis();
    angulo += STEP;
    if (angulo > ANG_MAX) angulo = ANG_MAX;
    servo.write(angulo);
    Serial.print("Derecha -> ");
    Serial.println(angulo);
  }
}
