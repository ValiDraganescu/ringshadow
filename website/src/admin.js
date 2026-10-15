// The admin: its pages under /admin/ (static files the Worker serves only
// with a session) and its API under /api/admin/. D1 binding ADMIN.
// See docs/website-analytics.md, "The admin".

import { compileRules } from "./agents.js";
import { history } from "./rollup.js";
import { addRule, changeRule, deleteRule, listRules, testRules } from "./rules.js";
import {
  DAY, agentsView, byKeyFrom, droppedSql, eventsFrom, eventsSql, pageByColumnSql, pageSeriesSql, parseRange,
  seriesFrom, serverByAgentSql, serverPathsSql,
} from "./stats.js";
import {
  LOCK_FAILS, LOCK_MINUTES, PW_ITERS, SESSION_DAYS,
  hashPassword, isoNow, lockKeys, newSessionToken, normalEmail, oneTimePassword,
  passwordProblem, pbkdf2, readCookie, sameBytes, sessionCookie, sha256Hex,
  validEmail, verifyPassword,
} from "./auth.js";

const PRIVATE = { "Cache-Control": "no-store", "X-Robots-Tag": "noindex, nofollow" };
const DUMMY_SALT = new Uint8Array(16);

export function json(body, status = 200, extra = {}) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "Content-Type": "application/json; charset=utf-8", ...PRIVATE, ...extra },
  });
}

function privately(response) {
  const r = new Response(response.body, response);
  for (const [k, v] of Object.entries(PRIVATE)) r.headers.set(k, v);
  return r;
}

const redirect = (to) => new Response(null, { status: 302, headers: { Location: to, ...PRIVATE } });
const clearCookie = () => sessionCookie("", 0);

async function readJson(request) {
  try {
    const body = await request.json();
    return body && typeof body === "object" ? body : null;
  } catch {
    return null;
  }
}

async function adminCount(env) {
  return (await env.ADMIN.prepare("SELECT count(*) AS n FROM admins").first()).n;
}

// The signed-in admin of a request, or null.
export async function currentSession(request, env) {
  const token = readCookie(request);
  if (!token) return null;
  const id = await sha256Hex(token);
  return env.ADMIN.prepare(
    `SELECT s.id, s.admin_id, a.email, a.must_change_pw
       FROM sessions s JOIN admins a ON a.id = s.admin_id
      WHERE s.id = ? AND s.expires_at > ?`,
  ).bind(id, isoNow()).first();
}

async function startSession(env, adminId, request) {
  const { token, id } = await newSessionToken();
  const now = Date.now();
  await env.ADMIN.prepare("INSERT INTO sessions (id, admin_id, created_at, expires_at, ua) VALUES (?, ?, ?, ?, ?)")
    .bind(id, adminId, isoNow(now), isoNow(now + SESSION_DAYS * 86_400_000), (request.headers.get("User-Agent") || "").slice(0, 256))
    .run();
  return sessionCookie(token, SESSION_DAYS * 86_400);
}

// --- Lockout: five failures on an email or a network lock it for 15 minutes.

async function lockedUntil(env, keys) {
  const { results } = await env.ADMIN.prepare("SELECT locked_until FROM login_attempts WHERE key IN (?, ?) AND locked_until > ?")
    .bind(keys[0], keys[1], isoNow()).all();
  return results.map((r) => r.locked_until).sort().pop() || null;
}

async function recordFailure(env, keys) {
  let locked = null;
  for (const key of keys) {
    const row = await env.ADMIN.prepare(
      `INSERT INTO login_attempts (key, fails, locked_until) VALUES (?, 1, NULL)
       ON CONFLICT(key) DO UPDATE SET fails = fails + 1 RETURNING fails`,
    ).bind(key).first();
    if (row.fails >= LOCK_FAILS) {
      locked = isoNow(Date.now() + LOCK_MINUTES * 60_000);
      await env.ADMIN.prepare("UPDATE login_attempts SET fails = 0, locked_until = ? WHERE key = ?").bind(locked, key).run();
    }
  }
  return locked;
}

const clearFailures = (env, keys) => env.ADMIN.prepare("DELETE FROM login_attempts WHERE key IN (?, ?)").bind(keys[0], keys[1]).run();

// --- The API.

