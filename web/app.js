// ===== RFID Web Simulator - Lógica completa (réplica do ESP32) =====

// ===== ESTADO GLOBAL =====
const STATE_NORMAL = 0;
const STATE_SELECT_FIELD = 1;
const STATE_EDIT_VALUE = 2;

const ORD_DISPLAY_NORMAL = 0;
const ORD_DISPLAY_READING = 1;
const ORD_DISPLAY_VERIFYING = 2;
const ORD_DISPLAY_NOT_FOUND = 3;
const ORD_DISPLAY_FOUND = 4;
const ORD_DISPLAY_CONFIRM = 5;
const ORD_DISPLAY_WRITING = 6;

const OP_DISPLAY_NORMAL = 0;
const OP_DISPLAY_MODE_SELECT = 1;
const OP_DISPLAY_IN_READING = 2;
const OP_DISPLAY_IN_SUCCESS = 3;
const OP_DISPLAY_OUT_READING = 4;
const OP_DISPLAY_OUT_SUCCESS = 5;
const OP_DISPLAY_NOT_FOUND_OP = 6;

const OP_MODE_IN = 0;
const OP_MODE_OUT = 1;

// Andon display modes
const ANDON_DISPLAY_NORMAL = 0;
const ANDON_DISPLAY_READING_OP = 1;
const ANDON_DISPLAY_OP_NOT_FOUND = 2;
const ANDON_DISPLAY_SELECT_DEFECT = 3;

// Andon defect options
const ANDON_DEFECTS = [
  "Falta peca",
  "Avaria Equip",
  "Ajuste tec/qual",
  "Defeito",
  "Outros"
];
const NUM_ANDON_DEFECTS = ANDON_DEFECTS.length;
const ANDON_NOT_FOUND_DISPLAY_TIME = 1000;  // 1 segundo
const ANDON_LOOKUP_DELAY = 500;

const NUM_ESTACOES = ESTACOES.length;
const EDIT_TIMEOUT = 30000;
const ORD_NOT_FOUND_DISPLAY_TIME = 2000;
const ORD_LOOKUP_DELAY = 500;
const OP_LOOKUP_DELAY = 500;
const SLEEP_TIMEOUT = 120000; // 2 minutos na web

const state = {
  // Variáveis principais (persistidas em localStorage)
  varA: "",  // Estação
  varB: "",  // Ordem de produção
  varC: "",  // Número de operadores
  varD: "",
  varE: "",

  // Lista de operadores (persistida)
  operadores: [],  // Array de strings RFID

  // Máquina de estados de edição
  editState: STATE_NORMAL,
  selectedField: 0,  // 0=Est, 1=Ord, 2=Op#
  currentEstacaoIndex: 0,

  // Timers
  editModeStart: 0,
  lastStateChangeTime: 0,
  lastActivityTime: Date.now(),

  // Ord
  ordDisplayMode: ORD_DISPLAY_NORMAL,
  ordFromDatabase: "",
  lastRFIDSuccess: "",
  rfidReadingInProgress: false,
  ordReadOnceSuccess: false,
  ordNotFoundTime: -1,
  ordLookupPending: false,
  ordLookupStartTime: -1,
  ordInitialized: false,
  lastOrdRead: "",
  ordConfirmStartTime: -1,

  // Op#
  opMode: OP_MODE_IN,
  opDisplayMode: OP_DISPLAY_NORMAL,
  opReadingInProgress: false,
  lastRFIDOperador: "",
  operadorFromDatabase: "",
  opLookupPending: false,
  opLookupStartTime: -1,
  opGreetingName: "",
  opFeedbackMessage: "",

  // Andon
  andonDisplayMode: ANDON_DISPLAY_NORMAL,
  currentAndonDefectIndex: 0,
  lastRFIDAndon: "",
  andonReadingInProgress: false,
  andonNotFoundTime: -1,
  andonOldVarD: "",

  // Display
  battery: 85,
  rssi: -58,
  isSleeping: false,

  // Último RFID lido (debug)
  lastRfidValue: "",
};

// ===== ELEMENTOS DOM =====
const line1El = document.getElementById("line1");
const line2El = document.getElementById("line2");
const line3El = document.getElementById("line3");
const line4El = document.getElementById("line4");
const line5El = document.getElementById("line5");
const batteryFillEl = document.getElementById("batteryFill");
const wifiIconEl = document.getElementById("wifiIcon");
const ledEl = document.getElementById("rfidLed");
const manualInputEl = document.getElementById("manualInput");
const displayCoverEl = document.getElementById("displayCover");

const btnUp = document.getElementById("btnUp");
const btnSel = document.getElementById("btnSel");
const btnDown = document.getElementById("btnDown");
const rfidTouch = document.getElementById("rfidTouch");

