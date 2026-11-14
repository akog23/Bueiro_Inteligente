/*
 * ============================================================
 * Projeto: Bueiro Inteligente
 * Universidade Presbiteriana Mackenzie
 * Disciplina: Objetos Inteligentes Conectados
 * ------------------------------------------------------------
 * Dispositivo: Nó Atuador (ESP32 + servo motor da grade autolimpante)
 *
 * Função: receber do Node-RED, via MQTT, o comando de limpeza,
 * movimentar a grade do bueiro com um servo motor (duas passadas
 * de 0° a 180°) para soltar os resíduos e publicar a confirmação
 * do início e do fim da limpeza.
 *
 * Pinagem (diagram.json):
 *   Servo PWM -> GPIO 13 (D13)
 *   Servo V+  -> VIN (5 V)
 *   Servo GND -> GND
 *
 * Tópicos:
 *   Assina:  mackenzie/bueiro-inteligente/bueiro01/atuador/comando      ("limpar" ou "repouso")
 *   Publica: mackenzie/bueiro-inteligente/bueiro01/atuador/confirmacao  ("limpando", "limpeza_concluida", "repouso")
 *   Publica: mackenzie/bueiro-inteligente/bueiro01/atuador/conexao      (LWT: "online"/"offline")
 * ============================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

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

char topicoComando[80];
char topicoConfirmacao[80];
char topicoConexao[80];

// ---------- Servo da grade ----------
const int PIN_SERVO       = 13;
const int ANGULO_REPOUSO  = 0;
const int ANGULO_FINAL    = 180;
const int PASSADAS        = 2;   // quantas vezes a grade vai e volta
const int ATRASO_PASSO_MS = 10;  // velocidade do movimento (ms por grau)

Servo servoGrade;
bool limpando = false;

WiFiClient espClient;
PubSubClient mqtt(espClient);

// Move o servo grau a grau, mantendo a conexão MQTT ativa durante o movimento
void moverServo(int de, int ate) {
  int passo = (ate > de) ? 1 : -1;
  for (int angulo = de; angulo != ate + passo; angulo += passo) {
    servoGrade.write(angulo);
    delay(ATRASO_PASSO_MS);
    mqtt.loop();
  }
}

void executarLimpeza() {
  limpando = true;
  mqtt.publish(topicoConfirmacao, "limpando");
  Serial.println("Iniciando limpeza da grade...");

  for (int i = 1; i <= PASSADAS; i++) {
    Serial.print("Passada ");
    Serial.println(i);
    moverServo(ANGULO_REPOUSO, ANGULO_FINAL);
    moverServo(ANGULO_FINAL, ANGULO_REPOUSO);
  }

  mqtt.publish(topicoConfirmacao, "limpeza_concluida");
  Serial.println("Limpeza concluída.");
  limpando = false;
}

// Chamada sempre que chega uma mensagem em um tópico assinado
void callback(char* topic, byte* payload, unsigned int length) {
  String comando;
  for (unsigned int i = 0; i < length; i++) comando += (char)payload[i];
  comando.trim();
  comando.toLowerCase();

  Serial.print("Comando recebido: ");
  Serial.println(comando);

  if (limpando) {
    Serial.println("Limpeza em andamento, comando ignorado.");
    return;
  }

  if (comando == "limpar") {
    executarLimpeza();
  } else if (comando == "repouso") {
    servoGrade.write(ANGULO_REPOUSO);
    mqtt.publish(topicoConfirmacao, "repouso");
    Serial.println("Grade em repouso.");
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
      mqtt.subscribe(topicoComando, 1);
    } else {
      Serial.print(" falhou, rc=");
      Serial.print(mqtt.state());
      Serial.println(". Nova tentativa em 2s");
      delay(2000);
    }
  }
}

void montarTopicos() {
  snprintf(topicoComando,     sizeof(topicoComando),     "%s/%s/atuador/comando",     PREFIXO, ID_BUEIRO);
  snprintf(topicoConfirmacao, sizeof(topicoConfirmacao), "%s/%s/atuador/confirmacao", PREFIXO, ID_BUEIRO);
  snprintf(topicoConexao,     sizeof(topicoConexao),     "%s/%s/atuador/conexao",     PREFIXO, ID_BUEIRO);
}

void setup() {
  Serial.begin(115200);

  servoGrade.setPeriodHertz(50);           // servo padrão: 50 Hz
  servoGrade.attach(PIN_SERVO, 500, 2400); // pulso mínimo e máximo (µs)
  servoGrade.write(ANGULO_REPOUSO);

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