async function register(request, env) {
  if ((await adminCount(env)) > 0) return json({ error: "There is an admin already." }, 404);
  if (!env.ADMIN_SETUP_CODE) return json({ error: "No setup code is set on the Worker." }, 503);
  const body = await readJson(request);
  if (!body) return json({ error: "Bad request." }, 400);
  const email = normalEmail(body.email);
  const code = String(body.code || "").trim();
  const codeOk = sameBytes(new TextEncoder().encode(await sha256Hex(code)), new TextEncoder().encode(await sha256Hex(env.ADMIN_SETUP_CODE)));
  if (!codeOk) return json({ error: "The setup code is wrong." }, 403);
  if (!validEmail(email)) return json({ error: "That email doesn't look right." }, 400);
  const problem = passwordProblem(body.password, email);
  if (problem) return json({ error: problem }, 400);
  const h = await hashPassword(String(body.password));
  const now = isoNow();
  const row = await env.ADMIN.prepare(
    `INSERT INTO admins (email, pw_hash, pw_salt, pw_iters, must_change_pw, created_at, last_login_at)
     SELECT ?, ?, ?, ?, 0, ?, ? WHERE NOT EXISTS (SELECT 1 FROM admins) RETURNING id`,
  ).bind(email, h.pw_hash, h.pw_salt, h.pw_iters, now, now).first();
  if (!row) return json({ error: "There is an admin already." }, 404);
  return json({ ok: true }, 201, { "Set-Cookie": await startSession(env, row.id, request) });
}

async function login(request, env) {
  const body = await readJson(request);
  if (!body) return json({ error: "Bad request." }, 400);
  const email = normalEmail(body.email);
  const password = String(body.password || "");
  const keys = await lockKeys(email, request.headers.get("CF-Connecting-IP"));
  const until = await lockedUntil(env, keys);
  if (until) return json({ error: "Too many wrong tries. Locked until " + until + ".", until }, 429);
  const row = await env.ADMIN.prepare("SELECT * FROM admins WHERE email = ?").bind(email).first();
  // An unknown email costs the same hash, so timing doesn't tell which emails exist.
  const ok = row ? await verifyPassword(password, row) : (await pbkdf2(password, DUMMY_SALT, PW_ITERS), false);
  if (!ok) {
    const locked = await recordFailure(env, keys);
    if (locked) return json({ error: "Too many wrong tries. Locked until " + locked + ".", until: locked }, 429);
    return json({ error: "Wrong email or password." }, 401);
  }
  const now = isoNow();
  await clearFailures(env, keys);
  await env.ADMIN.batch([
    env.ADMIN.prepare("UPDATE admins SET last_login_at = ? WHERE id = ?").bind(now, row.id),
    env.ADMIN.prepare("DELETE FROM sessions WHERE expires_at <= ?").bind(now),
  ]);
  return json({ ok: true, mustChange: !!row.must_change_pw }, 200, { "Set-Cookie": await startSession(env, row.id, request) });
}

async function logout(env, session) {
  await env.ADMIN.prepare("DELETE FROM sessions WHERE id = ?").bind(session.id).run();
  return json({ ok: true }, 200, { "Set-Cookie": clearCookie() });
}

// Every session row, every admin, every device.
async function signOutEverywhere(env) {
  const r = await env.ADMIN.prepare("DELETE FROM sessions").run();
  return json({ ok: true, cleared: r.meta.changes }, 200, { "Set-Cookie": clearCookie() });
}

async function changePassword(request, env, session) {
  const body = await readJson(request);
  if (!body) return json({ error: "Bad request." }, 400);
  const row = await env.ADMIN.prepare("SELECT * FROM admins WHERE id = ?").bind(session.admin_id).first();
  if (!(await verifyPassword(String(body.current || ""), row))) return json({ error: "The current password is wrong." }, 403);
  if (String(body.next) === String(body.current)) return json({ error: "The new password has to differ from the old one." }, 400);
  const problem = passwordProblem(body.next, row.email);
  if (problem) return json({ error: problem }, 400);
  const h = await hashPassword(String(body.next));
  await env.ADMIN.batch([
    env.ADMIN.prepare("UPDATE admins SET pw_hash = ?, pw_salt = ?, pw_iters = ?, must_change_pw = 0 WHERE id = ?")
      .bind(h.pw_hash, h.pw_salt, h.pw_iters, row.id),
    // The admin's other sessions end; this one stays.
    env.ADMIN.prepare("DELETE FROM sessions WHERE admin_id = ? AND id <> ?").bind(row.id, session.id),
  ]);
  return json({ ok: true });
}