// ===== PERSISTÊNCIA (localStorage = NVS do ESP32) =====
function loadNVS() {
  try {
    state.varA = localStorage.getItem("rfid_varA") || "";
    state.varB = localStorage.getItem("rfid_varB") || "";
    state.varC = localStorage.getItem("rfid_varC") || "";
    state.varD = localStorage.getItem("rfid_varD") || "Verde";
    state.varE = localStorage.getItem("rfid_varE") || "";
    state.andonOldVarD = state.varD;
    const ops = localStorage.getItem("rfid_operadores");
    state.operadores = ops ? JSON.parse(ops) : [];
    console.log("[NVS] Carregado:", { varA: state.varA, varB: state.varB, varC: state.varC, ops: state.operadores.length });
  } catch (e) {
    console.error("[NVS] Erro ao carregar:", e);
  }
}

function saveNVS() {
  localStorage.setItem("rfid_varA", state.varA);
  localStorage.setItem("rfid_varB", state.varB);
  localStorage.setItem("rfid_varC", state.varC);
  localStorage.setItem("rfid_varD", state.varD);
  localStorage.setItem("rfid_varE", state.varE);
  localStorage.setItem("rfid_operadores", JSON.stringify(state.operadores));
  console.log("[NVS] Salvo");
}

function updateVariable(varName, value) {
  state["var" + varName] = value;
  saveNVS();
  console.log(`[Updated] Var ${varName}: ${value}`);
}

// ===== OPERADORES =====
function operadorExists(rfid) {
  return state.operadores.includes(rfid);
}

function addOperador(rfid) {
  if (operadorExists(rfid)) return false;
  if (state.operadores.length >= 50) return false;
  state.operadores.push(rfid);
  saveNVS();
  console.log("[NVS-OP] Adicionado:", rfid);
  return true;
}

function removeOperador(rfid) {
  const idx = state.operadores.indexOf(rfid);
  if (idx === -1) return false;
  state.operadores.splice(idx, 1);
  saveNVS();
  console.log("[NVS-OP] Removido:", rfid);
  return true;
}

// ===== API (Netlify Functions / Supabase) =====
async function supabaseLookup(table, filterCol, filterVal, targetCol) {
  try {
    const res = await fetch("/api/lookup", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ table, filterCol, filterVal, targetCol }),
    });
    if (!res.ok) return "Nao encontrado";
    const data = await res.json();
    return data.ok ? data.value : "Nao encontrado";
  } catch (e) {
    console.error("[API] Lookup erro:", e);
    return "Erro: Offline";
  }
}

async function supabaseInsert(table, data) {
  try {
    const res = await fetch("/api/insert", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ table, data }),
    });
    const result = await res.json();
    if (!result.ok) {
      console.error("[API] Insert falhou:", result.error || res.status);
    }
    return result.ok;
  } catch (e) {
    console.error("[API] Insert erro:", e);
    return false;
  }
}

function getFormattedTime() {
  const now = new Date();
  const dd = String(now.getDate()).padStart(2, "0");
  const mm = String(now.getMonth() + 1).padStart(2, "0");
  const yy = String(now.getFullYear()).slice(-2);
  const hh = String(now.getHours()).padStart(2, "0");
  const min = String(now.getMinutes()).padStart(2, "0");
  return `${dd}/${mm}/${yy} ${hh}:${min}`;
}

async function gravarTempoOperador(rfid, ordemFabrico, estado) {
  const tempo = getFormattedTime();
  const data = { rfid, day_time: tempo, prod_order: ordemFabrico, workstation: state.varA, status: estado };
  console.log("[DB] Gravando tempo:", data);
  return await supabaseInsert("data_time_iot", data);
}

// ===== ANDON: ROTINA CHAMADA QUANDO VARD MUDA =====
function onAndonChanged(newValue) {
  console.log("[ANDON] VarD mudou para:", newValue);
  
  // Grava alerta na tabela alertas_andon
  if (newValue !== "Verde" && newValue.length > 0) {
    const alertData = {
      operador_rfid: state.lastRFIDAndon || "",
      tipo_alerta: newValue,
    };
    console.log("[ANDON] Gravando alerta na BD:", alertData);
    supabaseInsert("alertas_andon", alertData).then(ok => {
      if (ok) {
        console.log("[ANDON] Alerta gravado com sucesso");
      } else {
        console.error("[ANDON] Falha ao gravar alerta");
      }
    });
  }
}

// ===== BLINK HELPER =====
function shouldBlink() {
  const normalBlink = Math.floor(Date.now() / 500) % 2 === 0;
  if (Date.now() - state.lastStateChangeTime < 250) return true;
  return normalBlink;
}

