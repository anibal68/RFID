#include <Adafruit_PN532.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

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

// Desenha ícone de WiFi no topo direito
// Desenha ícone de bateria proporcional
void drawBatteryIcon(int percentage) {
  int x = 0;
  int y = 2;
  int w = 14;
  int h = 10;
  // Corpo da bateria
  u8g2.drawFrame(x, y, w, h);
  // Ponta da bateria
  u8g2.drawBox(x + w, y + 2, 2, 6);
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
  int y = 10;
  if (WiFi.status() != WL_CONNECTED) {
    u8g2.drawStr(x, y, "X");
    return;
  }
  if (rssi > -90)
    u8g2.drawBox(x, y - 2, 2, 2);
  if (rssi > -80)
    u8g2.drawBox(x + 3, y - 4, 2, 4);
  if (rssi > -70)
    u8g2.drawBox(x + 6, y - 6, 2, 6);
  if (rssi > -60)
    u8g2.drawBox(x + 9, y - 8, 2, 8);
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
}

// Estados para detecção de borda nos botões
bool btn1LastState = HIGH;
bool btn2LastState = HIGH;
bool btn3LastState = HIGH;

void loop() {
  // Serial.print("L"); // Debug rápido para ver se o loop corre
  bool activity = false;

  // Lógica de detecção de borda (Trigger apenas no momento do clique)
  bool btn1State = digitalRead(BTN1);
  bool btn2State = digitalRead(BTN2);
  bool btn3State = digitalRead(BTN3);

  // Botão 1: Escreve na tabela "tempos"
  if (btn1State == LOW && btn1LastState == HIGH) {
    lastPressed = BTN1;
    activity = true;

    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Enviando Tempo...");
    u8g2.sendBuffer();

    JsonDocument data;
    data["operador"] = "000747";
    data["tempo"] = getFormattedTime();

    if (supabaseGenericInsert("tempos", data)) {
      u8g2.drawStr(0, 30, "Sucesso!");
    } else {
      u8g2.drawStr(0, 30, "Erro!");
    }
    u8g2.sendBuffer();
    delay(2000);
  }

  // Botão 2: Procura na tabela "barcos"
  if (btn2State == LOW && btn2LastState == HIGH) {
    lastPressed = BTN2;
    activity = true;

    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Buscando Barco...");
    u8g2.sendBuffer();

    String ordem =
        supabaseGenericLookup("barcos", "barco", "01010", "ordem_fabrico");

    u8g2.drawStr(0, 30, "Ordem:");
    u8g2.drawStr(0, 50, ordem.c_str());
    u8g2.sendBuffer();
    delay(3000);
  }

  // Botão 3: Procura na tabela "operadores"
  if (btn3State == LOW && btn3LastState == HIGH) {
    lastPressed = BTN3;
    activity = true;

    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Buscando Operador...");
    u8g2.sendBuffer();

    String nome =
        supabaseGenericLookup("operadores", "numero", "000747", "nome");

    u8g2.drawStr(0, 30, "Nome:");
    u8g2.drawStr(0, 50, nome.c_str());
    u8g2.sendBuffer();
    delay(3000);
  }

  // Atualiza estados anteriores
  btn1LastState = btn1State;
  btn2LastState = btn2State;
  btn3LastState = btn3State;

  if (activity) {
    lastActivityTime = millis();
  }

  // Atualiza bateria e WiFi apenas a cada 10s para evitar lag
  if (millis() - lastBatteryCheck > checkInterval || lastBatteryCheck == 0) {
    cachedBattery = getBatteryPercentage();
    lastBatteryCheck = millis();
    if (WiFi.status() == WL_CONNECTED) {
      cachedRssi = WiFi.RSSI();
    }
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  // Status Bar (topo)
  drawBatteryIcon(cachedBattery);
  drawWiFiIcon(cachedRssi);

  // Conteúdo Central
  u8g2.drawStr(0, 30, WiFi.localIP().toString().c_str());
  if (lastPressed != -1) {
    char buf[30];
    sprintf(buf, "Ultimo: %d", lastPressed);
    u8g2.drawStr(0, 50, buf);
  } else {
    u8g2.drawStr(0, 50, "Pronto...");
  }
  u8g2.sendBuffer();

  // Leitura NFC (Timeout reduzido para 30ms para resposta imediata dos botões)
  uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0};
  uint8_t uidLength;
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 30)) {
    activity = true;
    String uidStr = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10)
        uidStr += "0";
      uidStr += String(uid[i], HEX);
    }
    uidStr.toUpperCase();

    u8g2.clearBuffer();
    drawBatteryIcon(cachedBattery);
    drawWiFiIcon(cachedRssi);
    u8g2.drawStr(0, 30, "NFC:");
    u8g2.drawStr(35, 30, uidStr.c_str());
    u8g2.sendBuffer();
    delay(1500);
  }

  // Verifica se é hora de dormir
  if (millis() - lastActivityTime > sleepTimeout) {
    goToSleep();
  }

  delay(50); // Loop mais responsivo
}