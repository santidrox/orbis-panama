#include <WiFi.h>
#include <HTTPClient.h>
#include "DHT.h"

// ===== WiFi del HUB (ESP32-CAM en AP) =====
const char* SSID = "ESP-CAM-HUB";
const char* PASS = "12345678";

// ===== DHT11 =====
#define DHTPIN 5          // <-- tu pin actual (puedes cambiar a 4 o 27 si prefieres)
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const unsigned long POST_EVERY_MS = 2000;
unsigned long lastPost = 0;

bool readDHT(float &t, float &h) {
  // hasta 5 intentos
  for (uint8_t i=0; i<5; i++) {
    h = dht.readHumidity();
    t = dht.readTemperature();
    if (!isnan(t) && !isnan(h)) return true;
    delay(800);
  }
  return false;
}

void setup(){
  Serial.begin(115200);
  // Si NO tienes la resistencia de 10k, deja este pull-up interno:
  pinMode(DHTPIN, INPUT_PULLUP);

  dht.begin();
  delay(2000); // estabilización del DHT

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  Serial.print("Conectando al HUB");
  while(WiFi.status()!=WL_CONNECTED){ Serial.print("."); delay(400); }
  Serial.printf("\nOK. IP cliente: %s  (HUB: 192.168.4.1)\n", WiFi.localIP().toString().c_str());
}

void loop(){
  if(millis() - lastPost >= POST_EVERY_MS){
    lastPost = millis();

    float t, h;
    if(!readDHT(t, h)){
      Serial.println(">> ERROR: DHT no responde. Revisa: 3.3V, GND, DATA->GPIO, pull-up 10k o INPUT_PULLUP, cable corto.");
      return;
    }

    if(WiFi.status()==WL_CONNECTED){
      String url = "http://192.168.4.1/metrics"; // HUB (IP del AP)
      String payload = String("{\"temp\":") + String(t,1) + ",\"humidity\":" + String(h,1) + "}";
      HTTPClient http;
      http.begin(url);
      http.addHeader("Content-Type","application/json");
      int code = http.POST(payload);
      Serial.printf("POST %s -> %d  %s\n", url.c_str(), code, payload.c_str());
      http.end();
    } else {
      Serial.println("WiFi desconectado, reintentando...");
      WiFi.reconnect();
    }
  }
}
