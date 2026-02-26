# RFID Simulator

Simulador local em Python (Tkinter) para o dispositivo RFID, com interface gráfica baseada na imagem de referência.

## Requisitos

- Windows com Python 3.10+
- Tkinter (já incluído na maioria das instalações Python)

## Configuração

1. Copie `.env.example` para `Config.env` (ou `.env`).
2. Preencha as variáveis:

```env
SUPABASE_URL=https://seu-projeto.supabase.co
SUPABASE_KEY=sua-chave
```

`Config.env` e `.env` estão no `.gitignore` para evitar envio de segredos.

## Executar

### Opção 1 (script)

- `run_simulator.bat` (duplo clique)
- ou `run_simulator.ps1`

### Opção 2 (terminal)

```powershell
python .\rfid_simulator.py
```

## Sincronização GitHub

```powershell
git add .
git commit -m "RFID simulator updates"
git pull --rebase origin main
git push origin main
```
