const { response, supabaseInsert } = require("./_supabase");

function formattedTime() {
  const timezone = process.env.APP_TIMEZONE || "Europe/Lisbon";
  const formatter = new Intl.DateTimeFormat("pt-PT", {
    timeZone: timezone,
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    day: "2-digit",
    month: "2-digit",
    hour12: false,
  });

  const parts = formatter.formatToParts(new Date());
  const get = (type) => parts.find((part) => part.type === type)?.value || "00";

  const hh = get("hour");
  const mm = get("minute");
  const ss = get("second");
  const dd = get("day");
  const mo = get("month");

  return `${hh}:${mm}:${ss} ${dd}/${mo}`;
}

exports.handler = async () => {
  const operador = process.env.OPERADOR_DEFAULT || "000747";
  const tempo = formattedTime();
  const result = await supabaseInsert({
    table: "tempos",
    data: {
      operador,
      tempo,
    },
  });

  return response(200, { ok: result.ok, tempo });
};
