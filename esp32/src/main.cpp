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
#define BTN2 14       // Button 2 (GPIO14)
#define BTN3 13       // Button 3 (GPIO13)
#define BAT_PIN 34     // Battery ADC (GPIO34 - ADC1, sem conflito com WiFi)

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
long lastStateChangeTime = 0;  // Para feedback visual melhorado da pisca
const long EDIT_TIMEOUT = 30000; // 30 segundos (timeout edição)
const long LONG_PRESS_DURATION = 2000; // 2 segundos para long press

// Variables para detecção de botão
long btn2PressStart = -1;  // Quando pressionou Enter
bool btn2Pressed = false;   // Marcador de press anterior

// Debug: último valor RFID lido
String lastRfidValue = "";

// Variáveis para leitura RFID em modo Ord
String lastRFIDSuccess = "";      // Último RFID com sucesso na BD
String ordFromDatabase = "";      // Ordem devolvida da base de dados
long rfidReadStart = -1;          // Quando começou a tentar ler RFID
const long RFID_READ_TIMEOUT = 5000; // 5 segundos para primeira leitura
bool rfidReadingInProgress = false;   // Flag para saber se está em modo leitura contínua

// Variáveis para controlar display de mensagens Ord
enum OrdDisplayMode {
  ORD_DISPLAY_NORMAL = 0,    // Mostra valor fixo
  ORD_DISPLAY_READING = 1,   // Mostra "... a ler" a piscar
  ORD_DISPLAY_VERIFYING = 2, // Mostra "... verificar" a piscar
  ORD_DISPLAY_NOT_FOUND = 3, // Mostra "não existe ord" fixo (2 seg)
  ORD_DISPLAY_FOUND = 4      // Mostra valor encontrado a piscar
};
OrdDisplayMode ordDisplayMode = ORD_DISPLAY_NORMAL;
long ordNotFoundTime = -1;  // Quando iniciou exibição "não existe ord"
const long ORD_NOT_FOUND_DISPLAY_TIME = 2000; // 2 segundos

