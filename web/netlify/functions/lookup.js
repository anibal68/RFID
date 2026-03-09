const { response, supabaseLookup } = require("./_supabase");

exports.handler = async (event) => {
  let table, filterCol, filterVal, targetCol;

  try {
    const payload = event.body ? JSON.parse(event.body) : {};
    table = String(payload.table || "").trim();
    filterCol = String(payload.filterCol || "").trim();
    filterVal = String(payload.filterVal || "").trim();
    targetCol = String(payload.targetCol || "").trim();
  } catch {
    return response(400, { ok: false, message: "Payload invalido" });
  }

  if (!table || !filterCol || !filterVal || !targetCol) {
    return response(400, { ok: false, message: "Parametros em falta" });
  }

  const lookup = await supabaseLookup({ table, filterCol, filterVal, targetCol });
  return response(200, { ok: lookup.ok, value: lookup.value || "Nao encontrado" });
};
