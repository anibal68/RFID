#include <Adafruit_PN532.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Preferences.h>
#include "../include/estacoes.h"

// Configuração para OLED 1.3" (geralmente SH1106)
// Se o seu display for SSD1306, mude SH1106 para SSD1306
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// ESP32-S3 Pin Configuration
#define SDA_PIN 16    // I2C SDA (GPIO16)
#define SCL_PIN 17    // I2C SCL (GPIO17)
#define BTN1 12       // Button 1 (GPIO12) - Wakeup button
#define BTN2 13       // Button 2 (GPIO13)
#define BTN3 14       // Button 3 (GPIO14)
#define BAT_PIN 4     // Battery ADC (GPIO4)

// Instância do PN532 via I2C (usando pinos dummy para IRQ e Reset para evitar
// conflito com SDA/SCL)
Adafruit_PN532 nfc(3, 2);

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

// WiFi Configuration
const char* ssid = "Vodafone-428F66 2,4G";
const char* password = "yQQA3Af3GY";

// Supabase Credentials - PLACEHOLDERS
const char *supabase_url = "https://zxjwkvepgqfgkajhuyaf.supabase.co";
const char *supabase_key =
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZS"
    "IsInJlZiI6Inp4andrdmVwZ3FmZ2thamh1eWFmIiwicm9sZSI6ImFub24iLC"
    "JpYXQiOjE3Njk4OTM0NzUsImV4cCI6MjA4NTQ2OTQ3NX0._"
    "kYCojqYUn7SrAfausdkgqfirTlYLtj3hdEae2jMmFM";

#include <time.h>

int lastPressed = -1;

long lastActivityTime = 0;
const long sleepTimeout = 30000; // 30 segundos
long lastNfcPollTime = 0;        // Para evitar lag no loop

// ===== PERSISTÊNCIA DE DADOS (NVS) =====
Preferences preferences;

// Variáveis principais com persistência
String varA = "";  // Estação de trabalho
String varB = "";  // Ordem de produção
String varC = "";  // Número de operadores
String varD = "";  // Variável extra 4
String varE = "";  // Variável extra 5

// Array 2D: 50 linhas x 2 colunas (nomes e números)
#define MAX_OPERATORS 50
struct Operator {
  String nome;
  String numero;
};
Operator operadores[MAX_OPERATORS];

// Configuração NTP
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0; // UTC
const int daylightOffset_sec = 0;

// Timers para evitar lag no loop principal
long lastBatteryCheck = 0;
int cachedBattery = 0;
long lastRssiCheck = 0;
int cachedRssi = -100;
const long checkInterval = 10000; // 10 segundos

// ===== SISTEMA DE EDIÇÃO =====
enum EditState {
  STATE_NORMAL = 0,      // Modo normal
  STATE_SELECT_FIELD = 1, // Seleção de campo (Est, Ord, Op# piscando)
  STATE_EDIT_VALUE = 2    // Edição de valor (variável piscando)
};

EditState editState = STATE_NORMAL;
int selectedField = 0;  // 0=Est, 1=Ord, 2=Op#
int currentEstacaoIndex = 0;  // Índice no array de estações (0-49)

long editModeStart = 0;  // Quando entrou em modo edição
const long EDIT_TIMEOUT = 30000; // 30 segundos (timeout edição)
const long LONG_PRESS_DURATION = 2000; // 2 segundos para long press

// Variables para detecção de long press
long btn2PressStart = -1;  // Quando pressionou Enter
bool btn2LongPressDetected = false;

void goToSleep() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 10, "Entrando em Sleep...");
  u8g2.drawStr(0, 30, "Acorde pelo G1");
  u8g2.sendBuffer();
  delay(1000);
  u8g2.setPowerSave(1); // Desliga o display para economizar energia

  // Configura despertar por GPIO12 (LOW level) - ESP32 clássico
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 0); // 0 = LOW level

  Serial.println("Indo para Deep Sleep agora...");
  Serial.flush();
  esp_deep_sleep_start();
}

// Retorna a hora formatada HH:mm dd/mm
String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "--:-- --/--";
  }
  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%H:%M %d/%m", &timeinfo);
  return String(timeStr);
}

