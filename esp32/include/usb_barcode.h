// ===== USB Host HID Barcode Reader para ESP32-S3 =====
// Leitores de código de barras USB aparecem como teclados HID.
// Este módulo inicializa o USB Host, deteta o dispositivo,
// reivindica a interface HID, e lê os keycodes convertendo-os para texto.
//
// HARDWARE: O leitor de barras liga na porta USB nativa do ESP32-S3
//   (GPIO19/20). É necessário um adaptador OTG (USB-A fêmea → micro-USB/USB-C).
//   O ESP32-S3-DevKitC-1 tem duas portas USB:
//     - USB-UART (via bridge chip) → programação e Serial monitor
//     - USB nativa (GPIO19/20)     → leitor de barras (USB Host)
//
// API:
//   usbBarcodeInit()      → Inicializa USB Host (chamar no setup())
//   usbBarcodeRead()      → Leitura não-bloqueante (retorna String, vazia se nada)
//   usbBarcodeConnected() → true se leitor está conectado

#ifndef USB_BARCODE_H
#define USB_BARCODE_H

#if CONFIG_IDF_TARGET_ESP32S3

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "usb/usb_host.h"

#define USB_BC_MAX_LEN 128

// ===== State variables =====
static SemaphoreHandle_t _ubcMutex = NULL;
static volatile bool _ubcReady = false;
static char _ubcResult[USB_BC_MAX_LEN];
static char _ubcAccum[USB_BC_MAX_LEN];
static int _ubcAccumIdx = 0;
static uint8_t _ubcPrevKeys[6];

static bool _ubcInitDone = false;
static volatile bool _ubcDevConnected = false;

static usb_host_client_handle_t _ubcClientHdl = NULL;
static usb_device_handle_t _ubcDevHdl = NULL;
static usb_transfer_t* _ubcInXfer = NULL;
static uint8_t _ubcEpAddr = 0;
static uint16_t _ubcEpMPS = 8;
static int _ubcIfaceNum = -1;

// ===== HID Keyboard keycode → ASCII =====
static char _ubcKeyToChar(uint8_t code, bool shift) {
  // Letras a-z (0x04–0x1D)
  if (code >= 0x04 && code <= 0x1D) {
    char c = 'a' + (code - 0x04);
    return shift ? (c - 32) : c;
  }
  // Números 1-9, 0 (0x1E–0x27)
  if (code >= 0x1E && code <= 0x27) {
    if (code == 0x27) return shift ? ')' : '0';
    if (!shift) return '1' + (code - 0x1E);
    const char s[] = "!@#$%^&*(";
    return s[code - 0x1E];
  }
  // Outros caracteres comuns em códigos de barras
  switch (code) {
    case 0x2C: return ' ';
    case 0x2D: return shift ? '_' : '-';
    case 0x2E: return shift ? '+' : '=';
    case 0x2F: return shift ? '{' : '[';
    case 0x30: return shift ? '}' : ']';
    case 0x31: return shift ? '|' : '\\';
    case 0x33: return shift ? ':' : ';';
    case 0x34: return shift ? '"' : '\'';
    case 0x36: return shift ? '<' : ',';
    case 0x37: return shift ? '>' : '.';
    case 0x38: return shift ? '?' : '/';
    default: return 0;
  }
}

