// The collector for the tracker (assets/js/count.js): /c/cache answers the
// visit counter through the browser's HTTP cache, /c/collect writes one
// kind=page or kind=event point and answers a 1x1 GIF. Same origin only.
// The visitor and bounce logic is Counterscale's (src/vendor/counterscale/).

import { currentSession } from "./admin.js";
import { browserFamily } from "./agents.js";
import { SITE_ID, isSelfCheck, point } from "./hits.js";
import { getBounceValue, handleCacheHeaders, hitsFromParam, maskBrowserVersion } from "./vendor/counterscale/visitors.js";

// The events the site sends (docs/website-analytics.md): a click on the
// download button, one name per build, and a video's first play.
export const EVENTS = ["download-mac", "download-windows", "download-linux", "video-play"];
// Only the live site is counted: localhost and wrangler dev are not.
export const HOSTS = ["ringshadow.com"];

const GIF = Uint8Array.from(atob("R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7"), (c) => c.charCodeAt(0));
const NO_CACHE = { Expires: "Mon, 01 Jan 1990 00:00:00 GMT", "Cache-Control": "no-cache", Pragma: "no-cache", Tk: "N" };

export function browserVersion(ua) {
  const s = String(ua || "");
  const m = s.match(/Edg(?:e|A|iOS)?\/([\d.]+)/) || s.match(/OPR\/([\d.]+)/) || s.match(/(?:Firefox|FxiOS)\/([\d.]+)/) ||
    s.match(/(?:Chrome|CriOS)\/([\d.]+)/) || s.match(/Version\/([\d.]+).*Safari\//);
  return m ? maskBrowserVersion(m[1]) : "";
}

export function deviceType(ua) {
  const s = String(ua || "");
  if (/iPad|Tablet|PlayBook|Silk|Android(?!.*Mobi)/i.test(s)) return "tablet";
  if (/Mobi|iPhone|iPod|Android|Opera Mini|IEMobile/i.test(s)) return "mobile";
  return "desktop";
}

// The referrer's host: the tracker sends a host; anything else is cut down to one.
export function cleanHost(r) {
  const s = String(r || "").trim().toLowerCase();
  if (!s) return "";
  try {
    if (/^[a-z][a-z0-9+.-]*:\/\//.test(s)) return new URL(s).host;
  } catch {
    return "";
  }
  return /^[a-z0-9.-]{1,253}(:\d{1,5})?$/.test(s) ? s : "";
}

// The point for one tracker request, or null when it isn't counted (our own
// checks, with a User-Agent starting "ringshadow-", never are).
export function collectPoint(request) {
  if (isSelfCheck(request)) return null;
  const q = new URL(request.url).searchParams;
  if (q.get("sid") !== SITE_ID || !HOSTS.includes(q.get("h"))) return null;
  const event = q.get("e") || "";
  if (event && !EVENTS.includes(event)) return null;
  const path = q.get("p") || "/";
  if (!path.startsWith("/")) return null;
  const ua = request.headers.get("User-Agent") || "";
  const hits = hitsFromParam(q.get("ht"));
  return point({
    kind: event ? "event" : "page",
    path: path.slice(0, 512),
    host: q.get("h"),
    referrer: cleanHost(q.get("r")),
    country: request.cf?.country || "",
    browser: browserFamily(ua),
    browserVersion: browserVersion(ua),
    device: deviceType(ua),
    event,
    utmSource: event ? "" : (q.get("us") || "").slice(0, 100),
    utmMedium: event ? "" : (q.get("um") || "").slice(0, 100),
    // An event is not a view: it moves neither visitors nor bounces.
    newVisitor: !event && hits === 1 ? 1 : 0,
    bounce: event ? 0 : getBounceValue(hits),
  });
}

// A signed-in admin is never counted: their own visits would fill the
// numbers they read. Readers carry no cookie, so D1 is only asked when one is
// there.
async function isAdmin(request, env) {
  if (!(request.headers.get("Cookie") || "").includes("admin_session=")) return false;
  try {
    return !!(await currentSession(request, env));
  } catch {
    return false;
  }
}

export async function handleCollect(request, env) {
  const path = new URL(request.url).pathname;
  if (request.method !== "GET") return new Response(null, { status: 405, headers: { Allow: "GET" } });
  if (path === "/c/cache") {
    const { hits, nextLastModifiedDate } = handleCacheHeaders(request.headers.get("If-Modified-Since"));
    return new Response(JSON.stringify({ ht: hits }), {
      headers: { "Content-Type": "application/json", "Last-Modified": nextLastModifiedDate.toUTCString(), ...NO_CACHE },
    });
  }
  if (path === "/c/collect") {
    const p = collectPoint(request);
    if (p && env.HITS && !(await isAdmin(request, env))) env.HITS.writeDataPoint(p);
    return new Response(GIF, { headers: { "Content-Type": "image/gif", ...NO_CACHE } });
  }
  return new Response("Not found", { status: 404 });
}
