const { response, supabaseLookup } = require("./_supabase");

exports.handler = async () => {
  const operador = process.env.OPERADOR_DEFAULT || "000747";
  const lookup = await supabaseLookup({
    table: "operadores",
    filterCol: "numero",
    filterVal: operador,
    targetCol: "nome",
  });

  return response(200, { ok: lookup.ok, value: lookup.value || "Nao encontrado" });
};