async function listAdmins(env) {
  const { results } = await env.ADMIN.prepare(
    "SELECT id, email, must_change_pw, created_at, last_login_at FROM admins ORDER BY id",
  ).all();
  return json({ admins: results });
}

async function addAdmin(request, env) {
  const body = await readJson(request);
  if (!body) return json({ error: "Bad request." }, 400);
  const email = normalEmail(body.email);
  if (!validEmail(email)) return json({ error: "That email doesn't look right." }, 400);
  const password = oneTimePassword();
  const h = await hashPassword(password);
  const row = await env.ADMIN.prepare(
    `INSERT INTO admins (email, pw_hash, pw_salt, pw_iters, must_change_pw, created_at)
     VALUES (?, ?, ?, ?, 1, ?) ON CONFLICT(email) DO NOTHING RETURNING id`,
  ).bind(email, h.pw_hash, h.pw_salt, h.pw_iters, isoNow()).first();
  if (!row) return json({ error: "That email is an admin already." }, 409);
  return json({ ok: true, email, password }, 201);
}

// --- The analytics.

// The Agents view. It reads the rules fresh from D1 rather than from the
// isolate's cache, so a rule added a moment ago already relabels the history.
async function agentsStats(request, env) {
  const search = new URL(request.url).searchParams;
  const range = parseRange(search);
  if (range.error) return json({ error: range.error }, 400);
  const probesOnly = search.get("probes") === "1";
  const params = { from: range.from, to: range.to };
  let byAgent, byPath, dropped;
  try {
    [byAgent, byPath, dropped] = await Promise.all([
      env.HITS_SQL.query({ query: serverByAgentSql(range.bucket, probesOnly), params }),
      env.HITS_SQL.query({ query: serverPathsSql(probesOnly), params }),
      env.HITS_SQL.query({ query: droppedSql(), params }),
    ]);
  } catch (err) {
    console.error("analytics sql failed", err);
    return json({ error: "The analytics query failed" + (err.retryable ? "; try again." : ": " + err.message) }, 502);
  }
  const rules = compileRules(await listRules(env));
  return json({ range, probesOnly, dropped: Number(dropped.data[0]?.dropped) || 0, ...agentsView(byAgent.data, byPath.data, rules) });
}

// The people's views: each runs its queries over the range and adds up the
// rows the Counterscale way. Overview ignores the range: today, 7 and 30 days.
const sqlError = (err) => {
  console.error("analytics sql failed", err);
  return json({ error: "The analytics query failed" + (err.retryable ? "; try again." : ": " + err.message) }, 502);
};

async function rows(env, query, range) {
  return (await env.HITS_SQL.query({ query, params: { from: range.from, to: range.to } })).data;
}

const VIEWS = {
  async overview(env) {
    const now = Date.now();
    const month = { from: new Date(now - 30 * DAY).toISOString(), to: new Date(now).toISOString(), bucket: "day" };
    const [series, paths, referrers, countries] = await Promise.all([
      rows(env, pageSeriesSql("day"), month),
      rows(env, pageByColumnSql("path"), month),
      rows(env, pageByColumnSql("referrer"), month),
      rows(env, pageByColumnSql("country"), month),
    ]);
    const days = seriesFrom(series, month);
    const sum = (list) => list.reduce((a, d) => ({ views: a.views + d.views, visitors: a.visitors + d.visitors }), { views: 0, visitors: 0 });
    return {
      range: month,
      today: sum(days.slice(-1)),
      week: sum(days.slice(-7)),
      month: sum(days),
      series: days,
      pages: byKeyFrom(paths, 5),
      referrers: byKeyFrom(referrers.filter((r) => r.key), 5),
      countries: byKeyFrom(countries, 5),
    };
  },
  async pages(env, range) {
    return { range, series: seriesFrom(await rows(env, pageSeriesSql(range.bucket), range), range), pages: byKeyFrom(await rows(env, pageByColumnSql("path"), range), 200) };
  },
  async sources(env, range) {
    const [referrers, sources, media] = await Promise.all(["referrer", "utmSource", "utmMedium"].map((c) => rows(env, pageByColumnSql(c), range)));
    return { range, referrers: byKeyFrom(referrers), utmSources: byKeyFrom(sources.filter((r) => r.key)), utmMedia: byKeyFrom(media.filter((r) => r.key)) };
  },
  async visitors(env, range) {
    const [countries, devices, browsers] = await Promise.all(["country", "device", "browser"].map((c) => rows(env, pageByColumnSql(c), range)));
    return { range, countries: byKeyFrom(countries), devices: byKeyFrom(devices), browsers: byKeyFrom(browsers) };
  },
  async events(env, range) {
    return { range, events: eventsFrom(await rows(env, eventsSql(range.bucket), range), range) };
  },
};

