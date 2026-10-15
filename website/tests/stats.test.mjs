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
  assert.match(sql, /COUNT\(\*\) AS hits/);
  assert.doesNotMatch(sql, /sample/i);
  assert.match(sql, /timestamp >= \$from AND timestamp < \$to/);
  assert.match(sql, /toStartOfDay\(timestamp\)/);
  assert.match(serverByAgentSql("hour"), /toStartOfHour\(timestamp\)/);
});

const rows = [
  { ua: "GPTBot", wclass: "ai-crawler", wlabel: "OpenAI", t: "2026-10-14 00:00:00", hits: "3" },
  { ua: "GPTBot", wclass: "ai-crawler", wlabel: "OpenAI", t: "2026-10-15 00:00:00", hits: "2" },
  { ua: "NewThing-Fetcher/2.0", wclass: "unlabelled", wlabel: "unlabelled", t: "2026-10-14 00:00:00", hits: "4" },
  { ua: "NewThing-Fetcher/2.0", wclass: "unlabelled", wlabel: "unlabelled", t: "2026-10-15 00:00:00", hits: "1" },
  { ua: "Safari", wclass: "browser", wlabel: "Browsers", t: "2026-10-15 00:00:00", hits: "7" },
];
const paths = [
  { ua: "GPTBot", wclass: "ai-crawler", wlabel: "OpenAI", path: "/llms-full.txt", hits: "4" },
  { ua: "GPTBot", wclass: "ai-crawler", wlabel: "OpenAI", path: "/blog/the-brood", hits: "1" },
  { ua: "NewThing-Fetcher/2.0", wclass: "unlabelled", wlabel: "unlabelled", path: "/", hits: "5" },
];

test("the agents view: labels, paths, the unlabelled strings, browsers apart", () => {
  const v = agentsView(rows, paths, compileRules(SEED_RULES));
  assert.equal(v.total, 10);
  assert.equal(v.browsers, 7);
  assert.deepEqual(v.labels.map((l) => [l.label, l.class, l.hits]), [["OpenAI", "ai-crawler", 5], ["unlabelled", "unlabelled", 5]]);
  assert.deepEqual(v.labels[0].paths, [{ path: "/llms-full.txt", hits: 4 }, { path: "/blog/the-brood", hits: 1 }]);
  assert.deepEqual(v.unlabelled, [{ ua: "NewThing-Fetcher/2.0", hits: 5 }]);
  assert.deepEqual(v.series, [
    { t: "2026-10-14 00:00:00", byClass: { "ai-crawler": 3, unlabelled: 4 } },
    { t: "2026-10-15 00:00:00", byClass: { "ai-crawler": 2, unlabelled: 1 } },
  ]);
});

test("a new rule moves a string's history under its label", () => {
  const rules = compileRules([...SEED_RULES, { id: 99, label: "NewThing", class: "ai-assistant", match: "contains", pattern: "NewThing-Fetcher", priority: 100 }]);
  const v = agentsView(rows, paths, rules);
  assert.deepEqual(v.labels.map((l) => [l.label, l.class, l.hits]), [["OpenAI", "ai-crawler", 5], ["NewThing", "ai-assistant", 5]]);
  assert.deepEqual(v.unlabelled, []);
  assert.deepEqual(v.series[0].byClass, { "ai-crawler": 3, "ai-assistant": 4 });
  assert.deepEqual(v.labels[1].paths, [{ path: "/", hits: 5 }]);
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