// ===== FUNÇÕES DE PERSISTÊNCIA (NVS) =====

void initNVS() {
  preferences.begin("rfid_config", false); // false = read-write mode
  
  // Carrega variáveis do NVS
  varA = preferences.getString("varA", "");
  varB = preferences.getString("varB", "");
  varC = preferences.getString("varC", "");
  varD = preferences.getString("varD", "");
  varE = preferences.getString("varE", "");
  
  Serial.println("[NVS] Variáveis carregadas:");
  Serial.print("  A: "); Serial.println(varA);
  Serial.print("  B: "); Serial.println(varB);
  Serial.print("  C: "); Serial.println(varC);
  Serial.print("  D: "); Serial.println(varD);
  Serial.print("  E: "); Serial.println(varE);
}

void saveNVS() {
  preferences.putString("varA", varA);
  preferences.putString("varB", varB);
  preferences.putString("varC", varC);
  preferences.putString("varD", varD);
  preferences.putString("varE", varE);
  Serial.println("[NVS] Variáveis salvas");
}

void updateVariable(char varName, String value) {
  switch(varName) {
    case 'A': varA = value; break;
    case 'B': varB = value; break;
    case 'C': varC = value; break;
    case 'D': varD = value; break;
    case 'E': varE = value; break;
  }
  saveNVS();
  Serial.print("[Updated] Var "); Serial.print(varName); Serial.print(": "); Serial.println(value);
}

// ===== FUNÇÕES DE DISPLAY =====

// Forward declarations
void drawBatteryIcon(int percentage);
void drawWiFiIcon(int rssi);

// Função auxiliar para piscar texto
bool shouldBlink() {
  return (millis() / 500) % 2;  // Pisca a cada 500ms
}

// Função para desenhar a tela de edição
void drawEditScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  
  // Linha 1: Bateria e WiFi
  drawBatteryIcon(cachedBattery);
  drawWiFiIcon(cachedRssi);
  
  // Piscante do campo selecionado
  bool blink = shouldBlink();
  
  // Linha 2: Estação (pisca se selecionado)
  if (selectedField == 0 && blink) {
    u8g2.drawStr(0, 20, "[Est: ]");
    if (editState == STATE_EDIT_VALUE) {
      // Mostra estação actual em edição
      u8g2.drawStr(42, 20, procurarEstacao(currentEstacaoIndex + 1).c_str());
    } else {
      u8g2.drawStr(30, 20, ">>>"); // Sinal de que pode editar
    }
  } else {
    u8g2.drawStr(0, 20, "Est:");
    if (editState == STATE_EDIT_VALUE && selectedField == 0) {
      u8g2.drawStr(30, 20, procurarEstacao(currentEstacaoIndex + 1).c_str());
    } else {
      u8g2.drawStr(30, 20, varA == "" ? "---" : varA.c_str());
    }
  }
  
  // Linha 3: Ordem (pisca se selecionado)
  if (selectedField == 1 && blink) {
    u8g2.drawStr(0, 35, "[Ord:]");
  } else {
    u8g2.drawStr(0, 35, "Ord:");
    u8g2.drawStr(30, 35, varB == "" ? "---" : varB.c_str());
  }
  
  // Linha 4: Operadores (pisca se selecionado)
  if (selectedField == 2 && blink) {
    u8g2.drawStr(0, 50, "[Op#:]");
  } else {
    u8g2.drawStr(0, 50, "Op#:");
    u8g2.drawStr(30, 50, varC == "" ? "---" : varC.c_str());
  }
  
  // Linha 5: Teste
  u8g2.drawStr(0, 63, "Teste");
  
  u8g2.sendBuffer();
}

void drawMainScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  
  // Linha 1: Bateria e WiFi
  drawBatteryIcon(cachedBattery);
  drawWiFiIcon(cachedRssi);
  
  // Linha 2: Estação
  u8g2.drawStr(0, 20, "Est:");
  u8g2.drawStr(30, 20, varA == "" ? "---" : varA.c_str());
  
  // Linha 3: Ordem de Produção
  u8g2.drawStr(0, 35, "Ord:");
  u8g2.drawStr(30, 35, varB == "" ? "---" : varB.c_str());
  
  // Linha 4: Número de Operadores
  u8g2.drawStr(0, 50, "Op#:");
  u8g2.drawStr(30, 50, varC == "" ? "---" : varC.c_str());
  
  // Linha 5: Teste
  u8g2.drawStr(0, 63, "Teste");
  
  u8g2.sendBuffer();
}