async function peopleStats(request, env, view) {
  const range = parseRange(new URL(request.url).searchParams);
  if (range.error) return json({ error: range.error }, 400);
  try {
    return json(await VIEWS[view](env, range));
  } catch (err) {
    return sqlError(err);
  }
}

// The History view: D1's daily rollups, any span of days (default a year).
async function historyStats(request, env) {
  const q = new URL(request.url).searchParams;
  const today = new Date().toISOString().slice(0, 10);
  const to = q.get("to") || today;
  const from = q.get("from") || new Date(Date.parse(to + "T00:00:00Z") - 365 * DAY).toISOString().slice(0, 10);
  const date = /^\d{4}-\d{2}-\d{2}$/;
  if (!date.test(from) || !date.test(to) || from > to) return json({ error: "History needs from and to dates, from first." }, 400);
  return json(await history(env, from, to));
}

const answer = (r) => json(r.body, r.status);

// Routes under /api/admin/. Writes need an Origin from this site; everything
// but the state, login and register needs a session; an admin with a
// one-time password can only change it or sign out.
export async function handleApi(request, env) {
  const url = new URL(request.url);
  const route = request.method + " " + url.pathname.replace(/\/+$/, "");
  if (request.method !== "GET" && request.method !== "HEAD" && request.headers.get("Origin") !== url.origin) {
    return json({ error: "Bad origin." }, 403);
  }
  if (route === "GET /api/admin/state") return json({ hasAdmin: (await adminCount(env)) > 0 });
  if (route === "POST /api/admin/register") return register(request, env);
  if (route === "POST /api/admin/login") return login(request, env);

  const session = await currentSession(request, env);
  if (!session) return json({ error: "Sign in first." }, 401);
  if (route === "GET /api/admin/me") return json({ email: session.email, mustChange: !!session.must_change_pw });
  if (route === "POST /api/admin/logout") return logout(env, session);
  if (route === "POST /api/admin/password") return changePassword(request, env, session);
  if (route === "POST /api/admin/signout-all") return signOutEverywhere(env);
  if (session.must_change_pw) return json({ error: "Change the one-time password first." }, 403);
  if (route === "GET /api/admin/admins") return listAdmins(env);
  if (route === "POST /api/admin/admins") return addAdmin(request, env);
  if (route === "GET /api/admin/stats/agents") return agentsStats(request, env);
  const view = route.match(/^GET \/api\/admin\/stats\/(overview|pages|sources|visitors|events)$/);
  if (view) return peopleStats(request, env, view[1]);
  if (route === "GET /api/admin/history") return historyStats(request, env);
  if (route === "GET /api/admin/rules") return json({ rules: await listRules(env) });
  if (route === "POST /api/admin/rules/test") return json(await testRules(env, (await readJson(request))?.ua));
  if (route === "POST /api/admin/rules") return answer(await addRule(env, await readJson(request), session.admin_id));
  const rule = url.pathname.match(/^\/api\/admin\/rules\/(\d+)$/);
  if (rule && request.method === "PUT") return answer(await changeRule(env, Number(rule[1]), await readJson(request)));
  if (rule && request.method === "DELETE") return answer(await deleteRule(env, Number(rule[1])));
  return json({ error: "Not found." }, 404);
}

// Pages under /admin/. The stylesheet and script are public (they hold no
// data); login and register are open (register only while there is no
// admin); everything else needs a session.
export async function handlePage(request, env) {
  const url = new URL(request.url);
  const path = url.pathname;
  if (path === "/admin/admin.css" || path === "/admin/admin.js") return privately(await env.ASSETS.fetch(request));
  if (/^\/admin\/register(\.html)?$/.test(path)) {
    if ((await adminCount(env)) > 0) {
      return privately(await env.ASSETS.fetch(new Request(new URL("/admin/no-such-page", url), { headers: request.headers })));
    }
    return privately(await env.ASSETS.fetch(request));
  }
  const session = await currentSession(request, env);
  if (/^\/admin\/login(\.html)?$/.test(path)) {
    return session ? redirect("/admin/") : privately(await env.ASSETS.fetch(request));
  }
  if (!session) return redirect("/admin/login");
  return privately(await env.ASSETS.fetch(request));
}
