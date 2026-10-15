// The nightly rollup (cron 02:10 UTC): the previous UTC day's numbers from
// Analytics Engine into D1's rollups_day, which keeps them after the 90 days
// Analytics Engine does. Rows: (day, kind, dim, key, hits, visitors).
//   page   total ""        views, visitors
//   page   path/referrer/country <key>   views, visitors
//   server total ""        fetches (browsers included)
//   server label <label>   fetches, by the rules at rollup time
//   server class <class>   fetches
//   event  event <name>    count

import { compileRules } from "./agents.js";
import { listRules } from "./rules.js";
import {
  DAY, byKeyFrom, eventsSql, pageByColumnSql, pageTotalsSql, relabel, serverByAgentSql, totalsFrom,
} from "./stats.js";

export const dayBefore = (ms) => new Date(ms - DAY).toISOString().slice(0, 10);

// The rows for one day from the query results (pure, for the tests).
export function rollupRows(day, r, compiled) {
  const rows = [];
  const add = (kind, dim, key, hits, visitors = null) => rows.push({ day, kind, dim, key: String(key ?? ""), hits, visitors });
  const total = totalsFrom(r.pageTotals);
  add("page", "total", "", total.views, total.visitors);
  for (const [dim, list] of [["path", r.paths], ["referrer", r.referrers], ["country", r.countries]]) {
    for (const k of byKeyFrom(list, Infinity)) add("page", dim, k.key, k.views, k.visitors);
  }
  const labels = new Map();
  const classes = new Map();
  let fetches = 0;
  for (const row of r.server) {
    const a = relabel(row, compiled);
    const hits = Number(row.hits) || 0;
    fetches += hits;
    labels.set(a.label, (labels.get(a.label) || 0) + hits);
    classes.set(a.class, (classes.get(a.class) || 0) + hits);
  }
  add("server", "total", "", fetches);
  for (const [k, v] of labels) add("server", "label", k, v);
  for (const [k, v] of classes) add("server", "class", k, v);
  const events = new Map();
  for (const row of r.events) events.set(row.event, (events.get(row.event) || 0) + (Number(row.count) || 0));
  for (const [k, v] of events) add("event", "event", k, v);
  return rows;
}

// Roll one day up: the queries, then the day's rows replaced in one batch.
export async function rollupDay(env, day) {
  const params = { from: day + "T00:00:00.000Z", to: new Date(Date.parse(day + "T00:00:00Z") + DAY).toISOString() };
  const q = async (query) => (await env.HITS_SQL.query({ query, params })).data;
  const [pageTotals, paths, referrers, countries, server, events] = await Promise.all([
    q(pageTotalsSql()),
    q(pageByColumnSql("path")),
    q(pageByColumnSql("referrer")),
    q(pageByColumnSql("country")),
    q(serverByAgentSql("day")),
    q(eventsSql("day")),
  ]);
  const rows = rollupRows(day, { pageTotals, paths, referrers, countries, server, events }, compileRules(await listRules(env)));
  const insert = env.ADMIN.prepare("INSERT INTO rollups_day (day, kind, dim, key, hits, visitors) VALUES (?, ?, ?, ?, ?, ?)");
  await env.ADMIN.batch([
    env.ADMIN.prepare("DELETE FROM rollups_day WHERE day = ?").bind(day),
    ...rows.map((r) => insert.bind(r.day, r.kind, r.dim, r.key, r.hits, r.visitors)),
  ]);
  return { day, rows: rows.length };
}

// The History view: per-day totals and the top keys over a span of days.
export async function history(env, from, to) {
  const { results: days } = await env.ADMIN.prepare(
    `SELECT day, kind, dim, key, hits, visitors FROM rollups_day
      WHERE day >= ? AND day <= ? AND (dim = 'total' OR (kind = 'server' AND dim = 'class' AND key = 'browser') OR kind = 'event')
      ORDER BY day`,
  ).bind(from, to).all();
  const byDay = new Map();
  for (const r of days) {
    const d = byDay.get(r.day) || { day: r.day, views: 0, visitors: 0, fetches: 0, browsers: 0, events: 0 };
    if (r.kind === "page") Object.assign(d, { views: r.hits, visitors: r.visitors || 0 });
    else if (r.kind === "server" && r.dim === "total") d.fetches = r.hits;
    else if (r.kind === "server") d.browsers = r.hits;
    else if (r.kind === "event") d.events += r.hits;
    byDay.set(r.day, d);
  }
  const { results: tops } = await env.ADMIN.prepare(
    `SELECT kind, dim, key, SUM(hits) AS hits, SUM(visitors) AS visitors FROM rollups_day
      WHERE day >= ? AND day <= ? AND dim <> 'total'
      GROUP BY kind, dim, key ORDER BY hits DESC`,
  ).bind(from, to).all();
  const top = {};
  for (const r of tops) {
    const k = r.kind + "." + r.dim;
    (top[k] ||= []).length < 10 && top[k].push({ key: r.key, hits: r.hits, visitors: r.visitors });
  }
  return {
    from,
    to,
    days: [...byDay.values()].map((d) => ({ ...d, agents: d.fetches - d.browsers })),
    top,
  };
}
