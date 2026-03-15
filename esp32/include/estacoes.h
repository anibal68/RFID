#ifndef ESTACOES_H
#define ESTACOES_H

#include <Arduino.h>

// Estrutura para cada estação (UUID + nome)
struct Estacao {
  String id;
  String nome;
};

// Array com 115 estações (ordenado alfabeticamente)
const Estacao estacoes[] = {
  {"4809dd62-cff1-41dd-aaa4-75951dcca0b5", "& - Empaste Big Parts"},
  {"be9027a7-5312-4e5b-8b87-d677edfd6120", "& - Empaste Small Parts"},
  {"cf859d36-559d-4d19-8d65-b92024248752", "& - Reparação Big Parts"},
  {"b3dc8e86-0fdb-442f-86c6-d623291c44a4", "& - Reparação Laminação"},
  {"f29ca7c5-8742-4af0-adb7-ffa38ebe3733", "& - Reparação Small Parts"},
  {"11c613de-178e-46ad-96f3-0a76fe1cecdf", "A - Auditoria Gate 5"},
  {"47ff6f0d-84a0-4579-a91f-b2a4171f772c", "A - Auditoria Gate 6"},
  {"5e8e3a8b-fbfe-4b99-a5f0-04f1940c43b0", "A - Embalamento"},
  {"38de4553-88a8-42c0-857c-b33db358ae07", "A - Final"},
  {"e5751dce-8e8a-476c-a1db-bd38a8ef38ac", "A - Open Deck"},
  {"2d3f3602-fde7-48af-81df-023883913ce9", "A - Open Hull"},
  {"ffb19a83-8dad-4c86-918f-62c6f9298656", "A - Reparação"},
  {"119677d7-3091-4ba7-a418-01cf647e7390", "A - Sikas"},
  {"b32fe739-76e2-4539-8dc6-0b87145c2657", "A - Teste D´água"},
  {"16e0ee37-2de7-48fe-81d3-6e310179861b", "A - União"},
  {"b703edad-b00f-4387-a562-056dc6bf9c3b", "B - Auditoria Gate 5"},
  {"4d1e81bd-393d-4f57-b750-40a641acc392", "B - Auditoria Gate 6"},
  {"f01218f5-628e-4a8c-bdc9-99aa6aa1195d", "B - Embalamento"},
  {"a4ee50f5-a293-4482-adf7-a8600656d0ab", "B - Final"},
  {"941eb7a4-79df-4c22-96c7-14c9979a7c25", "B - Open Deck"},
  {"7cf74f95-cb93-4790-8d6f-d13b74d8624b", "B - Open Hull"},
  {"52571c30-3a9b-4950-a3fa-1cda115f2c44", "B - Reparação"},
  {"84058798-fec0-47d1-8e46-1d9086f243c5", "B - Sikas"},
  {"08e18327-365e-497e-970f-2ee6a0c996a0", "B - Teste D´Água"},
  {"f2117c67-de6c-4b97-95e3-0e70f8ff729e", "B - União"},
  {"d63f9b18-7c4f-48f5-908c-ee7483e879c0", "C - Auditoria Gate 5"},
  {"d736b465-9f45-4ad1-b3c2-fd1c2042d9b4", "C - Auditoria Gate 6"},
  {"8b3aa2bd-d126-402c-a4b8-89276b52e3ed", "C - Embalamento"},
  {"248cd7f9-0b7f-4c55-9a44-e1bef6ed8f80", "C - Final"},
  {"c38604f5-d3e6-4578-8e35-e7e96bd1150a", "C - Open Deck"},
  {"d355ae93-52ac-4ea5-9170-c80639b6d847", "C - Open Hull"},
  {"19a34490-c93f-496e-aeb1-2f9ffcd02cb8", "C - Reparação"},
  {"31c0c0cf-6ca3-49c2-aec6-0aac9e75c6cc", "C - Sikas"},
  {"1fe603e1-b588-4da8-be43-76c108de2c86", "C - Teste D´Água"},
  {"59f505cf-24b4-4a89-a00d-629d5503fe99", "C - União"},
  {"67c9b3ef-e386-4a28-beb2-1c1949237b96", "D - Auditoria Gate 5"},
  {"7d812f51-f94e-45c8-937c-2d12d194d777", "D - Auditoria Gate 6"},
  {"81acaffe-8692-4e51-982c-9b4afce9065b", "D - Embalamento"},
  {"d5d58b3a-cb7a-4a7b-9c19-d719759adc99", "D - Final"},
  {"1910b5e6-728e-4981-9d3d-d1e4fe27541c", "D - Open Deck 01"},
  {"b50d91e6-3638-4138-a924-8261c9169bc7", "D - Open Deck 02"},
  {"18df9d1e-dce0-4bbb-8484-34c7aef801ec", "D - Open Hull 01"},
  {"3bc3a18b-acc8-46d3-b128-51b36a29d0a4", "D - Open Hull 02"},
  {"dd44b316-6f45-47ae-8e88-f67e72eb3fb7", "D - Reparação"},
  {"31af6b19-f4e6-4f22-8a26-2fbc2283ed01", "D - Sikas"},
  {"50b6a0d4-1718-448a-a7e4-2240b0c45c35", "D - Teste D´Água"},
  {"f456953e-c573-4e6e-b5da-6b072fc2e47c", "D - União"},
  {"658865bc-b73b-40cc-b0a6-8f4ae8591ff1", "E - Bancos"},
  {"15dd49af-3272-40ec-99d0-fe31fe955629", "E - CNC"},
  {"61bf9c2e-11a6-4dfa-8138-f1b5e1c48209", "E - Costura"},
  {"034afd64-e502-4450-83e1-c2200dad8906", "E - Lonas"},
  {"41917f13-b691-4931-acfe-4e9b8beec974", "E - Montagem Estofos"},
  {"c05d1599-3e2c-404d-985e-cdc1a371c914", "E - Pick"},
  {"dfd1129d-028e-4dc2-bca2-8be11bea0924", "E - Tapizados"},
  {"1e6d417e-6b2e-40f8-ac17-986ab994b356", "H - Cabine de Pintura"},
  {"a41ef798-13ac-4935-baed-1921e5b8f36b", "H - Estruturas"},
  {"108faac8-dfe4-402d-8694-42ccc2b297b8", "H - Lam 1"},
  {"571c7ff3-7287-421f-af0a-d82de68144b6", "H - Lam 2"},
  {"5c4c911a-937b-48b5-9e77-d29c346ab305", "H - POP"},
  {"0d08de2e-6e15-418d-bc7d-44cec587b940", "H - Preparação"},
  {"474f4577-99a2-4f8d-97cf-40929a3f5fae", "H - Repassagem"},
  {"1b9142f1-53e5-4b4c-9412-65220a6b6351", "H - Skiin"},
  {"49700c6c-fa90-4138-8b36-aa5b9ff786de", "H - Stiffen"},
  {"a3e2ada3-325b-4913-a603-0e5a72bfd5e2", "H - Topcoat"},
  {"81ebe876-2452-4080-b519-cc94937e04f3", "H - Trampson/Espumas"},
  {"74d16309-6c9a-4418-9842-9323c17af104", "H - União Liner"},
  {"e110a298-3856-4334-8051-67a2c4250c75", "K - Cabine de Pintura"},
  {"3ad14703-8d9e-406a-a6a9-b1517ee1a826", "K - Estruturas"},
  {"128e3a69-c1fc-4b99-8877-25300dcbefd7", "K - Liner/Banheira"},
  {"80f82655-4eff-4949-a039-eba18a04ac0c", "K - Marcação"},
  {"2a269f21-05bd-417b-b10e-1b7c4c6f37ef", "K - POP"},
  {"f23ba5e3-8d64-4a36-968e-8e6bee108011", "K - Preparação"},
  {"ed028920-4fcc-44fa-b06e-ceb1aa64145d", "K - Repassagem"},
  {"b0df4adc-442f-40bc-b3b7-7f0cd4185579", "K - Skin"},
  {"f0712ec6-1bfb-464b-a220-ab5424aa12a4", "K - Stiffen"},
  {"0838415f-2cb5-4d2c-8a73-fa58acd44c88", "K - TopCoat"},
  {"98dba1f4-c677-43a2-b4a8-edb5937dcd27", "M - Cabine de Pintura"},
  {"bcbb1d9f-862c-4a5e-9d50-16deeac0ab34", "M - Estrutura"},
  {"1ac24cb0-8d65-435a-9a0f-df32c28253c1", "M - Marcação"},
  {"d13d15f9-27f4-4077-a3c2-ab5eb0c5a767", "M - POP"},
  {"a111048f-e947-47f2-b7b9-718418b16bd9", "M - Preparação"},
  {"2e5dfa9d-6177-4d19-be3f-9a685c8e2557", "M - Repassagem"},
  {"aa465b87-f5a4-4f22-96ba-04d580623aae", "M - Skin"},
  {"5aaa783e-2702-46dc-9781-3f92726bfcf2", "M - Stiffen"},
  {"c8d6eff8-72c5-4111-a68f-0da332de57f2", "M - TopCoat"},
  {"8819b8d7-e3db-483c-9484-d46ae79adb8c", "N - Ceras"},
  {"24d0be70-e1c2-4fac-a22f-fa763c15e123", "N - Empaste"},
  {"766db194-c183-4747-8352-83e4d7b48c66", "N - Reparação"},
  {"f96ab930-98a8-4f4a-b9fb-09b1eed06a93", "P - Consola/Módulos"},
  {"ea80f970-f3c9-44ef-9d04-2a7457425b1c", "P - Liners/Hardtop"},
  {"94bc4c6d-99ee-4543-8ae7-990b9ea60178", "P - Madeiras"},
  {"7e259ab1-1a42-4405-a5d1-35a62282fa35", "P - Móveis/Bancos"},
  {"6069bed1-6d35-4ed2-8abe-9f2270c18c43", "P - Tampas"},
  {"7e8a6b31-d97a-4feb-8478-8346231c84db", "P - Tanques"},
  {"06e8ef5a-ceb0-46f1-8602-fd2b285ac976", "P - Tekas"},
  {"8ad6203a-2e6e-4165-a008-58b781e15b38", "Rework"},
  {"aa08ac44-aa97-4f74-971e-855d2f3d5180", "S - Cabine de Pintura"},
  {"d64846c6-5129-42dc-958b-c2b756bb029f", "S - Colagem"},
  {"cbb3f3c6-81a3-4717-898e-9241636bcf67", "S - POP"},
  {"bccdc7ac-22a6-4c93-b59b-33e835466741", "S - Preparação"},
  {"4f48f133-3c36-45dd-b89c-43abd11bad77", "S - Repassagem"},
  {"b318182f-b197-47e3-b5fd-f535e0f9a4ab", "S - Skin"},
  {"b4ac3b60-2642-433e-b7a8-5ccd23faca5d", "S - Stiffen"},
  {"fc1bca43-5142-4123-a151-ade04483e6a5", "T - BottomPaint"},
  {"ed77b5a4-f4e7-44c3-9c3e-d898be4ef342", "T - Casco"},
  {"337d1532-d516-4950-9ac5-a6d18f2615b2", "T - Coberta"},
  {"120167f7-48d6-44fe-ba73-867e7b8fe3d7", "T - Medium Parts"},
  {"3decaadc-ba32-4979-82b1-5f61f631e9ba", "T - Small Parts"},
  {"07e2697c-cb5d-426a-9bc2-8c1f4fd8e65c", "W - Acabamentos Montagem"},
  {"db82516f-8f5e-4bda-9f2d-7603cadb7e58", "W - CNC - FANUC"},
  {"0c84ce31-975e-47c6-a1ec-6d1b9e0b326b", "W - CNC - LECTRA"},
  {"166fb3e5-4e27-4834-ab21-cd8e9a5c464b", "W - CNC - MORBIDELLI"},
  {"c2cdd90e-f1be-4d85-ad50-47ff039c2408", "W - Espumas Laminação"},
  {"a567bbbb-0983-4104-bf5c-3706c51df240", "W - Madeiras Laminação"},
  {"994fbb9c-75f2-4bcc-a93d-0a6686c9ee92", "W - Pick Assy/PM"}
};

