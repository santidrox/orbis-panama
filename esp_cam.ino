#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ====== Punto de Acceso (AP) propio de la cámara ======
const char* AP_SSID = "ESP-CAM-HUB";
const char* AP_PASS = "12345678";   // min 8 chars

// IP del AP (fija)
IPAddress apIP(192,168,4,1);
IPAddress netMsk(255,255,255,0);

// ====== Pines AI-Thinker ESP32-CAM ======
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

// ====== Calidad / tamaño ======
// Para buena fluidez: QVGA (320x240) y calidad 12–15 (menor número = más calidad)
#define CAM_FRAME_SIZE   FRAMESIZE_QVGA
#define CAM_JPEG_QUALITY 12
#define CAM_FB_COUNT     2    // doble buffer (requiere PSRAM)

WebServer server(80);

// ====== HTML embebido ======
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="es">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CENTRO DE MONITOREO</title>
<style>
:root{--bg:#1e2227;--panel:#2b323a;--panel2:#23292f;--fg:#e8eaed;--muted:#9aa0a6;--ok:#22c55e;--bad:#ef4444;--accent:#ff3b30;--cel1:#63d2ff;--cel2:#2fb0ff;--cel3:#16b1f0}
*{box-sizing:border-box}html,body{height:100%}body{margin:0;background:var(--bg);color:var(--fg);font-family:system-ui,Segoe UI,Roboto,Arial}
.header{position:sticky;top:0;z-index:5;background:linear-gradient(180deg,rgba(22,27,32,.9),rgba(22,27,32,.65));border-bottom:1px solid #33404a;padding:18px 20px;margin:0 0 14px;box-shadow:0 10px 30px rgba(0,0,0,.25)}
.title{margin:0;line-height:1;font-weight:900;letter-spacing:.12em;text-transform:uppercase;font-size:clamp(18px,3.2vw,28px);
background:linear-gradient(90deg,var(--cel1),var(--cel2) 50%,var(--cel3));-webkit-background-clip:text;background-clip:text;color:transparent;text-shadow:0 0 14px rgba(55,196,255,.25),0 0 4px rgba(55,196,255,.35)}
.subtitle{margin:8px 0 0;font-size:12px;color:#9fb6c6;letter-spacing:.18em;text-transform:uppercase}

.app{display:grid;grid-template-columns:1fr;gap:18px;padding:0 18px 18px}
.right{background:var(--panel2);border:1px solid #3a424a;border-radius:18px;padding:14px;min-height:70vh;display:flex;align-items:center;justify-content:center}
.viewer{position:relative;width:min(92vw,960px);height:70vh;max-height:720px;border-radius:12px;background:#000;overflow:hidden}
.viewer img{position:absolute;inset:0;width:100%;height:100%;object-fit:cover}
.grid-overlay{position:absolute;inset:0;pointer-events:none;background:
linear-gradient(to right,transparent calc(33.333% - 1px),var(--accent) calc(33.333% - 1px),var(--accent) calc(33.333% + 1px),transparent calc(33.333% + 1px)),
linear-gradient(to right,transparent calc(66.666% - 1px),var(--accent) calc(66.666% - 1px),var(--accent) calc(66.666% + 1px),transparent calc(66.666% + 1px)),
linear-gradient(to bottom,transparent calc(33.333% - 1px),var(--accent) calc(33.333% - 1px),var(--accent) calc(33.333% + 1px),transparent calc(33.333% + 1px)),
linear-gradient(to bottom,transparent calc(66.666% - 1px),var(--accent) calc(66.666% - 1px),var(--accent) calc(66.666% + 1px),transparent calc(66.666% + 1px))}
.cross{position:absolute;inset:0;display:grid;place-items:center;pointer-events:none}
.cross::before,.cross::after{content:"";display:block;background:var(--accent);border-radius:2px}
.cross::before{width:22%;height:4px}.cross::after{width:4px;height:22%}
.panel{display:flex;flex-wrap:wrap;gap:12px;align-items:center;justify-content:center;margin:8px 0}
.pill{display:inline-flex;gap:8px;align-items:center;padding:6px 10px;border:1px solid #3a424a;border-radius:999px;background:#1d2329;color:#cfd3d7;font-size:12px}
.btn{cursor:pointer;background:#1a1f24;border:1px solid #3a424a;color:#fff;border-radius:10px;padding:8px 12px}
.btn:hover{filter:brightness(1.05)}
.small{font-size:12px;color:var(--muted)}
</style>
</head>
<body>
<header class="header">
  <h1 class="title">CENTRO DE MONITOREO</h1>
  <div class="subtitle">ESP32-CAM • Punto de Acceso</div>
</header>

<div class="app">
  <div class="panel">
    <span class="pill">Red: <b>ESP-CAM-HUB</b></span>
    <span class="pill">Clave: <b>12345678</b></span>
    <span class="pill">IP: <b>192.168.4.1</b></span>
    <button class="btn" onclick="snap()">Captura (/jpg)</button>
    <a id="dl" class="btn" href="#" download="snapshot.jpg" style="display:none">Descargar</a>
  </div>

  <div class="right">
    <div class="viewer">
      <img id="img" alt="ESP32-CAM stream">
      <div class="grid-overlay"></div>
      <div class="cross"></div>
    </div>
  </div>

  <div class="panel small" id="status">Estado: iniciando…</div>
</div>

<script>
const img = document.getElementById('img');
const sts = document.getElementById('status');
const dl  = document.getElementById('dl');

function start(){
  img.onerror = ()=>{ sts.textContent = "Estado: reconectando stream…"; setTimeout(()=>img.src='/stream', 600); };
  img.onload  = ()=>{ sts.textContent = "Estado: stream OK"; };
  img.src = '/stream';
}
async function snap(){
  try{
    const r = await fetch('/jpg?t='+Date.now(), {cache:'no-store'});
    if(!r.ok) throw new Error(r.status);
    const blob = await r.blob();
    dl.href = URL.createObjectURL(blob);
    dl.download = 'snapshot_' + Date.now() + '.jpg';
    dl.style.display='inline-block';
    dl.click();
    setTimeout(()=>{ URL.revokeObjectURL(dl.href); dl.style.display='none'; }, 800);
  }catch(e){
    alert('No se pudo capturar');
  }
}
start();
</script>
</body></html>)HTML";

// ====== Handlers ======
void handle_root(){ server.send_P(200,"text/html", INDEX_HTML); }

void handle_jpg(){
  camera_fb_t* fb = esp_camera_fb_get();
  if(!fb){ server.send(503,"text/plain","capture failed"); return; }
  WiFiClient client = server.client();
  client.setNoDelay(true);
  server.setContentLength(fb->len);
  server.send(200,"image/jpeg");
  client.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// MJPEG optimizado
void handle_stream(){
  WiFiClient client = server.client();
  if(!client) return;
  client.setNoDelay(true);

  client.print(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n\r\n"
  );
  while(client.connected()){
    camera_fb_t* fb = esp_camera_fb_get();
    if(!fb) break;

    client.print("--frame\r\n");
    client.print("Content-Type: image/jpeg\r\n");
    client.printf("Content-Length: %u\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);
    delay(5); // 0–10 ms según fluidez
  }
}

bool cam_init(){
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0       = Y2_GPIO_NUM;
  c.pin_d1       = Y3_GPIO_NUM;
  c.pin_d2       = Y4_GPIO_NUM;
  c.pin_d3       = Y5_GPIO_NUM;
  c.pin_d4       = Y6_GPIO_NUM;
  c.pin_d5       = Y7_GPIO_NUM;
  c.pin_d6       = Y8_GPIO_NUM;
  c.pin_d7       = Y9_GPIO_NUM;
  c.pin_xclk     = XCLK_GPIO_NUM;
  c.pin_pclk     = PCLK_GPIO_NUM;
  c.pin_vsync    = VSYNC_GPIO_NUM;
  c.pin_href     = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn     = PWDN_GPIO_NUM;
  c.pin_reset    = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;

  c.fb_location  = CAMERA_FB_IN_PSRAM;
  c.frame_size   = CAM_FRAME_SIZE;
  c.jpeg_quality = CAM_JPEG_QUALITY;
  c.fb_count     = CAM_FB_COUNT;
  // c.grab_mode = CAMERA_GRAB_LATEST; // si tu versión lo soporta

  return (esp_camera_init(&c) == ESP_OK);
}

void startServer(){
  server.on("/",       HTTP_GET, handle_root);
  server.on("/jpg",    HTTP_GET, handle_jpg);
  server.on("/stream", HTTP_GET, handle_stream);
  server.begin();
}

void setup(){
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(200);

  if(!cam_init()){
    Serial.println("Camera init failed");
    return;
  }

  // ====== AP propio con IP 192.168.4.1 ======
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP %s %s  IP: %s\n", AP_SSID, ok?"OK":"FAIL", WiFi.softAPIP().toString().c_str());

  startServer();
  Serial.println("Server listo. Abre: http://192.168.4.1/");
}

void loop(){
  server.handleClient();
  delay(1);
}
