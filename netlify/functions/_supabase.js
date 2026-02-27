const jsonHeaders = {
  "Content-Type": "application/json",
};

function response(statusCode, body) {
  return {
    statusCode,
    headers: jsonHeaders,
    body: JSON.stringify(body),
  };
}

function getSupabaseEnv() {
  const url = process.env.SUPABASE_URL || "";
  const key = process.env.SUPABASE_SERVICE_ROLE_KEY || process.env.SUPABASE_KEY || "";
  return { url, key, configured: Boolean(url && key) };
}

function authHeaders(key) {
  return {
    apikey: key,
    Authorization: `Bearer ${key}`,
  };
}

async function supabaseLookup({ table, filterCol, filterVal, targetCol }) {
  const { url, key, configured } = getSupabaseEnv();
  if (!configured) {
    return { ok: false, value: "Erro: Config ENV" };
  }

  const encoded = encodeURIComponent(filterVal);
  const endpoint = `${url}/rest/v1/${table}?${filterCol}=eq.${encoded}`;

  try {
    const res = await fetch(endpoint, {
      method: "GET",
      headers: authHeaders(key),
    });

    if (!res.ok) {
      return { ok: false, value: "Nao encontrado" };
    }

    const payload = await res.json();
    if (Array.isArray(payload) && payload.length > 0 && payload[0][targetCol] != null) {
      return { ok: true, value: String(payload[0][targetCol]) };
    }

    return { ok: true, value: "Nao encontrado" };
  } catch {
    return { ok: false, value: "Nao encontrado" };
  }
}

async function supabaseInsert({ table, data }) {
  const { url, key, configured } = getSupabaseEnv();
  if (!configured) {
    return { ok: false };
  }

  const endpoint = `${url}/rest/v1/${table}`;

  try {
    const res = await fetch(endpoint, {
      method: "POST",
      headers: {
        ...authHeaders(key),
        "Content-Type": "application/json",
      },
      body: JSON.stringify(data),
    });

    return { ok: res.status === 201 || res.status === 200 };
  } catch {
    return { ok: false };
  }
}

module.exports = {
  response,
  supabaseLookup,
  supabaseInsert,
};
