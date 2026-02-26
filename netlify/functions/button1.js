const { response, supabaseInsert } = require("./_supabase");

function formattedTime() {
  const now = new Date();
  const hh = String(now.getHours()).padStart(2, "0");
  const mm = String(now.getMinutes()).padStart(2, "0");
  const dd = String(now.getDate()).padStart(2, "0");
  const mo = String(now.getMonth() + 1).padStart(2, "0");
  return `${hh}:${mm} ${dd}/${mo}`;
}

exports.handler = async () => {
  const operador = process.env.OPERADOR_DEFAULT || "000747";
  const result = await supabaseInsert({
    table: "tempos",
    data: {
      operador,
      tempo: formattedTime(),
    },
  });

  return response(200, { ok: result.ok });
};
