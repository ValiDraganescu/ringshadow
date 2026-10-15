// The site's Worker: it stands in front of the static assets (this folder),
// serves them, and counts every page, feed, text and Markdown fetch as one
// Analytics Engine point labelled with the agent that fetched it. /admin/
// and /api/admin/ are the admin, behind a login.
// See docs/website-analytics.md.

import { handleApi, handlePage, json } from "./admin.js";
import { Bucket, isCounted, serverPoint } from "./hits.js";
import { loadRules } from "./rules.js";

const bucket = new Bucket();

// After the response has gone out: the rules come from D1, cached a minute.
async function count(env, request, response) {
  const asset = isCounted(request, response);
  if (!asset || !env.HITS) return;
  const now = Date.now();
  const passed = bucket.take(now);
  const meta = bucket.report(now, new URL(request.url).host);
  if (meta) env.HITS.writeDataPoint(meta);
  if (passed) env.HITS.writeDataPoint(serverPoint(request, asset, await loadRules(env)));
}

export default {
  async fetch(request, env, ctx) {
    const path = new URL(request.url).pathname;
    if (path.startsWith("/api/admin/")) {
      try {
        return await handleApi(request, env);
      } catch (err) {
        console.error("admin api failed", err);
        return json({ error: "Something broke on the server." }, 500);
      }
    }
    if (path === "/admin" || path.startsWith("/admin/")) return handlePage(request, env);

    const response = await env.ASSETS.fetch(request);
    ctx.waitUntil(count(env, request, response).catch((err) => console.error("count failed", err)));
    return response;
  },
};