// ===== LED =====
function blinkLed(times = 3) {
  let remaining = times * 2;
  const blink = () => {
    if (remaining <= 0) {
      ledEl.style.background = "#1ec15c";
      ledEl.style.boxShadow = "0 0 8px #1ec15c";
      return;
    }
    const red = remaining % 2 === 0;
    ledEl.style.background = red ? "#e93b4f" : "#1ec15c";
    ledEl.style.boxShadow = red ? "0 0 8px #e93b4f" : "0 0 8px #1ec15c";
    remaining--;
    setTimeout(blink, 130);
  };
  blink();
}

// ===== BEEP (Web Audio) =====
let audioCtx = null;
function beep(frequency, duration) {
  try {
    if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.frequency.value = frequency;
    gain.gain.value = 0.15;
    osc.start();
    setTimeout(() => { osc.stop(); }, duration);
  } catch (e) { /* silenciar se audio não suportado */ }
}

// ===== DISPLAY =====
function drawDisplay() {
  // Bateria
  batteryFillEl.style.width = `${state.battery}%`;

  // WiFi
  const bars = wifiIconEl.querySelectorAll("span");
  const thresholds = [-90, -80, -70, -60];
  bars.forEach((bar, i) => {
    bar.style.opacity = state.rssi > thresholds[i] ? "1" : "0.2";
  });

  if (state.isSleeping) {
    line1El.textContent = "";
    line2El.textContent = "   ZZZ...";
    line3El.textContent = "";
    line4El.textContent = "";
    line5El.textContent = "";
    return;
  }

  if (state.editState !== STATE_NORMAL) {
    drawEditScreen();
  } else {
    drawMainScreen();
  }
}

function drawMainScreen() {
  line1El.textContent = "Est: " + (state.varA || "---");
  line2El.textContent = "Ord: " + (state.varB || "---");
  line3El.textContent = "Op#: " + (state.varC || "---");
  line4El.textContent = "And: " + (state.varD || "---");
  line5El.textContent = "";
}

function drawEditScreen() {
  const blink = shouldBlink();

  // ===== LINHA 1: EST =====
  if (state.editState === STATE_SELECT_FIELD) {
    if (state.selectedField === 0) {
      line1El.textContent = blink ? "Est: " + (state.varA || "---") : "     " + (state.varA || "---");
    } else {
      line1El.textContent = "Est: " + (state.varA || "---");
    }
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 0) {
    const estNome = procurarEstacao(state.currentEstacaoIndex + 1);
    line1El.textContent = "Est: " + (blink ? estNome : "");
  } else {
    line1El.textContent = "Est: " + (state.varA || "---");
  }

  // ===== LINHA 2: ORD =====
  if (state.editState === STATE_SELECT_FIELD) {
    if (state.selectedField === 1) {
      line2El.textContent = blink ? "Ord: " + (state.varB || "---") : "     " + (state.varB || "---");
    } else {
      line2El.textContent = "Ord: " + (state.varB || "---");
    }
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 1) {
    let displayOrd = "";
    switch (state.ordDisplayMode) {
      case ORD_DISPLAY_READING:
        displayOrd = blink ? "... a ler" : "";
        break;
      case ORD_DISPLAY_VERIFYING:
        displayOrd = blink ? "... verificar" : "";
        break;
      case ORD_DISPLAY_NOT_FOUND:
        displayOrd = "nao existe";
        break;
      case ORD_DISPLAY_FOUND:
        displayOrd = blink ? state.ordFromDatabase : "";
        break;
      case ORD_DISPLAY_CONFIRM: {
        const confirmBlink = Math.floor((Date.now() - state.ordConfirmStartTime) / 800) % 2 === 0;
        displayOrd = confirmBlink ? "terminar?" : state.ordFromDatabase;
        break;
      }
      case ORD_DISPLAY_WRITING:
        displayOrd = blink ? "a escrever" : "";
        break;
      default:
        displayOrd = state.ordFromDatabase || "---";
    }
    line2El.textContent = "Ord: " + displayOrd;
  } else {
    line2El.textContent = "Ord: " + (state.varB || "---");
  }

  // ===== LINHA 3: OP# =====
  if (state.editState === STATE_SELECT_FIELD) {
    if (state.selectedField === 2) {
      line3El.textContent = blink ? "Op#: " + (state.varC || "---") : "     " + (state.varC || "---");
    } else {
      line3El.textContent = "Op#: " + (state.varC || "---");
    }
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 2) {
    if (state.opDisplayMode === OP_DISPLAY_MODE_SELECT) {
      line3El.textContent = "Op#: " + (blink ? (state.opMode === OP_MODE_IN ? "In" : "Out") : "");
    } else if (state.opDisplayMode === OP_DISPLAY_IN_READING) {
      line3El.textContent = "Op#: " + (blink ? "in ... a ler" : "in");
    } else if (state.opDisplayMode === OP_DISPLAY_OUT_READING) {
      line3El.textContent = "Op#: " + (blink ? "out ... a ler" : "out");
    } else {
      line3El.textContent = "Op#: " + state.operadores.length;
    }
  } else if (state.opDisplayMode === OP_DISPLAY_IN_READING || state.opDisplayMode === OP_DISPLAY_OUT_READING) {
    if (state.opDisplayMode === OP_DISPLAY_IN_READING) {
      line3El.textContent = "Op#: " + (blink ? "in ... a ler" : "in");
    } else {
      line3El.textContent = "Op#: " + (blink ? "out ... a ler" : "out");
    }
  } else {
    line3El.textContent = "Op#: " + (state.varC || "---");
  }

  // ===== LINHA 4: ANDON =====
  if (state.editState === STATE_SELECT_FIELD) {
    if (state.selectedField === 3) {
      line4El.textContent = blink ? "And: " + (state.varD || "---") : "     " + (state.varD || "---");
    } else {
      line4El.textContent = "And: " + (state.varD || "---");
    }
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 3) {
    if (state.andonDisplayMode === ANDON_DISPLAY_READING_OP) {
      line4El.textContent = "And: " + (blink ? "....operador" : "");
    } else if (state.andonDisplayMode === ANDON_DISPLAY_OP_NOT_FOUND) {
      line4El.textContent = "And: op n encontr";
    } else if (state.andonDisplayMode === ANDON_DISPLAY_SELECT_DEFECT) {
      line4El.textContent = "And: " + (blink ? ANDON_DEFECTS[state.currentAndonDefectIndex] : "");
    } else {
      line4El.textContent = "And: " + (state.varD || "---");
    }
  } else {
    line4El.textContent = "And: " + (state.varD || "---");
  }

  // ===== LINHA 5: FEEDBACK =====
  if (state.editState === STATE_EDIT_VALUE && state.selectedField === 2) {
    line5El.textContent = state.opFeedbackMessage || "";
  } else {
    line5El.textContent = "";
  }
}

