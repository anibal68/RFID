// Lista de estações (réplica de estacoes.h do ESP32)
const ESTACOES = [
  { id: 1, nome: "Corte de materiais" },
  { id: 2, nome: "Preparação de pças" },
  { id: 3, nome: "Montagem A" },
  { id: 4, nome: "Montagem B" },
  { id: 5, nome: "Soldadura 1" },
  { id: 6, nome: "Soldadura 2" },
  { id: 7, nome: "Tratamento térmico" },
  { id: 8, nome: "Polimento" },
  { id: 9, nome: "Pintura 1" },
  { id: 10, nome: "Pintura 2" },
  { id: 11, nome: "Secagem" },
  { id: 12, nome: "Controlo visual" },
  { id: 13, nome: "Embalagem A" },
  { id: 14, nome: "Embalagem B" },
  { id: 15, nome: "Etiquetagem" },
  { id: 16, nome: "Armazenamento" },
  { id: 17, nome: "Eletrónica A" },
  { id: 18, nome: "Eletrónica B" },
  { id: 19, nome: "Testes A" },
  { id: 20, nome: "Testes B" },
  { id: 21, nome: "Calibração" },
  { id: 22, nome: "Programação" },
  { id: 23, nome: "Revisão final" },
  { id: 24, nome: "Gravação" },
  { id: 25, nome: "Inspeção" },
  { id: 26, nome: "Dobragem" },
  { id: 27, nome: "Corte laser" },
  { id: 28, nome: "Furação" },
  { id: 29, nome: "Roscagem" },
  { id: 30, nome: "Acabamento A" },
  { id: 31, nome: "Acabamento B" },
  { id: 32, nome: "Limpeza" },
  { id: 33, nome: "Desengorduramento" },
  { id: 34, nome: "Zincagem" },
  { id: 35, nome: "Cromagem" },
  { id: 36, nome: "Anodização" },
  { id: 37, nome: "Verniz A" },
  { id: 38, nome: "Verniz B" },
  { id: 39, nome: "Estufa" },
  { id: 40, nome: "Resfriamento" },
  { id: 41, nome: "Montagem final" },
  { id: 42, nome: "Encaixe" },
  { id: 43, nome: "Aparafusamento" },
  { id: 44, nome: "Colagem" },
  { id: 45, nome: "Prensa" },
  { id: 46, nome: "Cura" },
  { id: 47, nome: "Verificação" },
  { id: 48, nome: "Ajuste fino" },
  { id: 49, nome: "Embalagem final" },
  { id: 50, nome: "Expedição" },
];

function procurarEstacao(id) {
  const e = ESTACOES.find((e) => e.id === id);
  return e ? e.nome : "NAO_ENCONTRADA";
}

function procurarEstacaoPorNome(nome) {
  const e = ESTACOES.find((e) => e.nome === nome);
  return e ? e.id : -1;
}

function procurarIndiceEstacao(nome) {
  const idx = ESTACOES.findIndex((e) => e.nome === nome);
  return idx >= 0 ? idx : 0;
}
