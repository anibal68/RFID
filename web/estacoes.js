// Lista de estações (réplica de estacoes.h do ESP32) - 115 estações ordenadas alfabeticamente
const ESTACOES = [
  { id: "4809dd62-cff1-41dd-aaa4-75951dcca0b5", nome: "& - Empaste Big Parts" },
  { id: "be9027a7-5312-4e5b-8b87-d677edfd6120", nome: "& - Empaste Small Parts" },
  { id: "cf859d36-559d-4d19-8d65-b92024248752", nome: "& - Reparação Big Parts" },
  { id: "b3dc8e86-0fdb-442f-86c6-d623291c44a4", nome: "& - Reparação Laminação" },
  { id: "f29ca7c5-8742-4af0-adb7-ffa38ebe3733", nome: "& - Reparação Small Parts" },
  { id: "11c613de-178e-46ad-96f3-0a76fe1cecdf", nome: "A - Auditoria Gate 5" },
  { id: "47ff6f0d-84a0-4579-a91f-b2a4171f772c", nome: "A - Auditoria Gate 6" },
  { id: "5e8e3a8b-fbfe-4b99-a5f0-04f1940c43b0", nome: "A - Embalamento" },
  { id: "38de4553-88a8-42c0-857c-b33db358ae07", nome: "A - Final" },
  { id: "e5751dce-8e8a-476c-a1db-bd38a8ef38ac", nome: "A - Open Deck" },
  { id: "2d3f3602-fde7-48af-81df-023883913ce9", nome: "A - Open Hull" },
  { id: "ffb19a83-8dad-4c86-918f-62c6f9298656", nome: "A - Reparação" },
  { id: "119677d7-3091-4ba7-a418-01cf647e7390", nome: "A - Sikas" },
  { id: "b32fe739-76e2-4539-8dc6-0b87145c2657", nome: "A - Teste D´água" },
  { id: "16e0ee37-2de7-48fe-81d3-6e310179861b", nome: "A - União" },
  { id: "b703edad-b00f-4387-a562-056dc6bf9c3b", nome: "B - Auditoria Gate 5" },
  { id: "4d1e81bd-393d-4f57-b750-40a641acc392", nome: "B - Auditoria Gate 6" },
  { id: "f01218f5-628e-4a8c-bdc9-99aa6aa1195d", nome: "B - Embalamento" },
  { id: "a4ee50f5-a293-4482-adf7-a8600656d0ab", nome: "B - Final" },
  { id: "941eb7a4-79df-4c22-96c7-14c9979a7c25", nome: "B - Open Deck" },
  { id: "7cf74f95-cb93-4790-8d6f-d13b74d8624b", nome: "B - Open Hull" },
  { id: "52571c30-3a9b-4950-a3fa-1cda115f2c44", nome: "B - Reparação" },
  { id: "84058798-fec0-47d1-8e46-1d9086f243c5", nome: "B - Sikas" },
  { id: "08e18327-365e-497e-970f-2ee6a0c996a0", nome: "B - Teste D´Água" },
  { id: "f2117c67-de6c-4b97-95e3-0e70f8ff729e", nome: "B - União" },
  { id: "d63f9b18-7c4f-48f5-908c-ee7483e879c0", nome: "C - Auditoria Gate 5" },
  { id: "d736b465-9f45-4ad1-b3c2-fd1c2042d9b4", nome: "C - Auditoria Gate 6" },
  { id: "8b3aa2bd-d126-402c-a4b8-89276b52e3ed", nome: "C - Embalamento" },
  { id: "248cd7f9-0b7f-4c55-9a44-e1bef6ed8f80", nome: "C - Final" },
  { id: "c38604f5-d3e6-4578-8e35-e7e96bd1150a", nome: "C - Open Deck" },
  { id: "d355ae93-52ac-4ea5-9170-c80639b6d847", nome: "C - Open Hull" },
  { id: "19a34490-c93f-496e-aeb1-2f9ffcd02cb8", nome: "C - Reparação" },
  { id: "31c0c0cf-6ca3-49c2-aec6-0aac9e75c6cc", nome: "C - Sikas" },
  { id: "1fe603e1-b588-4da8-be43-76c108de2c86", nome: "C - Teste D´Água" },
  { id: "59f505cf-24b4-4a89-a00d-629d5503fe99", nome: "C - União" },
  { id: "67c9b3ef-e386-4a28-beb2-1c1949237b96", nome: "D - Auditoria Gate 5" },
  { id: "7d812f51-f94e-45c8-937c-2d12d194d777", nome: "D - Auditoria Gate 6" },
  { id: "81acaffe-8692-4e51-982c-9b4afce9065b", nome: "D - Embalamento" },
  { id: "d5d58b3a-cb7a-4a7b-9c19-d719759adc99", nome: "D - Final" },
  { id: "1910b5e6-728e-4981-9d3d-d1e4fe27541c", nome: "D - Open Deck 01" },
  { id: "b50d91e6-3638-4138-a924-8261c9169bc7", nome: "D - Open Deck 02" },
  { id: "18df9d1e-dce0-4bbb-8484-34c7aef801ec", nome: "D - Open Hull 01" },
  { id: "3bc3a18b-acc8-46d3-b128-51b36a29d0a4", nome: "D - Open Hull 02" },
  { id: "dd44b316-6f45-47ae-8e88-f67e72eb3fb7", nome: "D - Reparação" },
  { id: "31af6b19-f4e6-4f22-8a26-2fbc2283ed01", nome: "D - Sikas" },
  { id: "50b6a0d4-1718-448a-a7e4-2240b0c45c35", nome: "D - Teste D´Água" },
  { id: "f456953e-c573-4e6e-b5da-6b072fc2e47c", nome: "D - União" },
  { id: "658865bc-b73b-40cc-b0a6-8f4ae8591ff1", nome: "E - Bancos" },
  { id: "15dd49af-3272-40ec-99d0-fe31fe955629", nome: "E - CNC" },
  { id: "61bf9c2e-11a6-4dfa-8138-f1b5e1c48209", nome: "E - Costura" },
  { id: "034afd64-e502-4450-83e1-c2200dad8906", nome: "E - Lonas" },
  { id: "41917f13-b691-4931-acfe-4e9b8beec974", nome: "E - Montagem Estofos" },
  { id: "c05d1599-3e2c-404d-985e-cdc1a371c914", nome: "E - Pick" },
  { id: "dfd1129d-028e-4dc2-bca2-8be11bea0924", nome: "E - Tapizados" },
  { id: "1e6d417e-6b2e-40f8-ac17-986ab994b356", nome: "H - Cabine de Pintura" },
  { id: "a41ef798-13ac-4935-baed-1921e5b8f36b", nome: "H - Estruturas" },
  { id: "108faac8-dfe4-402d-8694-42ccc2b297b8", nome: "H - Lam 1" },
  { id: "571c7ff3-7287-421f-af0a-d82de68144b6", nome: "H - Lam 2" },
  { id: "5c4c911a-937b-48b5-9e77-d29c346ab305", nome: "H - POP" },
  { id: "0d08de2e-6e15-418d-bc7d-44cec587b940", nome: "H - Preparação" },
  { id: "474f4577-99a2-4f8d-97cf-40929a3f5fae", nome: "H - Repassagem" },
  { id: "1b9142f1-53e5-4b4c-9412-65220a6b6351", nome: "H - Skiin" },
  { id: "49700c6c-fa90-4138-8b36-aa5b9ff786de", nome: "H - Stiffen" },
  { id: "a3e2ada3-325b-4913-a603-0e5a72bfd5e2", nome: "H - Topcoat" },
  { id: "81ebe876-2452-4080-b519-cc94937e04f3", nome: "H - Trampson/Espumas" },
  { id: "74d16309-6c9a-4418-9842-9323c17af104", nome: "H - União Liner" },
  { id: "e110a298-3856-4334-8051-67a2c4250c75", nome: "K - Cabine de Pintura" },
  { id: "3ad14703-8d9e-406a-a6a9-b1517ee1a826", nome: "K - Estruturas" },
  { id: "128e3a69-c1fc-4b99-8877-25300dcbefd7", nome: "K - Liner/Banheira" },
  { id: "80f82655-4eff-4949-a039-eba18a04ac0c", nome: "K - Marcação" },
  { id: "2a269f21-05bd-417b-b10e-1b7c4c6f37ef", nome: "K - POP" },
  { id: "f23ba5e3-8d64-4a36-968e-8e6bee108011", nome: "K - Preparação" },
  { id: "ed028920-4fcc-44fa-b06e-ceb1aa64145d", nome: "K - Repassagem" },
  { id: "b0df4adc-442f-40bc-b3b7-7f0cd4185579", nome: "K - Skin" },
  { id: "f0712ec6-1bfb-464b-a220-ab5424aa12a4", nome: "K - Stiffen" },
  { id: "0838415f-2cb5-4d2c-8a73-fa58acd44c88", nome: "K - TopCoat" },
  { id: "98dba1f4-c677-43a2-b4a8-edb5937dcd27", nome: "M - Cabine de Pintura" },
  { id: "bcbb1d9f-862c-4a5e-9d50-16deeac0ab34", nome: "M - Estrutura" },
  { id: "1ac24cb0-8d65-435a-9a0f-df32c28253c1", nome: "M - Marcação" },
  { id: "d13d15f9-27f4-4077-a3c2-ab5eb0c5a767", nome: "M - POP" },
  { id: "a111048f-e947-47f2-b7b9-718418b16bd9", nome: "M - Preparação" },
  { id: "2e5dfa9d-6177-4d19-be3f-9a685c8e2557", nome: "M - Repassagem" },
  { id: "aa465b87-f5a4-4f22-96ba-04d580623aae", nome: "M - Skin" },
  { id: "5aaa783e-2702-46dc-9781-3f92726bfcf2", nome: "M - Stiffen" },
  { id: "c8d6eff8-72c5-4111-a68f-0da332de57f2", nome: "M - TopCoat" },
  { id: "8819b8d7-e3db-483c-9484-d46ae79adb8c", nome: "N - Ceras" },
  { id: "24d0be70-e1c2-4fac-a22f-fa763c15e123", nome: "N - Empaste" },
  { id: "766db194-c183-4747-8352-83e4d7b48c66", nome: "N - Reparação" },
  { id: "f96ab930-98a8-4f4a-b9fb-09b1eed06a93", nome: "P - Consola/Módulos" },
  { id: "ea80f970-f3c9-44ef-9d04-2a7457425b1c", nome: "P - Liners/Hardtop" },
  { id: "94bc4c6d-99ee-4543-8ae7-990b9ea60178", nome: "P - Madeiras" },
  { id: "7e259ab1-1a42-4405-a5d1-35a62282fa35", nome: "P - Móveis/Bancos" },
  { id: "6069bed1-6d35-4ed2-8abe-9f2270c18c43", nome: "P - Tampas" },
  { id: "7e8a6b31-d97a-4feb-8478-8346231c84db", nome: "P - Tanques" },
  { id: "06e8ef5a-ceb0-46f1-8602-fd2b285ac976", nome: "P - Tekas" },
  { id: "8ad6203a-2e6e-4165-a008-58b781e15b38", nome: "Rework" },
  { id: "aa08ac44-aa97-4f74-971e-855d2f3d5180", nome: "S - Cabine de Pintura" },
  { id: "d64846c6-5129-42dc-958b-c2b756bb029f", nome: "S - Colagem" },
  { id: "cbb3f3c6-81a3-4717-898e-9241636bcf67", nome: "S - POP" },
  { id: "bccdc7ac-22a6-4c93-b59b-33e835466741", nome: "S - Preparação" },
  { id: "4f48f133-3c36-45dd-b89c-43abd11bad77", nome: "S - Repassagem" },
  { id: "b318182f-b197-47e3-b5fd-f535e0f9a4ab", nome: "S - Skin" },
  { id: "b4ac3b60-2642-433e-b7a8-5ccd23faca5d", nome: "S - Stiffen" },
  { id: "fc1bca43-5142-4123-a151-ade04483e6a5", nome: "T - BottomPaint" },
  { id: "ed77b5a4-f4e7-44c3-9c3e-d898be4ef342", nome: "T - Casco" },
  { id: "337d1532-d516-4950-9ac5-a6d18f2615b2", nome: "T - Coberta" },
  { id: "120167f7-48d6-44fe-ba73-867e7b8fe3d7", nome: "T - Medium Parts" },
  { id: "3decaadc-ba32-4979-82b1-5f61f631e9ba", nome: "T - Small Parts" },
  { id: "07e2697c-cb5d-426a-9bc2-8c1f4fd8e65c", nome: "W - Acabamentos Montagem" },
  { id: "db82516f-8f5e-4bda-9f2d-7603cadb7e58", nome: "W - CNC - FANUC" },
  { id: "0c84ce31-975e-47c6-a1ec-6d1b9e0b326b", nome: "W - CNC - LECTRA" },
  { id: "166fb3e5-4e27-4834-ab21-cd8e9a5c464b", nome: "W - CNC - MORBIDELLI" },
  { id: "c2cdd90e-f1be-4d85-ad50-47ff039c2408", nome: "W - Espumas Laminação" },
  { id: "a567bbbb-0983-4104-bf5c-3706c51df240", nome: "W - Madeiras Laminação" },
  { id: "994fbb9c-75f2-4bcc-a93d-0a6686c9ee92", nome: "W - Pick Assy/PM" }
];

const NUM_ESTACOES = ESTACOES.length;

function procurarEstacao(id) {
  const e = ESTACOES.find((e) => e.id === id);
  return e ? e.nome : "NAO_ENCONTRADA";
}

function procurarEstacaoPorNome(nome) {
  const e = ESTACOES.find((e) => e.nome === nome);
  return e ? e.id : "";
}

function procurarIndiceEstacao(nome) {
  const idx = ESTACOES.findIndex((e) => e.nome === nome);
  return idx >= 0 ? idx : 0;
}

function procurarEstacaoPorIndice(idx) {
  if (idx >= 0 && idx < NUM_ESTACOES) {
    return ESTACOES[idx].nome;
  }
  return "NAO_ENCONTRADA";
}
