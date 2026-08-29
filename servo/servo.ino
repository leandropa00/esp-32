/*
 * servo.ino
 * ESP32: controla un servo motor desde una página web.
 *
 * Dónde conectar (solo el servo, sin botones físicos):
 *   - Servo: señal (naranja/amarillo) -> GPIO 13
 *            VCC (rojo)              -> 5V (o 3.3V si el servo es pequeño)
 *            GND (marrón)            -> GND de la placa
 *
 * El navegador muestra dos botones (izquierda / derecha) que giran el
 * servo 5° por clic, y el ángulo actual se actualiza en tiempo real.
 *
 * Configura antes de subir:
 *   - SSID / PASSWORD de tu red WiFi
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ==================== CONFIGURACIÓN ====================
const char* SSID      = "El Pan";
const char* PASSWORD  = "12345678";

const int PIN_SERVO = 13;   // Señal del servo

const int ANG_MIN   = 0;    // Ángulo mínimo (0°)
const int ANG_MAX   = 180;  // Ángulo máximo (180°)
const int STEP      = 5;    // Pasos por clic (grados)
// =======================================================

WebServer server(80);
Servo servo;
int angulo = 90;   // Posición inicial (centro)

// ---------------- Página web embebida ----------------
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Servo - Control web</title>
<style>
  :root{
    --bg:#0d1117; --card:#161b22; --border:#30363d; --txt:#e6edf3;
    --acc:#58a6ff; --ok:#3fb950;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--txt);font-family:'Segoe UI',system-ui,sans-serif;
       display:flex;flex-direction:column;align-items:center;justify-content:center;
       min-height:100vh;padding:16px;gap:24px}
  h1{font-size:22px}
  .angle{font-size:64px;font-weight:700;color:var(--acc)}
  .angle small{font-size:20px;color:#8b949e}
  .bar{width:min(400px,80vw);height:14px;background:#21262d;border-radius:8px;
       overflow:hidden;border:1px solid var(--border)}
  .bar > div{height:100%;width:50%;background:linear-gradient(90deg,var(--acc),#2ea043);
       transition:width .2s}
  .btns{display:flex;gap:16px;flex-wrap:wrap;justify-content:center}
  .btn{width:130px;height:130px;border-radius:20px;border:2px solid var(--border);
       background:var(--card);color:var(--txt);font-size:40px;font-weight:700;
       cursor:pointer;transition:transform .08s, background .15s}
  .btn:active{transform:scale(.92)}
  .btn:hover{background:#1f2630}
  .btn:disabled{opacity:.4;cursor:not-allowed}
  .lbl{color:#8b949e;font-size:13px;text-align:center;margin-top:6px}
</style>
</head>
<body>
  <h1>Control de Servo</h1>
  <div class="angle"><span id="ang">90</span><small>&deg;</small></div>
  <div class="bar"><div id="fill"></div></div>

  <div class="btns">
    <div>
      <button class="btn" id="izq" onclick="mover('izq')">&lt;&lt; Izq</button>
      <div class="lbl">Gira a la izquierda (-5&deg;)</div>
    </div>
    <div>
      <button class="btn" id="der" onclick="mover('der')">Der &gt;&gt;</button>
      <div class="lbl">Gira a la derecha (+5&deg;)</div>
    </div>
  </div>

<script>
const angEl  = document.getElementById('ang');
const fillEl = document.getElementById('fill');
let busy = false;

function mostrar(a){
  angEl.textContent = a;
  fillEl.style.width = (a/180*100) + '%';
}

async function mover(dir){
  if (busy) return;
  busy = true;
  document.getElementById('izq').disabled = true;
  document.getElementById('der').disabled = true;
  try {
    const res = await fetch('/estado?dir=' + dir);
    const data = await res.json();
    mostrar(data.ang);
  } catch(e){}
  busy = false;
  document.getElementById('izq').disabled = false;
  document.getElementById('der').disabled = false;
}

(async function init(){
  try {
    const res = await fetch('/estado');
    const data = await res.json();
    mostrar(data.ang);
  } catch(e){}
})();
</script>
</body>
</html>
)rawliteral";

// ---------------- Manejo de peticiones ----------------
String jsonEstado() {
  return String("{\"ang\":") + String(angulo) + String("}");
}

void handleRoot() {
  // Si viene ?dir=izq|der, movemos el servo y devolvemos JSON
  if (server.hasArg("dir")) {
    String dir = server.arg("dir");
    if (dir == "izq") {
      angulo -= STEP;
      if (angulo < ANG_MIN) angulo = ANG_MIN;
      servo.write(angulo);
    } else if (dir == "der") {
      angulo += STEP;
      if (angulo > ANG_MAX) angulo = ANG_MAX;
      servo.write(angulo);
    }
    server.send(200, "application/json", jsonEstado());
    Serial.print("Ángulo -> ");
    Serial.println(angulo);
    return;
  }
  // Primera carga (fetch '/' sin parámetro) devuelve el estado
  server.send(200, "application/json", jsonEstado());
}

void handlePage() {
  server.send(200, "text/html", index_html);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);

  servo.attach(PIN_SERVO);
  servo.write(angulo);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Conectando a WiFi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Fallo al conectar a WiFi. Revisa SSID/PASSWORD.");
    ESP.restart();
  }

  server.on("/", HTTP_GET, handlePage);
  server.on("/estado", HTTP_GET, handleRoot);   // devuelve JSON del ángulo
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.print("Servidor listo. Abre http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
}