const int NUM_ESTACOES = sizeof(estacoes) / sizeof(estacoes[0]);

// ===== FUNÇÕES HELPER =====

// Busca nome da estação pelo UUID
String procurarEstacao(String id) {
  for (int i = 0; i < NUM_ESTACOES; i++) {
    if (estacoes[i].id == id) {
      return estacoes[i].nome;
    }
  }
  return "NAO_ENCONTRADA";
}

// Busca UUID pelo nome
String procurarEstacaoPorNome(String nome) {
  for (int i = 0; i < NUM_ESTACOES; i++) {
    if (estacoes[i].nome == nome) {
      return estacoes[i].id;
    }
  }
  return "";
}

// Busca ÍNDICE do array pelo nome da estação
int procurarIndiceEstacao(String nome) {
  for (int i = 0; i < NUM_ESTACOES; i++) {
    if (estacoes[i].nome == nome) {
      return i;
    }
  }
  return 0;  // Default ao índice 0 se não encontrar
}

// Busca nome da estação pelo índice do array
String procurarEstacaoPorIndice(int idx) {
  if (idx >= 0 && idx < NUM_ESTACOES) {
    return estacoes[idx].nome;
  }
  return "NAO_ENCONTRADA";
}

// Lista todas as estações no Serial (debug)
void listarEstacoes() {
  Serial.println("\n===== ESTACOES DISPONIVEIS (115 TOTAL) =====");
  for (int i = 0; i < NUM_ESTACOES; i++) {
    Serial.print(estacoes[i].id);
    Serial.print(" - ");
    Serial.println(estacoes[i].nome);
  }
  Serial.println("=========================================");
}

#endif // ESTACOES_H
