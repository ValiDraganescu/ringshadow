// Server hits: one Analytics Engine point for every page, feed, text or
// Markdown file the Worker serves from the assets. The slot layout is the
// plan's (docs/website-analytics.md, "The data"); every writer goes through
// point() so the slots stay in one place.

import { browserFamily, classify } from "./agents.js";

export const SITE_ID = "ringshadow";
export const SITE_HOST = "ringshadow.com";
const MAX_UA = 512;
const MAX_PATH = 512;
const MAX_INDEX_BYTES = 96;

// Our own checks (the crawl, tails, curls) send a User-Agent starting with
// this and are never written.
export const SELF_CHECK_PREFIX = "ringshadow-";
export const isSelfCheck = (request) => (request.headers.get("User-Agent") || "").startsWith(SELF_CHECK_PREFIX);

// Analytics Engine samples by index when one index writes fast. Server hits
// are indexed by their User-Agent, so a crawler storm thins only the
// stormer's own rows; people's hits and meta rows by the site id.
export function indexOf(s) {
  const bytes = new TextEncoder().encode(String(s || "") || "-");
  if (bytes.length <= MAX_INDEX_BYTES) return String(s || "") || "-";
  // cut at a character boundary: drop UTF-8 continuation bytes at the end
  let end = MAX_INDEX_BYTES;
  while (end > 0 && (bytes[end] & 0xc0) === 0x80) end--;
  return new TextDecoder().decode(bytes.slice(0, end));
}

// One point in the dataset's layout. Missing fields are empty strings or 0.
export function point(f) {
  return {
    indexes: [f.index || SITE_ID],
    blobs: [
      f.kind, f.path, f.host, f.referrer, f.country,
      f.browser, f.browserVersion, f.device, f.event,
      f.agentLabel, f.agentClass, f.userAgent, f.asset,
    ].map((v) => (v == null ? "" : String(v))),
    doubles: [f.newVisitor || 0, f.bounce || 0, f.dropped || 0, f.status || 0],
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
// a counted content type, no 304 or redirect (nothing was served), and not
// one of our own checks. A 404 is counted: probes for holes are worth seeing.
export function isCounted(request, response) {
  if (request.method !== "GET" || request.headers.has("Range") || isSelfCheck(request)) return null;
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
export function serverPoint(request, asset, compiledRules, status = 0) {
  const url = new URL(request.url);
  const ua = request.headers.get("User-Agent") || "";
  const agent = classify(ua, compiledRules);
  const kept = agent.class === "browser" ? browserFamily(ua) : ua.slice(0, MAX_UA);
  return point({
    index: indexOf(kept),
    kind: "server",
    path: url.pathname.slice(0, MAX_PATH),
    host: url.host,
    referrer: referrerHost(request),
    country: request.cf?.country || "",
    agentLabel: agent.label,
    agentClass: agent.class,
    userAgent: kept,
    asset,
    status,
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
