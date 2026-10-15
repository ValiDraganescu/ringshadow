// node --test "website/tests/*.test.mjs"
import assert from "node:assert/strict";
import { test } from "node:test";
import { browserVersion, cleanHost, collectPoint, deviceType, handleCollect } from "../src/collect.js";
import { accumulateCountsFromRowResult, fixNegativeBounces } from "../src/vendor/counterscale/counts.js";
import { handleCacheHeaders } from "../src/vendor/counterscale/visitors.js";
import { byKeyFrom, eventsFrom, seriesFrom } from "../src/stats.js";

const CHROME = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.7390.54 Safari/537.36";
const IPHONE = "Mozilla/5.0 (iPhone; CPU iPhone OS 26_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Mobile/15E148 Safari/604.1";
const IPAD = "Mozilla/5.0 (iPad; CPU OS 26_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Mobile/15E148 Safari/604.1";

function req(query, headers = {}) {
  const r = new Request("https://ringshadow.com/c/collect?" + query, { headers: { "User-Agent": CHROME, ...headers } });
  Object.defineProperty(r, "cf", { value: { country: "DE" } });
  return r;
}

test("a first page view: one visitor, a bounce", () => {
  const p = collectPoint(req("sid=ringshadow&h=ringshadow.com&p=/blog/the-brood&r=news.example.org&ht=1&us=x&um=social"));
  assert.deepEqual(p.indexes, ["ringshadow"]);
  assert.deepEqual(p.blobs, ["page", "/blog/the-brood", "ringshadow.com", "news.example.org", "DE", "Chrome", "141.x.x.x", "desktop", "", "", "", "", "", "x", "social"]);
  assert.deepEqual(p.doubles, [1, 1, 0, 0]);
});

test("the second view takes the bounce back, the third is plain", () => {
  assert.deepEqual(collectPoint(req("sid=ringshadow&h=ringshadow.com&p=/&ht=2")).doubles, [0, -1, 0, 0]);
  assert.deepEqual(collectPoint(req("sid=ringshadow&h=ringshadow.com&p=/&ht=3")).doubles, [0, 0, 0, 0]);
  assert.deepEqual(collectPoint(req("sid=ringshadow&h=ringshadow.com&p=/&ht=9")).doubles, [0, 0, 0, 0]);
  assert.deepEqual(collectPoint(req("sid=ringshadow&h=ringshadow.com&p=/")).doubles, [1, 1, 0, 0]); // no counter: a first hit
});

test("an event is neither a view nor a visitor", () => {
  const p = collectPoint(req("sid=ringshadow&h=ringshadow.com&p=/&e=download-windows&us=x"));
  assert.equal(p.blobs[0], "event");
  assert.equal(p.blobs[8], "download-windows");
  assert.equal(p.blobs[13], "");
  assert.deepEqual(p.doubles, [0, 0, 0, 0]);
  assert.equal(collectPoint(req("sid=ringshadow&h=ringshadow.com&p=/&e=something-else")), null);
});

test("only the live site, the site id and a path are counted", () => {
  assert.equal(collectPoint(req("sid=ringshadow&h=localhost:8787&p=/")), null);
  assert.equal(collectPoint(req("sid=other&h=ringshadow.com&p=/")), null);
  assert.equal(collectPoint(req("sid=ringshadow&h=ringshadow.com&p=https://x.org/")), null);
  assert.equal(collectPoint(req("sid=ringshadow&h=ringshadow.com&p=/", { "User-Agent": "ringshadow-collect-check" })), null);
});

test("devices, browser versions, referrer hosts", () => {
  assert.equal(deviceType(CHROME), "desktop");
  assert.equal(deviceType(IPHONE), "mobile");
  assert.equal(deviceType(IPAD), "tablet");
  assert.equal(browserVersion(IPHONE), "26.x");
  assert.equal(browserVersion("curl/8"), "");
  assert.equal(cleanHost("https://news.example.org/a?b"), "news.example.org");
  assert.equal(cleanHost("News.Example.org"), "news.example.org");
  assert.equal(cleanHost("<script>"), "");
});

test("the visit counter through the HTTP cache (Counterscale's)", () => {
  const first = handleCacheHeaders(null);
  assert.equal(first.hits, 1);
  const second = handleCacheHeaders(first.nextLastModifiedDate.toUTCString());
  assert.equal(second.hits, 2);
  const third = handleCacheHeaders(second.nextLastModifiedDate.toUTCString());
  assert.equal(third.hits, 3);
  assert.equal(handleCacheHeaders(third.nextLastModifiedDate.toUTCString()).hits, 3);
  assert.equal(handleCacheHeaders("Mon, 01 Jan 2024 00:00:01 GMT").hits, 1); // another day: a new visitor
});

