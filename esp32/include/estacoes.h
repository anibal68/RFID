#ifndef ESTACOES_H
#define ESTACOES_H

#include <Arduino.h>

// Estrutura para cada estação (apenas id e nome)
struct Estacao {
  int id;
  String nome;
};

// Array com 50 estações
const Estacao estacoes[] = {
  {1, "Corte de materiais"},
  {2, "Preparação de pças"},
  {3, "Montagem A"},
  {4, "Montagem B"},
  {5, "Soldadura 1"},
  {6, "Soldadura 2"},
  {7, "Tratamento térmico"},
  {8, "Polimento"},
  {9, "Pintura 1"},
  {10, "Pintura 2"},
  {11, "Secagem"},
  {12, "Controlo visual"},
  {13, "Embalagem A"},
  {14, "Embalagem B"},
  {15, "Etiquetagem"},
  {16, "Armazenamento"},
  {17, "Eletrónica A"},
  {18, "Eletrónica B"},
  {19, "Testes A"},
  {20, "Testes B"},
  {21, "Calibração"},
  {22, "Programação"},
  {23, "Revisão final"},
  {24, "Gravação"},
  {25, "Inspeção"},
  {26, "Dobragem"},
  {27, "Corte laser"},
  {28, "Furação"},
  {29, "Roscagem"},
  {30, "Acabamento A"},
  {31, "Acabamento B"},
  {32, "Limpeza"},
  {33, "Desengorduramento"},
  {34, "Zincagem"},
  {35, "Cromagem"},
  {36, "Anodização"},
  {37, "Verniz A"},
  {38, "Verniz B"},
  {39, "Estufa"},
  {40, "Resfriamento"},
  {41, "Montagem final"},
  {42, "Encaixe"},
  {43, "Aparafusamento"},
  {44, "Colagem"},
  {45, "Prensa"},
  {46, "Cura"},
  {47, "Verificação"},
  {48, "Ajuste fino"},
  {49, "Embalagem final"},
  {50, "Expedição"}
};

const int NUM_ESTACOES = sizeof(estacoes) / sizeof(estacoes[0]);

// ===== FUNÇÕES HELPER =====

// Busca estação pelo ID
String procurarEstacao(int id) {
  for (int i = 0; i < NUM_ESTACOES; i++) {
    if (estacoes[i].id == id) {
      return estacoes[i].nome;
    }
  }
  return "NAO_ENCONTRADA";
}

// Busca ID pelo nome
int procurarEstacaoPorNome(String nome) {
  for (int i = 0; i < NUM_ESTACOES; i++) {
    if (estacoes[i].nome == nome) {
      return estacoes[i].id;
    }
  }
  return -1;
}

// Busca ÍNDICE do array pela nome da estação
int procurarIndiceEstacao(String nome) {
  for (int i = 0; i < NUM_ESTACOES; i++) {
    if (estacoes[i].nome == nome) {
      return i;  // Retorna índice (0-49)
    }
  }
  return 0;  // Default ao índice 0 se não encontrar
}

// Lista todas as estações no Serial (debug)
void listarEstacoes() {
  Serial.println("\n===== ESTACOES DISPONIVEIS (50 TOTAL) =====");
  for (int i = 0; i < NUM_ESTACOES; i++) {
    Serial.print(estacoes[i].id);
    Serial.print(" - ");
    Serial.println(estacoes[i].nome);
  }
  Serial.println("=========================================\n");
}

#endif // ESTACOES_H
