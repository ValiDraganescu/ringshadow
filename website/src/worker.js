// The site's Worker: it stands in front of the static assets (this folder),
// serves them, and logs every request it answers as one Analytics Engine
// point: kind=server for a page, feed, text or Markdown fetch (labelled with
// the agent that fetched it), kind=request for everything else. /c/ is the
// collector for the tracker (assets/js/count.js); /admin/ and /api/admin/
// are the admin, behind a login. Files under /assets/ and /blog/media/ never
// reach the Worker. See docs/website-analytics.md.

import { handleApi, handlePage, isSignedIn, json } from "./admin.js";
import { handleCollect } from "./collect.js";
import { Bucket, isCounted, isSelfCheck, measured, requestPoint, sourceOf } from "./hits.js";
import { dayBefore, rollupDay } from "./rollup.js";
import { loadRules } from "./rules.js";

const bucket = new Bucket();

// After the response has gone out: one point per request, through the token
// bucket; the rules come from D1, cached a minute. The admin flag marks a
// signed-in admin's requests (logged, never counted as a reader).
async function log(env, request, response, { route, ms, bytes = 0 }) {
  if (!env.HITS || isSelfCheck(request)) return;
  const asset = route === "assets" ? isCounted(request, response) : null;
  const now = Date.now();
  const passed = bucket.take(now);
  const meta = bucket.report(now, new URL(request.url).host);
  if (meta) env.HITS.writeDataPoint(meta);
  if (!passed) return;
  const [rules, admin] = await Promise.all([loadRules(env), isSignedIn(request, env)]);
  env.HITS.writeDataPoint(requestPoint(request, rules, {
    kind: asset ? "server" : "request",
    asset,
    status: response.status,
    contentType: response.headers.get("Content-Type"),
    source: sourceOf(route, response.status),
    bytes,
    ms,
    admin,
  }));
}

async function answer(request, env, path) {
  if (path.startsWith("/c/")) return ["collect", await handleCollect(request, env)];
  if (path.startsWith("/api/admin/")) {
    try {
      return ["api", await handleApi(request, env)];
    } catch (err) {
      console.error("admin api failed", err);
      return ["api", json({ error: "Something broke on the server." }, 500)];
    }
  }
  if (path === "/admin" || path.startsWith("/admin/")) return ["admin", await handlePage(request, env)];
  return ["assets", await env.ASSETS.fetch(request)];
}

export default {
  async fetch(request, env, ctx) {
    // Workers move the clock only at I/O, so this is the wait for the
    // assets, D1 or the SQL API up to the response's headers.
    const started = Date.now();
    const [route, response] = await answer(request, env, new URL(request.url).pathname);
    const ms = Date.now() - started;
    const out = await measured(request, response);
    ctx.waitUntil(log(env, request, out.response, { route, ms, bytes: out.bytes }).catch((err) => console.error("log failed", err)));
    return out.response;
  },

  // The nightly rollup (wrangler.toml, cron 02:10 UTC): yesterday into D1.
  async scheduled(controller, env, ctx) {
    const day = dayBefore(controller.scheduledTime);
    ctx.waitUntil(rollupDay(env, day).then((r) => console.log("rollup", JSON.stringify(r))));
  },
};
