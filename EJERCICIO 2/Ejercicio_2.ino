#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <ESP8266HTTPClient.h>
#include "secrets.h"


#define TIME_ZONE -5
#define LED_PIN 14
#define MQTT_TOPIC "sensor/command"

unsigned long lastMillis = 0;
int commandValue = 3; // Valor de comando recibido

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

  // Suscribirse al tópico para recibir comandos
  client.subscribe(MQTT_TOPIC);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT); // Configuramos el pin GPIO 14 como salida
  connectAWS();
}

// Función para obtener el comando del URL
void getCommandFromURL() {
  HTTPClient http;

  // Realizar la solicitud GET al URL
  String url = "https://fkcmxu52vb7naznafl2ciivoei0ipuea.lambda-url.us-east-1.on.aws/command?value=";
  url += commandValue;
  
  Serial.print("Solicitud HTTP GET: ");
  Serial.println(url);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    commandValue = payload.toInt();
    Serial.print("Comando recibido: ");
    Serial.println(commandValue);
  } else {
    Serial.print("Error en la solicitud HTTP: ");
    Serial.println(httpCode);
  }

  http.end();
}


void callback(char* topic, byte* payload, unsigned int length) {
  // Procesar el mensaje recibido desde el tópico "sensor/command"
  if (String(topic) == MQTT_TOPIC) {
    String payloadStr;
    for (int i = 0; i < length; i++) {
      payloadStr += (char)payload[i];
    }
    commandValue = payloadStr.toInt();
    if (commandValue >= 0 && commandValue <= 100) {
      // Ajustar el brillo del LED basado en el valor recibido
      int ledValue = map(commandValue, 0, 100, 0, 1023);
      analogWrite(LED_PIN, ledValue);
    }
  }
}

void loop() {
  now = time(nullptr);

  if (!client.connected()) {
    connectAWS();
  } else {
    client.loop();
    if (millis() - lastMillis > 5000) { // Obtener comando desde el Cloud cada 5 segundos
      lastMillis = millis();
      getCommandFromURL();
    }
  }
}
