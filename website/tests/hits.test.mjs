// node --test "website/tests/*.test.mjs"
import assert from "node:assert/strict";
import { test } from "node:test";
import { compileRules } from "../src/agents.js";
import { SEED_RULES } from "./seed.mjs";
import { Bucket, SELF_CHECK_PREFIX, assetKind, indexOf, isCounted, point, serverPoint } from "../src/hits.js";

const rules = compileRules(SEED_RULES);

function req(path, { method = "GET", headers = {}, country = "RO" } = {}) {
  const r = new Request("https://ringshadow.com" + path, { method, headers });
  Object.defineProperty(r, "cf", { value: { country } });
  return r;
}
const res = (status, type) => new Response(status === 304 ? null : "x", { status, headers: type ? { "Content-Type": type } : {} });

test("asset kinds", () => {
  assert.equal(assetKind("/blog/the-brood", "text/html; charset=utf-8"), "html");
  assert.equal(assetKind("/blog/feed.xml", "application/xml"), "feed");
  assert.equal(assetKind("/llms.txt", "text/plain; charset=utf-8"), "llms");
  assert.equal(assetKind("/llms-full.txt", "text/plain; charset=utf-8"), "llms");
  assert.equal(assetKind("/robots.txt", "text/plain"), "other");
  assert.equal(assetKind("/blog/the-brood.md", "text/markdown; charset=utf-8"), "md");
  assert.equal(assetKind("/sitemap.xml", "application/xml"), null);
  assert.equal(assetKind("/favicon.png", "image/png"), null);
});

test("what is counted", () => {
  assert.equal(isCounted(req("/blog/x"), res(200, "text/html")), "html");
  assert.equal(isCounted(req("/no-such"), res(404, "text/html")), "html");
  assert.equal(isCounted(req("/blog/x", { method: "HEAD" }), res(200, "text/html")), null);
  assert.equal(isCounted(req("/blog/x", { headers: { Range: "bytes=0-10" } }), res(200, "text/html")), null);
  assert.equal(isCounted(req("/blog/x"), res(304)), null);
  assert.equal(isCounted(req("/blog/x.html"), res(307)), null);
  assert.equal(isCounted(req("/x.jpg"), res(200, "image/jpeg")), null);
});

test("our own checks are never written; a marker inside a real agent's string is", () => {
  assert.equal(SELF_CHECK_PREFIX, "ringshadow-");
  assert.equal(isCounted(req("/blog/x", { headers: { "User-Agent": "ringshadow-crawl-check" } }), res(200, "text/html")), null);
  assert.equal(isCounted(req("/blog/x", { headers: { "User-Agent": "Ringshadow-other" } }), res(200, "text/html")), "html");
  assert.equal(isCounted(req("/blog/x", { headers: { "User-Agent": "Mozilla/5.0 (compatible; ClaudeBot/1.0; ringshadow-fold-check)" } }), res(200, "text/html")), "html");
});

test("a server point carries the plan's slots", () => {
  const p = serverPoint(req("/blog/the-brood", { headers: { "User-Agent": "GPTBot/1.2", Referer: "https://example.org/a/b?c" } }), "html", rules, 200);
  assert.deepEqual(p.indexes, ["GPTBot/1.2"]);
  assert.deepEqual(p.blobs, ["server", "/blog/the-brood", "ringshadow.com", "example.org", "RO", "", "", "", "", "OpenAI", "ai-crawler", "GPTBot/1.2", "html", "", ""]);
  assert.deepEqual(p.doubles, [0, 0, 0, 200]);
  assert.deepEqual(serverPoint(req("/wp-login.php"), "html", rules, 404).doubles, [0, 0, 0, 404]);
});

test("a browser's server point keeps only its family", () => {
  const ua = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36";
  const p = serverPoint(req("/", { headers: { "User-Agent": ua } }), "html", rules);
  assert.equal(p.blobs[9], "Browsers");
  assert.equal(p.blobs[10], "browser");
  assert.equal(p.blobs[11], "Chrome");
  assert.deepEqual(p.indexes, ["Chrome"]);
});

test("the index: the User-Agent cut to 96 bytes at a character boundary, the site id otherwise", () => {
  assert.equal(indexOf("x".repeat(200)), "x".repeat(96));
  const long = "a".repeat(95) + "é" + "b"; // é is 2 bytes and would straddle byte 96
  assert.equal(indexOf(long), "a".repeat(95));
  assert.equal(new TextEncoder().encode(indexOf("€".repeat(40))).length, 96);
  assert.equal(indexOf(""), "-");
  assert.deepEqual(point({ kind: "meta" }).indexes, ["ringshadow"]);
  const none = serverPoint(req("/"), "html", rules);
  assert.deepEqual(none.indexes, ["-"]);
});

test("the bucket passes 20 a second and reports what it drops once a minute", () => {
  const b = new Bucket(20, 60_000);
  let t = 1_000_000, passed = 0;
  for (let i = 0; i < 30; i++) passed += b.take(t) ? 1 : 0;
  assert.equal(passed, 20);
  assert.equal(b.report(t + 1000, "ringshadow.com"), null);
  t += 500; // half a second refills 10
  for (let i = 0; i < 15; i++) passed += b.take(t) ? 1 : 0;
  assert.equal(passed, 30);
  const meta = b.report(t + 60_000, "ringshadow.com");
  assert.deepEqual(meta, point({ kind: "meta", path: "dropped-server-points", host: "ringshadow.com", dropped: 15 }));
  assert.equal(b.report(t + 60_001, "ringshadow.com"), null);
});
