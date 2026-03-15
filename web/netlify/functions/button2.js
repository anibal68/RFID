const { response, supabaseLookup } = require("./_supabase");

exports.handler = async () => {
  const barco = process.env.BARCO_DEFAULT || "01010";
  const lookup = await supabaseLookup({
    table: "ordens_producao",
    filterCol: "rfid_token",
    filterVal: barco,
    targetCol: "display_nome",
  });

  return response(200, { ok: lookup.ok, value: lookup.value || "Nao encontrado" });
};
