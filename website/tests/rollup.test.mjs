// node --test "website/tests/*.test.mjs"
import assert from "node:assert/strict";
import { test } from "node:test";
import { compileRules } from "../src/agents.js";
import { dayBefore, rollupRows } from "../src/rollup.js";
import { SEED_RULES } from "./seed.mjs";

test("the cron at 02:10 rolls up the day before", () => {
  assert.equal(dayBefore(Date.parse("2026-10-16T02:10:00Z")), "2026-10-15");
  assert.equal(dayBefore(Date.parse("2026-01-01T02:10:00Z")), "2025-12-31");
});

test("a day's rows: page totals and keys, agents by label and class, events", () => {
  const rows = rollupRows("2026-10-15", {
    pageTotals: [{ isVisitor: 1, isBounce: 1, count: "3" }, { isVisitor: 0, isBounce: -1, count: "2" }],
    paths: [{ key: "/", isVisitor: 1, isBounce: 1, count: "3" }, { key: "/blog/", isVisitor: 0, isBounce: -1, count: "2" }],
    referrers: [{ key: "", isVisitor: 1, isBounce: 1, count: "3" }, { key: "news.example.org", isVisitor: 0, isBounce: 0, count: "2" }],
    countries: [{ key: "DE", isVisitor: 1, isBounce: 1, count: "3" }, { key: "DE", isVisitor: 0, isBounce: -1, count: "2" }],
    server: [
      { ua: "GPTBot", wclass: "ai-crawler", wlabel: "OpenAI", hits: "4", probes: "0" },
      { ua: "OAI-SearchBot/1.0", wclass: "ai-crawler", wlabel: "OpenAI", hits: "1", probes: "0" },
      { ua: "Safari", wclass: "browser", wlabel: "Browsers", hits: "6", probes: "0" },
      { ua: "odd/1", wclass: "unlabelled", wlabel: "unlabelled", hits: "2", probes: "2" },
    ],
    events: [{ event: "download-mac", path: "/", t: "x", count: "2" }, { event: "download-mac", path: "/blog/", t: "x", count: "1" }],
  }, compileRules(SEED_RULES));
  const get = (kind, dim, key) => rows.find((r) => r.kind === kind && r.dim === dim && r.key === key);
  assert.deepEqual(get("page", "total", ""), { day: "2026-10-15", kind: "page", dim: "total", key: "", hits: 5, visitors: 3 });
  assert.deepEqual([get("page", "path", "/").hits, get("page", "path", "/blog/").hits], [3, 2]);
  assert.equal(get("page", "referrer", "news.example.org").hits, 2);
  assert.deepEqual([get("page", "country", "DE").hits, get("page", "country", "DE").visitors], [5, 3]);
  assert.equal(get("server", "total", "").hits, 13);
  assert.equal(get("server", "label", "OpenAI").hits, 5);
  assert.equal(get("server", "label", "unlabelled").hits, 2);
  assert.equal(get("server", "class", "browser").hits, 6);
  assert.equal(get("server", "class", "ai-crawler").hits, 5);
  assert.equal(get("event", "event", "download-mac").hits, 3);
  assert.equal(get("server", "total", "").visitors, null);
});
