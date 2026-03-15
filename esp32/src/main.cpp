#include <Adafruit_PN532.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Preferences.h>
#include "driver/rtc_io.h"
#include "../include/estacoes.h"

// Configuração para OLED 1.3" (geralmente SH1106)
// Se o seu display for SSD1306, mude SH1106 para SSD1306
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// ESP32 Pin Configuration
#define SDA_PIN 16    // I2C SDA (GPIO16)
#define SCL_PIN 17    // I2C SCL (GPIO17)
#define BTN1 12       // Button 1 (GPIO12) - Wakeup button
#define BTN2 14       // Button 2 (GPIO14)
#define BTN3 13       // Button 3 (GPIO13)
#define BAT_PIN 34     // Battery ADC (GPIO34 - ADC1, sem conflito com WiFi)
#define SPEAKER_PIN 25 // Speaker PWM (GPIO25)

// Barcode Reader (Serial2 / UART)
#define BARCODE_RX 18  // GPIO18 - RX do leitor de barras
#define BARCODE_TX 19  // GPIO19 - TX do leitor de barras (opcional)
#define BARCODE_BAUD 9600  // Baud rate do leitor (ajustar conforme modelo)

// Instância do PN532 via I2C (usando pinos dummy para IRQ e Reset para evitar
// conflito com SDA/SCL)
Adafruit_PN532 nfc(3, 2);

// Leitor de código de barras via Serial2 (UART)
HardwareSerial barcodeSerial(2);
#define BARCODE_BUFFER_SIZE 64
char barcodeBuffer[BARCODE_BUFFER_SIZE];
int barcodeBufferIndex = 0;
long lastBarcodeCharTime = 0;
const long BARCODE_CHAR_TIMEOUT = 100; // 100ms timeout entre caracteres

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

// WiFi Configuration
const char* ssid = "Vodafone-428F66 2,4G";
const char* password = "yQQA3Af3GY";

// Supabase Credentials
const char *supabase_url = "https://efenntgldjizgyyttiiw.supabase.co";
const char *supabase_key =
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZS"
    "IsInJlZiI6ImVmZW5udGdsZGppemd5eXR0aWl3Iiwicm9sZSI6ImFub24iLC"
    "JpYXQiOjE3NzE2NzI5ODIsImV4cCI6MjA4NzI0ODk4Mn0."
    "1KPq3FBSo5Nn3qNQoHMEMvPBKBa1SYeI72QaUZMXSMc";

#include <time.h>

// Forward declarations
bool supabaseGenericInsert(String table, JsonDocument data);
String supabaseGenericLookup(String table, String filterCol, String filterVal, String targetCol);

int lastPressed = -1;

long lastActivityTime = 0;
const long sleepTimeout = 180000; // 3 minutos de inatividade (debug)
long lastNfcPollTime = 0;        // Para evitar lag no loop

// ===== PERSISTÊNCIA DE DADOS (NVS) =====
Preferences preferences;

// Variáveis principais com persistência
String varA = "";  // Estação de trabalho
String varB = "";  // Ordem de produção
String varC = "";  // Número de operadores
String varD = "";  // Variável extra 4
String varE = "";  // Variável extra 5

// Array 1D: Lista de RFIDs de operadores (sem repetições)
// Estrutura: apenas armazenar RFID + ID do operador
#define MAX_OPERATORS 50
struct OperadorNVS {
  String rfid;      // RFID do cartão (identificador único)
};
OperadorNVS operadoresNVS[MAX_OPERATORS];
int operadoresCount = 0;  // Número atual de operadores registados

// Configuração NTP
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0; // UTC
const int daylightOffset_sec = 0;

// Timers para evitar lag no loop principal
long lastBatteryCheck = 0;
int cachedBattery = 0;
long lastRssiCheck = 0;
int cachedRssi = -100;
const long checkInterval = 5000; // 5 segundos

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
  ORD_DISPLAY_FOUND = 4,     // Mostra valor encontrado a piscar
  ORD_DISPLAY_CONFIRM = 5,   // Mostra "terminar?" a piscar
  ORD_DISPLAY_WRITING = 6    // Mostra "a escrever" enquanto grava na BD
};
OrdDisplayMode ordDisplayMode = ORD_DISPLAY_NORMAL;
long ordNotFoundTime = -1;  // Quando iniciou exibição "não existe ord"
const long ORD_NOT_FOUND_DISPLAY_TIME = 2000; // 2 segundos

// Variáveis para controlar delay da lookup (deixar mostrar "... verificar")
bool ordLookupPending = false;  // Flag para aguardar antes de fazer lookup
long ordLookupStartTime = -1;   // Quando começou o delay
const long ORD_LOOKUP_DELAY = 500; // 500ms antes de fazer a lookup

// Variáveis para controle de ordem (in/out)
bool ordInitialized = false;    // Se ordem foi iniciada (lida 1ª vez)
String lastOrdRead = "";        // Última ordem lida
long ordConfirmStartTime = -1;  // Quando entrou em modo CONFIRM (para alternância)
bool ordReadOnceSuccess = false; // Flag para parar de ler após 1ª leitura bem-sucedida

// ===== OPERADORES (Op#) - Novos estados In/Out =====
enum OpMode {
  OP_MODE_IN = 0,   // Entrada de operador
  OP_MODE_OUT = 1   // Saída de operador
};
OpMode opMode = OP_MODE_IN;  // Modo atual (In ou Out)

// Variáveis para leitura RFID de operadores
String lastRFIDOperador = "";  // Último RFID lido para operador
String operadorFromDatabase = "";  // Nome do operador devolvido da BD
long opReadStart = -1;  // Quando começou a tentar ler operador

enum OpDisplayMode {
  OP_DISPLAY_NORMAL = 0,     // Mostra valor fixo
  OP_DISPLAY_MODE_SELECT = 1, // Mostra "In" ou "Out" a piscar
  OP_DISPLAY_IN_READING = 2,  // Mostra "in ... a ler"
  OP_DISPLAY_IN_SUCCESS = 3,  // Mostra "in sucesso"
  OP_DISPLAY_OUT_READING = 4, // Mostra "out ... a ler"
  OP_DISPLAY_OUT_SUCCESS = 5, // Mostra "out sucesso"
  OP_DISPLAY_NOT_FOUND = 6    // Mostra "op. nao encontrado"
};
OpDisplayMode opDisplayMode = OP_DISPLAY_NORMAL;
bool opReadingInProgress = false;

// Controle de exibição - mensagens persistentes na linha 5 (feedback)
String opGreetingName = "";  // Nome a mostrar no feedback
String opFeedbackMessage = "";  // Mensagem persistente da linha 5

// Variáveis para controlar delay da lookup
bool opLookupPending = false;
long opLookupStartTime = -1;
const long OP_LOOKUP_DELAY = 500;  // 500ms antes de fazer a lookup

// ===== ANDON (campo 3 - linha 4) =====
enum AndonDisplayMode {
  ANDON_DISPLAY_NORMAL = 0,        // Mostra valor fixo (varD)
  ANDON_DISPLAY_READING_OP = 1,    // Mostra "....operador" a piscar
  ANDON_DISPLAY_OP_NOT_FOUND = 2,  // Mostra "op nao encontr" (1s)
  ANDON_DISPLAY_SELECT_DEFECT = 3  // Navega lista de defeitos a piscar
};
AndonDisplayMode andonDisplayMode = ANDON_DISPLAY_NORMAL;
int currentAndonDefectIndex = 0;
String lastRFIDAndon = "";
bool andonReadingInProgress = false;
bool andonLookupPending = false;
long andonLookupStartTime = -1;
long andonNotFoundTime = -1;
const long ANDON_NOT_FOUND_DISPLAY_TIME = 1000;  // 1 segundo
const long ANDON_LOOKUP_DELAY = 500;
String andonOldVarD = "";  // Para detetar mudança de varD

