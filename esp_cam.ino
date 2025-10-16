#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ===== WiFi del router (misma red que el PC con hub.py) =====
const char* SSID = "Bandido_Y_Bella";
const char* PASS = "Worldbikess2025";

// ===== Pines AI-Thinker ESP32-CAM =====
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

WebServer server(80);

// ===== Perfil por defecto (equilibrado) =====
framesize_t g_fs = FRAMESIZE_QVGA; // 320x240
int g_quality = 10;                // 10 (mejor) ... 30 (peor)
sensor_t* g_sens = nullptr;

framesize_t parse_fs(const String& s){
  if (s == "qqvga") return FRAMESIZE_QQVGA; // 160x120
  if (s == "qvga")  return FRAMESIZE_QVGA;  // 320x240
  if (s == "hvga")  return FRAMESIZE_HVGA;  // 480x320
  if (s == "vga")   return FRAMESIZE_VGA;   // 640x480
  if (s == "svga")  return FRAMESIZE_SVGA;  // 800x600
  // puedes añadir más si tu módulo lo soporta
  return g_fs;
}

const char* fs_name(framesize_t fs){
  switch(fs){
    case FRAMESIZE_QQVGA: return "qqvga";
    case FRAMESIZE_QVGA:  return "qvga";
    case FRAMESIZE_HVGA:  return "hvga";
    case FRAMESIZE_VGA:   return "vga";
    case FRAMESIZE_SVGA:  return "svga";
    default:              return "other";
  }
}

void apply_camera_params(){
  if (!g_sens) return;
  g_sens->set_framesize(g_sens, g_fs);
  g_sens->set_quality(g_sens, g_quality);
  g_sens->set_lenc(g_sens, 1);
  g_sens->set_aec2(g_sens, 1);
}

void handle_status(){
  String j = "{";
  j += "\"framesize\":\"" + String(fs_name(g_fs)) + "\",";
  j += "\"quality\":" + String(g_quality) + ",";
  j += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  j += "}";
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json", j);
}

void handle_set(){
  bool changed = false;

  if (server.hasArg("fs")) {
    framesize_t nf = parse_fs(server.arg("fs"));
    if (nf != g_fs){ g_fs = nf; changed = true; }
  }
  if (server.hasArg("q")) {
    int q = server.arg("q").toInt();
    if (q < 10) q = 10;
    if (q > 30) q = 30;
    if (q != g_quality){ g_quality = q; changed = true; }
  }

  if (changed){
    apply_camera_params();
  }

  handle_status();
}

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
    delay(1); yield();
  }
}

void setup(){
  Serial.begin(115200);
  delay(100);

  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0=Y2_GPIO_NUM; c.pin_d1=Y3_GPIO_NUM; c.pin_d2=Y4_GPIO_NUM; c.pin_d3=Y5_GPIO_NUM;
  c.pin_d4=Y6_GPIO_NUM; c.pin_d5=Y7_GPIO_NUM; c.pin_d6=Y8_GPIO_NUM; c.pin_d7=Y9_GPIO_NUM;
  c.pin_xclk=XCLK_GPIO_NUM; c.pin_pclk=PCLK_GPIO_NUM; c.pin_vsync=VSYNC_GPIO_NUM; c.pin_href=HREF_GPIO_NUM;
  c.pin_sccb_sda=SIOD_GPIO_NUM; c.pin_sccb_scl=SIOC_GPIO_NUM;
  c.pin_pwdn=PWDN_GPIO_NUM; c.pin_reset=RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;

  // PERFIL FPS + calidad ajustable
  c.frame_size   = g_fs;
  c.jpeg_quality = g_quality;
  c.fb_count     = 2;
  c.fb_location  = CAMERA_FB_IN_PSRAM;
  c.grab_mode    = CAMERA_GRAB_LATEST;

  if(esp_camera_init(&c) != ESP_OK){
    Serial.println("Camera init failed");
    while(true){ delay(1000); }
  }

  g_sens = esp_camera_sensor_get();
  apply_camera_params();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(SSID, PASS);
  Serial.printf("[WiFi] Conectando a %s", SSID);
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(300); }
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  Serial.printf("\n[WiFi] OK IP=%s\n", WiFi.localIP().toString().c_str());

  server.on("/stream", HTTP_GET, handle_stream);
  server.on("/status", HTTP_GET, handle_status);
  server.on("/set",    HTTP_GET, handle_set);
  server.begin();
  Serial.println("Listo: /stream, /status, /set?fs=...&q=...");
}

void loop(){
  server.handleClient();
  delay(1);
}
