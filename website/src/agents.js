// Agent labels: rules that map a User-Agent string to a label and a class.
// A rule is one pattern; several rules can carry the same label, which is how
// one label groups every spelling of a crawler. The rules are tried by
// priority (lower first) and the first hit wins; no hit is "unlabelled".
// The rules live in D1 (agent_labels; the seed list is in migration 0001)
// and src/rules.js loads them. See docs/website-analytics.md, "Labels and
// grouping".

export const CLASSES = ["ai-crawler", "ai-assistant", "search", "monitor", "other-bot", "browser"];
export const UNLABELLED = { label: "unlabelled", class: "unlabelled" };

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

// The first rule that matches a User-Agent, or null.
export function matchRule(ua, compiled) {
  const s = String(ua || "");
  if (!s) return null;
  for (const rule of compiled) {
    if (rule.test(s)) return rule;
  }
  return null;
}

// The label and class of a User-Agent under the compiled rules.
export function classify(ua, compiled) {
  const rule = matchRule(ua, compiled);
  return rule ? { label: rule.label, class: rule.class } : UNLABELLED;
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
