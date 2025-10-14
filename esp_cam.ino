#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ===== AP (Hotspot) de la cámara =====
const char* AP_SSID = "ESP-CAM-HUB";
const char* AP_PASS = "12345678"; // min 8 chars

// ===== Pines AI-Thinker ESP32-CAM =====
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WebServer server(80);

// ===== Estado de métricas =====
float g_temp = NAN, g_hum = NAN;
unsigned long g_ts = 0;

// ===== HTML embebido (HUD: título tech, cámara full y datos con actualización clara) =====
static const char HUD_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CENTRO DE MONITOREO</title>
<style>
:root{
  --bg:#1e2227; --panel:#2b323a; --panel2:#23292f; --fg:#e8eaed;
  --muted:#9aa0a6; --ok:#22c55e; --bad:#ef4444; --accent:#ff3b30;
  --celeste1:#63d2ff; --celeste2:#2fb0ff; --celeste3:#16b1f0;
}
*{box-sizing:border-box} html,body{height:100%}
body{margin:0;background:var(--bg);color:var(--fg);font-family:system-ui,Segoe UI,Roboto,Arial}

/* ====== HEADER con título tech ====== */
.header{
  position:sticky; top:0; z-index:5;
  background:linear-gradient(180deg, rgba(22,27,32,.9), rgba(22,27,32,.65));
  border-bottom:1px solid #33404a;
  padding:18px 20px; margin:0 0 14px 0;
  box-shadow:0 10px 30px rgba(0,0,0,.25);
}
.title{
  margin:0; line-height:1;
  font-weight:900; letter-spacing:.12em; text-transform:uppercase;
  font-size: clamp(18px, 3.2vw, 28px);
  background:linear-gradient(90deg, var(--celeste1), var(--celeste2) 50%, var(--celeste3));
  -webkit-background-clip:text; background-clip:text; color:transparent;
  text-shadow:0 0 14px rgba(55,196,255,.25), 0 0 4px rgba(55,196,255,.35);
  filter:drop-shadow(0 4px 14px rgba(24,177,255,.15));
}
.subtitle{margin:8px 0 0 0; font-size:12px; color:#9fb6c6; letter-spacing:.18em; text-transform:uppercase}

/* ====== LAYOUT ====== */
.app{display:grid;grid-template-columns:320px 1fr;gap:18px;padding:0 18px 18px}
.left{display:flex;flex-direction:column;gap:16px}
.card{background:linear-gradient(#2b323a,#272e35);border:1px solid #3a424a;border-radius:16px;padding:16px;box-shadow:inset 0 1px 0 rgba(255,255,255,0.03)}
.card h3{margin:0 0 6px;font-size:14px;font-weight:700;letter-spacing:.04em;color:#cfd3d7}
.val{font-size:24px;font-weight:800}
.unit{margin-left:6px;font-size:12px;color:var(--muted)}
.kpi{display:flex;align-items:center;justify-content:space-between}
.pill{display:inline-flex;align-items:center;gap:8px;padding:6px 10px;border-radius:999px;background:#1d2329;border:1px solid #3a424a;color:#cfd3d7;font-size:12px;font-weight:600}
.pill.ok{background:#082414;border-color:#1f8f47;color:#a7efbf}
.pill.bad{background:#2a1414;border-color:#743333;color:#ffc5c5}
.input{width:100%;background:#1a1f24;border:1px solid #3a424a;color:var(--fg);border-radius:10px;padding:10px 12px}
.btn{cursor:pointer;user-select:none;background:#1a1f24;border:1px solid #3a424a;color:var(--fg);border-radius:10px;padding:10px 12px}
.btn:hover{filter:brightness(1.05)}

/* Panel de video: forzamos a que el stream LLENE el contenedor */
.right{background:var(--panel2);border:1px solid #3a424a;border-radius:18px;padding:14px;position:relative;min-height:68vh}
.viewer{position:relative;height:100%;border-radius:12px;background:#000;overflow:hidden}
.viewer img{
  position:absolute; inset:0;
  width:100%; height:100%;
  object-fit:cover;            /* << llena todo el espacio (recorta si hace falta) */
  image-rendering:auto;
}

/* Retícula roja y cruz */
.grid-overlay{
  position:absolute;inset:0;pointer-events:none;
  background:
    linear-gradient(to right, transparent calc(33.333% - 1px), var(--accent) calc(33.333% - 1px), var(--accent) calc(33.333% + 1px), transparent calc(33.333% + 1px)),
    linear-gradient(to right, transparent calc(66.666% - 1px), var(--accent) calc(66.666% - 1px), var(--accent) calc(66.666% + 1px), transparent calc(66.666% + 1px)),
    linear-gradient(to bottom, transparent calc(33.333% - 1px), var(--accent) calc(33.333% - 1px), var(--accent) calc(33.333% + 1px), transparent calc(33.333% + 1px)),
    linear-gradient(to bottom, transparent calc(66.666% - 1px), var(--accent) calc(66.666% - 1px), var(--accent) calc(66.666% + 1px), transparent calc(66.666% + 1px));
}
.cross{position:absolute;inset:0;display:grid;place-items:center;pointer-events:none}
.cross::before,.cross::after{content:"";display:block;background:var(--accent);border-radius:2px}
.cross::before{width:22%;height:4px}
.cross::after{width:4px;height:22%}

.row{display:flex;gap:10px;flex-wrap:wrap}
.small{font-size:12px;color:var(--muted)}
@media(max-width:980px){.app{grid-template-columns:1fr}}

/* Animación sutil cuando cambia un valor */
@keyframes bump{0%{transform:scale(1)}30%{transform:scale(1.06)}100%{transform:scale(1)}}
.val.bump{animation:bump .35s ease; filter:drop-shadow(0 0 8px rgba(99,210,255,.25))}
</style>
</head>
<body>

<header class="header">
  <h1 class="title">CENTRO DE MONITOREO</h1>
  <div class="subtitle">ESP32-CAM • Sensores en tiempo real</div>
</header>

<div class="app">
  <!-- Sidebar -->
  <div class="left">
    <div class="card">
      <h3>ESTADO</h3>
      <div id="status" class="pill bad">Sin conexión al servidor</div>
    </div>

    <div class="card">
      <h3>TEMPERATURA</h3>
      <div class="kpi"><div class="val" id="temp">—</div><div class="unit">°C</div></div>
    </div>

    <div class="card">
      <h3>HUMEDAD</h3>
      <div class="kpi"><div class="val" id="hum">—</div><div class="unit">%RH</div></div>
    </div>

    <div class="card">
      <h3>POLVO</h3>
      <div class="kpi"><div class="val" id="dust">—</div><div class="unit">µg/m³</div></div>
    </div>

    <div class="card">
      <h3>GRAVEDAD</h3>
      <div class="kpi"><div class="val" id="grav">9.81</div><div class="unit">m/s²</div></div>
    </div>

    <div class="card">
      <h3>ATMÓSFERA</h3>
      <div class="kpi"><div class="val" id="atm">1013</div><div class="unit">hPa</div></div>
    </div>

    <div class="card">
      <h3>CÁMARA</h3>
      <div class="row">
        <input id="camIp" class="input" placeholder="IP de la cámara (ej. 192.168.4.1)">
        <button class="btn" onclick="applyCam()">Conectar</button>
      </div>
      <div class="small" style="margin-top:6px">Sugerido: <code>192.168.4.1</code></div>
    </div>
  </div>

  <!-- Video -->
  <div class="right">
    <div class="viewer">
      <img id="img" alt="ESP32-CAM stream">
      <div class="grid-overlay"></div>
      <div class="cross"></div>
    </div>
  </div>
</div>

<script>
const sts  = document.getElementById('status');
const tEl  = document.getElementById('temp');
const hEl  = document.getElementById('hum');
const img  = document.getElementById('img');
const ipIn = document.getElementById('camIp');

let lastOkUnix = 0;
let prevT = null, prevH = null;

function setStatus(ok){
  if(ok){ sts.textContent="Conectado"; sts.classList.remove('bad'); sts.classList.add('ok'); }
  else  { sts.textContent="Sin conexión al servidor"; sts.classList.remove('ok'); sts.classList.add('bad'); }
}

function bump(el){ el.classList.remove('bump'); void el.offsetWidth; el.classList.add('bump'); }

function applyCam(){
  const host = (ipIn.value||"").trim();
  if(!host) return;
  img.src = `http://${host}/stream`;   // MJPEG
}

async function pull(){
  try{
    const r = await fetch('/metrics.json?t='+Date.now(), {cache:'no-store'});
    if(!r.ok) throw new Error(r.status);
    const j = await r.json();

    // ---- Temperatura
    if (j && j.temp != null){
      const t = Number(j.temp);
      if (prevT === null || Math.abs(t - prevT) > 0.01){ tEl.textContent = t.toFixed(1); bump(tEl); prevT = t; }
      else { tEl.textContent = t.toFixed(1); }
    } else { tEl.textContent = "—"; }

    // ---- Humedad
    if (j && j.humidity != null){
      const h = Number(j.humidity);
      if (prevH === null || Math.abs(h - prevH) > 0.01){ hEl.textContent = h.toFixed(1); bump(hEl); prevH = h; }
      else { hEl.textContent = h.toFixed(1); }
    } else { hEl.textContent = "—"; }

    // Marca “vivo” si recibimos algo
    lastOkUnix = Math.floor(Date.now()/1000);
  }catch(e){
    // silencio
  }finally{
    // Si pasaron > 8 s sin nuevas lecturas, marcar desconectado
    const now = Math.floor(Date.now()/1000);
    setStatus( (now - lastOkUnix) <= 8 );
    setTimeout(pull, 1000);
  }
}

(function init(){
  ipIn.value = "192.168.4.1";   // modo AP por defecto
  applyCam();
  pull();
})();
</script>
</body>
</html>
)HTML";

void handle_root(){ server.send_P(200,"text/html",HUD_HTML); }

void handle_jpg(){
  camera_fb_t* fb=esp_camera_fb_get();
  if(!fb){ server.send(503,"text/plain","Camera capture failed"); return; }
  WiFiClient client=server.client();
  server.setContentLength(fb->len);
  server.send(200,"image/jpeg");
  client.write(fb->buf,fb->len);
  esp_camera_fb_return(fb);
}

void handle_stream(){
  WiFiClient client=server.client();
  if(!client) return;
  client.print(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n\r\n"
  );
  while(client.connected()){
    camera_fb_t* fb=esp_camera_fb_get();
    if(!fb) break;
    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %u\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    esp_camera_fb_return(fb);
    delay(10);
  }
}

// --- REEMPLAZA estas funciones en el ESP32-CAM (HUB) ---

// 1) POST /metrics: más robusto y con logs
void handle_metrics_post(){
  String body;
  if (server.hasArg("plain")) {
    body = server.arg("plain");
  } else if (server.args() > 0) {
    // Si llegara como x-www-form-urlencoded, toma el primer arg
    body = server.arg(0);
  } else {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"no body\"}");
    return;
  }

  // LOG útil para saber que SÍ llegó algo
  Serial.printf("[/metrics] body: %s\n", body.c_str());

  // Parseo simple (sin librerías): busca "temp": y "humidity":
  float t = NAN, h = NAN;

  int it = body.indexOf("\"temp\"");
  if (it >= 0) {
    it = body.indexOf(':', it);
    if (it >= 0) {
      // toma solo hasta coma o llave para evitar “basura”
      int end = body.indexOf(',', it+1);
      if (end < 0) end = body.indexOf('}', it+1);
      String chunk = (end > 0) ? body.substring(it+1, end) : body.substring(it+1);
      t = chunk.toFloat();
    }
  }

  int ih = body.indexOf("\"humidity\"");
  if (ih >= 0) {
    ih = body.indexOf(':', ih);
    if (ih >= 0) {
      int end = body.indexOf(',', ih+1);
      if (end < 0) end = body.indexOf('}', ih+1);
      String chunk = (end > 0) ? body.substring(ih+1, end) : body.substring(ih+1);
      h = chunk.toFloat();
    }
  }

  if (!isnan(t)) g_temp = t;
  if (!isnan(h)) g_hum  = h;
  g_ts = millis()/1000;

  // Desactivar caché y responder OK
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.send(200, "application/json", "{\"ok\":true}");
}

// 2) GET /metrics.json: sin caché y con JSON claro
void handle_metrics_get(){
  char buf[160];
  // Si no hay datos aún, devuelve null
  const char* tstr = isnan(g_temp) ? "null" : nullptr;
  const char* hstr = isnan(g_hum)  ? "null" : nullptr;

  if (tstr || hstr) {
    snprintf(buf, sizeof(buf),
      "{\"temp\":%s,\"humidity\":%s,\"ts\":%lu}",
      tstr ? tstr : String(g_temp,1).c_str(),
      hstr ? hstr : String(g_hum,1).c_str(),
      g_ts
    );
  } else {
    snprintf(buf, sizeof(buf),
      "{\"temp\":%.1f,\"humidity\":%.1f,\"ts\":%lu}",
      g_temp, g_hum, g_ts
    );
  }

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.send(200, "application/json", buf);
}

void startServer(){
  server.on("/", HTTP_GET, handle_root);
  server.on("/jpg", HTTP_GET, handle_jpg);
  server.on("/stream", HTTP_GET, handle_stream);
  server.on("/metrics", HTTP_POST, handle_metrics_post);
  server.on("/metrics.json", HTTP_GET, handle_metrics_get);
  server.begin();
}

void setup(){
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  camera_config_t c;
  c.ledc_channel=LEDC_CHANNEL_0;
  c.ledc_timer=LEDC_TIMER_0;
  c.pin_d0=Y2_GPIO_NUM;   c.pin_d1=Y3_GPIO_NUM;   c.pin_d2=Y4_GPIO_NUM;   c.pin_d3=Y5_GPIO_NUM;
  c.pin_d4=Y6_GPIO_NUM;   c.pin_d5=Y7_GPIO_NUM;   c.pin_d6=Y8_GPIO_NUM;   c.pin_d7=Y9_GPIO_NUM;
  c.pin_xclk=XCLK_GPIO_NUM; c.pin_pclk=PCLK_GPIO_NUM; c.pin_vsync=VSYNC_GPIO_NUM; c.pin_href=HREF_GPIO_NUM;
  c.pin_sccb_sda=SIOD_GPIO_NUM; c.pin_sccb_scl=SIOC_GPIO_NUM;
  c.pin_pwdn=PWDN_GPIO_NUM; c.pin_reset=RESET_GPIO_NUM;
  c.xclk_freq_hz=20000000; c.pixel_format=PIXFORMAT_JPEG;
  c.frame_size=FRAMESIZE_QVGA;
  c.jpeg_quality=12;
  c.fb_count=1;

  if(esp_camera_init(&c)!=ESP_OK){ Serial.println("Camera init failed"); return; }

  // Fijar IP del AP y encenderlo
  WiFi.mode(WIFI_AP);
  IPAddress apIP(192,168,4,1), netMsk(255,255,255,0);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP %s %s  IP: %s\n", AP_SSID, ok?"OK":"FAIL", WiFi.softAPIP().toString().c_str());

  startServer();
  Serial.println("Servidor listo. Abre: http://192.168.4.1/");
}

void loop(){
  // *** IMPORTANTE: despachar peticiones HTTP ***
  server.handleClient();
  // ceder un poco de tiempo
  delay(1);
}
