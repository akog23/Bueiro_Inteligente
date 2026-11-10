/*
 * ============================================================
 * Projeto: Bueiro Inteligente
 * Universidade Presbiteriana Mackenzie
 * Disciplina: Objetos Inteligentes Conectados
 * ------------------------------------------------------------
 * Dispositivo: Nó Atuador (ESP32 + semáforo de LEDs)
 *
 * Função: receber do Node-RED, via MQTT, o status do bueiro
 * (verde, amarelo ou vermelho), acender o LED correspondente
 * e publicar a confirmação da execução do comando.
 *
 * Pinagem (diagram.json):
 *   LED verde    -> GPIO 25 (D25) + resistor 220 ohms
 *   LED amarelo  -> GPIO 26 (D26) + resistor 220 ohms
 *   LED vermelho -> GPIO 27 (D27) + resistor 220 ohms
 *   Catodos      -> GND
 *
 * Tópicos:
 *   Assina:  mackenzie/bueiro-inteligente/bueiro01/atuador/status
 *   Publica: mackenzie/bueiro-inteligente/bueiro01/atuador/confirmacao
 *   Publica: mackenzie/bueiro-inteligente/bueiro01/atuador/conexao (LWT)
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
const char* MQTT_CLIENT_ID = "bueiro01-atuador";

// ---------- Identificação e tópicos ----------
const char* ID_BUEIRO = "bueiro01";
const char* PREFIXO   = "mackenzie/bueiro-inteligente";

char topicoStatus[80];
char topicoConfirmacao[80];
char topicoConexao[80];

// ---------- Pinos dos LEDs ----------
const int PIN_LED_VERDE    = 25;
const int PIN_LED_AMARELO  = 26;
const int PIN_LED_VERMELHO = 27;

WiFiClient espClient;
PubSubClient mqtt(espClient);

void apagarTodosLeds() {
  digitalWrite(PIN_LED_VERDE,    LOW);
  digitalWrite(PIN_LED_AMARELO,  LOW);
  digitalWrite(PIN_LED_VERMELHO, LOW);
}

// Acende o LED do comando recebido. Retorna false se o comando for inválido.
bool aplicarComando(const String& comando) {
  if (comando == "verde") {
    apagarTodosLeds();
    digitalWrite(PIN_LED_VERDE, HIGH);
  } else if (comando == "amarelo") {
    apagarTodosLeds();
    digitalWrite(PIN_LED_AMARELO, HIGH);
  } else if (comando == "vermelho") {
    apagarTodosLeds();
    digitalWrite(PIN_LED_VERMELHO, HIGH);
  } else {
    return false;
  }
  return true;
}

// Chamada sempre que chega uma mensagem em um tópico assinado
void callback(char* topic, byte* payload, unsigned int length) {
  String comando;
  for (unsigned int i = 0; i < length; i++) comando += (char)payload[i];
  comando.trim();
  comando.toLowerCase();

  Serial.print("Comando recebido: ");
  Serial.println(comando);

  if (aplicarComando(comando)) {
    mqtt.publish(topicoConfirmacao, comando.c_str());
    Serial.println("Comando executado e confirmado.");
  } else {
    Serial.println("Comando desconhecido, ignorado.");
  }
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
      mqtt.subscribe(topicoStatus, 1); // recebe o último status retido
    } else {
      Serial.print(" falhou, rc=");
      Serial.print(mqtt.state());
      Serial.println(". Nova tentativa em 2s");
      delay(2000);
    }
  }
}

void montarTopicos() {
  snprintf(topicoStatus,      sizeof(topicoStatus),      "%s/%s/atuador/status",      PREFIXO, ID_BUEIRO);
  snprintf(topicoConfirmacao, sizeof(topicoConfirmacao), "%s/%s/atuador/confirmacao", PREFIXO, ID_BUEIRO);
  snprintf(topicoConexao,     sizeof(topicoConexao),     "%s/%s/atuador/conexao",     PREFIXO, ID_BUEIRO);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_VERDE,    OUTPUT);
  pinMode(PIN_LED_AMARELO,  OUTPUT);
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  apagarTodosLeds();

  montarTopicos();
  conectarWiFi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(callback);
  mqtt.setKeepAlive(60); // tolera melhor a lentidão da simulação
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) conectarWiFi();
  if (!mqtt.connected()) conectarMQTT();
  mqtt.loop();
}