// Tracking de RFIDs já lidos para contagem de operadores
#define MAX_RFID_HISTORY 10
String rfidHistory[MAX_RFID_HISTORY];   // Último RFID lido
int rfidHistoryIndex = 0;
bool isNewRFID = false;  // Flag para saber se é novo RFID

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
  // Pisca a cada 500ms normalmente
  bool normalBlink = (millis() / 500) % 2;
  
  // Mas força a estar "ON" nos primeiros 250ms após mudança de estado
  // para melhorar feedback visual
  if ((millis() - lastStateChangeTime) < 250) {
    return true;  // Força ON nos primeiros 250ms
  }
  
  return normalBlink;
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
  
  // ===== LINHA 2: ESTAÇÃO =====
  // Em STATE_SELECT_FIELD: pisca o rótulo se selecionado
  // Em STATE_EDIT_VALUE: pisca o valor se selecionado
  
  if (editState == STATE_SELECT_FIELD) {
    // Modo seleção: pisca o rótulo
    if (selectedField == 0 && blink) {
      u8g2.drawStr(0, 20, "Est:");  // Pisca
    } else if (selectedField != 0) {
      u8g2.drawStr(0, 20, "Est:");  // Sempre visível se não selecionado
    }
    // Mostra o valor guardado (sempre visível)
    u8g2.drawStr(30, 20, varA == "" ? "---" : varA.c_str());
  } else if (editState == STATE_EDIT_VALUE && selectedField == 0) {
    // Modo edição de Est: pisca o valor
    u8g2.drawStr(0, 20, "Est:");  // Rótulo sempre visível
    if (blink) {
      u8g2.drawStr(30, 20, procurarEstacao(currentEstacaoIndex + 1).c_str());  // Pisca
    }
  } else {
    // Estado normal ou outro estado
    u8g2.drawStr(0, 20, "Est:");
    u8g2.drawStr(30, 20, varA == "" ? "---" : varA.c_str());
  }
  
  // ===== LINHA 3: ORDEM =====
  if (editState == STATE_SELECT_FIELD) {
    // Modo seleção: pisca o rótulo
    if (selectedField == 1 && blink) {
      u8g2.drawStr(0, 35, "Ord:");  // Pisca
    } else if (selectedField != 1) {
      u8g2.drawStr(0, 35, "Ord:");  // Sempre visível se não selecionado
    }
    // Mostra o valor guardado (sempre visível)
    u8g2.drawStr(30, 35, varB == "" ? "---" : varB.c_str());
  } else if (editState == STATE_EDIT_VALUE && selectedField == 1) {
    // Modo edição de Ord: comportamento depende do modo de display
    u8g2.drawStr(0, 35, "Ord:");  // Rótulo sempre visível
    
    String displayOrd = "";
    bool shouldDisplay = true;
    
    if (ordDisplayMode == ORD_DISPLAY_READING) {
      // Mostra "... a ler" a piscar
      displayOrd = blink ? "... a ler" : "";
    } else if (ordDisplayMode == ORD_DISPLAY_VERIFYING) {
      // Mostra "... verificar" a piscar
      displayOrd = blink ? "... verificar" : "";
    } else if (ordDisplayMode == ORD_DISPLAY_NOT_FOUND) {
      // Mostra "não existe ord" fixo (sem piscar)
      displayOrd = "nao existe";
      shouldDisplay = true;  // Sempre visível neste modo
    } else if (ordDisplayMode == ORD_DISPLAY_FOUND) {
      // Mostra o valor encontrado a piscar
      displayOrd = blink ? ordFromDatabase : "";
    } else {
      // ORD_DISPLAY_NORMAL
      displayOrd = ordFromDatabase.length() > 0 ? ordFromDatabase : "---";
      shouldDisplay = true;
    }
    
    if (shouldDisplay) {
      u8g2.drawStr(30, 35, displayOrd.c_str());
    }
  } else {
    // Estado normal ou outro estado
    u8g2.drawStr(0, 35, "Ord:");
    u8g2.drawStr(30, 35, varB == "" ? "---" : varB.c_str());
  }
  
  // ===== LINHA 4: OPERADORES =====
  if (editState == STATE_SELECT_FIELD) {
    // Modo seleção: pisca o rótulo
    if (selectedField == 2 && blink) {
      u8g2.drawStr(0, 50, "Op#:");  // Pisca
    } else if (selectedField != 2) {
      u8g2.drawStr(0, 50, "Op#:");  // Sempre visível se não selecionado
    }
    // Mostra o valor guardado (sempre visível)
    u8g2.drawStr(30, 50, varC == "" ? "---" : varC.c_str());
  } else if (editState == STATE_EDIT_VALUE && selectedField == 2) {
    // Modo edição de Op# (se implementado no futuro)
    u8g2.drawStr(0, 50, "Op#:");  // Rótulo sempre visível
    if (blink) {
      u8g2.drawStr(30, 50, varC == "" ? "---" : varC.c_str());  // Pisca
    }
  } else {
    // Estado normal ou outro estado
    u8g2.drawStr(0, 50, "Op#:");
    u8g2.drawStr(30, 50, varC == "" ? "---" : varC.c_str());
  }
  
  // Linha 5: Debug - Valor RFID lido
  u8g2.drawStr(0, 63, lastRfidValue.c_str());
  
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
  int h = 6;  // Altura do símbolo
  
  // Corpo da bateria
  u8g2.drawFrame(x, y, w, h);
  // Ponta da bateria
  u8g2.drawBox(x + w, y + 1, 2, 4);
  // Preenchimento interno
  if (percentage > 0) {
    int fill = map(percentage, 0, 100, 0, w - 2);
    u8g2.drawBox(x + 1, y + 1, fill, h - 2);
  }
  
  // Mostra percentagem ao lado do símbolo (fonte pequena)
  u8g2.setFont(u8g2_font_tom_thumb_4x6_mr);  // Fonte pequena para percentagem
  char batStr[5];
  sprintf(batStr, "%d%%", percentage);
  u8g2.drawStr(x + w + 6, y + 6, batStr);  // Alinhado à direita do símbolo
  
  // Restaura a fonte anterior para o resto
  u8g2.setFont(u8g2_font_ncenB08_tr);
}

