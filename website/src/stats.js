// The stats API's queries and views. Reads go through the Analytics SQL
// binding (HITS_SQL). In that SQL API the Analytics Engine datasets are
// adaptively sampled and COUNT, SUM and countIf already apply the sample
// weights, so the queries count rows with COUNT(*) where the older Analytics
// Engine SQL API (and Counterscale) summed _sample_interval.

import { classify } from "./agents.js";
import { SITE_HOST } from "./hits.js";
import { accumulateCountsFromRowResult, fixNegativeBounces } from "./vendor/counterscale/counts.js";

export const DATASET = "events.analyticsEngine.ringshadow_hits2";
export const RETENTION_DAYS = 90;
export const DAY = 86_400_000;
const SPANS = { "24h": 1, "7d": 7, "30d": 30, "90d": 90 };

// The range a request asks for: ?range=24h|7d|30d|90d, or ?range=custom
// &from=YYYY-MM-DD&to=YYYY-MM-DD (both days included). Clamped to the 90
// days Analytics Engine keeps. Hours as buckets up to two days, else days.
export function parseRange(params, now = Date.now()) {
  const range = params.get("range") || "7d";
  let from;
  let to = now;
  if (SPANS[range]) {
    from = now - SPANS[range] * DAY;
  } else if (range === "custom") {
    const f = Date.parse(params.get("from") + "T00:00:00Z");
    const t = Date.parse(params.get("to") + "T00:00:00Z");
    if (Number.isNaN(f) || Number.isNaN(t) || t < f) return { error: "A custom range needs from and to dates, from first." };
    from = f;
    to = Math.min(now, t + DAY);
  } else {
    return { error: "Unknown range." };
  }
  from = Math.max(from, now - RETENTION_DAYS * DAY);
  if (from >= to) return { error: "The range is empty." };
  return {
    range,
    from: new Date(from).toISOString(),
    to: new Date(to).toISOString(),
    bucket: to - from <= 2 * DAY ? "hour" : "day",
  };
}

// The index is the sampling key (the User-Agent for server hits), so the
// queries select by kind and host instead.
const where = (kind, extra = "") =>
  `WHERE blob1 = '${kind}' AND blob3 = '${SITE_HOST}' AND timestamp >= $from AND timestamp < $to${extra ? " AND " + extra : ""}`;
// A probe: a server hit answered 400 or above (double4 is the status; rows
// written before it was recorded have 0 and count as fetches).
const PROBE = "double4 >= 400";
const bucketOf = (bucket) => (bucket === "hour" ? "toStartOfHour(timestamp)" : "toStartOfDay(timestamp)");

// Server hits by raw User-Agent and time bucket; the labels are applied in
// the Worker from the current rules, so a new rule relabels the history.
// probesOnly keeps only the hits answered 400 or above.
export function serverByAgentSql(bucket, probesOnly = false) {
  return `SELECT blob12 AS ua, blob11 AS wclass, blob10 AS wlabel, ${bucketOf(bucket)} AS t, COUNT(*) AS hits, countIf(${PROBE}) AS probes
FROM ${DATASET}
${where("server", probesOnly ? PROBE : "")}
GROUP BY ua, wclass, wlabel, t`;
}

export function serverPathsSql(probesOnly = false) {
  return `SELECT blob12 AS ua, blob11 AS wclass, blob10 AS wlabel, blob2 AS path, double4 AS status, COUNT(*) AS hits
FROM ${DATASET}
${where("server", probesOnly ? PROBE : "")}
GROUP BY ua, wclass, wlabel, path, status`;
}

export function droppedSql() {
  return `SELECT SUM(double3) AS dropped FROM ${DATASET} ${where("meta")}`;
}

// A row's label under the current rules. A browser's raw string isn't kept
// (only its family), so browser rows keep the label they were written with.
export function relabel(row, compiled) {
  if (row.wclass === "browser") return { label: row.wlabel || "Browsers", class: "browser" };
  return classify(row.ua, compiled);
}

const n = (v) => Number(v) || 0;
const key = (a) => a.label + "\u0000" + a.class;

