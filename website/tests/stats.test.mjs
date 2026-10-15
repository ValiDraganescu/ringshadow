// node --test "website/tests/*.test.mjs"
import assert from "node:assert/strict";
import { test } from "node:test";
import { compileRules } from "../src/agents.js";
import { ruleProblem } from "../src/rules.js";
import { agentsView, parseRange, relabel, serverByAgentSql } from "../src/stats.js";
import { SEED_RULES } from "./seed.mjs";

const NOW = Date.parse("2026-10-15T12:00:00Z");
const q = (s) => new URLSearchParams(s);

test("ranges", () => {
  assert.deepEqual(parseRange(q("range=24h"), NOW), { range: "24h", from: "2026-10-14T12:00:00.000Z", to: "2026-10-15T12:00:00.000Z", bucket: "hour" });
  assert.equal(parseRange(q("range=7d"), NOW).bucket, "day");
  assert.equal(parseRange(q(""), NOW).range, "7d");
  const c = parseRange(q("range=custom&from=2026-10-01&to=2026-10-03"), NOW);
  assert.deepEqual([c.from, c.to, c.bucket], ["2026-10-01T00:00:00.000Z", "2026-10-04T00:00:00.000Z", "day"]);
  assert.equal(parseRange(q("range=custom&from=2026-01-01&to=2026-10-20"), NOW).from, "2026-07-17T12:00:00.000Z"); // 90 days back
  assert.equal(parseRange(q("range=custom&from=2026-10-01&to=2026-10-03"), NOW).to, "2026-10-04T00:00:00.000Z");
  assert.ok(parseRange(q("range=custom&from=2026-10-05&to=2026-10-03"), NOW).error);
  assert.ok(parseRange(q("range=1y"), NOW).error);
});

test("the SQL counts rows (the API applies the sample weights) inside the time bound", () => {
  const sql = serverByAgentSql("day");
  assert.match(sql, /COUNT\(\*\) AS hits, countIf\(double4 >= 400\) AS probes/);
  assert.match(serverByAgentSql("day", true), /AND double4 >= 400\n/);
  assert.doesNotMatch(sql, /AND double4 >= 400\n/);
  assert.doesNotMatch(sql, /sample/i);
  assert.match(sql, /timestamp >= \$from AND timestamp < \$to/);
  assert.match(sql, /blob1 = 'server' AND blob3 = 'ringshadow.com'/);
  assert.doesNotMatch(sql, /index1/);
  assert.match(sql, /toStartOfDay\(timestamp\)/);
  assert.match(serverByAgentSql("hour"), /toStartOfHour\(timestamp\)/);
});

const rows = [
  { ua: "GPTBot", wclass: "ai-crawler", wlabel: "OpenAI", t: "2026-10-14 00:00:00", hits: "3", probes: "0" },
  { ua: "GPTBot", wclass: "ai-crawler", wlabel: "OpenAI", t: "2026-10-15 00:00:00", hits: "2", probes: "0" },
  { ua: "NewThing-Fetcher/2.0", wclass: "unlabelled", wlabel: "unlabelled", t: "2026-10-14 00:00:00", hits: "4", probes: "3" },
  { ua: "NewThing-Fetcher/2.0", wclass: "unlabelled", wlabel: "unlabelled", t: "2026-10-15 00:00:00", hits: "1", probes: "0" },
  { ua: "Safari", wclass: "browser", wlabel: "Browsers", t: "2026-10-15 00:00:00", hits: "7", probes: "1" },
];
const paths = [
  { ua: "GPTBot", wclass: "ai-crawler", wlabel: "OpenAI", path: "/llms-full.txt", status: 200, hits: "4" },
  { ua: "GPTBot", wclass: "ai-crawler", wlabel: "OpenAI", path: "/blog/the-brood", status: 200, hits: "1" },
  { ua: "NewThing-Fetcher/2.0", wclass: "unlabelled", wlabel: "unlabelled", path: "/wp-login.php", status: 404, hits: "3" },
  { ua: "NewThing-Fetcher/2.0", wclass: "unlabelled", wlabel: "unlabelled", path: "/", status: 200, hits: "2" },
];