// Retorna a porcentagem da bateria
int getBatteryPercentage() {
  // O divisor de tensão divide por 2.
  // Bateria 0% -> 3.8V -> ADC vê 1.9V (1900mV)
  // Bateria 100% -> 4.1V -> ADC vê 2.05V (2050mV)

  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogReadMilliVolts(BAT_PIN);
    delay(5);
  }
  int avgMv = sum / 10;

  // Limites de segurança
  if (avgMv < 500 || avgMv > 2300)
    return 0;

  // Mapeamento: 1900mV (0%) a 2050mV (100%)
  int percentage = map(avgMv, 1900, 2050, 0, 100);
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

  // Remove espaços em branco antes e depois
  filterVal.trim();

  HTTPClient http;
  String url = String(supabase_url) + "/rest/v1/" + table + "?" + filterCol +
               "=eq." + filterVal;

  // DEBUG: mostra a URL construída
  Serial.print("[DB] URL: ");
  Serial.println(url);
  Serial.print("[DB] Procurando: ");
  Serial.print(filterCol);
  Serial.print("=");
  Serial.println(filterVal);

  http.begin(url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + String(supabase_key));

  int httpCode = http.GET();
  String result = "Nao encontrado";

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.print("[DB] Resposta 200: ");
    Serial.println(payload);
    
    JsonDocument doc;
    deserializeJson(doc, payload);

    if (doc.size() > 0) {
      result = doc[0][targetCol].as<String>();
      Serial.print("[DB] Resultado encontrado: ");
      Serial.println(result);
    } else {
      Serial.println("[DB] Nenhum resultado na resposta JSON");
    }
  } else {
    String payload = http.getString();
    Serial.print("Erro GET em " + table + ": ");
    Serial.println(httpCode);
    Serial.print("[DB] Resposta: ");
    Serial.println(payload);
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

// ===== FUNÇÕES DE LEITURA RFID =====
// Lê um cartão RFID e retorna o UID como string hexadecimal
String readRFIDCard() {
  uint8_t success;
  uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0};
  uint8_t uidLength;

  // Tenta ler um cartão passivo (timeout de 100ms para não travar)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100);

  if (success) {
    String rfidValue = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) rfidValue += "0";
      rfidValue += String(uid[i], HEX);
    }
    Serial.print("[RFID] UID lido: ");
    Serial.println(rfidValue);
    return rfidValue;
  }

  return "";  // Nada lido
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

  // ===== SISTEMA DE EDIÇÃO NOVO =====
  
  // Detecta SHORT PRESS no Enter (BTN2) - apenas ao libertar
  if (btn2State == LOW && !btn2Pressed) {
    btn2Pressed = true;  // Marca como pressionado
  }
  
  // Processa a libertação da tecla (SHORT PRESS)
  if (btn2State == HIGH && btn2Pressed) {
    btn2Pressed = false;  // Reset
    
    if (editState == STATE_NORMAL) {
      // Entra em modo seleção de campo
      editState = STATE_SELECT_FIELD;
      selectedField = 0;  // Começa em Est
      currentEstacaoIndex = procurarIndiceEstacao(varA);
      editModeStart = millis();
      lastStateChangeTime = millis();
      rfidReadingInProgress = false;
      lastRFIDSuccess = "";
      Serial.println("[EDIT] Entrou em modo seleção de campo (Est)");
    } else if (editState == STATE_SELECT_FIELD) {
      // Confirmação do campo selecionado
      if (selectedField == 0) {
        // Est: entra em EDIT_VALUE com navegação
        editState = STATE_EDIT_VALUE;
        editModeStart = millis();
        lastStateChangeTime = millis();
        Serial.println("[EDIT] Entrou em modo edição de Estação");
      } else if (selectedField == 1) {
        // Ord: inicia leitura RFID contínua
        editState = STATE_EDIT_VALUE;
        rfidReadingInProgress = true;
        rfidReadStart = millis();
        lastRFIDSuccess = "";
        ordFromDatabase = "";
        ordDisplayMode = ORD_DISPLAY_READING;  // Começa com "... a ler"
        editModeStart = millis();
        lastStateChangeTime = millis();
        Serial.println("[EDIT] Entrou em modo edição de Ordem - Iniciando leitura RFID");
      } else if (selectedField == 2) {
        // Op#: para futuro (ainda não implementado)
        editState = STATE_EDIT_VALUE;
        editModeStart = millis();
        lastStateChangeTime = millis();
        Serial.println("[EDIT] Entrou em modo edição de Op#");
      }
    } else if (editState == STATE_EDIT_VALUE) {
      // Confirmação do valor
      if (selectedField == 0) {
        // Est: confirma valor e sai de edição
        String estacaoSelecionada = procurarEstacao(currentEstacaoIndex + 1);
        updateVariable('A', estacaoSelecionada);
        updateVariable('C', "0");
        for (int i = 0; i < MAX_RFID_HISTORY; i++) {
          rfidHistory[i] = "";
        }
        rfidHistoryIndex = 0;
        editState = STATE_NORMAL;
        rfidReadingInProgress = false;
        Serial.print("[EDIT] Confirmou estação: ");
        Serial.println(estacaoSelecionada);
        Serial.println("[EDIT] Voltou ao modo normal");
      } else if (selectedField == 1) {
        // Ord: se BD encontrou valor, confirma e sai; se não encontrou, volta a ler
        rfidReadingInProgress = false;
        if (ordFromDatabase.length() > 0) {
          // Confirma o valor encontrado e sai de edição
          updateVariable('B', ordFromDatabase);
          updateVariable('C', "0");
          for (int i = 0; i < MAX_RFID_HISTORY; i++) {
            rfidHistory[i] = "";
          }
          rfidHistoryIndex = 0;
          editState = STATE_NORMAL;
          ordDisplayMode = ORD_DISPLAY_NORMAL;
          Serial.print("[EDIT] Confirmou ordem: ");
          Serial.println(ordFromDatabase);
          Serial.println("[EDIT] Voltou ao modo normal");
        } else {
          // Nenhum valor encontrado ainda - volta a ler
          rfidReadingInProgress = true;
          rfidReadStart = millis();
          ordDisplayMode = ORD_DISPLAY_READING;
          Serial.println("[EDIT] Nenhuma ordem encontrada, voltou a ler");
        }
      } else if (selectedField == 2) {
        // Op#: confirma e sai (por implementar)
        editState = STATE_NORMAL;
        rfidReadingInProgress = false;
        Serial.println("[EDIT] Confirmou Op#");
      }
    }
  }
  
  // BTN1 (Cima) - navega para trás no array
  if (btn1State == LOW && btn1LastState == HIGH) {
    if (editState != STATE_NORMAL) {
      if (editState == STATE_SELECT_FIELD) {
        // Roda entre Est, Ord, Op#
        selectedField = (selectedField - 1 + 3) % 3;
        editModeStart = millis();  // Reset timeout
        lastStateChangeTime = millis();  // Feedback visual
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
        lastStateChangeTime = millis();  // Feedback visual
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

  // ===== LEITURA RFID CONTÍNUA PARA CAMPO ORD =====
  if (rfidReadingInProgress && selectedField == 1 && editState == STATE_EDIT_VALUE) {
    // Tenta ler um cartão RFID de forma contínua
    String rfidRead = readRFIDCard();
    
    if (rfidRead.length() > 0) {
      // Cartão lido com sucesso - guarda e processa
      if (lastRFIDSuccess != rfidRead) {
        // Novo RFID (diferente do anterior)
        lastRFIDSuccess = rfidRead;
        ordDisplayMode = ORD_DISPLAY_VERIFYING;  // Muda para "... verificar"
        Serial.print("[RFID] Cartão lido: ");
        Serial.println(rfidRead);
        
        // Verifica se é um RFID novo (nunca lido antes)
        isNewRFID = true;
        for (int i = 0; i < MAX_RFID_HISTORY; i++) {
          if (rfidHistory[i] == rfidRead) {
            isNewRFID = false;
            break;
          }
        }
        
        // Tenta fazer lookup na tabela "barcos"
        Serial.print("[DB] Procurando na tabela 'barcos' com rfid=");
        Serial.println(rfidRead);
        
        String result = supabaseGenericLookup("barcos", "rfid", rfidRead, "ordem_fabrico");
        
        if (result != "Nao encontrado" && result != "Erro: Offline") {
          // Ordem encontrada!
          ordFromDatabase = result;
          ordDisplayMode = ORD_DISPLAY_FOUND;  // Muda para mostrar valor a piscar
          Serial.print("[DB] Ordem encontrada: ");
          Serial.println(result);
          
          // Se é novo RFID, incrementa contador de operadores
          if (isNewRFID) {
            int currentCount = varC.toInt();
            currentCount++;
            updateVariable('C', String(currentCount));
            
            // Adiciona à history
            rfidHistory[rfidHistoryIndex] = rfidRead;
            rfidHistoryIndex = (rfidHistoryIndex + 1) % MAX_RFID_HISTORY;
            
            Serial.print("[RFID] Novo operador detectado! varC = ");
            Serial.println(currentCount);
          } else {
            Serial.println("[RFID] RFID já lido antes - varC não incrementa");
          }
        } else {
          // Ordem não encontrada
          ordDisplayMode = ORD_DISPLAY_NOT_FOUND;  // Muda para "não existe ord"
          ordNotFoundTime = millis();  // Marca o tempo de início
          ordFromDatabase = "";  // Limpa o valor
          Serial.println("[DB] RFID não encontrado na BD");
        }
      }
      // Continua tentando ler (não para após o 1º)
      editModeStart = millis();  // Reset de timeout a cada leitura bem-sucedida
    }
  }
  
  // ===== CONTROLE DE EXIBIÇÃO "NÃO EXISTE ORD" =====
  // Se estamos exibindo "não existe ord", aguarda 2 segundos e volta a "... a ler"
  if (ordDisplayMode == ORD_DISPLAY_NOT_FOUND && ordNotFoundTime != -1) {
    if ((millis() - ordNotFoundTime) > ORD_NOT_FOUND_DISPLAY_TIME) {
      // Passou 2 segundos, volta a tentar ler
      ordDisplayMode = ORD_DISPLAY_READING;
      ordFromDatabase = "";
      lastRFIDSuccess = "";
      ordNotFoundTime = -1;
      Serial.println("[RFID] Voltou a tentar ler após 2 segundos");
    }
  }

  
  // Verifica timeout de edição (30 segundos)
  if (editState != STATE_NORMAL && (millis() - editModeStart) > EDIT_TIMEOUT) {
    editState = STATE_NORMAL;
    rfidReadingInProgress = false;
    lastRFIDSuccess = "";
    ordFromDatabase = "";
    // Mantém lastRfidValue com o último valor para feedback ao sair
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