// Desenha ícone de WiFi no topo direito
// Desenha ícone de bateria proporcional
void drawBatteryIcon(int percentage) {
  int x = 0;
  int y = 0;
  int w = 14;
  int h = 6;  // Reduzido de 10 para 6
  // Corpo da bateria
  u8g2.drawFrame(x, y, w, h);
  // Ponta da bateria
  u8g2.drawBox(x + w, y + 1, 2, 4);
  // Preenchimento interno
  if (percentage > 0) {
    int fill = map(percentage, 0, 100, 0, w - 2);
    u8g2.drawBox(x + 1, y + 1, fill, h - 2);
  }
}

// Retorna a porcentagem da bateria
int getBatteryPercentage() {
  // O divisor de tensão divide por 2.
  // Bateria 4.2V -> ADC vê 2.1V
  // No ESP32-C3, o ADC tem range de 0-2500mV com atenuação de 11dB (padrão)

  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogReadMilliVolts(BAT_PIN);
    delay(5);
  }
  int avgMv = sum / 10;

  // Tensão da bateria = avgMv * 2 (devido ao divisor 1/2)
  // Mas como o usuário disse que 100% seria 2.1V no pino, usamos 2100mV como
  // referência. LiPo range: 3.0V (vazio) a 4.2V (cheio) No pino ADC (1/2): 1.5V
  // (1500mV) a 2.1V (2100mV)

  // Se o valor for absurdamente alto (acima de 2.3V) ou muito baixo,
  // consideramos que não há bateria ou o pino está flutuando.
  if (avgMv < 500 || avgMv > 2300)
    return 0;

  int percentage = map(avgMv, 1500, 2100, 0, 100);
  if (percentage > 100)
    percentage = 100;
  if (percentage < 0)
    percentage = 0;

  return percentage;
}

void drawWiFiIcon(int rssi) {
  int x = 110;
  int y = 0;  // Movido para cima (alinhado com bateria)
  if (WiFi.status() != WL_CONNECTED) {
    u8g2.drawStr(x, y + 8, "X");
    return;
  }
  // Reduzido - apenas 3 linhas em vez de 4
  if (rssi > -90)
    u8g2.drawBox(x, y + 2, 2, 2);
  if (rssi > -80)
    u8g2.drawBox(x + 3, y, 2, 4);
  if (rssi > -70)
    u8g2.drawBox(x + 6, y - 2, 2, 4);
}

// Helper to decode WiFi status
const char* decodeWiFiStatus(int status) {
  switch (status) {
    case WL_IDLE_STATUS: return "Idle";
    case WL_NO_SSID_AVAIL: return "No SSID";
    case WL_SCAN_COMPLETED: return "Scan Done";
    case WL_CONNECTED: return "Connected";
    case WL_CONNECT_FAILED: return "Connect Failed";
    case WL_CONNECTION_LOST: return "Lost";
    case WL_DISCONNECTED: return "Disconnected";
    default: return "Unknown";
  }
}

// WiFi event handler
void WiFiEvent(WiFiEvent_t event) {
  Serial.print("[WiFi-event] event: ");
  Serial.println(event);
  switch (event) {
    case SYSTEM_EVENT_WIFI_READY:
      Serial.println("WiFi interface ready");
      break;
    case SYSTEM_EVENT_STA_START:
      Serial.println("WiFi client started");
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      Serial.println("Connected to access point");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("Obtained IP address");
      Serial.print("  IP: ");
      Serial.println(WiFi.localIP());
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("Disconnected from WiFi");
      break;
    default:
      break;
  }
}

