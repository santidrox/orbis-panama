#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"

// ---- SENSOR ----
#define DHTPIN 5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---- WIFI (2.4 GHz) ----
const char* WIFI_SSID = "LICEO TEC";
const char* WIFI_PASS = "Liceotec2024*";

// ---- SERVIDOR LOCAL (tu PC con Flask) ----
const char* SERVER_HOST = "192.168.68.117"; // <--- CAMBIA a la IP de tu PC
const int   SERVER_PORT = 5000;
const char* DEVICE_ID   = "esp_dht11_local";

const int LED = 2;
const unsigned long SEND_INTERVAL_MS = 1000;
unsigned long lastSend = 0;

void setup(){
  Serial.begin(115200);
  pinMode(LED, OUTPUT); digitalWrite(LED, LOW);
  dht.begin();

  Serial.println("\nConectando WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status()!=WL_CONNECTED && millis()-t0<20000){ Serial.print("."); delay(300); }
  Serial.println();
  if (WiFi.status()==WL_CONNECTED){ Serial.print("WiFi OK IP: "); Serial.println(WiFi.localIP()); digitalWrite(LED, HIGH);}
  else { Serial.println("⚠️ No se conectó al WiFi (timeout)."); }
}

void loop(){
  if (WiFi.status() != WL_CONNECTED){
    WiFi.disconnect(); WiFi.begin(WIFI_SSID, WIFI_PASS);
    delay(1500);
    return;
  }

  if (millis() - lastSend < SEND_INTERVAL_MS) return;
  lastSend = millis();

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)){ Serial.println("Lectura DHT inválida"); return; }

  String payload = "{\"device_id\":\"" + String(DEVICE_ID) +
                   "\",\"temp\":" + String(t,1) +
                   ",\"humidity\":" + String(h,1) + "}";

  HTTPClient http;
  http.setTimeout(4000);
  String url = "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT) + "/metrics";
  http.begin(url);
  http.addHeader("Content-Type","application/json");
  int code = http.POST(payload);
  Serial.printf("POST %d -> %s | %s\n", code, url.c_str(), payload.c_str());
  http.end();

  if (code==200){ digitalWrite(LED, LOW); delay(40); digitalWrite(LED, HIGH); }
}