// ===== SLEEP =====
function goToSleep() {
  state.isSleeping = true;
  state.editState = STATE_NORMAL;
  state.rfidReadingInProgress = false;
  state.opReadingInProgress = false;
  displayCoverEl.classList.add("sleeping");
  console.log("[SLEEP] Display desligado");
}

async function wakeUp() {
  state.isSleeping = false;
  state.lastActivityTime = Date.now();
  displayCoverEl.classList.remove("sleeping");
  console.log("[WAKE] Display ligado");
  
  // Verifica se o alerta Andon foi resolvido
  if (state.varD && state.varD !== "Verde") {
    console.log("[ANDON] Verificando se alerta foi resolvido...");
    const resolvido = await supabaseLookup("alertas_andon", "tipo_alerta", state.varD, "resolvido");
    if (resolvido === "TRUE" || resolvido === "true") {
      console.log("[ANDON] Alerta resolvido! Voltando a Verde");
      updateVariable("D", "Verde");
    } else {
      console.log("[ANDON] Alerta ainda não resolvido. resolvido=", resolvido);
    }
  }
}

// ===== BOTÃO SEL (BTN2 / Enter) =====
function handleBtnSel() {
  if (state.isSleeping) { wakeUp(); return; }
  state.lastActivityTime = Date.now();

  if (state.editState === STATE_NORMAL) {
    // Entra em modo seleção de campo
    state.editState = STATE_SELECT_FIELD;
    state.selectedField = 1;  // Começa em Ord
    state.currentEstacaoIndex = procurarIndiceEstacao(state.varA);
    state.editModeStart = Date.now();
    state.lastStateChangeTime = Date.now();
    state.rfidReadingInProgress = false;
    state.ordReadOnceSuccess = false;
    state.lastRFIDSuccess = "";
    console.log("[EDIT] Entrou em modo seleção de campo (Ord)");

  } else if (state.editState === STATE_SELECT_FIELD) {
    // Confirmação do campo selecionado
    if (state.selectedField === 0) {
      state.editState = STATE_EDIT_VALUE;
      state.editModeStart = Date.now();
      state.lastStateChangeTime = Date.now();
      console.log("[EDIT] Entrou em modo edição de Estação");

    } else if (state.selectedField === 1) {
      state.editState = STATE_EDIT_VALUE;
      state.rfidReadingInProgress = true;
      state.lastRFIDSuccess = "";
      state.ordFromDatabase = "";
      state.ordDisplayMode = ORD_DISPLAY_READING;
      if (state.varB.length > 0) {
        state.lastOrdRead = state.varB;
        state.ordInitialized = true;
      }
      state.editModeStart = Date.now();
      state.lastStateChangeTime = Date.now();
      manualInputEl.focus();
      manualInputEl.placeholder = "RFID / cód. barras da ordem...";
      console.log("[EDIT] Entrou em modo edição de Ordem");

    } else if (state.selectedField === 2) {
      state.editState = STATE_EDIT_VALUE;
      state.opDisplayMode = OP_DISPLAY_MODE_SELECT;
      state.opMode = OP_MODE_IN;
      state.editModeStart = Date.now();
      state.lastStateChangeTime = Date.now();
      console.log("[EDIT] Entrou em modo Op# - Selecionando In/Out");

    } else if (state.selectedField === 3) {
      // Andon: inicia leitura RFID de operador
      state.editState = STATE_EDIT_VALUE;
      state.andonDisplayMode = ANDON_DISPLAY_READING_OP;
      state.andonReadingInProgress = true;
      state.lastRFIDAndon = "";
      state.currentAndonDefectIndex = 0;
      state.editModeStart = Date.now();
      state.lastStateChangeTime = Date.now();
      manualInputEl.focus();
      manualInputEl.placeholder = "RFID / cód. barras operador...";
      console.log("[EDIT] Entrou em modo Andon - Aguardando RFID operador");
    }

  } else if (state.editState === STATE_EDIT_VALUE) {
    if (state.selectedField === 0) {
      const estNome = procurarEstacao(state.currentEstacaoIndex + 1);
      updateVariable("A", estNome);
      state.editState = STATE_NORMAL;
      state.rfidReadingInProgress = false;
      state.ordReadOnceSuccess = false;
      console.log("[EDIT] Confirmou estação:", estNome);

    } else if (state.selectedField === 1) {
      handleOrdConfirm();

    } else if (state.selectedField === 2) {
      handleOpConfirm();

    } else if (state.selectedField === 3) {
      handleAndonConfirm();
    }
  }
}