// Lista de defeitos Andon (abreviados para OLED)
const char* ANDON_DEFECTS[] = {
  "Falta peca",
  "Avaria Equip",
  "Ajuste tec/qual",
  "Defeito",
  "Outros"
};
const int NUM_ANDON_DEFECTS = 5;

// ===== POSIÇÕES DE DISPLAY (Y) - 5 linhas =====
const int DISPLAY_LINE1_Y = 17;  // Linha 1: Est:
const int DISPLAY_LINE2_Y = 28;  // Linha 2: Ord:
const int DISPLAY_LINE3_Y = 39;  // Linha 3: Op#:
const int DISPLAY_LINE4_Y = 50;  // Linha 4: And: (Andon)
const int DISPLAY_LINE5_Y = 61;  // Linha 5: Feedback/Debug
const int DISPLAY_VALUE_X = 30;  // Posição X dos valores

// ===== FORWARD DECLARATIONS =====
void loadOperadoresNVS();
void saveOperadoresNVS();
bool operadorExists(String rfid);
bool addOperador(String rfid);
bool removeOperador(String rfid);

void goToSleep() {
  // Aguarda um pouco para os botões estarem definitivamente soltos (sem ruído)
  delay(500);
  
  // Verifica que todos os botões estão HIGH (soltos)
  if (digitalRead(BTN1) == LOW || digitalRead(BTN2) == LOW || digitalRead(BTN3) == LOW) {
    Serial.println("[SLEEP] Botão ainda pressionado, adiando sleep");
    lastActivityTime = millis();  // Adia o sleep
    return;
  }
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 10, "Entrando em Sleep...");
  u8g2.drawStr(0, 30, "Acorde por BTN1");
  u8g2.sendBuffer();
  delay(1000);
  u8g2.setPowerSave(1); // Desliga o display para economizar energia

  // Configura despertar por ext0 no BTN1 (GPIO12)
  rtc_gpio_pullup_en(GPIO_NUM_12);      // Pull-up ativo durante deep sleep
  rtc_gpio_pulldown_dis(GPIO_NUM_12);   // Desativa pull-down
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_12, 0);  // 0 = acorda quando LOW (botão premido)

  Serial.println("Indo para Deep Sleep agora...");
  Serial.println("Wake source: BTN1 (GPIO12) via ext0 com pull-up RTC");
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
  
  // Carrega as variáveis persistidas (permite continuar ordem no próximo dia)
  varA = preferences.getString("varA", "");
  varB = preferences.getString("varB", "");
  varC = preferences.getString("varC", "");
  varD = preferences.getString("varD", "Verde");
  varE = preferences.getString("varE", "");
  andonOldVarD = varD;  // Inicializa referência para detetar mudanças
  
  Serial.println("[NVS] Variáveis carregadas:");
  Serial.print("  A: "); Serial.println(varA);
  Serial.print("  B: "); Serial.println(varB);
  Serial.print("  C: "); Serial.println(varC);
  Serial.print("  D: "); Serial.println(varD);
  Serial.print("  E: "); Serial.println(varE);
  
  // Carrega lista de operadores
  loadOperadoresNVS();
}

// ===== ANDON: ROTINA CHAMADA QUANDO VARD MUDA =====
void onAndonChanged(String newValue) {
  Serial.print("[ANDON] VarD mudou para: ");
  Serial.println(newValue);
  
  // Grava alerta na tabela alertas_andon
  if (WiFi.status() == WL_CONNECTED && newValue != "Verde" && newValue.length() > 0) {
    JsonDocument doc;
    doc["operador_rfid"] = lastRFIDAndon;
    doc["tipo_alerta"] = newValue;
    doc["estacao"] = varA;  // Nome da estação
    // created_at é preenchido automaticamente pelo Supabase (DEFAULT now())
    
    if (supabaseGenericInsert("alertas_andon", doc)) {
      Serial.println("[ANDON] Alerta gravado na BD com sucesso");
    } else {
      Serial.println("[ANDON] Falha ao gravar alerta na BD");
    }
  }
}

// ===== FUNÇÕES DE PERSISTÊNCIA DE OPERADORES =====

void saveOperadoresNVS() {
  // Serializa armazenando cada RFID separado por "|"
  String serialized = "";
  for (int i = 0; i < operadoresCount; i++) {
    if (i > 0) serialized += "|";
    serialized += operadoresNVS[i].rfid;
  }
  
  preferences.putString("operadores", serialized);
  Serial.println("[NVS-OP] Operadores salvos:");
  Serial.print("  Total: "); Serial.println(operadoresCount);
  Serial.print("  String: "); Serial.println(serialized);
}

void loadOperadoresNVS() {
  String serialized = preferences.getString("operadores", "");
  operadoresCount = 0;
  
  if (serialized.length() == 0) {
    Serial.println("[NVS-OP] Nenhum operador carregado (primeiro acesso)");
    return;
  }
  
  // Deserializa - cada RFID separado por "|"
  int lastIdx = 0;
  
  while (lastIdx < serialized.length() && operadoresCount < MAX_OPERATORS) {
    int nextIdx = serialized.indexOf('|', lastIdx);
    if (nextIdx == -1) nextIdx = serialized.length();
    
    String rfid = serialized.substring(lastIdx, nextIdx);
    if (rfid.length() > 0) {
      operadoresNVS[operadoresCount].rfid = rfid;
      operadoresCount++;
      Serial.print("[NVS-OP] Carregado: ");
      Serial.println(rfid);
    }
    
    lastIdx = nextIdx + 1;
  }
  
  Serial.print("[NVS-OP] Total de operadores: ");
  Serial.println(operadoresCount);
}

// Verifica se um RFID já existe na lista de operadores
bool operadorExists(String rfid) {
  for (int i = 0; i < operadoresCount; i++) {
    if (operadoresNVS[i].rfid == rfid) {
      return true;
    }
  }
  return false;
}

// Adiciona um novo operador à lista (sem duplicatas)
bool addOperador(String rfid) {
  if (operadorExists(rfid)) {
    Serial.print("[NVS-OP] Operador já existe: ");
    Serial.println(rfid);
    return false;
  }
  
  if (operadoresCount >= MAX_OPERATORS) {
    Serial.println("[NVS-OP] Array de operadores cheio!");
    return false;
  }
  
  operadoresNVS[operadoresCount].rfid = rfid;
  operadoresCount++;
  
  saveOperadoresNVS();
  Serial.print("[NVS-OP] Novo operador adicionado: ");
  Serial.println(rfid);
  return true;
}

