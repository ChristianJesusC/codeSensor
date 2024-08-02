#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>

const char *mqtt_broker = "broker.emqx.io";
const char *topic = "/petMaps/ids/digitales2024";
const char *mqtt_username = "petmaps";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

const int pinPulso = 32;
int valorPulso = 0;
int umbral = 512;
unsigned long tiempoUltimoLatido = 0;
int conteoLatidos = 0;
float latidosPorMinuto = 0;
unsigned long tiempoInicio = 0;
unsigned long tiempoActual = 0;

#define DHT_SENSOR_PIN 23
#define DHT_SENSOR_TYPE DHT11
DHT dht(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

TinyGPSPlus gps;
SoftwareSerial mySerial(16, 17);

WiFiClient espClient;
PubSubClient client(espClient);

String esp32_id;

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  
  WiFiManager wifiManager;
  wifiManager.autoConnect("ESP32_AP");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Conectado a WiFi: ");
    Serial.println(WiFi.SSID());
  } else {
    Serial.println("No conectado a WiFi");
  }

  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    esp32_id = WiFi.macAddress();
    String client_id = "esp32-client-" + esp32_id;
    Serial.printf("El cliente %s se está conectando a un broker público MQTT\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Conectado a Public emqx MQTT broker");
    } else {
      Serial.print("Fallo con estado ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  client.subscribe(topic);
  dht.begin();
  tiempoInicio = millis();
}

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Mensaje en el topic: ");
  Serial.println(topic);
  Serial.print("Mensaje:");
  for (int i = 0; i < length; i++) {
    Serial.print((char) payload[i]);
  }
  Serial.println();
  Serial.println("-----------------------");
}

void loop() {
  client.loop();

  while (mySerial.available() > 0) {
    gps.encode(mySerial.read());
  }

  valorPulso = analogRead(pinPulso);
  if (valorPulso > umbral) {
    if (millis() - tiempoUltimoLatido > 250) {
      conteoLatidos++;
      tiempoUltimoLatido = millis();
    }
  } else {
    if (millis() - tiempoUltimoLatido > 3000) {
      conteoLatidos = 0;
    }
  }

  float humedad = dht.readHumidity();
  float temperatura = dht.readTemperature();

  tiempoActual = millis();
  if (tiempoActual - tiempoInicio >= 5000) {
    latidosPorMinuto = (conteoLatidos / 15.0) * 60.0;
    if (!isnan(temperatura) && !isnan(humedad) && latidosPorMinuto > 0 && gps.location.isUpdated()) {
      StaticJsonDocument<200> doc;
      doc["esp32_id"] = esp32_id;
      doc["latidos_por_minuto"] = latidosPorMinuto;
      doc["temperatura"] = temperatura;
      doc["latitud"] = gps.location.lat();
      doc["longitud"] = gps.location.lng();

      char jsonBuffer[512];
      serializeJson(doc, jsonBuffer);

      client.publish(topic, jsonBuffer);

      Serial.println("Datos enviados al broker MQTT");
    } else {
      Serial.println("Error al leer los datos del sensor DHT11, del GPS o no se detectaron latidos.");
    }
    conteoLatidos = 0;
    tiempoInicio = millis();
  }
delay(20);
}