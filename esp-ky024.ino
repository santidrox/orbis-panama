#include <WiFi.h>
#include <HTTPClient.h>

// ===== Pines KY-024 =====
#define PIN_ANALOGO 34   // AO del KY-024 (ADC1_CH6)
#define PIN_DIGITAL 25   // DO del KY-024 (opcional, umbral por potenciómetro)

// ===== WiFi (misma red que la PC con hub.py) =====
const char* SSID = "LICEO TEC";
const char* PASS = "Liceotec2024*";

// ===== Hub HTTP =====
String HUB = "http://192.168.68.107:8000/ingest";  // <--- IP:PUERTO de tu PC con hub.py

// ===== Temporización =====
unsigned long lastPost   = 0;
const unsigned long PERIOD_MS = 1000;  // 1 s

// ===== Autocalibración =====
// baseline: lectura (invertida) sin imán  -> ~ 0%
// hiMark : mayor lectura (invertida) vista -> ~100%
float baseline = NAN;
float hiMark   = NAN;

// margen de histéresis para decidir "CAMPO DETECTADO" con analógico
const int DETECT_MARGIN = 80;   // suma a baseline para confirmar campo si no usas DO

// control de método de detección
const bool USE_DO_PIN = true;   // true = usa DO; false = calcula umbral analógico

// suavizado EMA
float emaRaw = NAN;

// utilidades
static inline float clampf(float v, float lo, float hi){
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

void setup(){
  Serial.begin(115200);
  delay(200);

  // ADC: rango 0..4095 y atenuación para ~0..3.3V
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_ANALOGO, ADC_11db);
  pinMode(PIN_DIGITAL, INPUT);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(SSID, PASS);
  Serial.printf("[WiFi] Conectando a %s", SSID);
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(300); }
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  Serial.printf("\n[WiFi] OK  IP=%s\n", WiFi.localIP().toString().c_str());

  Serial.println("=== KY-024 Autocalibrado ===");
  Serial.println("1) Mantén el sensor SIN imán unos segundos: fijará baseline (0%).");
  Serial.println("2) Acerca el imán para que aprenda el máximo (100%).");
  Serial.println("Tip: puedes mover el imán para que el 100% se ajuste al pico real.");

  lastPost = millis();
}

void loop(){
  if (WiFi.status() != WL_CONNECTED) { WiFi.reconnect(); delay(50); }

  // periodo 1 s
  unsigned long now = millis();
  if (now - lastPost < PERIOD_MS) { delay(1); return; }
  lastPost = now;

  // Lectura cruda e invertida (sube con el imán)
  int raw = analogRead(PIN_ANALOGO);     // 0..4095
  int inv = 4095 - raw;                  // invertimos: "más imán" => más grande

  // Suavizado
  if (isnan(emaRaw)) emaRaw = inv;
  emaRaw = emaRaw + 0.25f * (inv - emaRaw);   // alpha=0.25
  int invSmooth = (int)roundf(emaRaw);

  // ====== Autocalibración ======
  // baseline: si no está, tómalo de la primera(s) lecturas (sin imán)
  if (isnan(baseline)) {
    baseline = invSmooth;
  } else {
    // si claramente baja (menos campo), permite que baseline caiga lentamente (deriva)
    baseline = baseline + 0.02f * (invSmooth - baseline); // seguimiento lento
  }

  // hiMark: rastrea el máximo visto (para estirar a 100%)
  if (isnan(hiMark) || invSmooth > hiMark) {
    hiMark = invSmooth;
  } else {
    // decaimiento MUY lento para no perder fácilmente 100%
    hiMark = hiMark - 0.005f * (hiMark - baseline);
  }

  // evita colapsos (hiMark debe estar por encima de baseline)
  if (hiMark < baseline + 40) hiMark = baseline + 40;

  // ====== Porcentaje 0..100 ======
  float span = (hiMark - baseline);
  float percent = 0.0f;
  if (span > 1.0f) {
    percent = (invSmooth - baseline) * 100.0f / span;
  }
  percent = clampf(percent, 0.0f, 100.0f);

  // ====== Estado digital / umbral ======
  bool state;
  if (USE_DO_PIN) {
    int d = digitalRead(PIN_DIGITAL); // 1 = supera umbral del potenciómetro
    state = (d == HIGH);
  } else {
    // Si no quieres usar DO, decide por analógico con margen sobre baseline
    state = (invSmooth >= (baseline + DETECT_MARGIN));
  }

  // ------ Log Serial (formato pedido) ------
  Serial.print("Valor analógico (invertido): ");
  Serial.print(invSmooth);
  Serial.print("\t | Intensidad: ");
  Serial.print(percent, 1);
  Serial.print("%");
  Serial.print("\t | Estado digital: ");
  Serial.println(state ? "CAMPO DETECTADO" : "SIN CAMPO");

  // ------ Envío al hub ------
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setConnectTimeout(1500);
    http.begin(HUB);
    http.addHeader("Content-Type", "application/json");

    String payload = String("{\"mag_raw\":") + invSmooth +
                     ",\"mag_percent\":" + String(percent, 1) +
                     ",\"mag_state\":" + (state ? "true":"false") +
                     "}";
    int code = http.POST(payload);
    String resp = http.getString();
    http.end();

    Serial.printf("[POST %d] %s\n", code, resp.c_str());
  }
}
