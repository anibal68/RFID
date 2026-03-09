const { response, supabaseInsert } = require("./_supabase");

exports.handler = async (event) => {
  let table, data;

  try {
    const payload = event.body ? JSON.parse(event.body) : {};
    table = String(payload.table || "").trim();
    data = payload.data || {};
  } catch {
    return response(400, { ok: false, message: "Payload invalido" });
  }

  if (!table || typeof data !== "object" || Object.keys(data).length === 0) {
    return response(400, { ok: false, message: "Parametros em falta" });
  }

  const result = await supabaseInsert({ table, data });
  return response(200, { ok: result.ok });
};
