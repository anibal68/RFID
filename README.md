# RFID Simulator

Este projeto tem duas versões do simulador:

- Desktop em Python (Tkinter)
- Web para tablet/telemóvel com Netlify (Opção B: frontend + backend seguro)

## 1) Versão Desktop (Python)

### Requisitos

- Windows com Python 3.10+
- Tkinter (normalmente já incluído)

### Configuração

1. Copie `.env.example` para `Config.env` (ou `.env`).
2. Preencha as variáveis necessárias para Supabase.

`Config.env` e `.env` estão no `.gitignore` para não enviar segredos.

### Executar

- `run_simulator.bat` (duplo clique)
- ou `run_simulator.ps1`
- ou `python .\rfid_simulator.py`

## 2) Versão Web (Netlify - Opção B)

### O que foi criado

- Frontend web: `web/`
	- `web/index.html`
	- `web/styles.css`
	- `web/app.js`
- Backend Netlify Functions: `netlify/functions/`
	- `button1.js` (insert em `tempos`)
	- `button2.js` (lookup em `barcos`)
	- `button3.js` (lookup em `operadores`)
	- `rfid.js` (validação de código RFID)
	- `_supabase.js` (helper)
- Configuração Netlify: `netlify.toml`

### Arquitetura

- Browser chama `/api/button1`, `/api/button2`, `/api/button3`, `/api/rfid`
- O backend em Netlify Functions fala com Supabase
- Chaves ficam no ambiente do Netlify (não expostas no frontend)

### Variáveis no Netlify (Site settings → Environment variables)

Use os nomes de `.env.example`:

- `SUPABASE_URL`
- `SUPABASE_SERVICE_ROLE_KEY`
- `OPERADOR_DEFAULT` (opcional, default `000747`)
- `BARCO_DEFAULT` (opcional, default `01010`)

### Deploy no Netlify

1. Importar o repo GitHub no Netlify
2. Build settings:
	 - Publish directory: `web`
	 - Functions directory: `netlify/functions`
3. Definir variáveis de ambiente
4. Fazer Deploy

### Teste local da versão web

Sem Functions (apenas UI):

```powershell
python -m http.server 8080
```

Com Functions (recomendado):

```powershell
npm i -g netlify-cli
netlify dev
```

## 3) Sincronização GitHub

```powershell
git add .
git commit -m "RFID simulator updates"
git pull --rebase origin main
git push origin main
```