// The Agents view from the two queries' rows: hits and probes by label and
// class, a series by class over time, the top paths per label (each with
// its status, so a probed hole shows as a 404), and the strings no rule
// matches. Browser document fetches are counted apart: they are not agents,
// and the human numbers come from the tracker.
export function agentsView(byAgent, byPath, compiled) {
  const labels = new Map();
  const series = new Map();
  const unlabelled = new Map();
  let total = 0;
  let probes = 0;
  let browsers = 0;
  for (const row of byAgent) {
    const a = relabel(row, compiled);
    const hits = n(row.hits);
    if (a.class === "browser") {
      browsers += hits;
      continue;
    }
    total += hits;
    probes += n(row.probes);
    const l = labels.get(key(a)) || { label: a.label, class: a.class, hits: 0, probes: 0, strings: new Set() };
    l.hits += hits;
    l.probes += n(row.probes);
    l.strings.add(row.ua);
    labels.set(key(a), l);
    const t = String(row.t);
    const s = series.get(t) || {};
    s[a.class] = (s[a.class] || 0) + hits;
    series.set(t, s);
    if (a.label === "unlabelled") {
      const u = unlabelled.get(row.ua) || { ua: row.ua, hits: 0, probes: 0 };
      u.hits += hits;
      u.probes += n(row.probes);
      unlabelled.set(row.ua, u);
    }
  }
  const paths = new Map();
  for (const row of byPath) {
    const a = relabel(row, compiled);
    if (a.class === "browser") continue;
    const m = paths.get(key(a)) || new Map();
    const status = n(row.status);
    const k = row.path + "\u0000" + status;
    const e = m.get(k) || { path: row.path, status, hits: 0 };
    e.hits += n(row.hits);
    m.set(k, e);
    paths.set(key(a), m);
  }
  const byHits = (a, b) => b.hits - a.hits;
  return {
    total,
    probes,
    browsers,
    labels: [...labels.values()]
      .map((l) => ({
        label: l.label,
        class: l.class,
        hits: l.hits,
        probes: l.probes,
        strings: l.strings.size,
        paths: [...(paths.get(key(l)) || new Map()).values()].sort(byHits).slice(0, 10),
      }))
      .sort(byHits),
    series: [...series].map(([t, byClass]) => ({ t, byClass })).sort((a, b) => (a.t < b.t ? -1 : 1)),
    unlabelled: [...unlabelled.values()].sort(byHits),
  };
}

// --- People: the tracker's kind=page points (and kind=event for events).
// Counterscale's way: group by the visitor and bounce columns and add up in
// the Worker (src/vendor/counterscale/counts.js).

// The page-hit columns the views group by.
export const COLUMNS = {
  path: "blob2",
  referrer: "blob4",
  country: "blob5",
  browser: "blob6",
  device: "blob8",
  utmSource: "blob14",
  utmMedium: "blob15",
};

export function pageTotalsSql() {
  return `SELECT double1 AS isVisitor, double2 AS isBounce, COUNT(*) AS count
FROM ${DATASET}
${where("page")}
GROUP BY isVisitor, isBounce`;
}

export function pageSeriesSql(bucket) {
  return `SELECT ${bucketOf(bucket)} AS t, double1 AS isVisitor, double2 AS isBounce, COUNT(*) AS count
FROM ${DATASET}
${where("page")}
GROUP BY t, isVisitor, isBounce`;
}

export function pageByColumnSql(column) {
  const blob = COLUMNS[column];
  if (!blob) throw new Error("unknown column " + column);
  return `SELECT ${blob} AS key, double1 AS isVisitor, double2 AS isBounce, COUNT(*) AS count
FROM ${DATASET}
${where("page")}
GROUP BY key, isVisitor, isBounce`;
}

export function eventsSql(bucket) {
  return `SELECT blob9 AS event, blob2 AS path, ${bucketOf(bucket)} AS t, COUNT(*) AS count
FROM ${DATASET}
${where("event")}
GROUP BY event, path, t`;
}

const zero = () => ({ views: 0, visitors: 0, bounces: 0 });
const asRow = (r) => ({ count: n(r.count), isVisitor: n(r.isVisitor), isBounce: n(r.isBounce) });

// Every bucket of a range, as the SQL API writes them ("2026-10-15 00:00:00").
export function buckets(range) {
  const step = range.bucket === "hour" ? 3_600_000 : DAY;
  const out = [];
  for (let t = Math.floor(Date.parse(range.from) / step) * step; t < Date.parse(range.to); t += step) {
    out.push(new Date(t).toISOString().slice(0, 19).replace("T", " "));
  }
  return out;
}

// Views, visitors and bounces per bucket, gaps filled, bounces moved back.
export function seriesFrom(rows, range) {
  const by = new Map(buckets(range).map((b) => [b, zero()]));
  for (const r of rows) {
    const k = String(r.t);
    if (!by.has(k)) by.set(k, zero());
    accumulateCountsFromRowResult(by.get(k), asRow(r));
  }
  const sorted = [...by].sort((a, b) => (a[0] < b[0] ? -1 : 1));
  return fixNegativeBounces(sorted).map(([t, c]) => ({ t, ...c }));
}

export function totalsFrom(rows) {
  const c = zero();
  for (const r of rows) accumulateCountsFromRowResult(c, asRow(r));
  return c;
}

// Views, visitors and bounces per key, most viewed first.
export function byKeyFrom(rows, limit = 100) {
  const by = new Map();
  for (const r of rows) {
    const k = String(r.key ?? "");
    if (!by.has(k)) by.set(k, zero());
    accumulateCountsFromRowResult(by.get(k), asRow(r));
  }
  return [...by]
    .map(([key, c]) => ({ key, ...c, bounces: Math.max(0, c.bounces) }))
    .sort((a, b) => b.views - a.views || (a.key < b.key ? -1 : 1))
    .slice(0, limit);
}

// Views, visitors and bounces per country, every country (the map colours
// them all). The collector stores "" when Cloudflare gives no country; it is
// counted as XX, Cloudflare's own code for unknown.
export function countriesFrom(rows) {
  return byKeyFrom(rows.map((r) => ({ ...r, key: r.key || "XX" })), Infinity);
}

