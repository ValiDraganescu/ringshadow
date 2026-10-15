// Server hits: one Analytics Engine point for every page, feed, text or
// Markdown file the Worker serves from the assets. The slot layout is the
// plan's (docs/website-analytics.md, "The data"); every writer goes through
// point() so the slots stay in one place.

import { browserFamily, classify } from "./agents.js";

export const SITE_ID = "ringshadow";
const MAX_UA = 512;
const MAX_PATH = 512;

// One point in the dataset's layout. Missing fields are empty strings or 0.
export function point(f) {
  return {
    indexes: [SITE_ID],
    blobs: [
      f.kind, f.path, f.host, f.referrer, f.country,
      f.browser, f.browserVersion, f.device, f.event,
      f.agentLabel, f.agentClass, f.userAgent, f.asset,
    ].map((v) => (v == null ? "" : String(v))),
    doubles: [f.newVisitor || 0, f.bounce || 0, f.dropped || 0],
  };
}

// What kind of file a response is, or null when it isn't counted: HTML,
// the Atom feed, plain text or Markdown.
export function assetKind(path, contentType) {
  const type = String(contentType || "").split(";")[0].trim().toLowerCase();
  if (path.endsWith("/feed.xml") || type === "application/atom+xml") return "feed";
  if (type === "text/html") return "html";
  if (type === "text/markdown") return "md";
  if (type === "text/plain") return /^\/llms(-full)?\.txt$/.test(path) ? "llms" : "other";
  return null;
}

// Whether a request and its answer make a server hit: GET only, no Range,
// a counted content type, and no 304 or redirect (nothing was served).
export function isCounted(request, response) {
  if (request.method !== "GET" || request.headers.has("Range")) return null;
  if (response.status === 304 || response.status === 206) return null;
  if (response.status >= 300 && response.status < 400) return null;
  return assetKind(new URL(request.url).pathname, response.headers.get("Content-Type"));
}

export function referrerHost(request) {
  const ref = request.headers.get("Referer");
  if (!ref) return "";
  try {
    return new URL(ref).host;
  } catch {
    return "";
  }
}

// The point for one server hit. A browser's raw User-Agent is not kept, only
// its family: the string is needed for labelling bots, not people.
export function serverPoint(request, asset, compiledRules) {
  const url = new URL(request.url);
  const ua = request.headers.get("User-Agent") || "";
  const agent = classify(ua, compiledRules);
  return point({
    kind: "server",
    path: url.pathname.slice(0, MAX_PATH),
    host: url.host,
    referrer: referrerHost(request),
    country: request.cf?.country || "",
    agentLabel: agent.label,
    agentClass: agent.class,
    userAgent: agent.class === "browser" ? browserFamily(ua) : ua.slice(0, MAX_UA),
    asset,
  });
}

// A token bucket per isolate: at most `rate` server points a second, bursts
// up to `rate`. What it drops is counted and reported once a minute as one
// kind=meta point, so a crawler storm shows as a number rather than silence.
export class Bucket {
  constructor(rate = 20, reportEveryMs = 60_000) {
    this.rate = rate;
    this.tokens = rate;
    this.last = 0;
    this.dropped = 0;
    this.reportEveryMs = reportEveryMs;
    this.lastReport = 0;
  }

  take(now) {
    if (this.last) this.tokens = Math.min(this.rate, this.tokens + ((now - this.last) / 1000) * this.rate);
    this.last = now;
    if (!this.lastReport) this.lastReport = now;
    if (this.tokens >= 1) {
      this.tokens -= 1;
      return true;
    }
    this.dropped += 1;
    return false;
  }

  // The meta point to write now, or null: once a minute, when something was dropped.
  report(now, host) {
    if (!this.dropped || now - this.lastReport < this.reportEveryMs) return null;
    const p = point({ kind: "meta", path: "dropped-server-points", host, dropped: this.dropped });
    this.dropped = 0;
    this.lastReport = now;
    return p;
  }
}
