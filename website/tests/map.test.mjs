// node --test "website/tests/*.test.mjs"
import assert from "node:assert/strict";
import { readFileSync, statSync } from "node:fs";
import { test } from "node:test";
import { peopleStats } from "../src/admin.js";
import { pageByColumnSql } from "../src/stats.js";

const WORLD = new URL("../admin/world.json", import.meta.url);
const world = JSON.parse(readFileSync(WORLD, "utf8"));

test("the map's geometry: alpha-2 codes, a path per shape, dots inside the map", () => {
  assert.ok(statSync(WORLD).size <= 150_000);
  assert.equal(world.width, 1000);
  const shapes = Object.entries(world.paths);
  assert.ok(shapes.length > 170);
  for (const [code, d] of shapes) {
    assert.match(code, /^[A-Z]{2}$/);
    assert.match(d, /^(M-?[\d.]+ -?[\d.]+l[-\d. ]+z)+$/, code);
  }
  for (const [code, [x, y]] of Object.entries(world.dots)) {
    assert.match(code, /^[A-Z]{2}$/);
    assert.ok(x >= 0 && x <= world.width && y >= 0 && y <= world.height, code);
  }
  // The odd Natural Earth codes: merged or recoded, none left as -99; no Antarctica.
  for (const code of ["CY", "SO", "XK", "FR", "NO", "TW"]) assert.ok(world.paths[code], code);
  assert.equal(world.paths.AQ, undefined);
  // Tiny countries get a dot.
  for (const code of ["SG", "MT", "HK", "BH", "QA", "KW", "LU"]) assert.ok(world.dots[code], code);
});

test("countries by period: the people's page views only, every country, unknown as XX", async () => {
  const calls = [];
  const countries = [{ key: "", isVisitor: 1, isBounce: 1, count: "2" }];
  for (let i = 0; i < 120; i++) countries.push({ key: "C" + String.fromCharCode(65 + (i % 26)) + i, isVisitor: 1, isBounce: 1, count: "1" });
  countries.push({ key: "DE", isVisitor: 1, isBounce: 1, count: "3" }, { key: "DE", isVisitor: 0, isBounce: -1, count: "4" });
  const env = {
    HITS_SQL: {
      async query({ query, params }) {
        calls.push({ query, params });
        return { data: query === pageByColumnSql("country") ? countries : [] };
      },
    },
  };
  const res = await peopleStats(new Request("https://ringshadow.com/api/admin/stats/visitors?range=custom&from=2026-10-01&to=2026-10-07"), env, "visitors");
  assert.equal(res.status, 200);
  const data = await res.json();
  const sql = calls.find((c) => c.query === pageByColumnSql("country"));
  assert.ok(sql);
  assert.deepEqual(sql.params, { from: "2026-10-01T00:00:00.000Z", to: "2026-10-08T00:00:00.000Z" });
  assert.match(sql.query, /SELECT blob5 AS key, double1 AS isVisitor/);
  assert.match(sql.query, /WHERE blob1 = 'page' AND blob3 = 'ringshadow.com' AND timestamp >= \$from AND timestamp < \$to/);
  assert.equal(data.countries.length, 122);
  assert.deepEqual(data.countries[0], { key: "DE", views: 7, visitors: 3, bounces: 0 });
  assert.deepEqual(data.countries.find((c) => c.key === "XX"), { key: "XX", views: 2, visitors: 2, bounces: 2 });
  assert.equal(data.countries.find((c) => c.key === ""), undefined);
});