// Events per name: the total, per bucket, and per page.
export function eventsFrom(rows, range) {
  const names = new Map();
  for (const r of rows) {
    const e = names.get(r.event) || { event: r.event, total: 0, series: new Map(buckets(range).map((b) => [b, 0])), pages: new Map() };
    const c = n(r.count);
    e.total += c;
    e.series.set(String(r.t), (e.series.get(String(r.t)) || 0) + c);
    e.pages.set(r.path, (e.pages.get(r.path) || 0) + c);
    names.set(r.event, e);
  }
  return [...names.values()]
    .map((e) => ({
      event: e.event,
      total: e.total,
      series: [...e.series].sort((a, b) => (a[0] < b[0] ? -1 : 1)).map(([t, count]) => ({ t, count })),
      pages: [...e.pages].map(([path, count]) => ({ path, count })).sort((a, b) => b.count - a.count),
    }))
    .sort((a, b) => b.total - a.total);
}

// --- The request log: every request the Worker answered (kind=server and
// kind=request), newest first, filtered, a page at a time.

export const LOG_PAGE = 50;
const STATUS_CLASSES = { "2xx": [200, 300], "3xx": [300, 400], "4xx": [400, 500], "5xx": [500, 600] };
const METHODS = ["GET", "HEAD", "POST", "PUT", "DELETE", "OPTIONS", "PATCH"];
const SOURCES = ["assets", "api", "collect", "notfound", "redirect"];

// The filters a request asks for, as SQL conditions and named parameters.
// Only known values pass; the free-text ones go in as parameters.
export function logFilters(q) {
  const conds = [];
  const params = {};
  const cls = STATUS_CLASSES[q.get("status")];
  if (cls) {
    conds.push("double4 >= $lo AND double4 < $hi");
    Object.assign(params, { lo: cls[0], hi: cls[1] });
  }
  const method = (q.get("method") || "").toUpperCase();
  if (METHODS.includes(method)) {
    conds.push("blob16 = $method");
    params.method = method;
  }
  const source = q.get("source");
  if (SOURCES.includes(source)) {
    conds.push("blob18 = $source");
    params.source = source;
  }
  const path = (q.get("path") || "").slice(0, 200);
  if (path) {
    conds.push("blob2 LIKE $path");
    params.path = "%" + path.replace(/[\\%_]/g, (c) => "\\" + c) + "%";
  }
  const label = (q.get("label") || "").slice(0, 80);
  if (label) {
    conds.push("blob10 = $label");
    params.label = label;
  }
  return { conds, params };
}

const requestWhere = (conds) =>
  `WHERE blob1 IN ('server', 'request') AND blob3 = '${SITE_HOST}' AND timestamp >= $from AND timestamp < $to` +
  conds.map((c) => " AND " + c).join("");

export function requestLogSql(conds, page) {
  const offset = Math.max(0, Math.floor(Number(page) || 0)) * LOG_PAGE;
  return `SELECT timestamp AS t, blob1 AS kind, blob16 AS method, blob2 AS path, double4 AS status, double5 AS bytes,
  double6 AS ms, blob18 AS source, blob17 AS type, blob10 AS label, blob11 AS class, blob12 AS ua, blob5 AS country,
  double7 AS admin, "sampleInterval" AS si
FROM ${DATASET}
${requestWhere(conds)}
ORDER BY timestamp DESC LIMIT ${LOG_PAGE + 1} OFFSET ${offset}`;
}

// Answers by status class over time (0 is a row written before the status was).
export function requestClassesSql(bucket, conds) {
  return `SELECT ${bucketOf(bucket)} AS t, floor(double4 / 100) AS cls, COUNT(*) AS n
FROM ${DATASET}
${requestWhere(conds)}
GROUP BY t, cls`;
}

export const classKey = (cls) => (Number(cls) >= 1 ? Math.floor(Number(cls)) + "xx" : "unknown");

export function requestClassesFrom(rows, range) {
  const by = new Map(buckets(range).map((b) => [b, {}]));
  for (const r of rows) {
    const t = String(r.t);
    const s = by.get(t) || {};
    const k = classKey(r.cls);
    s[k] = (s[k] || 0) + n(r.n);
    by.set(t, s);
  }
  return [...by].sort((a, b) => (a[0] < b[0] ? -1 : 1)).map(([t, byClass]) => ({ t, byClass }));
}

export function requestRow(r) {
  return {
    t: String(r.t), kind: r.kind, method: r.method || "", path: r.path, status: n(r.status), bytes: n(r.bytes),
    ms: n(r.ms), source: r.source || "", type: r.type || "", label: r.label, class: r.class, ua: r.ua,
    country: r.country, admin: n(r.admin) === 1, sample: n(r.si) || 1,
  };
}

// For the rollup: requests per status class over a day.
export function requestClassTotalsSql() {
  return `SELECT floor(double4 / 100) AS cls, COUNT(*) AS n
FROM ${DATASET}
${requestWhere([])}
GROUP BY cls`;
}
