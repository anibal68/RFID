#ifndef ESTACOES_H
#define ESTACOES_H

#include <Arduino.h>

// Estrutura para cada estação
struct Estacao {
  int id;
  String nome;
  String descricao;
};

// Array com 50 estações
const Estacao estacoes[] = {
  {1, "EST_01", "Corte de materiais"},
  {2, "EST_02", "Preparação de pças"},
  {3, "EST_03", "Montagem A"},
  {4, "EST_04", "Montagem B"},
  {5, "EST_05", "Soldadura 1"},
  {6, "EST_06", "Soldadura 2"},
  {7, "EST_07", "Tratamento térmico"},
  {8, "EST_08", "Polimento"},
  {9, "EST_09", "Pintura 1"},
  {10, "EST_10", "Pintura 2"},
  {11, "EST_11", "Secagem"},
  {12, "EST_12", "Controlo visual"},
  {13, "EST_13", "Embalagem A"},
  {14, "EST_14", "Embalagem B"},
  {15, "EST_15", "Etiquetagem"},
  {16, "EST_16", "Armazenamento"},
  {17, "EST_17", "Eletrónica A"},
  {18, "EST_18", "Eletrónica B"},
  {19, "EST_19", "Testes A"},
  {20, "EST_20", "Testes B"},
  {21, "EST_21", "Calibração"},
  {22, "EST_22", "Programação"},
  {23, "EST_23", "Revisão final"},
  {24, "EST_24", "Gravação"},
  {25, "EST_25", "Inspeção"},
  {26, "EST_26", "Dobragem"},
  {27, "EST_27", "Corte laser"},
  {28, "EST_28", "Furação"},
  {29, "EST_29", "Roscagem"},
  {30, "EST_30", "Acabamento A"},
  {31, "EST_31", "Acabamento B"},
  {32, "EST_32", "Limpeza"},
  {33, "EST_33", "Desengorduramento"},
  {34, "EST_34", "Zincagem"},
  {35, "EST_35", "Cromagem"},
  {36, "EST_36", "Anodização"},
  {37, "EST_37", "Verniz A"},
  {38, "EST_38", "Verniz B"},
  {39, "EST_39", "Estufa"},
  {40, "EST_40", "Resfriamento"},
  {41, "EST_41", "Montagem final"},
  {42, "EST_42", "Encaixe"},
  {43, "EST_43", "Aparafusamento"},
  {44, "EST_44", "Colagem"},
  {45, "EST_45", "Prensa"},
  {46, "EST_46", "Cura"},
  {47, "EST_47", "Verificação"},
  {48, "EST_48", "Ajuste fino"},
  {49, "EST_49", "Embalagem final"},
  {50, "EST_50", "Expedição"}
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

// Busca ID pela descrição (parcial)
int procurarEstacaoPorNome(String nome) {
  for (int i = 0; i < NUM_ESTACOES; i++) {
    if (estacoes[i].nome == nome) {
      return estacoes[i].id;
    }
  }
  return -1;
}

// Lista todas as estações no Serial (debug)
void listarEstacoes() {
  Serial.println("\n===== ESTACOES DISPONIVEIS (50 TOTAL) =====");
  for (int i = 0; i < NUM_ESTACOES; i++) {
    Serial.print(estacoes[i].id);
    Serial.print(" - ");
    Serial.print(estacoes[i].nome);
    Serial.print(" | ");
    Serial.println(estacoes[i].descricao);
  }
  Serial.println("=========================================\n");
}

#endif // ESTACOES_H