// Remove um operador da lista
bool removeOperador(String rfid) {
  for (int i = 0; i < operadoresCount; i++) {
    if (operadoresNVS[i].rfid == rfid) {
      // Remove deslocando os elementos após i
      for (int j = i; j < operadoresCount - 1; j++) {
        operadoresNVS[j] = operadoresNVS[j + 1];
      }
      operadoresCount--;
      
      saveOperadoresNVS();
      Serial.print("[NVS-OP] Operador removido: ");
      Serial.println(rfid);
      return true;
    }
  }
  
  Serial.print("[NVS-OP] Operador não encontrado para remover: ");
  Serial.println(rfid);
  return false;
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
  
  // ===== LINHA 1: ESTAÇÃO =====
  // Em STATE_SELECT_FIELD: pisca o rótulo se selecionado
  // Em STATE_EDIT_VALUE: pisca o valor se selecionado
  
  if (editState == STATE_SELECT_FIELD) {
    // Modo seleção: pisca o rótulo
    if (selectedField == 0 && blink) {
      u8g2.drawStr(0, DISPLAY_LINE1_Y, "Est:");  // Pisca
    } else if (selectedField != 0) {
      u8g2.drawStr(0, DISPLAY_LINE1_Y, "Est:");  // Sempre visível se não selecionado
    }
    // Mostra o valor guardado (sempre visível)
    u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE1_Y, varA == "" ? "---" : varA.c_str());
  } else if (editState == STATE_EDIT_VALUE && selectedField == 0) {
    // Modo edição de Est: pisca o valor
    u8g2.drawStr(0, DISPLAY_LINE1_Y, "Est:");  // Rótulo sempre visível
    if (blink) {
      u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE1_Y, estacoes[currentEstacaoIndex].nome.c_str());  // Pisca
    }
  } else {
    // Estado normal ou outro estado
    u8g2.drawStr(0, DISPLAY_LINE1_Y, "Est:");
    u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE1_Y, varA == "" ? "---" : varA.c_str());
  }
  
  // ===== LINHA 2: ORDEM =====
  if (editState == STATE_SELECT_FIELD) {
    // Modo seleção: pisca o rótulo
    if (selectedField == 1 && blink) {
      u8g2.drawStr(0, DISPLAY_LINE2_Y, "Ord:");  // Pisca
    } else if (selectedField != 1) {
      u8g2.drawStr(0, DISPLAY_LINE2_Y, "Ord:");  // Sempre visível se não selecionado
    }
    // Mostra o valor guardado (sempre visível)
    u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE2_Y, varB == "" ? "---" : varB.c_str());
  } else if (editState == STATE_EDIT_VALUE && selectedField == 1) {
    // Modo edição de Ord: comportamento depende do modo de display
    u8g2.drawStr(0, DISPLAY_LINE2_Y, "Ord:");  // Rótulo sempre visível
    
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
    } else if (ordDisplayMode == ORD_DISPLAY_CONFIRM) {
      // Mostra "terminar?" e ordem alternando (com período maior para ser bem visível)
      bool confirmBlink = ((millis() - ordConfirmStartTime) / 800) % 2;  // 800ms cada estado
      displayOrd = confirmBlink ? "terminar?" : ordFromDatabase;
    } else if (ordDisplayMode == ORD_DISPLAY_WRITING) {
      // Mostra "a escrever" enquanto grava na BD
      displayOrd = blink ? "a escrever" : "";
    } else {
      // ORD_DISPLAY_NORMAL
      displayOrd = ordFromDatabase.length() > 0 ? ordFromDatabase : "---";
      shouldDisplay = true;
    }
    
    if (shouldDisplay) {
      u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE2_Y, displayOrd.c_str());
    }
  } else {
    // Estado normal ou outro estado
    u8g2.drawStr(0, DISPLAY_LINE2_Y, "Ord:");
    u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE2_Y, varB == "" ? "---" : varB.c_str());
  }
  
  // ===== LINHA 3: OPERADORES =====
  if (editState == STATE_SELECT_FIELD) {
    // Modo seleção: pisca o rótulo
    if (selectedField == 2 && blink) {
      u8g2.drawStr(0, DISPLAY_LINE3_Y, "Op#:");  // Pisca
    } else if (selectedField != 2) {
      u8g2.drawStr(0, DISPLAY_LINE3_Y, "Op#:");  // Sempre visível se não selecionado
    }
    // Mostra o valor guardado (sempre visível)
    u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE3_Y, varC == "" ? "---" : varC.c_str());
  } else if (editState == STATE_EDIT_VALUE && selectedField == 2) {
    // Modo edição de Op#
    u8g2.drawStr(0, DISPLAY_LINE3_Y, "Op#:");  // Rótulo sempre visível
    
    // Mostra o estado apropriado conforme o modo
    if (opDisplayMode == OP_DISPLAY_MODE_SELECT) {
      // Seleção de In/Out: mostra "In" ou "Out" a piscar
      if (blink) {
        u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE3_Y, opMode == OP_MODE_IN ? "In" : "Out");  // Pisca
      }
    } 
    else if (opDisplayMode == OP_DISPLAY_IN_READING) {
      // Ciclo IN: mostra "in ... a ler" a piscar
      u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE3_Y, blink ? "in ... a ler" : "in");
    }
    else if (opDisplayMode == OP_DISPLAY_OUT_READING) {
      // Ciclo OUT: mostra "out ... a ler" a piscar
      u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE3_Y, blink ? "out ... a ler" : "out");
    }
    else {
      // Default: mostra contador de operadores
      u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE3_Y, String(operadoresCount).c_str());
    }
  } 
  else if (opDisplayMode == OP_DISPLAY_IN_READING || opDisplayMode == OP_DISPLAY_OUT_READING) {
    // Se está a ler, mostra mesmo sem estar em editState (em caso de delay)
    u8g2.drawStr(0, DISPLAY_LINE3_Y, "Op#:");
    if (opDisplayMode == OP_DISPLAY_IN_READING) {
      u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE3_Y, blink ? "in ... a ler" : "in");
    } else {
      u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE3_Y, blink ? "out ... a ler" : "out");
    }
  }
  else {
    // Estado normal ou outro estado
    u8g2.drawStr(0, DISPLAY_LINE3_Y, "Op#:");
    u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE3_Y, varC == "" ? "---" : varC.c_str());
  }
  
  // ===== LINHA 4: ANDON =====
  if (editState == STATE_SELECT_FIELD) {
    // Modo seleção: pisca o rótulo se selecionado
    if (selectedField == 3 && blink) {
      u8g2.drawStr(0, DISPLAY_LINE4_Y, "And:");  // Pisca
    } else if (selectedField != 3) {
      u8g2.drawStr(0, DISPLAY_LINE4_Y, "And:");  // Sempre visível se não selecionado
    }
    // Mostra o valor guardado (sempre visível)
    u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE4_Y, varD == "" ? "---" : varD.c_str());
  } else if (editState == STATE_EDIT_VALUE && selectedField == 3) {
    // Modo edição de Andon
    u8g2.drawStr(0, DISPLAY_LINE4_Y, "And:");  // Rótulo sempre visível
    
    if (andonDisplayMode == ANDON_DISPLAY_READING_OP) {
      // Mostra "....operador" a piscar
      u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE4_Y, blink ? "....operador" : "");
    } else if (andonDisplayMode == ANDON_DISPLAY_OP_NOT_FOUND) {
      // Mostra "op nao encontr" fixo
      u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE4_Y, "op n encontr");
    } else if (andonDisplayMode == ANDON_DISPLAY_SELECT_DEFECT) {
      // Mostra defeito atual a piscar
      if (blink) {
        u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE4_Y, ANDON_DEFECTS[currentAndonDefectIndex]);
      }
    } else {
      // ANDON_DISPLAY_NORMAL
      u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE4_Y, varD == "" ? "---" : varD.c_str());
    }
  } else {
    // Estado normal ou outro estado
    u8g2.drawStr(0, DISPLAY_LINE4_Y, "And:");
    u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE4_Y, varD == "" ? "---" : varD.c_str());
  }
  
  // ===== LINHA 5: FEEDBACK =====
  if (editState == STATE_EDIT_VALUE && selectedField == 2) {
    String displayFeedback = "";
    
    switch (opDisplayMode) {
      case OP_DISPLAY_MODE_SELECT:
        break;
      case OP_DISPLAY_IN_READING:
      case OP_DISPLAY_IN_SUCCESS:
      case OP_DISPLAY_OUT_READING:
      case OP_DISPLAY_OUT_SUCCESS:
      case OP_DISPLAY_NOT_FOUND:
        displayFeedback = opFeedbackMessage;
        break;
      default:
        break;
    }
    
    u8g2.drawStr(0, DISPLAY_LINE5_Y, displayFeedback.c_str());
  } else {
    // Linha 5 vazia quando não está em edição de Op#
    u8g2.drawStr(0, DISPLAY_LINE5_Y, "");
  }
  
  u8g2.sendBuffer();
}

void drawMainScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  
  // Linha 1: Bateria e WiFi
  drawBatteryIcon(cachedBattery);
  drawWiFiIcon(cachedRssi);
  
  // Linha 1: Estação
  u8g2.drawStr(0, DISPLAY_LINE1_Y, "Est:");
  u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE1_Y, varA == "" ? "---" : varA.c_str());
  
  // Linha 2: Ordem de Produção
  u8g2.drawStr(0, DISPLAY_LINE2_Y, "Ord:");
  u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE2_Y, varB == "" ? "---" : varB.c_str());
  
  // Linha 3: Número de Operadores
  u8g2.drawStr(0, DISPLAY_LINE3_Y, "Op#:");
  u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE3_Y, varC == "" ? "---" : varC.c_str());
  
  // Linha 4: Andon
  u8g2.drawStr(0, DISPLAY_LINE4_Y, "And:");
  u8g2.drawStr(DISPLAY_VALUE_X, DISPLAY_LINE4_Y, varD == "" ? "---" : varD.c_str());
  
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

// --- FUNÇÕES DE SPEAKER ---

void setupSpeaker() {
  ledcSetup(0, 1000, 8);  // Canal 0, 1kHz, 8-bit
  ledcAttachPin(SPEAKER_PIN, 0);
}

void beep(int frequency, int duration) {
  ledcSetup(0, frequency, 8);
  ledcWrite(0, 255);  // Máxima amplitude
  delay(duration);
  ledcWrite(0, 0);    // Desligar
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
  
  // DEBUG: mostra o JSON enviado
  Serial.print("[DB] Enviando JSON: ");
  Serial.println(jsonPayload);

  int httpCode = http.POST(jsonPayload);
  String response = http.getString();
  http.end();

  if (httpCode == 201) {
    Serial.println("POST em " + table + " sucesso!");
    return true;
  } else {
    Serial.print("Erro POST em " + table + ": ");
    Serial.println(httpCode);
    Serial.print("[DB] Resposta: ");
    Serial.println(response);
    return false;
  }
}

// Grava um único operador ou ordem na tabela "data_time_iot"
bool gravarTempoOperador(String rfid, String ordem_fabrico, String estado) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[DB] Offline - não conseguiu gravar tempo");
    return false;
  }

  // Obtém data/hora em formato dd/mm/aa hh:mm
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[DB] Não conseguiu obter hora");
    return false;
  }
  
  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%d/%m/%y %H:%M", &timeinfo);
  String tempoAtual = String(timeStr);

  JsonDocument doc;
  doc["rfid"] = rfid;
  doc["day_time"] = tempoAtual;
  doc["prod_order"] = ordem_fabrico;
  doc["workstation"] = varA;  // Nome da estação
  doc["status"] = estado;  // "in" ou "out"

  return supabaseGenericInsert("data_time_iot", doc);
}

// Grava os operadores atuais na tabela "data_time_iot"
bool gravarTemposOperadores() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[DB] Offline - não conseguiu gravar tempos");
    return false;
  }

  // Obtém data/hora em formato dd/mm/aa hh:mm
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[DB] Não conseguiu obter hora");
    return false;
  }
  
  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%d/%m/%y %H:%M", &timeinfo);
  String tempoAtual = String(timeStr);

  // Valida dados obrigatórios
  Serial.print("[DB] Validando: varA=");
  Serial.print(varA);
  Serial.print(", varB=");
  Serial.println(varB);

  // Grava cada operador na lista NVS atual
  bool allSuccess = true;
  for (int i = 0; i < operadoresCount; i++) {
    JsonDocument doc;
    doc["rfid"] = operadoresNVS[i].rfid;  // RFID do operador
    doc["day_time"] = tempoAtual;
    doc["prod_order"] = varB;  // ordem de produção
    doc["workstation"] = varA;  // Nome da estação
    doc["status"] = "in";  // Marca como entrada

    if (!supabaseGenericInsert("data_time_iot", doc)) {
      allSuccess = false;
    } else {
      Serial.print("[DB] Tempo registado: ");
      Serial.print(operadoresNVS[i].rfid);
      Serial.print(" - ");
      Serial.println(tempoAtual);
    }
  }

  return allSuccess;
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

// ===== LEITURA DE CÓDIGO DE BARRAS (Serial2 / UART) =====
// Lê dados do leitor de barras de forma não-bloqueante
String readBarcodeSerial() {
  while (barcodeSerial.available()) {
    char c = barcodeSerial.read();
    lastBarcodeCharTime = millis();
    
    if (c == '\n' || c == '\r') {
      // Caractere terminador - processa o buffer
      if (barcodeBufferIndex > 0) {
        barcodeBuffer[barcodeBufferIndex] = '\0';
        String result = String(barcodeBuffer);
        result.trim();
        result.toLowerCase();
        barcodeBufferIndex = 0;
        if (result.length() > 0) {
          Serial.print("[BARCODE] Código lido: ");
          Serial.println(result);
          return result;
        }
      }
    } else if (barcodeBufferIndex < BARCODE_BUFFER_SIZE - 1) {
      barcodeBuffer[barcodeBufferIndex++] = c;
    }
  }
  
  // Timeout: se temos dados no buffer mas não veio terminador
  if (barcodeBufferIndex > 0 && (millis() - lastBarcodeCharTime) > BARCODE_CHAR_TIMEOUT) {
    barcodeBuffer[barcodeBufferIndex] = '\0';
    String result = String(barcodeBuffer);
    result.trim();
    result.toLowerCase();
    barcodeBufferIndex = 0;
    if (result.length() > 0) {
      Serial.print("[BARCODE] Código lido (timeout): ");
      Serial.println(result);
      return result;
    }
  }
  
  return "";
}