test("the collector's answers", async () => {
  const writes = [];
  const env = { HITS: { writeDataPoint: (p) => writes.push(p) } };
  const cache = await handleCollect(new Request("https://ringshadow.com/c/cache?sid=ringshadow"), env);
  assert.deepEqual(await cache.json(), { ht: 1 });
  assert.ok(cache.headers.get("Last-Modified"));
  assert.equal(cache.headers.get("Cache-Control"), "no-cache");
  assert.equal(cache.headers.get("Access-Control-Allow-Origin"), null);
  assert.equal(cache.headers.get("Set-Cookie"), null);
  const gif = await handleCollect(req("sid=ringshadow&h=ringshadow.com&p=/&ht=1"), env);
  assert.equal(gif.headers.get("Content-Type"), "image/gif");
  assert.equal((await gif.arrayBuffer()).byteLength, 42);
  assert.equal(writes.length, 1);
  await handleCollect(req("sid=ringshadow&h=localhost&p=/&ht=1"), env);
  assert.equal(writes.length, 1);
  assert.equal((await handleCollect(new Request("https://ringshadow.com/c/collect", { method: "POST" }), env)).status, 405);
});

test("a signed-in admin is not counted; a reader never touches D1", async () => {
  const writes = [];
  let asked = 0;
  const d1 = (session) => ({
    prepare: () => ({ bind: () => ({ first: async () => { asked++; return session; } }) }),
  });
  const hits = { writeDataPoint: (p) => writes.push(p) };
  const q = "sid=ringshadow&h=ringshadow.com&p=/&ht=3";
  await handleCollect(req(q, { Cookie: "admin_session=abc" }), { HITS: hits, ADMIN: d1({ id: "x", admin_id: 1 }) });
  assert.deepEqual([writes.length, asked], [0, 1]);
  await handleCollect(req(q, { Cookie: "admin_session=expired" }), { HITS: hits, ADMIN: d1(null) });
  assert.deepEqual([writes.length, asked], [1, 2]);
  await handleCollect(req(q), { HITS: hits, ADMIN: d1({ id: "x" }) });
  assert.deepEqual([writes.length, asked], [2, 2]);
});

test("Counterscale's counting: views, visitors, bounces, and a bounce moved back", () => {
  const c = { views: 0, visitors: 0, bounces: 0 };
  accumulateCountsFromRowResult(c, { count: 5, isVisitor: 1, isBounce: 1 });
  accumulateCountsFromRowResult(c, { count: 3, isVisitor: 0, isBounce: -1 });
  accumulateCountsFromRowResult(c, { count: 4, isVisitor: 0, isBounce: 0 });
  assert.deepEqual(c, { views: 12, visitors: 5, bounces: 2 });
  const rows = fixNegativeBounces([["a", { bounces: 2 }], ["b", { bounces: 0 }], ["c", { bounces: -1 }]]);
  assert.deepEqual(rows.map((r) => r[1].bounces), [1, 0, 0]);
});

const range = { from: "2026-10-14T00:00:00.000Z", to: "2026-10-16T00:00:00.000Z", bucket: "day" };

test("a fresh visit, then a second page: two views, one visitor, no bounce", () => {
  const rows = [
    { t: "2026-10-15 00:00:00", isVisitor: 1, isBounce: 1, count: "1" },
    { t: "2026-10-15 00:00:00", isVisitor: 0, isBounce: -1, count: "1" },
  ];
  assert.deepEqual(seriesFrom(rows, range), [
    { t: "2026-10-14 00:00:00", views: 0, visitors: 0, bounces: 0 },
    { t: "2026-10-15 00:00:00", views: 2, visitors: 1, bounces: 0 },
  ]);
  const byPath = byKeyFrom([
    { key: "/", isVisitor: 1, isBounce: 1, count: "1" },
    { key: "/blog/", isVisitor: 0, isBounce: -1, count: "1" },
  ]);
  assert.deepEqual(byPath, [
    { key: "/", views: 1, visitors: 1, bounces: 1 },
    { key: "/blog/", views: 1, visitors: 0, bounces: 0 },
  ]);
});

test("events by name, bucket and page", () => {
  const ev = eventsFrom([
    { event: "download-mac", path: "/", t: "2026-10-15 00:00:00", count: "2" },
    { event: "download-linux", path: "/", t: "2026-10-14 00:00:00", count: "1" },
    { event: "download-mac", path: "/", t: "2026-10-14 00:00:00", count: "1" },
  ], range);
  assert.deepEqual(ev.map((e) => [e.event, e.total]), [["download-mac", 3], ["download-linux", 1]]);
  assert.deepEqual(ev[0].series, [{ t: "2026-10-14 00:00:00", count: 1 }, { t: "2026-10-15 00:00:00", count: 2 }]);
  assert.deepEqual(ev[0].pages, [{ path: "/", count: 3 }]);
});
