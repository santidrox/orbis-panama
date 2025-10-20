#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// ===== Pines KY-024 =====
#define PIN_ANALOGO 34   // AO del KY-024 (ADC1_CH6)
#define PIN_DIGITAL 25   // DO del KY-024 (opcional)

// ===== BME280 =====
#define BME_ADDR 0x76
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme;

// ===== WiFi (misma red que la PC con hub.py) =====
const char* SSID = "iPhone de Droxs";
const char* PASS = "ardilla69";

// ===== Hub HTTP =====
String HUB = "http://172.20.10.2:8000/ingest";

// ===== Temporización =====
unsigned long lastTick = 0;
const unsigned long PERIOD_MS = 1000;  // 1 s

// ===== Autocalibración KY024 =====
float baseline = NAN;
float hiMark = NAN;
const int DETECT_MARGIN = 80;
const bool USE_DO_PIN = true;
float emaRaw = NAN;

// ===== Utilidades =====
static inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(200);

  // --- Configurar ADC KY-024 ---
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_ANALOGO, ADC_11db);
  pinMode(PIN_DIGITAL, INPUT);

  // --- Configurar I2C y BME280 ---
  Wire.begin(21, 22);
  Wire.setClock(400000);
  Serial.println(F("[BME280] Inicio"));
  bool status = bme.begin(BME_ADDR);
  if (!status) {
    Serial.println("[BME280] ¡No se encontró el sensor en 0x76! Revisa conexiones SDA=21, SCL=22.");
    while (true) delay(1000);
  }
  Serial.println("[BME280] OK en 0x76");

  // --- Conectar WiFi ---
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(SSID, PASS);
  Serial.printf("[WiFi] Conectando a %s", SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  Serial.printf("\n[WiFi] OK  IP=%s\n", WiFi.localIP().toString().c_str());

  Serial.println("=== KY-024 Autocalibrado ===");
  Serial.println("1) Mantén el sensor SIN imán unos segundos: fijará baseline (0%).");
  Serial.println("2) Acerca el imán para que aprenda el máximo (100%).");

  lastTick = millis();
}

// ===== Loop =====
void loop() {
  if (millis() - lastTick < PERIOD_MS) return;
  lastTick = millis();

  // ====== LECTURA KY-024 ======
  int raw = analogRead(PIN_ANALOGO);
  int inv = 4095 - raw;

  // Suavizado EMA
  if (isnan(emaRaw)) emaRaw = inv;
  emaRaw = emaRaw + 0.25f * (inv - emaRaw);
  int invSmooth = (int)roundf(emaRaw);

  // Autocalibración
  if (isnan(baseline)) baseline = invSmooth;
  else baseline = baseline + 0.02f * (invSmooth - baseline);
  if (isnan(hiMark) || invSmooth > hiMark) hiMark = invSmooth;
  else hiMark = hiMark - 0.005f * (hiMark - baseline);
  if (hiMark < baseline + 40) hiMark = baseline + 40;

  float span = (hiMark - baseline);
  float percent = 0.0f;
  if (span > 1.0f) percent = (invSmooth - baseline) * 100.0f / span;
  percent = clampf(percent, 0.0f, 100.0f);

  bool magState;
  if (USE_DO_PIN) magState = (digitalRead(PIN_DIGITAL) == HIGH);
  else magState = (invSmooth >= (baseline + DETECT_MARGIN));

  // ====== LECTURA BME280 ======
  float t = bme.readTemperature();            // °C
  float p = bme.readPressure() / 100.0F;      // hPa
  float h = bme.readHumidity();               // %
  float alt = bme.readAltitude(SEALEVELPRESSURE_HPA); // m

  // ====== LOG LOCAL ======
  Serial.printf("[KY024] Inv=%d  %%=%.1f  Estado=%s\n", invSmooth, percent, magState ? "CAMPO" : "SIN CAMPO");
  Serial.printf("[BME280] T=%.1f°C  H=%.1f%%  P=%.1f hPa  Alt=%.1f m\n", t, h, p, alt);

  // ====== ENVÍO HTTP ======
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    return;
  }

  HTTPClient http;
  http.setConnectTimeout(2000);
  http.begin(HUB);
  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"mag_raw\":" + String(invSmooth) + ",";
  payload += "\"mag_percent\":" + String(percent, 1) + ",";
  payload += "\"mag_state\":" + String(magState ? "true" : "false") + ",";
  payload += "\"temp\":" + String(t, 1) + ",";
  payload += "\"pressure\":" + String(p, 1) + ",";
  payload += "\"altitude\":" + String(alt, 1) + ",";
  payload += "\"humidity\":" + String(h, 1);
  payload += "}";

  int code = http.POST(payload);
  String resp = http.getString();
  http.end();

  Serial.printf("[POST %d] %s\n", code, resp.c_str());
}