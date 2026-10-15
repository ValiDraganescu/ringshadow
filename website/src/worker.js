// The site's Worker: it stands in front of the static assets (this folder),
// serves them, and counts every page, feed, text and Markdown fetch as one
// Analytics Engine point labelled with the agent that fetched it.
// See docs/website-analytics.md.

import { SEED_RULES, compileRules } from "./agents.js";
import { Bucket, isCounted, serverPoint } from "./hits.js";

const rules = compileRules(SEED_RULES);
const bucket = new Bucket();

function count(env, request, response) {
  const asset = isCounted(request, response);
  if (!asset || !env.HITS) return;
  const now = Date.now();
  if (bucket.take(now)) env.HITS.writeDataPoint(serverPoint(request, asset, rules));
  const meta = bucket.report(now, new URL(request.url).host);
  if (meta) env.HITS.writeDataPoint(meta);
}

export default {
  async fetch(request, env) {
    const response = await env.ASSETS.fetch(request);
    try {
      count(env, request, response);
    } catch (err) {
      console.error("count failed", err);
    }
    return response;
  },
};
