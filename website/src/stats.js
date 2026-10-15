// The stats API's queries and views. Reads go through the Analytics SQL
// binding (HITS_SQL). In that SQL API the Analytics Engine datasets are
// adaptively sampled and COUNT, SUM and countIf already apply the sample
// weights, so the queries count rows with COUNT(*) where the older Analytics
// Engine SQL API (and Counterscale) summed _sample_interval.

import { classify } from "./agents.js";
import { SITE_HOST } from "./hits.js";

export const DATASET = "events.analyticsEngine.ringshadow_hits";
export const RETENTION_DAYS = 90;
const DAY = 86_400_000;
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
