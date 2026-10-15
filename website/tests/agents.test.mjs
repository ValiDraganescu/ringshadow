// node --test "website/tests/*.test.mjs"
import assert from "node:assert/strict";
import { test } from "node:test";
import { browserFamily, classify, compileRules } from "../src/agents.js";
import { SEED_RULES } from "./seed.mjs";

const rules = compileRules(SEED_RULES);
const label = (ua) => classify(ua, rules);

const UA = {
  gptbot: "Mozilla/5.0 AppleWebKit/537.36 (KHTML, like Gecko; compatible; GPTBot/1.2; +https://openai.com/gptbot)",
  chatgptUser: "Mozilla/5.0 AppleWebKit/537.36 (KHTML, like Gecko); compatible; ChatGPT-User/1.0; +https://openai.com/bot",
  claudebot: "Mozilla/5.0 AppleWebKit/537.36 (KHTML, like Gecko; compatible; ClaudeBot/1.0; +claudebot@anthropic.com)",
  googlebot: "Mozilla/5.0 AppleWebKit/537.36 (KHTML, like Gecko; compatible; Googlebot/2.1; +http://www.google.com/bot.html) Chrome/141.0.0.0 Safari/537.36",
  perplexityUser: "Mozilla/5.0 AppleWebKit/537.36 (KHTML, like Gecko; compatible; Perplexity-User/1.0; +https://perplexity.ai/perplexity-user)",
  chrome: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36",
  safari: "Mozilla/5.0 (iPhone; CPU iPhone OS 26_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Mobile/15E148 Safari/604.1",
  firefox: "Mozilla/5.0 (X11; Linux x86_64; rv:144.0) Gecko/20100101 Firefox/144.0",
  edge: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36 Edg/141.0.0.0",
  curl: "curl/8.7.1",
};

test("the seed list labels the crawlers it names", () => {
  assert.deepEqual(label(UA.gptbot), { label: "OpenAI", class: "ai-crawler" });
  assert.deepEqual(label("GPTBot"), { label: "OpenAI", class: "ai-crawler" });
  assert.deepEqual(label(UA.chatgptUser), { label: "ChatGPT (for a user)", class: "ai-assistant" });
  assert.deepEqual(label(UA.claudebot), { label: "Anthropic", class: "ai-crawler" });
  assert.deepEqual(label(UA.perplexityUser), { label: "Perplexity", class: "ai-assistant" });
  assert.deepEqual(label("Mozilla/5.0 (compatible; xAI-Grok/1.0)"), { label: "Grok", class: "ai-crawler" });
  assert.deepEqual(label("GrokBot/2"), { label: "Grok", class: "ai-crawler" });
  assert.deepEqual(label("NetNewsWire (RSS Reader; https://netnewswire.com/)"), { label: "Feed readers", class: "other-bot" });
});

test("a crawler dressed as a browser keeps its own label", () => {
  assert.deepEqual(label(UA.googlebot), { label: "Google", class: "search" });
});

test("browsers are browsers, other strings unlabelled", () => {
  for (const ua of [UA.chrome, UA.safari, UA.firefox, UA.edge]) {
    assert.deepEqual(label(ua), { label: "Browsers", class: "browser" }, ua);
  }
  assert.deepEqual(label(UA.curl), { label: "unlabelled", class: "unlabelled" });
  assert.deepEqual(label(""), { label: "unlabelled", class: "unlabelled" });
  assert.deepEqual(label("Mozilla/5.0 (compatible; SomeNewBot/1.0) Chrome/1 Safari/1"), { label: "unlabelled", class: "unlabelled" });
});

test("matching ignores case; priority decides; bad regexes never match", () => {
  const r = compileRules([
    { label: "B", class: "other-bot", match: "contains", pattern: "thing", priority: 200 },
    { label: "A", class: "monitor", match: "prefix", pattern: "MY", priority: 50 },
    { label: "X", class: "other-bot", match: "regex", pattern: "(", priority: 1 },
  ]);
  assert.equal(classify("my-thing/1", r).label, "A");
  assert.equal(classify("a THING", r).label, "B");
  assert.equal(classify("xx my", r).label, "unlabelled");
});

test("browser families", () => {
  assert.equal(browserFamily(UA.chrome), "Chrome");
  assert.equal(browserFamily(UA.safari), "Safari");
  assert.equal(browserFamily(UA.firefox), "Firefox");
  assert.equal(browserFamily(UA.edge), "Edge");
  assert.equal(browserFamily(UA.curl), "Other");
});
