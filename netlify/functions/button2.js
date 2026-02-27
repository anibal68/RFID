const { response, supabaseLookup } = require("./_supabase");

exports.handler = async () => {
  const barco = process.env.BARCO_DEFAULT || "01010";
  const lookup = await supabaseLookup({
    table: "barcos",
    filterCol: "barco",
    filterVal: barco,
    targetCol: "ordem_fabrico",
  });

  return response(200, { ok: lookup.ok, value: lookup.value || "Nao encontrado" });
};
