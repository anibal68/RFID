#ifndef ESTACOES_H
#define ESTACOES_H

#include <Arduino.h>

// Estrutura para cada estação (apenas id e nome)
struct Estacao {
  int id;
  String nome;
};

// Array com 115 estações
const Estacao estacoes[] = {
  {1, "E - Lonas"},
  {2, "P - Tekas"},
  {3, "W - Acabamentos Montagem"},
  {4, "K - TopCoat"},
  {5, "A - Teste D'Água"},
  {6, "W - CNC - LECTRA"},
  {7, "H - Preparação"},
  {8, "H - Lam 1"},
  {9, "A - Sikas"},
  {10, "A - Auditoria Gate 5"},
  {11, "T - Medium Parts"},
  {12, "K - Liner/Banheira"},
  {13, "E - CNC"},
  {14, "W - CNC - MORBIDELLI"},
  {15, "A - União"},
  {16, "A - Open Hull 01"},
  {17, "D - Open Deck 01"},
  {18, "C - Reparação"},
  {19, "P - Marcação"},
  {20, "H - Skiin"},
  {21, "H - Cabine de Pintura"},
  {22, "C - Teste D'Água"},
  {23, "C - Final"},
  {24, "N - Empaste"},
  {25, "K - POP"},
  {26, "A - Open Hull"},
  {27, "M - Repassagem"},
  {28, "D - Sikas"},
  {29, "S - Sikas"},
  {30, "T - Coberta"},
  {31, "A - Final"},
  {32, "K - Estruturas"},
  {33, "D - Open Hull 02"},
  {34, "T - Small Parts"},
  {35, "E - Montagem Estofos"},
  {36, "H - Repassagem"},
  {37, "A - Auditoria Gate 6"},
  {38, "& - Empaste Big Parts"},
  {39, "H - Stiffen"},
  {40, "S - Repassagem"},
  {41, "D - Teste D'Água"},
  {42, "B - Reparação"},
  {43, "H - Lam 2"},
  {44, "C - União"},
  {45, "M - Stiffen"},
  {46, "H - POP"},
  {47, "A - Embalamento"},
  {48, "P - Tampas"},
  {49, "S - Costura"},
  {50, "E - Bancos"},
  {51, "D - Auditoria Gate 5"},
  {52, "H - União Liner"},
  {53, "N - Reparação"},
  {54, "B - Open Hull"},
  {55, "D - Auditoria Gate 6"},
  {56, "P - Móveis/Bancos"},
  {57, "S - Tanques"},
  {58, "K - Marcação"},
  {59, "C - Embalamento"},
  {60, "H - Trampson/Espumas"},
  {61, "B - Sikas"},
  {62, "N - Ceras"},
  {63, "Rework"},
  {64, "C - Embalamento"},
  {65, "B - Open Deck"},
  {66, "M - Madeiras"},
  {67, "M - Cabine de Pintura"},
  {68, "W - Pick Assy/PM"},
  {69, "M - Preparação"},
  {70, "P - Topcoat"},
  {71, "B - Estruturas"},
  {72, "S - Final"},
  {73, "W - Madeiras Laminação"},
  {74, "S - Cabine de Pintura"},
  {75, "M - Skin"},
  {76, "K - Skin"},
  {77, "S - Skin"},
  {78, "A - Teste D'água"},
  {79, "& - Reparação Laminação"},
  {80, "D - Stiffen"},
  {81, "D - Open Deck 02"},
  {82, "B - Auditoria Gate 5"},
  {83, "M - Estrutura"},
  {84, "S - Preparação"},
  {85, "& - Empaste Small Parts"},
  {86, "E - Pick"},
  {87, "W - Espumas Laminação"},
  {88, "C - Open Deck"},
  {89, "M - TopCoat"},
  {90, "S - POP"},
  {91, "& - Reparação Big Parts"},
  {92, "M - POP"},
  {93, "C - Open Hull"},
  {94, "D - Final"},
  {95, "C - Auditoria Gate 5"},
  {96, "S - Colagem"},
  {97, "C - Auditoria Gate 6"},
  {98, "W - CNC - FANUC"},
  {99, "D - Reparação"},
  {100, "E - Tapizados"},
  {101, "K - Cabine de Pintura"},
  {102, "A - Open Deck"},
  {103, "P - Liners/Hardtop"},
  {104, "K - Repassagem"},
  {105, "T - Casco"},
  {106, "B - Embalamento"},
  {107, "K - Stiffen"},
  {108, "B - União"},
  {109, "K - Preparação"},
  {110, "& - Reparação Small Parts"},
  {111, "D - União"},
  {112, "A - Consola/Módulos"},
  {113, "P - MORBIDELLI"},
  {114, "T - BottomPaint"},
  {115, "A - Reparação"}
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
  Serial.println("\n===== ESTACOES DISPONIVEIS (115 TOTAL) =====");
  for (int i = 0; i < NUM_ESTACOES; i++) {
    Serial.print(estacoes[i].id);
    Serial.print(" - ");
    Serial.println(estacoes[i].nome);
  }
  Serial.println("=========================================\n");
}

#endif // ESTACOES_H
