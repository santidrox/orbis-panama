#include <WiFi.h>
#include <HTTPClient.h>

// ----------- AJUSTA ESTOS -----------
const char* WIFI_SSID = "Bandido_Y_Bella";
const char* WIFI_PASS = "Worldbikess2025";
const char* SERVER    = "http://192.168.1.15:8000"; // IP:PUERTO del hub.py

// Pines recomendados
#define HALL_AO 34   // AO del KY-024 -> GPIO34 (entrada analógica)
#define HALL_DO 27   // DO del KY-024 -> GPIO27 (entrada digital, opcional)

const unsigned long PERIOD_MS = 3000; // cada 3 segundos
unsigned long last_ms = 0;

// Filtro simple (media móvil) para AO
const int AVG_WIN = 8;       // ventana (más grande = más suave)
int samples[AVG_WIN];
int idx = 0;
bool filled = false;

void setup() {
  Serial.begin(115200);
  delay(200);

  // Pines
  pinMode(HALL_DO, INPUT);   // si no conectas DO, no pasa nada
  analogReadResolution(12);  // 0..4095 en ESP32
  // (opcional) estabiliza ADC:
  // analogSetPinAttenuation(HALL_AO, ADC_11db); // mayor rango útil

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // menor latencia
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WiFi] Conectando a %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(300); }
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  Serial.printf("\n[WiFi] OK  IP=%s\n", WiFi.localIP().toString().c_str());

  // Inicializa buffer del filtro
  for (int i=0;i<AVG_WIN;i++) samples[i]=0;
}

static int readAO() {
  // Lectura múltiple rápida + promedio para reducir ruido instantáneo
  const int N=4;
  long acc=0;
  for(int i=0;i<N;i++){ acc += analogRead(HALL_AO); delayMicroseconds(300); }
  return (int)(acc/N); // 0..4095
}

static int avgAO(int v){
  samples[idx++] = v;
  if (idx >= AVG_WIN){ idx=0; filled=true; }
  int n = filled ? AVG_WIN : idx;
  long acc=0;
  for(int i=0;i<n;i++) acc+=samples[i];
  return (int)(acc/n);
}

void loop() {
  if (millis() - last_ms < PERIOD_MS) return;
  last_ms = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Reconectando...");
    WiFi.reconnect();
    return;
  }

  // Lecturas
  int raw = readAO();              // 0..4095
  int smooth = avgAO(raw);         // suavizado
  int dig = digitalRead(HALL_DO);  // 0/1 (umbral del potenciómetro)

  // Muestra en Serial
  Serial.printf("[KY-024] AO=%d (avg=%d)  DO=%d\n", raw, smooth, dig);

  // Construye JSON (usa el suavizado para reportar)
  String payload = String("{\"device_id\":\"mag1\",\"mag_raw\":") + String(smooth) +
                   ",\"mag_state\":" + String(dig) + "}";

  // POST al hub
  HTTPClient http;
  http.setTimeout(2500); // ms
  http.begin(String(SERVER) + "/ingest");
  http.addHeader("Content-Type", "application/json");

  Serial.printf("[POST] %s\n", payload.c_str());
  int code = http.POST(payload);
  String resp = http.getString();
  http.end();

  Serial.printf("[RESP] code=%d body=%s\n", code, resp.c_str());
}
