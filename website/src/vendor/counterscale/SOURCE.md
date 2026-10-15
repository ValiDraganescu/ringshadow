# Vendored from Counterscale

Source: https://github.com/benvinegar/counterscale, tag `v3.4.1`, commit
`a04818f7d1c2adf6d3b141230e234cfdb06741e1`. MIT licence, in `LICENSE` beside
this file.

Ported to plain JavaScript, kept to the parts the site uses:

| Here | From | What |
|---|---|---|
| `visitors.js` | `packages/server/app/analytics/collect.ts` | The cookieless visitor and bounce logic: the `Last-Modified` / `If-Modified-Since` hit counter (`handleCacheHeaders`), the bounce values (`getBounceValue`), the hit count from the `ht` parameter. |
| `visitors.js` | `packages/server/app/lib/utils.ts` | `maskBrowserVersion`: only the major version is kept. |
| `counts.js` | `packages/server/app/analytics/query.ts` | Accumulating views, visitors and bounces from rows grouped by the visitor and bounce columns (`accumulateCountsFromRowResult`), and moving a negative bounce back to the bucket where the visit began. |

Changed in the port:

- The data point's layout is the site's (`src/hits.js`, `docs/website-analytics.md`), not Counterscale's column mapping.
- The SQL is the Analytics SQL API's, where `COUNT(*)` already applies the sample weights; Counterscale's `SUM(_sample_interval)` belongs to the older Analytics Engine SQL API (`src/stats.js`).
- No `ua-parser-js`: the browser family and device come from a small parser in `src/collect.js`.
- No `Access-Control-Allow-Origin: *`: the collector is same-origin only.
- Left out: the React dashboard, the CLI, the R2 archive, the tracker package (the site's tracker is `assets/js/count.js`).