async function handleOrdConfirm() {
  if (state.ordDisplayMode === ORD_DISPLAY_CONFIRM) {
    state.ordDisplayMode = ORD_DISPLAY_WRITING;
    if (state.ordFromDatabase.length > 0) {
      await gravarTempoOperador("", state.ordFromDatabase, "out");
      console.log("[DB] Ordem terminada:", state.ordFromDatabase);
    }
    updateVariable("B", "");
    state.ordFromDatabase = "";
    state.ordInitialized = false;
    state.lastOrdRead = "";
    state.ordConfirmStartTime = -1;
    state.ordReadOnceSuccess = false;
    state.rfidReadingInProgress = false;
    state.editState = STATE_NORMAL;
    state.ordDisplayMode = ORD_DISPLAY_NORMAL;
    manualInputEl.placeholder = "";
  } else {
    state.rfidReadingInProgress = false;
    state.ordReadOnceSuccess = false;

    if (state.ordFromDatabase.length > 0 && !state.ordInitialized) {
      state.ordDisplayMode = ORD_DISPLAY_WRITING;
      await gravarTempoOperador("", state.ordFromDatabase, "in");
      state.ordInitialized = true;
      state.lastOrdRead = state.ordFromDatabase;
      updateVariable("B", state.ordFromDatabase);
      console.log("[DB] Ordem iniciada:", state.ordFromDatabase);
    } else if (state.ordFromDatabase.length > 0 && state.ordInitialized && state.ordFromDatabase !== state.lastOrdRead) {
      state.ordDisplayMode = ORD_DISPLAY_WRITING;
      await gravarTempoOperador("", state.lastOrdRead, "out");
      await gravarTempoOperador("", state.ordFromDatabase, "in");
      state.lastOrdRead = state.ordFromDatabase;
      updateVariable("B", state.ordFromDatabase);
    } else if (state.ordFromDatabase.length > 0) {
      updateVariable("B", state.ordFromDatabase);
    }
    state.editState = STATE_NORMAL;
    state.ordDisplayMode = ORD_DISPLAY_NORMAL;
    state.ordConfirmStartTime = -1;
    manualInputEl.placeholder = "";
  }
}

async function handleOpConfirm() {
  if (state.opDisplayMode === OP_DISPLAY_MODE_SELECT) {
    if (state.opMode === OP_MODE_IN) {
      state.opDisplayMode = OP_DISPLAY_IN_READING;
      console.log("[EDIT] Iniciando leitura IN de operadores");
    } else {
      state.opDisplayMode = OP_DISPLAY_OUT_READING;
      console.log("[EDIT] Iniciando leitura OUT de operadores");
    }
    state.opReadingInProgress = true;
    state.lastRFIDOperador = "";
    state.operadorFromDatabase = "";
    state.editModeStart = Date.now();
    manualInputEl.focus();
    manualInputEl.placeholder = "RFID / cód. barras operador...";
  } else {
    state.opReadingInProgress = false;
    state.opGreetingName = "";
    state.opFeedbackMessage = "";
    updateVariable("C", String(state.operadores.length));
    state.editState = STATE_NORMAL;
    state.opDisplayMode = OP_DISPLAY_NORMAL;
    manualInputEl.placeholder = "";
    console.log("[EDIT] Finalizou Op#, total:", state.operadores.length);
  }
}