// ===== Transfer callback (chamado do contexto da task do cliente USB) =====
static void _ubcXferCb(usb_transfer_t* xfer) {
  if (xfer->status == USB_TRANSFER_STATUS_COMPLETED && xfer->actual_num_bytes >= 3) {
    uint8_t* d = xfer->data_buffer;
    // Byte 0: modificadores (Shift, Ctrl, Alt...)
    // Byte 1: reservado
    // Bytes 2-7: até 6 keycodes em simultâneo
    bool shift = (d[0] & 0x22) != 0;  // Left Shift (0x02) ou Right Shift (0x20)

    for (int i = 2; i < 8 && i < xfer->actual_num_bytes; i++) {
      uint8_t kc = d[i];
      if (kc == 0x00) continue;  // Sem tecla

      // Debounce: ignora se a tecla já estava no relatório anterior
      bool prev = false;
      for (int j = 0; j < 6; j++) {
        if (_ubcPrevKeys[j] == kc) { prev = true; break; }
      }
      if (prev) continue;

      if (kc == 0x28) {
        // Enter (0x28) = código de barras completo
        if (_ubcAccumIdx > 0) {
          _ubcAccum[_ubcAccumIdx] = '\0';
          if (xSemaphoreTake(_ubcMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            strncpy(_ubcResult, _ubcAccum, USB_BC_MAX_LEN);
            _ubcReady = true;
            xSemaphoreGive(_ubcMutex);
          }
          _ubcAccumIdx = 0;
        }
      } else {
        char c = _ubcKeyToChar(kc, shift);
        if (c && _ubcAccumIdx < USB_BC_MAX_LEN - 1) {
          _ubcAccum[_ubcAccumIdx++] = c;
        }
      }
    }

    // Guarda teclas atuais para debouncing no próximo relatório
    for (int i = 0; i < 6; i++) {
      _ubcPrevKeys[i] = (i + 2 < xfer->actual_num_bytes) ? d[i + 2] : 0;
    }
  }

  // Re-submete para leitura contínua
  if (_ubcDevConnected && _ubcInXfer) {
    usb_host_transfer_submit(_ubcInXfer);
  }
}

// ===== Procura interface HID e endpoint IN no config descriptor =====
static bool _ubcFindEndpoint(const usb_config_desc_t* cfg) {
  const uint8_t* p = (const uint8_t*)cfg;
  int off = 0, total = cfg->wTotalLength;
  int curIface = -1;
  bool isHID = false;

  while (off < total) {
    uint8_t dLen = p[off], dType = p[off + 1];
    if (dLen == 0) break;

    if (dType == 0x04 && dLen >= 9) {
      // Interface descriptor (type 0x04)
      curIface = p[off + 2];   // bInterfaceNumber
      uint8_t cls = p[off + 5]; // bInterfaceClass
      // HID class = 3 (aceita qualquer subclasse/protocolo para leitores de barras)
      isHID = (cls == 3);
    }
    else if (dType == 0x05 && dLen >= 7 && isHID) {
      // Endpoint descriptor (type 0x05)
      uint8_t addr = p[off + 2]; // bEndpointAddress
      uint8_t attr = p[off + 3]; // bmAttributes
      // Procura Interrupt IN endpoint (bit 7 = IN, bits 0-1 = Interrupt)
      if ((addr & 0x80) && (attr & 0x03) == 0x03) {
        _ubcEpAddr = addr;
        _ubcIfaceNum = curIface;
        _ubcEpMPS = p[off + 4] | (p[off + 5] << 8);
        Serial.printf("[USB] HID endpoint: 0x%02X iface:%d MPS:%d\n", addr, curIface, _ubcEpMPS);
        return true;
      }
    }
    off += dLen;
  }
  return false;
}

// ===== Callback de eventos do cliente USB =====
static void _ubcClientCb(const usb_host_client_event_msg_t* msg, void* arg) {
  if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
    // --- Novo dispositivo USB conectado ---
    Serial.printf("[USB] Dispositivo conectado (addr:%d)\n", msg->new_dev.address);

    esp_err_t err = usb_host_device_open(_ubcClientHdl, msg->new_dev.address, &_ubcDevHdl);
    if (err != ESP_OK) {
      Serial.printf("[USB] Erro ao abrir dispositivo: %d\n", err);
      return;
    }

    // Obtém o config descriptor ativo
    const usb_config_desc_t* cfgDesc;
    err = usb_host_get_active_config_descriptor(_ubcDevHdl, &cfgDesc);
    if (err != ESP_OK) {
      Serial.printf("[USB] Erro config descriptor: %d\n", err);
      usb_host_device_close(_ubcClientHdl, _ubcDevHdl);
      _ubcDevHdl = NULL;
      return;
    }

    // Procura endpoint HID
    if (!_ubcFindEndpoint(cfgDesc)) {
      Serial.println("[USB] Dispositivo sem interface HID - ignorado");
      usb_host_device_close(_ubcClientHdl, _ubcDevHdl);
      _ubcDevHdl = NULL;
      return;
    }

    // Reivindica a interface HID
    err = usb_host_interface_claim(_ubcClientHdl, _ubcDevHdl, _ubcIfaceNum, 0);
    if (err != ESP_OK) {
      Serial.printf("[USB] Erro ao reivindicar interface: %d\n", err);
      usb_host_device_close(_ubcClientHdl, _ubcDevHdl);
      _ubcDevHdl = NULL;
      return;
    }

    // Aloca transfer para leitura IN
    int allocSize = (_ubcEpMPS > 64) ? _ubcEpMPS : 64;
    err = usb_host_transfer_alloc(allocSize, 0, &_ubcInXfer);
    if (err != ESP_OK) {
      Serial.printf("[USB] Erro alocar transfer: %d\n", err);
      usb_host_interface_release(_ubcClientHdl, _ubcDevHdl, _ubcIfaceNum);
      usb_host_device_close(_ubcClientHdl, _ubcDevHdl);
      _ubcDevHdl = NULL;
      return;
    }

    // Configura o transfer de leitura contínua (Interrupt IN)
    _ubcInXfer->device_handle = _ubcDevHdl;
    _ubcInXfer->bEndpointAddress = _ubcEpAddr;
    _ubcInXfer->callback = _ubcXferCb;
    _ubcInXfer->context = NULL;
    _ubcInXfer->num_bytes = _ubcEpMPS;
    _ubcInXfer->timeout_ms = 0;

    memset(_ubcPrevKeys, 0, sizeof(_ubcPrevKeys));
    _ubcAccumIdx = 0;

    // Submete primeiro transfer
    err = usb_host_transfer_submit(_ubcInXfer);
    if (err == ESP_OK) {
      _ubcDevConnected = true;
      Serial.println("[USB] Leitor de barras USB conectado e pronto!");
    } else {
      Serial.printf("[USB] Erro submit transfer: %d\n", err);
      usb_host_transfer_free(_ubcInXfer); _ubcInXfer = NULL;
      usb_host_interface_release(_ubcClientHdl, _ubcDevHdl, _ubcIfaceNum);
      usb_host_device_close(_ubcClientHdl, _ubcDevHdl); _ubcDevHdl = NULL;
    }
  }
  else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
    // --- Dispositivo desconectado ---
    Serial.println("[USB] Leitor de barras desconectado");
    _ubcDevConnected = false;
    if (_ubcInXfer) {
      usb_host_transfer_free(_ubcInXfer);
      _ubcInXfer = NULL;
    }
    if (_ubcDevHdl) {
      usb_host_interface_release(_ubcClientHdl, _ubcDevHdl, _ubcIfaceNum);
      usb_host_device_close(_ubcClientHdl, _ubcDevHdl);
      _ubcDevHdl = NULL;
    }
  }
}