void connectToWiFi() {
  Serial.print("[WiFi] Conectando à rede: ");
  Serial.println(ssid);
  Serial.print("[WiFi] MAC: ");
  Serial.println(WiFi.macAddress());
  
  // Configurações de estabilidade
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setAutoConnect(true);
  WiFi.persistent(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.setHostname("ESP32-RFID");
  WiFi.onEvent(WiFiEvent);
  
  // Garante que começa desconectado
  WiFi.disconnect(false);
  delay(1000);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  int maxAttempts = 20;
  unsigned long connectionStart = millis();
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    if (attempts % 10 == 0) {
      unsigned long elapsed = millis() - connectionStart;
      Serial.print("[");
      Serial.print(elapsed / 1000);
      Serial.print("s]");
    }
  }
  Serial.println("");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] ✓ Conectado!");
    Serial.print("  IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("  RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    cachedRssi = WiFi.RSSI();
    
    // Configura NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    Serial.println("[WiFi] ✗ Falha ao conectar");
    Serial.print("  Status: ");
    Serial.print(WiFi.status());
    Serial.print(" (");
    Serial.print(decodeWiFiStatus(WiFi.status()));
    Serial.println(")");
  }
}

void initWiFi(bool isWakeup) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 10, "WiFi...");
  u8g2.sendBuffer();

  WiFi.mode(WIFI_STA);
  
  connectToWiFi();
}

// --- FUNÇÕES GENÉRICAS SUPABASE ---

// Busca genérica (VLOOKUP)
String supabaseGenericLookup(String table, String filterCol, String filterVal,
                             String targetCol) {
  if (WiFi.status() != WL_CONNECTED)
    return "Erro: Offline";

  HTTPClient http;
  String url = String(supabase_url) + "/rest/v1/" + table + "?" + filterCol +
               "=eq." + filterVal;

  http.begin(url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + String(supabase_key));

  int httpCode = http.GET();
  String result = "Nao encontrado";

  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    deserializeJson(doc, payload);

    if (doc.size() > 0) {
      result = doc[0][targetCol].as<String>();
    }
  } else {
    Serial.print("Erro GET em " + table + ": ");
    Serial.println(httpCode);
  }
  http.end();
  return result;
}

// Inserção genérica (POST)
bool supabaseGenericInsert(String table, JsonDocument data) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  HTTPClient http;
  String url = String(supabase_url) + "/rest/v1/" + table;

  http.begin(url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + String(supabase_key));
  http.addHeader("Content-Type", "application/json");

  String jsonPayload;
  serializeJson(data, jsonPayload);

  int httpCode = http.POST(jsonPayload);
  http.end();

  if (httpCode == 201) {
    Serial.println("POST em " + table + " sucesso!");
    return true;
  } else {
    Serial.print("Erro POST em " + table + ": ");
    Serial.println(httpCode);
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Inicializa Display primeiro mas fica em "silêncio" durante o WiFi
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  u8g2.setPowerSave(0);

  // Inicializa NVS (persistência de dados)
  initNVS();

  esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
  bool isWakeup = (wakeup_cause != ESP_SLEEP_WAKEUP_UNDEFINED);

  initWiFi(isWakeup);

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(BAT_PIN, INPUT);

  // PN532 apenas após WiFi (menos carga na bateria no arranque)
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (versiondata)
    nfc.SAMConfig();

  lastActivityTime = millis();

  // Debug: Lista as 50 estações disponíveis
  listarEstacoes();
}

// Estados para detecção de borda nos botões
bool btn1LastState = HIGH;
bool btn2LastState = HIGH;
bool btn3LastState = HIGH;