// ===== ANDON CONFIRM =====
function handleAndonConfirm() {
  if (state.andonDisplayMode === ANDON_DISPLAY_READING_OP) {
    // Enter durante "....operador" → sai sem alterar varD
    state.andonReadingInProgress = false;
    state.andonDisplayMode = ANDON_DISPLAY_NORMAL;
    state.editState = STATE_NORMAL;
    manualInputEl.placeholder = "";
    console.log("[EDIT] Andon: saiu sem alterar (enter durante leitura)");
  } else if (state.andonDisplayMode === ANDON_DISPLAY_SELECT_DEFECT) {
    // Enter na lista de defeitos → seleciona defeito e guarda em varD
    const selectedDefect = ANDON_DEFECTS[state.currentAndonDefectIndex];
    state.andonReadingInProgress = false;
    state.andonDisplayMode = ANDON_DISPLAY_NORMAL;
    state.editState = STATE_NORMAL;
    manualInputEl.placeholder = "";
    
    if (selectedDefect !== state.varD) {
      updateVariable("D", selectedDefect);
      onAndonChanged(selectedDefect);
      console.log("[EDIT] Andon: defeito selecionado:", selectedDefect);
    } else {
      console.log("[EDIT] Andon: mesmo valor, sem alteração");
    }
  } else {
    // Qualquer outro estado → sai
    state.andonReadingInProgress = false;
    state.andonDisplayMode = ANDON_DISPLAY_NORMAL;
    state.editState = STATE_NORMAL;
    manualInputEl.placeholder = "";
    console.log("[EDIT] Andon: saiu sem alterar");
  }
}

// ===== BOTÃO UP (BTN1) =====
function handleBtnUp() {
  if (state.isSleeping) { wakeUp(); return; }
  state.lastActivityTime = Date.now();

  if (state.editState === STATE_SELECT_FIELD) {
    state.selectedField = (state.selectedField - 1 + 4) % 4;
    state.editModeStart = Date.now();
    state.lastStateChangeTime = Date.now();
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 0) {
    state.currentEstacaoIndex = (state.currentEstacaoIndex - 1 + NUM_ESTACOES) % NUM_ESTACOES;
    state.editModeStart = Date.now();
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 1) {
    if (state.ordDisplayMode === ORD_DISPLAY_CONFIRM) {
      state.ordDisplayMode = ORD_DISPLAY_READING;
      state.ordReadOnceSuccess = false;
      state.editModeStart = Date.now();
      manualInputEl.focus();
    }
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 2) {
    if (state.opDisplayMode === OP_DISPLAY_MODE_SELECT) {
      state.opMode = state.opMode === OP_MODE_IN ? OP_MODE_OUT : OP_MODE_IN;
      state.lastStateChangeTime = Date.now();
    }
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 3) {
    // Andon: navega lista de defeitos para trás
    if (state.andonDisplayMode === ANDON_DISPLAY_SELECT_DEFECT) {
      state.currentAndonDefectIndex = (state.currentAndonDefectIndex - 1 + NUM_ANDON_DEFECTS) % NUM_ANDON_DEFECTS;
      state.editModeStart = Date.now();
    }
  }
}

// ===== BOTÃO DOWN (BTN3) =====
function handleBtnDown() {
  if (state.isSleeping) { wakeUp(); return; }
  state.lastActivityTime = Date.now();

  if (state.editState === STATE_SELECT_FIELD) {
    state.selectedField = (state.selectedField + 1) % 4;
    state.editModeStart = Date.now();
    state.lastStateChangeTime = Date.now();
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 0) {
    state.currentEstacaoIndex = (state.currentEstacaoIndex + 1) % NUM_ESTACOES;
    state.editModeStart = Date.now();
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 1) {
    if (state.ordDisplayMode === ORD_DISPLAY_CONFIRM) {
      state.ordDisplayMode = ORD_DISPLAY_READING;
      state.ordReadOnceSuccess = false;
      state.editModeStart = Date.now();
      manualInputEl.focus();
    }
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 2) {
    if (state.opDisplayMode === OP_DISPLAY_MODE_SELECT) {
      state.opMode = state.opMode === OP_MODE_IN ? OP_MODE_OUT : OP_MODE_IN;
      state.lastStateChangeTime = Date.now();
    }
  } else if (state.editState === STATE_EDIT_VALUE && state.selectedField === 3) {
    // Andon: navega lista de defeitos para frente
    if (state.andonDisplayMode === ANDON_DISPLAY_SELECT_DEFECT) {
      state.currentAndonDefectIndex = (state.currentAndonDefectIndex + 1) % NUM_ANDON_DEFECTS;
      state.editModeStart = Date.now();
    }
  }
}

