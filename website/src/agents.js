// Agent labels: rules that map a User-Agent string to a label and a class.
// A rule is one pattern; several rules can carry the same label, which is how
// one label groups every spelling of a crawler. The rules are tried by
// priority (lower first) and the first hit wins; no hit is "unlabelled".
// See docs/website-analytics.md, "Labels and grouping".

export const CLASSES = ["ai-crawler", "ai-assistant", "search", "monitor", "other-bot", "browser"];
export const UNLABELLED = { label: "unlabelled", class: "unlabelled" };

// The seed list. Until the rules move into D1 this table is the rules.
const seed = (label, cls, patterns, match = "contains", priority = 100) =>
  patterns.map((pattern) => ({ label, class: cls, match, pattern, priority }));

export const SEED_RULES = [
  ...seed("OpenAI", "ai-crawler", ["GPTBot", "OAI-SearchBot"]),
  ...seed("ChatGPT (for a user)", "ai-assistant", ["ChatGPT-User"]),
  ...seed("Anthropic", "ai-crawler", ["ClaudeBot", "Claude-SearchBot", "anthropic-ai"]),
  ...seed("Claude (for a user)", "ai-assistant", ["Claude-User"]),
  ...seed("Perplexity", "ai-crawler", ["PerplexityBot"]),
  ...seed("Perplexity", "ai-assistant", ["Perplexity-User"]),
  ...seed("Grok", "ai-crawler", ["xAI", "Grok"]),
  ...seed("Google", "search", ["Googlebot", "Google-Extended", "GoogleOther"]),
  ...seed("Bing", "search", ["bingbot"]),
  ...seed("Apple", "search", ["Applebot", "Applebot-Extended"]),
  ...seed("Meta", "ai-crawler", ["meta-externalagent", "FacebookBot"]),
  ...seed("Amazon", "ai-crawler", ["Amazonbot"]),
  ...seed("ByteDance", "ai-crawler", ["Bytespider"]),
  ...seed("Common Crawl", "ai-crawler", ["CCBot"]),
  ...seed("Feed readers", "other-bot", ["Feedly", "NetNewsWire", "Miniflux", "Feedbin", "Inoreader", "NewsBlur", "FreshRSS"]),
  ...seed("Monitors", "monitor", ["UptimeRobot", "Pingdom"]),
  // Browsers last: a crawler that dresses as a browser still carries its token.
  ...seed("Browsers", "browser", ["^(?!.*(bot|crawl|spider))Mozilla/5\\.0 .*(Chrome|Safari|Firefox|Edg)/"], "regex", 1000),
];

// Compile the rules once: sorted by priority, each with a test function.
// Matching ignores case. A rule whose regex doesn't compile never matches.
export function compileRules(rules) {
  return rules
    .map((rule, i) => ({ ...rule, order: i, test: tester(rule) }))
    .sort((a, b) => a.priority - b.priority || a.order - b.order);
}

function tester(rule) {
  const pattern = String(rule.pattern || "");
  if (!pattern) return () => false;
  if (rule.match === "regex") {
    try {
      const re = new RegExp(pattern, "i");
      return (ua) => re.test(ua);
    } catch {
      return () => false;
    }
  }
  const needle = pattern.toLowerCase();
  if (rule.match === "prefix") return (ua) => ua.toLowerCase().startsWith(needle);
  return (ua) => ua.toLowerCase().includes(needle);
}

// The label and class of a User-Agent under the compiled rules.
export function classify(ua, compiled) {
  const s = String(ua || "");
  if (s) {
    for (const rule of compiled) {
      if (rule.test(s)) return { label: rule.label, class: rule.class };
    }
  }
  return UNLABELLED;
}

// A browser's family from its User-Agent. Order matters: Edge and Opera say
// Chrome too, and Chrome says Safari.
export function browserFamily(ua) {
  const s = String(ua || "");
  if (/Edg(e|A|iOS)?\//.test(s)) return "Edge";
  if (/OPR\/|Opera/.test(s)) return "Opera";
  if (/Firefox\/|FxiOS\//.test(s)) return "Firefox";
  if (/Chrome\/|CriOS\//.test(s)) return "Chrome";
  if (/Safari\//.test(s)) return "Safari";
  return "Other";
}