void loop() {
  // Serial.print("L"); // Debug rápido para ver se o loop corre

  // Lógica de detecção de borda (Trigger apenas no momento do clique)
  bool btn1State = digitalRead(BTN1);
  bool btn2State = digitalRead(BTN2);
  bool btn3State = digitalRead(BTN3);

  // ===== SISTEMA DE EDIÇÃO =====
  
  // Detecta LONG PRESS no Enter (BTN2)
  if (btn2State == LOW && btn2PressStart == -1) {
    btn2PressStart = millis();  // Marca o início do press
    btn2LongPressDetected = false;
  }
  
  if (btn2State == HIGH && btn2PressStart != -1) {
    long pressDuration = millis() - btn2PressStart;
    
    if (pressDuration > LONG_PRESS_DURATION) {
      // LONG PRESS - Entra em modo edição
      if (editState == STATE_NORMAL) {
        editState = STATE_SELECT_FIELD;
        selectedField = 0;  // Começa em Est
        currentEstacaoIndex = 0;  // Começa na 1ª estação
        editModeStart = millis();
        Serial.println("[EDIT] Entrou em modo edição");
      }
    } else {
      // SHORT PRESS - Confirma seleção
      if (editState == STATE_SELECT_FIELD) {
        editState = STATE_EDIT_VALUE;
        editModeStart = millis();
        Serial.println("[EDIT] Modo de edição da variável");
      } else if (editState == STATE_EDIT_VALUE) {
        // Confirma valor e sai
        if (selectedField == 0) {
          String estacaoSelecionada = procurarEstacao(currentEstacaoIndex + 1);
          updateVariable('A', estacaoSelecionada);
          Serial.print("[EDIT] Atribuiu Est: ");
          Serial.println(estacaoSelecionada);
        }
        editState = STATE_SELECT_FIELD;  // Volta a seleccionar campo
        editModeStart = millis();
      }
    }
    btn2PressStart = -1;  // Reset
  }
  
  // BTN1 (Cima) - navega para trás no array
  if (btn1State == LOW && btn1LastState == HIGH) {
    if (editState != STATE_NORMAL) {
      if (editState == STATE_SELECT_FIELD) {
        // Roda entre Est, Ord, Op#
        selectedField = (selectedField - 1 + 3) % 3;
        editModeStart = millis();  // Reset timeout
        Serial.print("[EDIT] Campo: ");
        Serial.println(selectedField);
      } else if (editState == STATE_EDIT_VALUE && selectedField == 0) {
        // Navega no array para trás
        currentEstacaoIndex = (currentEstacaoIndex - 1 + NUM_ESTACOES) % NUM_ESTACOES;
        editModeStart = millis();  // Reset timeout
        Serial.print("[EDIT] Estação anterior: ");
        Serial.println(procurarEstacao(currentEstacaoIndex + 1));
      }
    }
  }
  
  // BTN3 (Baixo) - navega para frente no array
  if (btn3State == LOW && btn3LastState == HIGH) {
    if (editState != STATE_NORMAL) {
      if (editState == STATE_SELECT_FIELD) {
        // Roda entre Est, Ord, Op#
        selectedField = (selectedField + 1) % 3;
        editModeStart = millis();  // Reset timeout
        Serial.print("[EDIT] Campo: ");
        Serial.println(selectedField);
      } else if (editState == STATE_EDIT_VALUE && selectedField == 0) {
        // Navega no array para frente
        currentEstacaoIndex = (currentEstacaoIndex + 1) % NUM_ESTACOES;
        editModeStart = millis();  // Reset timeout
        Serial.print("[EDIT] Próxima estação: ");
        Serial.println(procurarEstacao(currentEstacaoIndex + 1));
      }
    }
  }
  
  // Verifica timeout de edição (30 segundos)
  if (editState != STATE_NORMAL && (millis() - editModeStart) > EDIT_TIMEOUT) {
    editState = STATE_NORMAL;
    Serial.println("[EDIT] Saiu por timeout");
  }

  // Atualiza estados anteriores
  btn1LastState = btn1State;
  btn2LastState = btn2State;
  btn3LastState = btn3State;

  // Atualiza bateria e WiFi apenas a cada 10s para evitar lag
  if (millis() - lastBatteryCheck > checkInterval || lastBatteryCheck == 0) {
    cachedBattery = getBatteryPercentage();
    lastBatteryCheck = millis();
    if (WiFi.status() == WL_CONNECTED) {
      cachedRssi = WiFi.RSSI();
    }
  }

  // ===== EXIBE TELA APROPRIADA =====
  if (editState != STATE_NORMAL) {
    drawEditScreen();  // Tela de edição
  } else {
    drawMainScreen();  // Tela normal
  }

  // Loop infinito - sem sleep
  delay(100); // Pequeno delay para evitar travamentos
}