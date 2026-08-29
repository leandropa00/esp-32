/*
 * wifi-capture.ino
 * ESP32: servidor web + flujo SSE (Server-Sent Events) que publica en tiempo
 * real todo lo que puede leer la tarjeta "así como está" (sin sensores externos):
 *   - Temperatura interna del chip
 *   - Voltaje de pines analógicos (ADC)
 *   - Estado de pines digitales de entrada
 *   - Recursos del sistema (heap, PSRAM, CPU, uptime)
 *
 * NO requiere librerías externas: solo WiFi.h y WebServer.h del core ESP32.
 *
 * Configura antes de subir:
 *   - SSID / PASSWORD de tu red WiFi
 */

#include <WiFi.h>
#include <WebServer.h>

// ==================== CONFIGURACIÓN ====================
const char* SSID      = "El Pan";
const char* PASSWORD  = "12345678";

// Pines analógicos utilizables (ADC1). GPIO 36,39 no soportan pull-up.
const int PIN_ADC[]   = {32, 33, 34, 35, 36, 39};
const int N_ADC       = sizeof(PIN_ADC) / sizeof(PIN_ADC[0]);

// Pines útiles para entradas digitales (GPIO practicables en la mayoría de devkits)
const int PIN_DIG[]   = {0, 2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19,
                         21, 22, 23, 25, 26, 27};
const int N_DIG       = sizeof(PIN_DIG) / sizeof(PIN_DIG[0]);

// =======================================================

WebServer server(80);      // Página web + flujo de datos
WiFiServer sseSrv(8888);   // Flujo SSE de datos en tiempo real
WiFiClient sseClient;
bool sseDead = true;       // true cuando no hay cliente SSE conectado

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 500;   // cada 500 ms (2 fps)

