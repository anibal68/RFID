const { response } = require("./_supabase");

exports.handler = async (event) => {
  let code = "";

  try {
    const payload = event.body ? JSON.parse(event.body) : {};
    code = String(payload.code || "").trim().toUpperCase();
  } catch {
    return response(400, { ok: false, message: "Payload invalido" });
  }

  if (!code || !/^[A-Z0-9]+$/.test(code)) {
    return response(400, { ok: false, message: "Codigo invalido" });
  }

  return response(200, { ok: true, uid: code });
};
