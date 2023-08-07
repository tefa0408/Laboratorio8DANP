#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"
#define TIME_ZONE -5

unsigned long lastMillis = 0;

#define AWS_IOT_PUBLISH_TOPIC   "mobile/mensajes"

WiFiClientSecure net;

BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);

PubSubClient client(net);

time_t now;
time_t nowish = 1510592825;

void NTPConnect(void) {
  Serial.print("Configuración de la hora mediante SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "3.pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void connectAWS() {
  delay(3000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println(String("Intentando conectarse al SSID: ") + String(WIFI_SSID));

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(200);
  }

  NTPConnect();

  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);

  client.setServer(MQTT_HOST, 8883);

  Serial.println("Conexión a AWS IOT");

  while (!client.connect("SensorLab")) {
    Serial.print(".");
    delay(1000);
  }

  if (!client.connected()) {
    Serial.println("Se acabó el tiempo de espera de AWS IoT!");
    return;
  }
  
  Serial.println("AWS IoT conectado!");
}

void publishMessage()
{
  StaticJsonDocument<350> doc;
  doc["Timestamp"] = now;
  doc["Value"] = random(0, 100); // Cambiar esto por el valor censado que desees enviar
  doc["Unit"] = "unidad_de_medida";
  doc["Notes"] = "TEST";

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer);

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void setup() {
  Serial.begin(115200);
  connectAWS();
}

void loop() {
  now = time(nullptr);

  if (!client.connected()){
    connectAWS();
  }
  else {
    client.loop();
    if (millis() - lastMillis > 30000) {// Enviar mensaje cada 30 segundos
      lastMillis = millis();
      publishMessage();
    }
  }
}
