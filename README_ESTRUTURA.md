# RFID - Estrutura do Projeto

O projeto está organizado em 3 pastas principais, separadas por tipo de código:

## 📁 Estrutura

```
RFID/
├── esp32/                    # Código para o microcontrolador ESP32
│   ├── platformio.ini        # Configuração do PlatformIO
│   ├── src/                  # Código-fonte C++
│   ├── include/              # Headers customizados
│   ├── lib/                  # Bibliotecas locais
│   └── test/                 # Testes
│
├── python/                   # Scripts e aplicações Python
│   ├── rfid_simulator.py     # Simulador RFID
│   └── run_simulator.*       # Scripts para executar o simulador
│
├── web/                      # Código Web (Frontend + Backend serverless)
│   ├── app.js                # Lógica da aplicação web
│   ├── index.html            # Interface HTML
│   ├── styles.css            # Estilos CSS
│   └── netlify/              # Funções serverless Netlify
│       └── functions/        # Funções AWS Lambda/Netlify
│
└── [outros arquivos de configuração]
```

## 🎯 Como usar cada seção

### ESP32 (`esp32/`)
- Projeto PlatformIO para upload no microcontrolador
- Execute: `platformio run --project-dir esp32 -t upload`

### Python (`python/`)
- Scripts utilitários e simuladores
- Execute: `python python/rfid_simulator.py`

### Web (`web/`)
- Aplicação web e APIs serverless
- Frontend: `web/index.html` + `app.js`
- Backend: Funções em `web/netlify/functions/`