// ---------------- Página web embebida ----------------
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 - Sensores en tiempo real</title>
<style>
  :root{
    --bg:#0d1117; --card:#161b22; --border:#30363d; --txt:#e6edf3;
    --acc:#58a6ff; --ok:#3fb950; --warn:#d29922; --err:#f85149;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--txt);font-family:'Segoe UI',system-ui,sans-serif;padding:16px}
  h1{font-size:20px;margin-bottom:4px;display:flex;align-items:center;gap:10px;flex-wrap:wrap}
  .status{font-size:13px;display:inline-flex;align-items:center;gap:6px;padding:2px 10px;border-radius:20px}
  .status.on{background:rgba(63,185,80,.15);color:var(--ok)}
  .status.off{background:rgba(248,81,73,.15);color:var(--err)}
  .dot{width:8px;height:8px;border-radius:50%;display:inline-block}
  .on .dot{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .off .dot{background:var(--err)}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px;margin-top:16px}
  .card{background:var(--card);border:1px solid var(--border);border-radius:10px;padding:14px}
  .card h3{font-size:11px;text-transform:uppercase;letter-spacing:.06em;color:#8b949e;margin-bottom:10px}
  .val{font-size:26px;font-weight:700}
  .sub{font-size:12px;color:#8b949e;margin-top:2px}
  .bar{height:8px;background:#21262d;border-radius:5px;overflow:hidden;margin-top:8px}
  .bar > div{height:100%;background:linear-gradient(90deg,var(--acc),#2ea043);transition:width .3s}
  .chip{display:flex;justify-content:space-between;align-items:center;padding:8px 10px;border-bottom:1px solid var(--border);font-size:13px}
  .chip:last-child{border-bottom:none}
  .chip .pin{font-family:monospace;font-weight:600}
  .chip .lvl{font-weight:600}
  .lvl.hi{color:var(--ok)} .lvl.lo{color:#8b949e}
  footer{margin-top:20px;font-size:12px;color:#8b949e;text-align:center}
</style>
</head>
<body>
  <h1>ESP32 <span id="status" class="status off"><span class="dot"></span>Desconectado</span></h1>
  <p style="font-size:13px;color:#8b949e" id="ipinfo">Conectando al flujo de datos...</p>

  <div class="grid">
    <div class="card">
      <h3>Chip</h3>
      <div class="val" id="temp">--&deg;C</div>
      <div class="sub">Temperatura interna</div>
      <div class="bar"><div id="tempbar" style="width:0%"></div></div>
    </div>

    <div class="card">
      <h3>Sistema</h3>
      <div class="val" id="heap">--</div>
      <div class="sub">Memoria libre (heap)</div>
      <div class="bar"><div id="heapbar" style="width:0%"></div></div>
    </div>

    <div class="card">
      <h3>CPU</h3>
      <div class="val" id="cpu">--</div>
      <div class="sub"><span id="freq">--</span> MHz</div>
      <div class="bar"><div id="cpubar" style="width:0%"></div></div>
    </div>

    <div class="card">
      <h3>Wifi</h3>
      <div class="val" id="rssi">--</div>
      <div class="sub">Fuerza de la señal</div>
      <div class="bar"><div id="rssibar" style="width:0%"></div></div>
    </div>

    <div class="card">
      <h3>Uptime</h3>
      <div class="val" id="uptime">--</div>
      <div class="sub">Tiempo encendido</div>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <h3>Analógicos (ADC)</h3>
      <div id="adc"></div>
    </div>

    <div class="card">
      <h3>Pines digitales (entrada)</h3>
      <div id="dig"></div>
    </div>
  </div>

  <footer>Datos publicados por la ESP32 (SSE) &mdash; actualización cada ~500ms</footer>

<script>
const statusEl = document.getElementById('status');

// Resumen informativo del estado (sin reintento agresivo del navegador)
const accumulator = [];
const es = new EventSource('http://' + location.hostname + ':8888/stream');

es.onopen = () => {
  statusEl.className = 'status on';
  statusEl.innerHTML = '<span class="dot"></span>En l&iacute;nea';
  document.getElementById('ipinfo').textContent = 'Flujo de datos conectado a :8888';
};
es.onerror = () => {
  statusEl.className = 'status off';
  statusEl.innerHTML = '<span class="dot"></span>Desconectado';
  document.getElementById('ipinfo').textContent = 'Reconectando...';
};
// Guardamos las tramas y las consumimos por lotes (evita sobrecarga de DOM)
es.onmessage = (e) => accumulator.push(e.data);

setInterval(() => {
  if (!accumulator.length) return;
  const d = JSON.parse(accumulator[accumulator.length - 1]);
  accumulator.length = 0;
  if (d.type !== 'data') return;

  // Chip
  const temp = d.temp;
  document.getElementById('temp').textContent = temp.toFixed(1) + '\u00B0C';
  document.getElementById('tempbar').style.width = Math.min(temp/60*100,100) + '%';

  // Sistema
  document.getElementById('heap').textContent = fmtBytes(d.freeHeap);
  document.getElementById('heapbar').style.width = d.heapPct + '%';
  document.getElementById('cpu').textContent = d.cpuFreq;
  document.getElementById('freq').textContent = d.cpuFreq;
  document.getElementById('cpubar').style.width = Math.min(d.cpuFreq/240*100,100) + '%';
  document.getElementById('uptime').textContent = fmtUptime(d.uptime);

  // WiFi (señal en dBm)
  const rssi = d.rssi;
  document.getElementById('rssi').textContent = rssi + ' dBm';
  document.getElementById('rssibar').style.width = Math.max(0, Math.min((rssi + 100) / 50 * 100, 100)) + '%';

  // ADC
  const adc = document.getElementById('adc');
  adc.innerHTML = '';
  d.adc.forEach(a => {
    const row = document.createElement('div');
    row.className = 'chip';
    const pct = Math.min(a.v/3.3*100,100);
    row.innerHTML = '<span class="pin">GPIO'+a.pin+'</span>' +
      '<span class="lvl">'+a.v.toFixed(2)+' V <span style="font-size:11px;color:#8b949e">('+pct.toFixed(0)+'%)</span></span>';
    adc.appendChild(row);
  });

  // Digital
  const dig = document.getElementById('dig');
  dig.innerHTML = '';
  d.dig.forEach(g => {
    const row = document.createElement('div');
    row.className = 'chip';
    row.innerHTML = '<span class="pin">GPIO'+g.pin+'</span>' +
      '<span class="lvl '+(g.v?'hi':'lo')+'">'+(g.v?'ALTO':'bajo')+'</span>';
    dig.appendChild(row);
  });
}, 250);

function fmtBytes(b){
  if (b >= 1024*1024) return (b/1024/1024).toFixed(1)+' MB';
  if (b >= 1024) return (b/1024).toFixed(0)+' KB';
  return b+' B';
}
function fmtUptime(s){
  const h = Math.floor(s/3600), m = Math.floor((s%3600)/60), sec = s%60;
  const pad = n => String(n).padStart(2,'0');
  return (h>0? h+'h ':'')+pad(m)+':'+pad(sec);
}
</script>
</body>
</html>
)rawliteral";

// ---------------- Construcción del JSON (sin librerías externas) ----------------
void buildPayload(String& out) {
  out = String("{\"type\":\"data\"");
  out += String(",\"temp\":")     + String(temperatureRead(), 1);
  out += String(",\"freeHeap\":") + String(ESP.getFreeHeap());
  out += String(",\"heapPct\":")  + String(((ESP.getHeapSize() - ESP.getFreeHeap()) * 100.0) / ESP.getHeapSize(), 1);
  out += String(",\"cpuFreq\":")  + String(ESP.getCpuFreqMHz());
  out += String(",\"uptime\":")   + String(millis() / 1000);
  out += String(",\"rssi\":")     + String(WiFi.RSSI());

  out += String(",\"adc\":[");
  for (int i = 0; i < N_ADC; i++) {
    int pin = PIN_ADC[i];
    uint16_t raw = analogRead(pin);            // 0..4095
    float v = (raw / 4095.0f) * 3.3f;
    if (i) out += ",";
    out += String("{\"pin\":") + String(pin) +
           String(",\"raw\":") + String(raw) +
           String(",\"v\":")   + String(v, 2) + String("}");
  }

  out += String("],\"dig\":[");
  for (int i = 0; i < N_DIG; i++) {
    int pin = PIN_DIG[i];
    pinMode(pin, INPUT_PULLUP);
    int v = digitalRead(pin);                  // 1 = ALTO, 0 = bajo
    if (i) out += ",";
    out += String("{\"pin\":") + String(pin) +
           String(",\"v\":")   + String(v)   + String("}");
  }
  out += String("]}");
}

// ---------------- Servidor SSE (flujo de datos en tiempo real) ----------------
bool sseReadRequest(WiFiClient& c) {
  // Lee la petición HTTP hasta encontrar el final de cabeceras
  // (una línea vacía = CRLF CRLF = dos \n consecutivos)
  unsigned long t0 = millis();
  int newlines = 0;
  while (c.connected() && millis() - t0 < 3000) {
    while (c.available()) {
      char ch = c.read();
      if (ch == '\n') {
        newlines++;
        if (newlines >= 2) return true;   // fin de cabeceras
      } else if (ch != '\r') {
        newlines = 0;                     // había contenido en la línea
      }
    }
    delay(1);
  }
  return true;   // timeout: responder de todos modos
}

// ---------------- HTTP (página) ----------------
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ---------------- Setup / Loop ----------------
void setup() {
  Serial.begin(115200);

  analogReadResolution(12);

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
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Abre http://" + WiFi.localIP().toString() + " en el navegador");

  server.on("/",       handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  sseSrv.begin();

  Serial.println("Servidores listos (HTTP:80, SSE:8888)");
  Serial.print("IP del server: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();

  // --- Gestión del cliente SSE ---
  if (sseDead) {
    sseClient = sseSrv.available();
    if (sseClient) {
      if (sseReadRequest(sseClient)) {
        sseClient.print("HTTP/1.1 200 OK\r\n");
        sseClient.print("Content-Type: text/event-stream\r\n");
        sseClient.print("Cache-Control: no-cache\r\n");
        sseClient.print("Connection: keep-alive\r\n");
        sseClient.print("Access-Control-Allow-Origin: *\r\n");
        sseClient.print("\r\n");
        sseDead = false;
      }
    }
  } else if (!sseClient.connected()) {
    sseClient.stop();
    sseDead = true;
  }

  // --- Emisión periódica de datos ---
  if (!sseDead && millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    String out;
    buildPayload(out);
    sseClient.print("data: ");
    sseClient.print(out);
    sseClient.print("\n\n");
  }
}