// ===== RFID INPUT (substitui readRFIDCard do ESP32) =====
async function processRfidInput(code) {
  if (!code) return;
  state.lastRfidValue = code;
  state.lastActivityTime = Date.now();
  blinkLed(2);

  // Modo ORD: leitura de ordem
  if (state.rfidReadingInProgress && state.selectedField === 1 && state.editState === STATE_EDIT_VALUE) {
    if (!state.ordReadOnceSuccess) {
      if (state.lastRFIDSuccess !== code) {
        state.lastRFIDSuccess = code;
        state.ordDisplayMode = ORD_DISPLAY_VERIFYING;
        state.ordReadOnceSuccess = true;
        state.editModeStart = Date.now();
        console.log("[RFID] Cartão lido para Ord:", code);

        setTimeout(async () => {
          const result = await supabaseLookup("ordens_producao", "rfid_token", code, "display_nome");

          if (result !== "Nao encontrado" && result !== "Erro: Offline") {
            state.ordFromDatabase = result;
            if (state.ordInitialized && result === state.lastOrdRead) {
              state.ordDisplayMode = ORD_DISPLAY_CONFIRM;
              state.ordConfirmStartTime = Date.now();
              console.log("[DB] Mesma ordem - confirmar terminar:", result);
            } else {
              state.ordDisplayMode = ORD_DISPLAY_FOUND;
              console.log("[DB] Ordem encontrada:", result);
            }
          } else {
            state.ordDisplayMode = ORD_DISPLAY_NOT_FOUND;
            state.ordNotFoundTime = Date.now();
            state.ordFromDatabase = "";
            console.log("[DB] RFID não encontrado na BD");
          }
        }, ORD_LOOKUP_DELAY);
      }
    }
    return;
  }

  // Modo OP: leitura de operador
  if (state.opReadingInProgress && state.selectedField === 2 && state.editState === STATE_EDIT_VALUE) {
    if (state.lastRFIDOperador !== code) {
      state.lastRFIDOperador = code;
      state.opFeedbackMessage = "... à espera";
      state.opDisplayMode = state.opMode === OP_MODE_IN ? OP_DISPLAY_IN_READING : OP_DISPLAY_OUT_READING;
      state.editModeStart = Date.now();
      console.log("[RFID-OP] Cartão lido:", code);

      setTimeout(async () => {
        if (state.opMode === OP_MODE_IN) {
          const nomeOperador = await supabaseLookup("operadores", "tag_rfid_operador", code, "nome_operador");

          if (nomeOperador !== "Nao encontrado" && nomeOperador !== "Erro: Offline") {
            state.operadorFromDatabase = nomeOperador;
            const exists = operadorExists(code);
            if (!exists) {
              addOperador(code);
              await gravarTempoOperador(code, "", "in");
              updateVariable("C", String(state.operadores.length));
            }
            state.opDisplayMode = OP_DISPLAY_IN_SUCCESS;
            state.opGreetingName = nomeOperador;
            state.opFeedbackMessage = "Ola " + nomeOperador;
            beep(1200, 100);
          } else {
            state.opDisplayMode = OP_DISPLAY_NOT_FOUND_OP;
            state.opFeedbackMessage = "erro op nao existe na bd";
            state.operadorFromDatabase = "";
            beep(400, 200);
          }
        } else {
          const exists = operadorExists(code);
          if (exists) {
            removeOperador(code);
            await gravarTempoOperador(code, "", "out");
            updateVariable("C", String(state.operadores.length));
            state.opDisplayMode = OP_DISPLAY_OUT_SUCCESS;
            state.opFeedbackMessage = "removido";
            beep(800, 100);
          } else {
            state.opDisplayMode = OP_DISPLAY_NOT_FOUND_OP;
            state.opFeedbackMessage = "op nao existe na estacao";
            beep(400, 200);
          }
        }

        // Volta a "a ler" após feedback
        setTimeout(() => {
          if (state.opReadingInProgress) {
            state.lastRFIDOperador = "";
            state.opDisplayMode = state.opMode === OP_MODE_IN ? OP_DISPLAY_IN_READING : OP_DISPLAY_OUT_READING;
            manualInputEl.focus();
          }
        }, 1500);
      }, OP_LOOKUP_DELAY);
    }
    return;
  }

  // Modo ANDON: leitura de operador para Andon
  if (state.andonReadingInProgress && state.selectedField === 3 && state.editState === STATE_EDIT_VALUE) {
    if (state.andonDisplayMode === ANDON_DISPLAY_READING_OP) {
      if (state.lastRFIDAndon !== code) {
        state.lastRFIDAndon = code;
        state.editModeStart = Date.now();
        console.log("[RFID-ANDON] Cartão lido:", code);

        setTimeout(async () => {
          const nomeOperador = await supabaseLookup("operadores", "tag_rfid_operador", code, "nome_operador");

          if (nomeOperador !== "Nao encontrado" && nomeOperador !== "Erro: Offline") {
            // Operador encontrado → passa para lista de defeitos
            state.andonDisplayMode = ANDON_DISPLAY_SELECT_DEFECT;
            state.currentAndonDefectIndex = 0;
            state.editModeStart = Date.now();
            beep(1200, 100);
            console.log("[DB-ANDON] Operador encontrado:", nomeOperador);
            console.log("[ANDON] Iniciando seleção de defeito");
          } else {
            // Operador não encontrado → mostra mensagem 1s
            state.andonDisplayMode = ANDON_DISPLAY_OP_NOT_FOUND;
            state.andonNotFoundTime = Date.now();
            state.lastRFIDAndon = "";
            beep(400, 200);
            console.log("[DB-ANDON] Operador não encontrado na BD");
          }
        }, ANDON_LOOKUP_DELAY);
      }
    }
    return;
  }
}

