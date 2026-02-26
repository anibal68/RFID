const state = {
  lastPressed: -1,
  tempUntil: 0,
  tempLine1: "",
  tempLine2: "",
  ip: "192.168.1.120",
  battery: 50,
  rssi: -58,
};

const line1El = document.getElementById("line1");
const line2El = document.getElementById("line2");
const batteryFillEl = document.getElementById("batteryFill");
const wifiIconEl = document.getElementById("wifiIcon");
const ledEl = document.getElementById("rfidLed");
const manualInputEl = document.getElementById("manualInput");

const btnUp = document.getElementById("btnUp");
const btnSel = document.getElementById("btnSel");
const btnDown = document.getElementById("btnDown");
const rfidTouch = document.getElementById("rfidTouch");

function drawDisplay() {
  batteryFillEl.style.width = `${state.battery}%`;

  const bars = wifiIconEl.querySelectorAll("span");
  const thresholds = [-90, -80, -70, -60];
  bars.forEach((bar, index) => {
    bar.style.opacity = state.rssi > thresholds[index] ? "1" : "0.2";
  });

  const now = Date.now();
  if (state.tempUntil > now) {
    line1El.textContent = state.tempLine1;
    line2El.textContent = state.tempLine2;
    return;
  }

  line1El.textContent = state.ip;
  line2El.textContent = state.lastPressed !== -1 ? `Ultimo: ${state.lastPressed}` : "Pronto...";
}

function setTemp(line1, line2, ms) {
  state.tempLine1 = line1;
  state.tempLine2 = line2;
  state.tempUntil = Date.now() + ms;
  drawDisplay();
}

async function callApi(route, body = {}) {
  const response = await fetch(`/api/${route}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });

  if (!response.ok) {
    return { ok: false, message: "Erro API" };
  }

  return response.json();
}

function animateButton(buttonElement) {
  buttonElement.classList.add("pressed");
  setTimeout(() => buttonElement.classList.remove("pressed"), 140);
}

async function handleButton(buttonNumber, element) {
  state.lastPressed = buttonNumber;
  if (buttonNumber === 1) {
    setTemp("Enviando Tempo...", "", 15000);
    const result = await callApi("button1");
    setTemp("Enviando Tempo...", result.ok ? "Sucesso!" : "Erro!", 2000);
  }

  if (buttonNumber === 2) {
    setTemp("Buscando Barco...", "", 15000);
    const result = await callApi("button2");
    setTemp("Ordem:", result.value || "Nao encontrado", 3000);
  }

  if (buttonNumber === 3) {
    setTemp("Buscando Operador...", "", 15000);
    const result = await callApi("button3");
    setTemp("Nome:", result.value || "Nao encontrado", 3000);
  }

  if (element) {
    animateButton(element);
  }
}

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
    remaining -= 1;
    setTimeout(blink, 130);
  };

  blink();
}

async function submitRfidCode() {
  const code = manualInputEl.value.trim().toUpperCase();
  if (!code || !/^[A-Z0-9]+$/.test(code)) {
    return;
  }

  const result = await callApi("rfid", { code });
  if (result.ok) {
    blinkLed(3);
    setTemp("NFC:", result.uid, 1500);
    manualInputEl.value = "";
  }
}

btnUp.addEventListener("click", () => handleButton(2, btnUp));
btnSel.addEventListener("click", () => handleButton(1, btnSel));
btnDown.addEventListener("click", () => handleButton(3, btnDown));
rfidTouch.addEventListener("click", () => manualInputEl.focus());

manualInputEl.addEventListener("keydown", async (event) => {
  if (event.key === "Enter") {
    event.preventDefault();
    await submitRfidCode();
  }
});

document.addEventListener("keydown", async (event) => {
  if (event.key === "ArrowUp") {
    event.preventDefault();
    await handleButton(2, btnUp);
  }

  if (event.key === "ArrowDown") {
    event.preventDefault();
    await handleButton(3, btnDown);
  }

  if (event.key === "Enter" && document.activeElement !== manualInputEl) {
    event.preventDefault();
    await handleButton(1, null);
  }
});

setInterval(drawDisplay, 250);
drawDisplay();
