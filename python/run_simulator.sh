#!/bin/bash

################################################################################
# RFID SIMULATOR - Run Script para Linux
# Simula o funcionamento da caixa RFID
################################################################################

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Paths
PROJECT_DIR="/home/abt/Documentos/PlatformIO/Projects/RFID"
SIMULATOR_SCRIPT="rfid_simulator.py"
VENV_DIR="${PROJECT_DIR}/venv"
PYTHON_CMD="python3"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║         RFID SIMULATOR - Linux Launcher                     ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Verificar se o script existe
if [ ! -f "${PROJECT_DIR}/${SIMULATOR_SCRIPT}" ]; then
    echo -e "${RED}✗ Erro: Arquivo ${SIMULATOR_SCRIPT} não encontrado${NC}"
    echo -e "${RED}  Local esperado: ${PROJECT_DIR}/${SIMULATOR_SCRIPT}${NC}"
    exit 1
fi

# Navegar para o diretório do projeto
cd "${PROJECT_DIR}" || exit 1
echo -e "${GREEN}✓ Diretório: ${PROJECT_DIR}${NC}"

# Verificar e ativar virtual environment (se existir)
if [ -d "${VENV_DIR}" ]; then
    echo -e "${YELLOW}→ Ativando virtual environment...${NC}"
    source "${VENV_DIR}/bin/activate" 2>/dev/null
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Virtual environment ativado${NC}"
    else
        echo -e "${YELLOW}⚠ Aviso: Virtual environment não pôde ser ativado${NC}"
    fi
else
    echo -e "${YELLOW}→ Nenhum virtual environment encontrado${NC}"
fi

# Verificar se Python está instalado
if ! command -v $PYTHON_CMD &> /dev/null; then
    echo -e "${RED}✗ Erro: Python 3 não encontrado${NC}"
    echo -e "${RED}  Instale com: sudo apt-get install python3${NC}"
    exit 1
fi

PYTHON_VERSION=$($PYTHON_CMD --version 2>&1)
echo -e "${GREEN}✓ ${PYTHON_VERSION}${NC}"

# Verificar dependências do Tkinter
echo -e "${YELLOW}→ Verificando dependências...${NC}"
$PYTHON_CMD -c "import tkinter" 2>/dev/null
if [ $? -ne 0 ]; then
    echo -e "${RED}✗ Erro: Tkinter não encontrado${NC}"
    echo -e "${YELLOW}  Instale com: sudo apt-get install python3-tk${NC}"
    read -p "Deseja instalar agora? (s/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Ss]$ ]]; then
        sudo apt-get install python3-tk python3-dev -y
        if [ $? -ne 0 ]; then
            echo -e "${RED}✗ Instalação falhou${NC}"
            exit 1
        fi
    else
        exit 1
    fi
fi

echo -e "${GREEN}✓ Dependências verificadas${NC}"
echo ""

# Executar o simulador
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║           Iniciando Simulador RFID...                      ║${NC}"
echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Executar com tratamento de erros
$PYTHON_CMD "${PROJECT_DIR}/${SIMULATOR_SCRIPT}"
EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✓ Simulador encerrado com sucesso${NC}"
else
    echo -e "${RED}✗ Simulador encerrado com erro (código: ${EXIT_CODE})${NC}"
fi

exit $EXIT_CODE
