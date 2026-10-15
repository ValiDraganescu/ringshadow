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
const where = (kind) => `WHERE blob1 = '${kind}' AND blob3 = '${SITE_HOST}' AND timestamp >= $from AND timestamp < $to`;
const bucketOf = (bucket) => (bucket === "hour" ? "toStartOfHour(timestamp)" : "toStartOfDay(timestamp)");

// Server hits by raw User-Agent and time bucket; the labels are applied in
// the Worker from the current rules, so a new rule relabels the history.
export function serverByAgentSql(bucket) {
  return `SELECT blob12 AS ua, blob11 AS wclass, blob10 AS wlabel, ${bucketOf(bucket)} AS t, COUNT(*) AS hits
FROM ${DATASET}
${where("server")}
GROUP BY ua, wclass, wlabel, t`;
}

export function serverPathsSql() {
  return `SELECT blob12 AS ua, blob11 AS wclass, blob10 AS wlabel, blob2 AS path, COUNT(*) AS hits
FROM ${DATASET}
${where("server")}
GROUP BY ua, wclass, wlabel, path`;
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

// The Agents view from the two queries' rows: hits by label and class, a
// series by class over time, the top paths per label, and the strings no
// rule matches. Browser document fetches are counted apart: they are not
// agents, and the human numbers come from the tracker.
export function agentsView(byAgent, byPath, compiled) {
  const labels = new Map();
  const series = new Map();
  const unlabelled = new Map();
  let total = 0;
  let browsers = 0;
  for (const row of byAgent) {
    const a = relabel(row, compiled);
    const hits = n(row.hits);
    if (a.class === "browser") {
      browsers += hits;
      continue;
    }
    total += hits;
    const l = labels.get(key(a)) || { label: a.label, class: a.class, hits: 0, strings: new Set() };
    l.hits += hits;
    l.strings.add(row.ua);
    labels.set(key(a), l);
    const t = String(row.t);
    const s = series.get(t) || {};
    s[a.class] = (s[a.class] || 0) + hits;
    series.set(t, s);
    if (a.label === "unlabelled") unlabelled.set(row.ua, (unlabelled.get(row.ua) || 0) + hits);
  }
  const paths = new Map();
  for (const row of byPath) {
    const a = relabel(row, compiled);
    if (a.class === "browser") continue;
    const m = paths.get(key(a)) || new Map();
    m.set(row.path, (m.get(row.path) || 0) + n(row.hits));
    paths.set(key(a), m);
  }
  const byHits = (a, b) => b.hits - a.hits;
  return {
    total,
    browsers,
    labels: [...labels.values()]
      .map((l) => ({
        label: l.label,
        class: l.class,
        hits: l.hits,
        strings: l.strings.size,
        paths: [...(paths.get(key(l)) || new Map())].map(([path, hits]) => ({ path, hits })).sort(byHits).slice(0, 10),
      }))
      .sort(byHits),
    series: [...series].map(([t, byClass]) => ({ t, byClass })).sort((a, b) => (a.t < b.t ? -1 : 1)),
    unlabelled: [...unlabelled].map(([ua, hits]) => ({ ua, hits })).sort(byHits),
  };
}
