/*
 * ============================================================
 * Projeto: Bueiro Inteligente
 * Universidade Presbiteriana Mackenzie
 * Disciplina: Objetos Inteligentes Conectados
 * ------------------------------------------------------------
 * Dispositivo: Nó Sensor (ESP32 + HC-SR04)
 *
 * Função: medir a distância entre o topo do bueiro e a
 * superfície dos resíduos e publicar, a cada 5 segundos,
 * a distância (cm) e o nível de obstrução (%) via MQTT.
 *
 * Pinagem (diagram.json):
 *   HC-SR04 VCC  -> VIN (5V)
 *   HC-SR04 GND  -> GND
 *   HC-SR04 TRIG -> GPIO 5  (D5)
 *   HC-SR04 ECHO -> GPIO 18 (D18)
 *
 * Tópicos publicados:
 *   mackenzie/bueiro-inteligente/bueiro01/sensor/distancia
 *   mackenzie/bueiro-inteligente/bueiro01/sensor/obstrucao
 *   mackenzie/bueiro-inteligente/bueiro01/sensor/conexao (LWT)
 * ============================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>

// ---------- Wi-Fi (rede simulada do Wokwi) ----------
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// ---------- Broker MQTT ----------
const char* MQTT_BROKER    = "broker.hivemq.com";
const int   MQTT_PORT      = 1883;
const char* MQTT_CLIENT_ID = "bueiro01-sensor";

// ---------- Identificação e tópicos ----------
const char* ID_BUEIRO = "bueiro01";
const char* PREFIXO   = "mackenzie/bueiro-inteligente";

char topicoDistancia[80];
char topicoObstrucao[80];
char topicoConexao[80];

// ---------- Pinos do HC-SR04 ----------
const int PIN_TRIG = 5;
const int PIN_ECHO = 18;

// ---------- Calibração do bueiro ----------
// Distância do sensor até o fundo com o bueiro vazio (0%).
// 400 cm corresponde ao alcance máximo do HC-SR04.
const float DISTANCIA_VAZIO_CM = 400.0;
// Distância mínima, com resíduos encostando no sensor (100%)
const float DISTANCIA_CHEIO_CM = 2.0;

// ---------- Temporização ----------
const unsigned long INTERVALO_ENVIO_MS = 5000;
unsigned long ultimoEnvio = 0;

WiFiClient espClient;
PubSubClient mqtt(espClient);

// Mede a distância em cm usando o HC-SR04
float medirDistanciaCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duracao = pulseIn(PIN_ECHO, HIGH, 30000); // timeout 30 ms (~5 m)
  if (duracao == 0) return DISTANCIA_VAZIO_CM;    // sem eco: considera vazio
  return duracao * 0.0343 / 2.0;
}

// Converte a distância em percentual de obstrução (0 a 100%)
float calcularObstrucaoPct(float distanciaCm) {
  float faixa = DISTANCIA_VAZIO_CM - DISTANCIA_CHEIO_CM;
  float pct = (DISTANCIA_VAZIO_CM - distanciaCm) / faixa * 100.0;
  return constrain(pct, 0.0, 100.0);
}

void conectarWiFi() {
  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" conectado!");
}

void conectarMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Conectando ao broker MQTT...");
    // LWT: se cair, o broker publica "offline" (QoS 1, retain)
    if (mqtt.connect(MQTT_CLIENT_ID, topicoConexao, 1, true, "offline")) {
      Serial.println(" conectado!");
      mqtt.publish(topicoConexao, "online", true);
    } else {
      Serial.print(" falhou, rc=");
      Serial.print(mqtt.state());
      Serial.println(". Nova tentativa em 2s");
      delay(2000);
    }
  }
}

void montarTopicos() {
  snprintf(topicoDistancia, sizeof(topicoDistancia), "%s/%s/sensor/distancia", PREFIXO, ID_BUEIRO);
  snprintf(topicoObstrucao, sizeof(topicoObstrucao), "%s/%s/sensor/obstrucao", PREFIXO, ID_BUEIRO);
  snprintf(topicoConexao,   sizeof(topicoConexao),   "%s/%s/sensor/conexao",   PREFIXO, ID_BUEIRO);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  montarTopicos();
  conectarWiFi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setKeepAlive(60); // tolera melhor a lentidão da simulação
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) conectarWiFi();
  if (!mqtt.connected()) conectarMQTT();
  mqtt.loop();

  unsigned long agora = millis();
  if (agora - ultimoEnvio < INTERVALO_ENVIO_MS) return;
  ultimoEnvio = agora;

  float distanciaCm  = medirDistanciaCm();
  float obstrucaoPct = calcularObstrucaoPct(distanciaCm);

  char bufDistancia[16];
  char bufObstrucao[16];
  dtostrf(distanciaCm,  0, 1, bufDistancia);
  dtostrf(obstrucaoPct, 0, 1, bufObstrucao);

  mqtt.publish(topicoDistancia, bufDistancia);
  mqtt.publish(topicoObstrucao, bufObstrucao);

  Serial.printf("Distancia: %s cm | Obstrucao: %s %%\n", bufDistancia, bufObstrucao);
}