test("the agents view: labels, probes, paths with their status, the unlabelled strings, browsers apart", () => {
  const v = agentsView(rows, paths, compileRules(SEED_RULES));
  assert.equal(v.total, 10);
  assert.equal(v.probes, 3);
  assert.equal(v.browsers, 7);
  assert.deepEqual(v.labels.map((l) => [l.label, l.class, l.hits, l.probes]), [["OpenAI", "ai-crawler", 5, 0], ["unlabelled", "unlabelled", 5, 3]]);
  assert.deepEqual(v.labels[0].paths, [{ path: "/llms-full.txt", status: 200, hits: 4 }, { path: "/blog/the-brood", status: 200, hits: 1 }]);
  assert.deepEqual(v.labels[1].paths, [{ path: "/wp-login.php", status: 404, hits: 3 }, { path: "/", status: 200, hits: 2 }]);
  assert.deepEqual(v.unlabelled, [{ ua: "NewThing-Fetcher/2.0", hits: 5, probes: 3 }]);
  assert.deepEqual(v.series, [
    { t: "2026-10-14 00:00:00", byClass: { "ai-crawler": 3, unlabelled: 4 } },
    { t: "2026-10-15 00:00:00", byClass: { "ai-crawler": 2, unlabelled: 1 } },
  ]);
});

test("a new rule moves a string's history under its label", () => {
  const rules = compileRules([...SEED_RULES, { id: 99, label: "NewThing", class: "ai-assistant", match: "contains", pattern: "NewThing-Fetcher", priority: 100 }]);
  const v = agentsView(rows, paths, rules);
  assert.deepEqual(v.labels.map((l) => [l.label, l.class, l.hits, l.probes]), [["OpenAI", "ai-crawler", 5, 0], ["NewThing", "ai-assistant", 5, 3]]);
  assert.deepEqual(v.unlabelled, []);
  assert.deepEqual(v.series[0].byClass, { "ai-crawler": 3, "ai-assistant": 4 });
  assert.deepEqual(v.labels[1].paths.map((p) => p.path), ["/wp-login.php", "/"]);
});

test("a browser row keeps the label it was written with", () => {
  assert.deepEqual(relabel({ ua: "Safari", wclass: "browser", wlabel: "Browsers" }, compileRules([])), { label: "Browsers", class: "browser" });
});

test("what a rule needs", () => {
  const ok = { label: "Grok", class: "ai-crawler", match: "contains", pattern: "xAI", priority: 100 };
  assert.equal(ruleProblem(ok), null);
  assert.match(ruleProblem({ ...ok, label: "" }), /label/);
  assert.match(ruleProblem({ ...ok, label: "unlabelled" }), /unlabelled/);
  assert.match(ruleProblem({ ...ok, class: "robot" }), /class/);
  assert.match(ruleProblem({ ...ok, match: "glob" }), /match/);
  assert.match(ruleProblem({ ...ok, pattern: " " }), /pattern/);
  assert.match(ruleProblem({ ...ok, match: "regex", pattern: "(" }), /regex/);
  assert.match(ruleProblem({ ...ok, priority: 1.5 }), /Priority/);
});

import { classKey, logFilters, requestClassesFrom, requestLogSql, requestRow } from "../src/stats.js";

test("the request log's filters: known values only, free text as parameters", () => {
  const f = logFilters(new URLSearchParams("status=4xx&method=head&source=notfound&path=wp_%25&label=Anthropic&junk=1"));
  assert.deepEqual(f.conds, ["double4 >= $lo AND double4 < $hi", "blob16 = $method", "blob18 = $source", "blob2 LIKE $path", "blob10 = $label"]);
  assert.deepEqual(f.params, { lo: 400, hi: 500, method: "HEAD", source: "notfound", path: "%wp\\_\\%%", label: "Anthropic" });
  assert.deepEqual(logFilters(new URLSearchParams("status=9xx&method=DROP&source=x")).conds, []);
  const sql = requestLogSql(f.conds, 2);
  assert.match(sql, /blob1 IN \('server', 'request'\) AND blob3 = 'ringshadow.com'/);
  assert.match(sql, /ORDER BY timestamp DESC LIMIT 51 OFFSET 100$/);
  assert.match(requestLogSql([], "x; DROP"), /LIMIT 51 OFFSET 0$/);
});

test("answers by status class, and a log row", () => {
  assert.deepEqual([classKey(2), classKey(4.0), classKey(0)], ["2xx", "4xx", "unknown"]);
  const range = { from: "2026-10-15T08:00:00.000Z", to: "2026-10-15T10:00:00.000Z", bucket: "hour" };
  assert.deepEqual(requestClassesFrom([{ t: "2026-10-15 09:00:00", cls: 2, n: "5" }, { t: "2026-10-15 09:00:00", cls: 4, n: "1" }], range), [
    { t: "2026-10-15 08:00:00", byClass: {} },
    { t: "2026-10-15 09:00:00", byClass: { "2xx": 5, "4xx": 1 } },
  ]);
  const r = requestRow({ t: "2026-10-15 09:00:01", kind: "request", method: "HEAD", path: "/", status: 200, bytes: "0", ms: 3, source: "assets", admin: 0, si: "1" });
  assert.deepEqual([r.method, r.status, r.bytes, r.ms, r.source, r.admin, r.sample], ["HEAD", 200, 0, 3, "assets", false, 1]);
});