// ===== Task: USB Host daemon (processa eventos da biblioteca) =====
static void _ubcDaemonTask(void* arg) {
  while (true) {
    uint32_t flags;
    usb_host_lib_handle_events(portMAX_DELAY, &flags);
  }
}

// ===== Task: USB Host client (processa eventos de dispositivos) =====
static void _ubcClientTask(void* arg) {
  usb_host_client_config_t ccfg;
  memset(&ccfg, 0, sizeof(ccfg));
  ccfg.is_synchronous = false;
  ccfg.max_num_event_msg = 5;
  ccfg.async.client_event_callback = _ubcClientCb;
  ccfg.async.callback_arg = NULL;

  esp_err_t err = usb_host_client_register(&ccfg, &_ubcClientHdl);
  if (err != ESP_OK) {
    Serial.printf("[USB] Erro registar cliente: %d\n", err);
    vTaskDelete(NULL);
    return;
  }

  Serial.println("[USB] Cliente USB registado, aguardando dispositivos...");
  while (true) {
    usb_host_client_handle_events(_ubcClientHdl, portMAX_DELAY);
  }
}

// ============ API PÚBLICA ============

// Inicializa USB Host para leitura de barras (chamar no setup())
void usbBarcodeInit() {
  if (_ubcInitDone) return;

  _ubcMutex = xSemaphoreCreateMutex();
  memset(_ubcPrevKeys, 0, sizeof(_ubcPrevKeys));

  usb_host_config_t hcfg;
  memset(&hcfg, 0, sizeof(hcfg));
  hcfg.skip_phy_setup = false;
  hcfg.intr_flags = ESP_INTR_FLAG_LEVEL1;

  esp_err_t err = usb_host_install(&hcfg);
  if (err != ESP_OK) {
    Serial.printf("[USB] Erro instalar USB Host: %d\n", err);
    return;
  }

  // Task daemon no core 0 (core 1 para Arduino loop)
  xTaskCreatePinnedToCore(_ubcDaemonTask, "usbDaemon", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(_ubcClientTask, "usbClient", 4096, NULL, 2, NULL, 0);

  _ubcInitDone = true;
  Serial.println("[USB] USB Host inicializado para leitor de barras");
}

// Leitura não-bloqueante de código de barras USB
// Retorna String com o código se disponível, ou "" se nada
String usbBarcodeRead() {
  if (!_ubcInitDone || !_ubcReady) return "";

  String res = "";
  if (xSemaphoreTake(_ubcMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    if (_ubcReady) {
      res = String(_ubcResult);
      _ubcReady = false;
    }
    xSemaphoreGive(_ubcMutex);
  }

  if (res.length() > 0) {
    res.toLowerCase();
    res.trim();
    Serial.print("[USB-BARCODE] Código: ");
    Serial.println(res);
  }
  return res;
}

// Verifica se leitor USB está conectado
bool usbBarcodeConnected() {
  return _ubcDevConnected;
}

#endif // CONFIG_IDF_TARGET_ESP32S3
#endif // USB_BARCODE_H