// ===== LEITURA UNIFICADA: RFID + CÓDIGO DE BARRAS =====
// Tenta RFID primeiro, depois código de barras
String readIdentifier() {
  String rfid = readRFIDCard();
  if (rfid.length() > 0) return rfid;

  String barcode = readBarcodeSerial();
  if (barcode.length() > 0) return barcode;

  return "";
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

  // Ao acordar, verifica se o alerta Andon foi resolvido
  if (isWakeup && WiFi.status() == WL_CONNECTED && varD != "Verde" && varD.length() > 0 && varA.length() > 0) {
    Serial.print("[ANDON] Verificando alerta para estação: ");
    Serial.println(varA);
    String resolvido = supabaseGenericLookup("alertas_andon", "estacao", varA, "resolvido");
    if (resolvido == "TRUE" || resolvido == "true") {
      Serial.println("[ANDON] Alerta resolvido! Voltando a Verde");
      updateVariable('D', "Verde");
    } else {
      Serial.print("[ANDON] Alerta ainda não resolvido. resolvido=");
      Serial.println(resolvido);
    }
  }

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);
  pinMode(BTN3, INPUT_PULLUP);
  pinMode(BAT_PIN, INPUT);
  
  // Inicializa Speaker
  setupSpeaker();

  // PN532 apenas após WiFi (menos carga na bateria no arranque)
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (versiondata)
    nfc.SAMConfig();

  // Inicializa leitor de código de barras (Serial2)
  barcodeSerial.begin(BARCODE_BAUD, SERIAL_8N1, BARCODE_RX, BARCODE_TX);
  Serial.println("[BARCODE] Leitor de barras inicializado (Serial2)");
  Serial.print("  RX: GPIO"); Serial.print(BARCODE_RX);
  Serial.print(", TX: GPIO"); Serial.print(BARCODE_TX);
  Serial.print(", Baud: "); Serial.println(BARCODE_BAUD);

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
    lastActivityTime = millis();  // Reset timer de sleep
    
    if (editState == STATE_NORMAL) {
      // Entra em modo seleção de campo
      editState = STATE_SELECT_FIELD;
      selectedField = 1;  // Começa em Ord
      currentEstacaoIndex = procurarIndiceEstacao(varA);
      editModeStart = millis();
      lastStateChangeTime = millis();
      rfidReadingInProgress = false;
      ordReadOnceSuccess = false;  // Reset quando sai
      lastRFIDSuccess = "";
      Serial.println("[EDIT] Entrou em modo seleção de campo (Ord)");
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
        
        // Se já existe ordem em varB (ex: após reinício), inicializa lastOrdRead
        if (varB.length() > 0) {
          lastOrdRead = varB;
          ordInitialized = true;
          Serial.print("[EDIT] Restaurado estado de ordem anterior: ");
          Serial.println(varB);
        }
        
        editModeStart = millis();
        lastStateChangeTime = millis();
        Serial.println("[EDIT] Entrou em modo edição de Ordem - Iniciando leitura RFID");
      } else if (selectedField == 2) {
        // Op#: entra em seleção de In/Out
        editState = STATE_EDIT_VALUE;
        opDisplayMode = OP_DISPLAY_MODE_SELECT;  // Mostra "In" ou "Out" a piscar
        opMode = OP_MODE_IN;  // Começa em In
        editModeStart = millis();
        lastStateChangeTime = millis();
        Serial.println("[EDIT] Entrou em modo Op# - Selecionando In/Out");
      } else if (selectedField == 3) {
        // Andon: inicia leitura RFID de operador
        editState = STATE_EDIT_VALUE;
        andonDisplayMode = ANDON_DISPLAY_READING_OP;
        andonReadingInProgress = true;
        lastRFIDAndon = "";
        currentAndonDefectIndex = 0;
        editModeStart = millis();
        lastStateChangeTime = millis();
        Serial.println("[EDIT] Entrou em modo Andon - Aguardando RFID operador");
      }
    } else if (editState == STATE_EDIT_VALUE) {
      // Confirmação do valor
      if (selectedField == 0) {
        // Est: confirma valor e sai de edição
        String estacaoSelecionada = estacoes[currentEstacaoIndex].nome;
        updateVariable('A', estacaoSelecionada);
        editState = STATE_NORMAL;
        rfidReadingInProgress = false;
        ordReadOnceSuccess = false;  // Reset quando sai
        Serial.print("[EDIT] Confirmou estação: ");
        Serial.println(estacaoSelecionada);
        Serial.println("[EDIT] Voltou ao modo normal");
      } else if (selectedField == 1) {
        // Ord: lógica depende do modo de display
        if (ordDisplayMode == ORD_DISPLAY_CONFIRM) {
          // Está a confirmar "terminar?" - grava como OUT
          ordDisplayMode = ORD_DISPLAY_WRITING;  // Mostra "a escrever"
          
          // Grava como saída
          if (ordFromDatabase.length() > 0) {
            gravarTempoOperador("", ordFromDatabase, "out");  // Ordem termina
            Serial.print("[DB] Ordem terminada: ");
            Serial.println(ordFromDatabase);
          }
          
          // Limpa tudo e sai
          updateVariable('B', "");  // Limpa varB
          ordFromDatabase = "";
          ordInitialized = false;
          lastOrdRead = "";
          ordConfirmStartTime = -1;  // Reset da alternância
          ordReadOnceSuccess = false;  // Reset quando sai do modo
          rfidReadingInProgress = false;
          editState = STATE_NORMAL;
          ordDisplayMode = ORD_DISPLAY_NORMAL;
          Serial.println("[EDIT] Ordem terminada e variável B limpa");
        } else {
          // Está em leitura ou mostrou ordem - sai normalmente
          rfidReadingInProgress = false;
          ordReadOnceSuccess = false;  // Reset quando sai do modo
          
          if (ordFromDatabase.length() > 0 && !ordInitialized) {
            // Primeira ordem - grava como entrada
            ordDisplayMode = ORD_DISPLAY_WRITING;  // Mostra "a escrever"
            gravarTempoOperador("", ordFromDatabase, "in");
            ordInitialized = true;
            lastOrdRead = ordFromDatabase;
            updateVariable('B', ordFromDatabase);
            Serial.print("[DB] Ordem iniciada: ");
            Serial.println(ordFromDatabase);
          } else if (ordFromDatabase.length() > 0 && ordInitialized && ordFromDatabase != lastOrdRead) {
            // Ordem diferente da anterior - grava anterior como OUT e nova como IN
            ordDisplayMode = ORD_DISPLAY_WRITING;  // Mostra "a escrever"
            gravarTempoOperador("", lastOrdRead, "out");
            Serial.print("[DB] Ordem anterior terminada: ");
            Serial.println(lastOrdRead);
            
            gravarTempoOperador("", ordFromDatabase, "in");  // Já está em WRITING do passo anterior
            lastOrdRead = ordFromDatabase;
            updateVariable('B', ordFromDatabase);
            Serial.print("[DB] Nova ordem iniciada: ");
            Serial.println(ordFromDatabase);
          } else if (ordFromDatabase.length() > 0) {
            // Ordem existente (mesma anterior) - confirma variável
            updateVariable('B', ordFromDatabase);
            Serial.print("[EDIT] Confirmou ordem: ");
            Serial.println(ordFromDatabase);
          } else {
            Serial.println("[EDIT] Saiu da leitura RFID sem encontrar ordem");
          }
          
          editState = STATE_NORMAL;
          ordDisplayMode = ORD_DISPLAY_NORMAL;
          ordConfirmStartTime = -1;  // Reset da alternância
          // NÃO limpa ordInitialized e lastOrdRead aqui - mantém estado para próxima leitura!
          Serial.println("[EDIT] Voltou ao modo normal");
        }

      } else if (selectedField == 2) {
        // Op#: comportamento depende do modo de display
        if (opDisplayMode == OP_DISPLAY_MODE_SELECT) {
          // Estava em seleção de In/Out, agora inicia leitura
          if (opMode == OP_MODE_IN) {
            opDisplayMode = OP_DISPLAY_IN_READING;
            Serial.println("[EDIT] Iniciando leitura IN de operadores");
          } else {
            opDisplayMode = OP_DISPLAY_OUT_READING;
            Serial.println("[EDIT] Iniciando leitura OUT de operadores");
          }
          opReadingInProgress = true;
          opReadStart = millis();
          lastRFIDOperador = "";
          operadorFromDatabase = "";
          editModeStart = millis();
        } else {
          // Estava em leitura (In ou Out), agora termina
          opReadingInProgress = false;
          
          // Limpeza de feedback
          opGreetingName = "";
          opFeedbackMessage = "";
          
          // Atualiza varC com número atual de operadores
          updateVariable('C', String(operadoresCount));
          
          editState = STATE_NORMAL;
          opDisplayMode = OP_DISPLAY_NORMAL;
          Serial.print("[EDIT] Finalizou Op#, total operadores: ");
          Serial.println(operadoresCount);
          Serial.println("[EDIT] Voltou ao modo normal");
        }
      } else if (selectedField == 3) {
        // Andon: comportamento depende do modo de display
        if (andonDisplayMode == ANDON_DISPLAY_READING_OP) {
          // Enter durante "....operador" → sai sem alterar varD
          andonReadingInProgress = false;
          andonDisplayMode = ANDON_DISPLAY_NORMAL;
          editState = STATE_NORMAL;
          Serial.println("[EDIT] Andon: saiu sem alterar (enter durante leitura)");
        } else if (andonDisplayMode == ANDON_DISPLAY_SELECT_DEFECT) {
          // Enter na lista de defeitos → seleciona defeito e guarda em varD
          String selectedDefect = String(ANDON_DEFECTS[currentAndonDefectIndex]);
          andonReadingInProgress = false;
          andonDisplayMode = ANDON_DISPLAY_NORMAL;
          editState = STATE_NORMAL;
          
          // Verifica se houve alteração
          if (selectedDefect != varD) {
            updateVariable('D', selectedDefect);
            onAndonChanged(selectedDefect);
            Serial.print("[EDIT] Andon: defeito selecionado: ");
            Serial.println(selectedDefect);
          } else {
            Serial.println("[EDIT] Andon: mesmo valor, sem alteração");
          }
          Serial.println("[EDIT] Voltou ao modo normal");
        } else {
          // Qualquer outro estado → sai
          andonReadingInProgress = false;
          andonDisplayMode = ANDON_DISPLAY_NORMAL;
          editState = STATE_NORMAL;
          Serial.println("[EDIT] Andon: saiu sem alterar");
        }
      }
    }
  }
  
  // BTN1 (Cima) - navega para trás no array
  if (btn1State == LOW && btn1LastState == HIGH) {
    lastActivityTime = millis();  // Reset timer de sleep
    if (editState != STATE_NORMAL) {
      if (editState == STATE_SELECT_FIELD) {
        // Roda entre Est, Ord, Op#, And
        selectedField = (selectedField - 1 + 4) % 4;
        editModeStart = millis();  // Reset timeout
        lastStateChangeTime = millis();  // Feedback visual
        Serial.print("[EDIT] Campo: ");
        Serial.println(selectedField);
      } else if (editState == STATE_EDIT_VALUE && selectedField == 0) {
        // Navega no array para trás
        currentEstacaoIndex = (currentEstacaoIndex - 1 + NUM_ESTACOES) % NUM_ESTACOES;
        editModeStart = millis();  // Reset timeout
        Serial.print("[EDIT] Estação anterior: ");
        Serial.println(estacoes[currentEstacaoIndex].nome);
      } else if (editState == STATE_EDIT_VALUE && selectedField == 1) {
        // Ord: se está em ORD_DISPLAY_CONFIRM, volta a "... a ler"
        if (ordDisplayMode == ORD_DISPLAY_CONFIRM) {
          ordDisplayMode = ORD_DISPLAY_READING;
          ordReadOnceSuccess = false;  // Permite nova leitura
          editModeStart = millis();  // Reset timeout para novo ciclo de leitura
          Serial.println("[EDIT] Voltou a ler ordem (cancelou terminar)");
        }
      } else if (editState == STATE_EDIT_VALUE && selectedField == 2) {
        // Comuta entre In e Out
        if (opDisplayMode == OP_DISPLAY_MODE_SELECT) {
          opMode = (opMode == OP_MODE_IN) ? OP_MODE_OUT : OP_MODE_IN;
          lastStateChangeTime = millis();  // Feedback visual
          Serial.print("[EDIT] Op# modo: ");
          Serial.println(opMode == OP_MODE_IN ? "IN" : "OUT");
        }
      } else if (editState == STATE_EDIT_VALUE && selectedField == 3) {
        // Andon: navega lista de defeitos para trás
        if (andonDisplayMode == ANDON_DISPLAY_SELECT_DEFECT) {
          currentAndonDefectIndex = (currentAndonDefectIndex - 1 + NUM_ANDON_DEFECTS) % NUM_ANDON_DEFECTS;
          editModeStart = millis();  // Reset timeout
          Serial.print("[EDIT] Andon defeito anterior: ");
          Serial.println(ANDON_DEFECTS[currentAndonDefectIndex]);
        }
      }
    }
  }
  
  // BTN3 (Baixo) - navega para frente no array
  if (btn3State == LOW && btn3LastState == HIGH) {
    lastActivityTime = millis();  // Reset timer de sleep
    if (editState != STATE_NORMAL) {
      if (editState == STATE_SELECT_FIELD) {
        // Roda entre Est, Ord, Op#, And
        selectedField = (selectedField + 1) % 4;
        editModeStart = millis();  // Reset timeout
        lastStateChangeTime = millis();  // Feedback visual
        Serial.print("[EDIT] Campo: ");
        Serial.println(selectedField);
      } else if (editState == STATE_EDIT_VALUE && selectedField == 0) {
        // Navega no array para frente
        currentEstacaoIndex = (currentEstacaoIndex + 1) % NUM_ESTACOES;
        editModeStart = millis();  // Reset timeout
        Serial.print("[EDIT] Próxima estação: ");
        Serial.println(estacoes[currentEstacaoIndex].nome);
      } else if (editState == STATE_EDIT_VALUE && selectedField == 1) {
        // Ord: se está em ORD_DISPLAY_CONFIRM, volta a "... a ler"
        if (ordDisplayMode == ORD_DISPLAY_CONFIRM) {
          ordDisplayMode = ORD_DISPLAY_READING;
          ordReadOnceSuccess = false;  // Permite nova leitura
          editModeStart = millis();  // Reset timeout para novo ciclo de leitura
          Serial.println("[EDIT] Voltou a ler ordem (cancelou terminar)");
        }
      } else if (editState == STATE_EDIT_VALUE && selectedField == 2) {
        // Comuta entre In e Out
        if (opDisplayMode == OP_DISPLAY_MODE_SELECT) {
          opMode = (opMode == OP_MODE_IN) ? OP_MODE_OUT : OP_MODE_IN;
          lastStateChangeTime = millis();  // Feedback visual
          Serial.print("[EDIT] Op# modo: ");
          Serial.println(opMode == OP_MODE_IN ? "IN" : "OUT");
        }
      } else if (editState == STATE_EDIT_VALUE && selectedField == 3) {
        // Andon: navega lista de defeitos para frente
        if (andonDisplayMode == ANDON_DISPLAY_SELECT_DEFECT) {
          currentAndonDefectIndex = (currentAndonDefectIndex + 1) % NUM_ANDON_DEFECTS;
          editModeStart = millis();  // Reset timeout
          Serial.print("[EDIT] Andon próximo defeito: ");
          Serial.println(ANDON_DEFECTS[currentAndonDefectIndex]);
        }
      }
    }
  }

  // ===== LEITURA RFID/BARCODE CONTÍNUA PARA CAMPO ORD =====
  if (rfidReadingInProgress && selectedField == 1 && editState == STATE_EDIT_VALUE) {
    // Tenta ler um cartão RFID ou código de barras, mas só processa uma leitura bem-sucedida
    if (!ordReadOnceSuccess) {  // Só lê se ainda não teve sucesso neste ciclo
      String rfidRead = readIdentifier();
      
      if (rfidRead.length() > 0) {
        // Cartão lido com sucesso - guarda e processa
        if (lastRFIDSuccess != rfidRead) {
          // Novo RFID (diferente do anterior)
          lastRFIDSuccess = rfidRead;
          ordDisplayMode = ORD_DISPLAY_VERIFYING;  // Muda para "... verificar"
          ordLookupPending = true;  // Marca para fazer lookup depois
          ordLookupStartTime = millis();  // Inicia o delay
          ordReadOnceSuccess = true;  // MARCA: já teve uma leitura bem-sucedida, para de ler
          Serial.print("[RFID] Cartão lido: ");
          Serial.println(rfidRead);
          editModeStart = millis();  // Reset de timeout quando lê com sucesso
          lastActivityTime = millis();  // Reset timer de sleep
        }
      }
    }
  }
  
  // ===== PROCESSAR LOOKUP DE ORD APÓS DELAY =====
  if (ordLookupPending && (millis() - ordLookupStartTime) > ORD_LOOKUP_DELAY) {
    // Agora faz o lookup (depois de mostrar "verificar")
    ordLookupPending = false;
    
    Serial.print("[DB] Procurando na tabela 'ordens_producao' com rfid_token=");
    Serial.println(lastRFIDSuccess);
    
    String result = supabaseGenericLookup("ordens_producao", "rfid_token", lastRFIDSuccess, "display_nome");
    
    if (result != "Nao encontrado" && result != "Erro: Offline") {
      // Ordem encontrada!
      ordFromDatabase = result;
      
      // Verifica se é a mesma ordem (para pedir confirmação de terminar)
      if (ordInitialized && result == lastOrdRead) {
        // Mesma ordem lida novamente - pede confirmação
        ordDisplayMode = ORD_DISPLAY_CONFIRM;
        ordConfirmStartTime = millis();  // Marca início da alternância
        Serial.print("[DB] Mesma ordem lida novamente, aguardando confirmação: ");
        Serial.println(result);
      } else {
        // Primeira ordem ou ordem diferente
        ordDisplayMode = ORD_DISPLAY_FOUND;  // Muda para mostrar valor a piscar
        Serial.print("[DB] Ordem encontrada: ");
        Serial.println(result);
      }
    } else {
      // Ordem não encontrada
      ordDisplayMode = ORD_DISPLAY_NOT_FOUND;  // Muda para "não existe ord"
      ordNotFoundTime = millis();  // Marca o tempo de início
      ordFromDatabase = "";  // Limpa o valor
      Serial.println("[DB] RFID não encontrado na BD");
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

  // ===== LEITURA RFID/BARCODE PARA OP# (IN E OUT) =====
  if (opReadingInProgress && selectedField == 2 && editState == STATE_EDIT_VALUE) {
    // Só lê novo cartão/código se não há lookup pendente
    if (!opLookupPending) {
      // Tenta ler um cartão RFID ou código de barras
      String rfidRead = readIdentifier();
      
      if (rfidRead.length() > 0) {
        // Cartão lido com sucesso
        if (lastRFIDOperador != rfidRead) {
          // Novo RFID (diferente do anterior) - limpa mensagens anteriores
          lastRFIDOperador = rfidRead;
          opLookupPending = true;
          opLookupStartTime = millis();
          // Mostra feedback imediato enquanto processa
          opFeedbackMessage = "... à espera";
          // Atualiza modo de display para mostrar "in/out ... a ler" a piscar
          opDisplayMode = (opMode == OP_MODE_IN ? OP_DISPLAY_IN_READING : OP_DISPLAY_OUT_READING);
          Serial.print("[RFID-OP] Cartão lido: ");
          Serial.println(rfidRead);
        }
        // Reset timeout a cada leitura bem-sucedida
        editModeStart = millis();
        lastActivityTime = millis();  // Reset timer de sleep
      }
    }
  }
  
  // ===== PROCESSAR LOOKUP DE OP#(IN/OUT) APÓS DELAY =====
  if (opLookupPending && (millis() - opLookupStartTime) > OP_LOOKUP_DELAY) {
    opLookupPending = false;
    
    if (opMode == OP_MODE_IN) {
      // MODO IN: faz lookup na BD para obter o nome
      Serial.print("[DB] Procurando operador com rfid=");
      Serial.println(lastRFIDOperador);
      
      // Lookup na BD (apenas para obter o nome)
      String nomeOperador = supabaseGenericLookup("operadores", "tag_rfid_operador", lastRFIDOperador, "nome_operador");
      
      if (nomeOperador != "Nao encontrado" && nomeOperador != "Erro: Offline") {
        // Operador encontrado na BD
        operadorFromDatabase = nomeOperador;
        Serial.print("[DB] Operador encontrado: ");
        Serial.print(lastRFIDOperador);
        Serial.print(" - ");
        Serial.println(nomeOperador);
        
        // MODO IN: adiciona à lista se não existir
        bool exists = operadorExists(lastRFIDOperador);
        
        if (!exists) {
          // Novo operador - adiciona à lista
          addOperador(lastRFIDOperador);
          
          // Grava apenas este operador na Supabase tempos
          gravarTempoOperador(lastRFIDOperador, "", "in");
          
          // Atualiza varC com novo contador
          updateVariable('C', String(operadoresCount));
          
          opDisplayMode = OP_DISPLAY_IN_SUCCESS;
          
          // Prepara mensagem de feedback
          opGreetingName = nomeOperador;
          opFeedbackMessage = "Ola " + nomeOperador;
          
          // Beep agudo rápido para sucesso IN
          beep(1200, 100);
          
          Serial.print("[RFID-OP] NOVO operador IN adicionado: ");
          Serial.println(lastRFIDOperador);
        } else {
          // Operador já existe - apenas mostra sucesso
          opDisplayMode = OP_DISPLAY_IN_SUCCESS;
          
          // Prepara mensagem de feedback
          opGreetingName = nomeOperador;
          opFeedbackMessage = "Ola " + nomeOperador;
          
          // Beep agudo rápido para sucesso IN
          beep(1200, 100);
          
          Serial.print("[RFID-OP] Operador já na lista: ");
          Serial.println(lastRFIDOperador);
        }
      } else {
        // Operador não encontrado na BD
        opDisplayMode = OP_DISPLAY_NOT_FOUND;
        opFeedbackMessage = "erro op nao existe na bd";
        operadorFromDatabase = "";
        
        // Beep grave para erro
        beep(400, 200);
        
        Serial.println("[DB] RFID não encontrado na BD operadores");
      }
    } else {
      // MODO OUT: apenas verifica se existe na lista local (sem lookup na BD)
      Serial.print("[DB-OP] Verificando RFID na lista OUT: ");
      Serial.println(lastRFIDOperador);
      
      bool exists = operadorExists(lastRFIDOperador);
      
      if (exists) {
        // Remove da lista
        removeOperador(lastRFIDOperador);
        
        // Grava na Supabase tempos (RFID, estação, ordem, tempo)
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
          char timeStr[20];
          strftime(timeStr, sizeof(timeStr), "%d/%m/%y %H:%M", &timeinfo);
          String tempoAtual = String(timeStr);
          
          // DEBUG: mostra valores antes de enviar
          Serial.print("[DB-OUT] Valores a enviar: rfid=");
          Serial.print(lastRFIDOperador);
          Serial.print(", tempo=");
          Serial.print(tempoAtual);
          Serial.print(", barco=");
          Serial.print(varB);
          Serial.print(", estacao=");
          Serial.println(varA);
          
          JsonDocument doc;
          doc["rfid"] = lastRFIDOperador;  // RFID do operador
          doc["day_time"] = tempoAtual;
          doc["prod_order"] = "";  // Em branco no registo de operador
          doc["workstation"] = varA;  // Nome da estação
          doc["status"] = "out";  // Marca como saída
          
          if (supabaseGenericInsert("data_time_iot", doc)) {
            Serial.print("[DB] Tempo OUT registado com sucesso: ");
            Serial.println(lastRFIDOperador);
          } else {
            Serial.print("[DB] Falha ao registar tempo OUT: ");
            Serial.println(lastRFIDOperador);
          }
        } else {
          Serial.println("[DB] Falha ao obter hora para OUT");
        }
        
        // Atualiza varC com novo contador
        updateVariable('C', String(operadoresCount));
        
        opDisplayMode = OP_DISPLAY_OUT_SUCCESS;
        opFeedbackMessage = "removido";
        
        // Beep médio rápido para sucesso OUT
        beep(800, 100);
        
        Serial.print("[RFID-OP] Operador OUT removido: ");
        Serial.println(lastRFIDOperador);
      } else {
        // Operador não está na lista local
        opDisplayMode = OP_DISPLAY_NOT_FOUND;
        opFeedbackMessage = "op nao existe na estacao";
        
        // Beep grave para erro
        beep(400, 200);
        
        Serial.print("[RFID-OP] Operador não consta da lista OUT: ");
        Serial.println(lastRFIDOperador);
      }
    }
  }
  
  // Volta imediatamente a "... a ler" (sem esperar 2 segundos)
  if ((opDisplayMode == OP_DISPLAY_IN_SUCCESS || opDisplayMode == OP_DISPLAY_OUT_SUCCESS || opDisplayMode == OP_DISPLAY_NOT_FOUND) &&
      opReadingInProgress) {
    // Limpa o RFID anterior para permitir nova leitura
    lastRFIDOperador = "";
    // Volta ao modo de leitura piscante
    opDisplayMode = (opMode == OP_MODE_IN ? OP_DISPLAY_IN_READING : OP_DISPLAY_OUT_READING);
    // NÃO limpa feedback aqui - mantém visível até sair do ciclo
  }

  // ===== LEITURA RFID/BARCODE PARA ANDON (OPERADOR) =====
  if (andonReadingInProgress && selectedField == 3 && editState == STATE_EDIT_VALUE) {
    if (andonDisplayMode == ANDON_DISPLAY_READING_OP && !andonLookupPending) {
      String rfidRead = readIdentifier();
      
      if (rfidRead.length() > 0) {
        if (lastRFIDAndon != rfidRead) {
          lastRFIDAndon = rfidRead;
          andonLookupPending = true;
          andonLookupStartTime = millis();
          editModeStart = millis();  // Reset timeout
          lastActivityTime = millis();
          Serial.print("[RFID-ANDON] Cartão lido: ");
          Serial.println(rfidRead);
        }
      }
    }
  }
  
  // ===== PROCESSAR LOOKUP DE ANDON OPERADOR APÓS DELAY =====
  if (andonLookupPending && (millis() - andonLookupStartTime) > ANDON_LOOKUP_DELAY) {
    andonLookupPending = false;
    
    Serial.print("[DB-ANDON] Procurando operador com rfid=");
    Serial.println(lastRFIDAndon);
    
    String nomeOperador = supabaseGenericLookup("operadores", "tag_rfid_operador", lastRFIDAndon, "nome_operador");
    
    if (nomeOperador != "Nao encontrado" && nomeOperador != "Erro: Offline") {
      // Operador encontrado → passa para lista de defeitos
      andonDisplayMode = ANDON_DISPLAY_SELECT_DEFECT;
      currentAndonDefectIndex = 0;
      editModeStart = millis();  // Reset timeout
      
      // Beep de sucesso
      beep(1200, 100);
      
      Serial.print("[DB-ANDON] Operador encontrado: ");
      Serial.println(nomeOperador);
      Serial.println("[ANDON] Iniciando seleção de defeito");
    } else {
      // Operador não encontrado → mostra mensagem 1s
      andonDisplayMode = ANDON_DISPLAY_OP_NOT_FOUND;
      andonNotFoundTime = millis();
      lastRFIDAndon = "";
      
      // Beep de erro
      beep(400, 200);
      
      Serial.println("[DB-ANDON] Operador não encontrado na BD");
    }
  }
  
  // ===== CONTROLE DE EXIBIÇÃO "OP NAO ENCONTR" ANDON =====
  if (andonDisplayMode == ANDON_DISPLAY_OP_NOT_FOUND && andonNotFoundTime != -1) {
    if ((millis() - andonNotFoundTime) > ANDON_NOT_FOUND_DISPLAY_TIME) {
      // Passou 1 segundo, volta a "....operador"
      andonDisplayMode = ANDON_DISPLAY_READING_OP;
      lastRFIDAndon = "";
      andonNotFoundTime = -1;
      Serial.println("[ANDON] Voltou a aguardar operador após 1 segundo");
    }
  }

  
  // Verifica timeout de edição (30 segundos)
  if (editState != STATE_NORMAL && (millis() - editModeStart) > EDIT_TIMEOUT) {
    editState = STATE_NORMAL;
    rfidReadingInProgress = false;
    ordReadOnceSuccess = false;  // Reset quando timeout
    lastRFIDSuccess = "";
    ordFromDatabase = "";
    // Reset Andon state
    andonReadingInProgress = false;
    andonDisplayMode = ANDON_DISPLAY_NORMAL;
    andonLookupPending = false;
    lastRFIDAndon = "";
    // Mantém lastRfidValue com o último valor para feedback ao sair
    Serial.println("[EDIT] Saiu por timeout");
  }

  // ===== VERIFICAR TIMEOUT PARA DEEP SLEEP (3 minutos) =====
  if (millis() - lastActivityTime > sleepTimeout) {
    Serial.println("[SLEEP] Timeout atingido - entrando em Deep Sleep");
    goToSleep();
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
  delay(50); // Delay reduzido de 100ms para melhor responsividade às teclas
}