// ===== LOOP PRINCIPAL =====
function mainLoop() {
  const now = Date.now();

  // Timeout de edição (30s)
  if (state.editState !== STATE_NORMAL && (now - state.editModeStart) > EDIT_TIMEOUT) {
    state.editState = STATE_NORMAL;
    state.rfidReadingInProgress = false;
    state.opReadingInProgress = false;
    state.ordReadOnceSuccess = false;
    state.lastRFIDSuccess = "";
    state.ordFromDatabase = "";
    state.opFeedbackMessage = "";
    // Reset Andon state
    state.andonReadingInProgress = false;
    state.andonDisplayMode = ANDON_DISPLAY_NORMAL;
    state.lastRFIDAndon = "";
    manualInputEl.placeholder = "";
    console.log("[EDIT] Saiu por timeout");
  }

  // Controle "não existe ord"
  if (state.ordDisplayMode === ORD_DISPLAY_NOT_FOUND && state.ordNotFoundTime !== -1) {
    if ((now - state.ordNotFoundTime) > ORD_NOT_FOUND_DISPLAY_TIME) {
      state.ordDisplayMode = ORD_DISPLAY_READING;
      state.ordFromDatabase = "";
      state.lastRFIDSuccess = "";
      state.ordReadOnceSuccess = false;
      state.ordNotFoundTime = -1;
      manualInputEl.focus();
    }
  }

  // Controle "op nao encontr" Andon (1 segundo)
  if (state.andonDisplayMode === ANDON_DISPLAY_OP_NOT_FOUND && state.andonNotFoundTime !== -1) {
    if ((now - state.andonNotFoundTime) > ANDON_NOT_FOUND_DISPLAY_TIME) {
      state.andonDisplayMode = ANDON_DISPLAY_READING_OP;
      state.lastRFIDAndon = "";
      state.andonNotFoundTime = -1;
      manualInputEl.focus();
    }
  }

  // Sleep timeout
  if (!state.isSleeping && (now - state.lastActivityTime) > SLEEP_TIMEOUT) {
    goToSleep();
  }

  drawDisplay();
}

// ===== EVENT LISTENERS =====
btnUp.addEventListener("click", () => {
  btnUp.classList.add("pressed");
  setTimeout(() => btnUp.classList.remove("pressed"), 140);
  handleBtnUp();
});

btnSel.addEventListener("click", () => {
  btnSel.classList.add("pressed");
  setTimeout(() => btnSel.classList.remove("pressed"), 140);
  handleBtnSel();
});

btnDown.addEventListener("click", () => {
  btnDown.classList.add("pressed");
  setTimeout(() => btnDown.classList.remove("pressed"), 140);
  handleBtnDown();
});

rfidTouch.addEventListener("click", () => {
  if (state.isSleeping) { wakeUp(); return; }
  manualInputEl.focus();
});

manualInputEl.addEventListener("keydown", async (e) => {
  if (e.key === "Enter") {
    e.preventDefault();
    const code = manualInputEl.value.trim();
    if (code && /^[a-z0-9\-_.]+$/i.test(code)) {
      manualInputEl.value = "";
      await processRfidInput(code);
    }
  }
});

document.addEventListener("keydown", (e) => {
  if (document.activeElement === manualInputEl) return;

  if (e.key === "ArrowUp") {
    e.preventDefault();
    handleBtnUp();
  } else if (e.key === "ArrowDown") {
    e.preventDefault();
    handleBtnDown();
  } else if (e.key === "Enter") {
    e.preventDefault();
    handleBtnSel();
  }
});

// ===== INIT =====
loadNVS();
setInterval(mainLoop, 100);
drawDisplay();
