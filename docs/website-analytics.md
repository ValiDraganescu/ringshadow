# Website analytics and the admin

An implementation plan for first-party, cookieless analytics on ringshadow.com,
with an admin page behind an email and password login. Written 2026-10-15 from
the research in the journalist's session (Grok, the Counterscale repo and the
Cloudflare docs); nothing here is built yet.

## What it has to do

- Count human visits: page views, visitors, referrers, countries, devices and
  browsers, over time, per page.
- Count two custom events: a click on the download button, a play of a video.
- Count every fetch of a page, the feed, `llms.txt`, `llms-full.txt` and the
  Markdown copies by **agents**: AI crawlers, AI assistants fetching for a user,
  search engines, monitors, anything that is not a browser.
- Let the admin **label agent strings** while the data comes in, and **group
  several strings under one label** ("Grok" for every xAI User-Agent, however
  they spell it this month). Labels apply to history as well as to new hits.
- An **admin page** at `/admin/`, email and password. When no admin exists, the
  page offers to register the first one. The analytics live in a section of it.
- Keep the site's promises: no cookies for readers, no third-party script, the
  Content-Security-Policy stays `'self'` only.
- Run on the Cloudflare free tier, in the one account that serves the site.

## The shape

One Worker, the site's own, which today only serves static assets. It gets a
small script in front of the assets and four bindings. No second Worker, no
service binding, no React build.

```
browser ──GET /blog/x──▶ Worker ──▶ ASSETS (the folder, as today)
                           │  writes a point: kind=server, UA, label, path, country
                           ▼
                     Analytics Engine  ◀── GET /c/collect  (the tracker, humans and events)
                           ▲
/admin/* ─── session cookie ──▶ Worker ──▶ D1: admins, sessions, agent_labels, rollups
                                        └▶ Analytics SQL binding: the queries
cron 02:10 ──▶ Worker: yesterday's rollup into D1
```

| Piece | Where | What |
|---|---|---|
| The Worker | `website/src/worker.js` | Routing: `/c/*` the collector, `/api/admin/*` the admin API, `/admin/*` gated static pages, everything else the assets, counted. Plain ES modules; wrangler bundles them, so the site still has no build step of its own. |
| Vendored Counterscale | `website/src/vendor/counterscale/` | The parts worth keeping from benvinegar/counterscale v3.4.1 (MIT): the Analytics Engine schema, the collect handler's visitor and bounce logic, the query SQL with its sampling arithmetic. Ported to plain JS, with the MIT licence text and the source commit beside it. Its React dashboard, CLI and R2 archive are left out. |
| The tracker | `website/assets/js/count.js` | About 30 lines, ours: on load, one GET to `/c/collect` with the path, the referrer, the site id and the hit counter; `count(event)` for the download button and video plays. Same origin, so the CSP needs no change. |
| The admin | `website/admin/` | Plain HTML, CSS and JS in the site's look: `index.html` (the shell), `login.html`, `register.html`, `admin.js`, `admin.css`. Served as assets, but only after the Worker has checked the session. |
| D1 schema | `website/migrations/0001_admin.sql` … | Applied with `wrangler d1 migrations apply`. |
| Tests | `website/tests/` | Node's test runner for the pure parts (the classifier, the label matcher, the SQL builders, the password hashing), and a `wrangler dev` smoke script with curl. |

`.assetsignore` grows by `src`, `migrations`, `tests` and `admin` is not
ignored: its files are assets the Worker gates.

`wrangler.toml` after the change (the current file plus the lines marked):

```toml
name = "ringshadow"
main = "src/worker.js"                       # new
compatibility_date = "2026-10-15"
workers_dev = false
preview_urls = false

[assets]
directory = "."
binding = "ASSETS"                           # new
not_found_handling = "404-page"
html_handling = "auto-trailing-slash"
run_worker_first = ["/*", "!/assets/*", "!/blog/media/*"]   # new

[[analytics_engine_datasets]]                # new
binding = "HITS"
dataset = "ringshadow_hits"

[[d1_databases]]                             # new
binding = "ADMIN"
database_name = "ringshadow-admin"
database_id = "…"
migrations_dir = "migrations"

[[analytics_sql]]                            # new: the read side, no API token needed
binding = "HITS_SQL"

[triggers]                                   # new
crons = ["10 2 * * *"]

[[routes]]
pattern = "ringshadow.com"
custom_domain = true
```

Static files under `/assets/` and `/blog/media/` skip the Worker, so pictures
and fonts cost no Worker requests and no data points. Everything else runs
through it: pages, the feed, the text files, the admin, the collector.

## The data

### Analytics Engine: one point per hit

Dataset `ringshadow_hits`, index = site id. Rows are immutable and kept three
months; counts use `SUM(_sample_interval)`, never `COUNT()`, as Counterscale does.

| Slot | Field | Set by |
|---|---|---|
| blob1 | `kind`: `page`, `event`, `server` | the collector or the Worker |
| blob2 | `path` | both |
| blob3 | `host` | both |
| blob4 | `referrer` host only (the path is dropped) | both |
| blob5 | `country` | `request.cf.country` |
| blob6 | `browser` name (parsed) | tracker hits only |
| blob7 | `browser` version, major only | tracker hits only |
| blob8 | `device`: desktop, mobile, tablet | tracker hits only |
| blob9 | `event` name (`download-mac`, `video-play`) with the page in blob2 | `kind=event` |
| blob10 | `agent_label` at write time (a cache; the query re-applies the current rules) | server hits |
| blob11 | `agent_class` at write time | server hits |
| blob12 | `user_agent`, raw, **server hits only**; for class `browser` only the family (`Chrome`) | the Worker |
| blob13 | `asset`: `html`, `feed`, `llms`, `md`, `other` | server hits |
| double1 | `new_visitor` (1 on the first hit of the day) | the collector, Counterscale's cache-header trick |
| double2 | `bounce` (1, then −1, then 0) | the collector |
| double3 | `dropped` server points since the last report | `kind=meta`, path `dropped-server-points`, the Worker's token bucket |

Why the raw User-Agent only on server hits: that is the string the admin
labels, and a bot has no privacy to keep. A human's tracker hit stores the
parsed browser family and nothing that identifies the person, which is what
the footer's promise means.

A browser's own document fetch is also a server hit (`kind=server`, labelled
`browser`). The admin's human numbers come from `kind=page`; the agent numbers
from `kind=server` with a class other than `browser`. The two never add up to
one total, and the admin says so on the page.

### D1: the admin's own tables

```sql
CREATE TABLE admins (
  id INTEGER PRIMARY KEY, email TEXT UNIQUE NOT NULL,
  pw_hash TEXT NOT NULL, pw_salt TEXT NOT NULL, pw_iters INTEGER NOT NULL,
  created_at TEXT NOT NULL, last_login_at TEXT);
CREATE TABLE sessions (
  id TEXT PRIMARY KEY, admin_id INTEGER NOT NULL REFERENCES admins(id),
  created_at TEXT NOT NULL, expires_at TEXT NOT NULL, ua TEXT);
CREATE TABLE login_attempts (
  key TEXT PRIMARY KEY,            -- email or /24 of the IP, hashed
  fails INTEGER NOT NULL, locked_until TEXT);
CREATE TABLE agent_labels (
  id INTEGER PRIMARY KEY,
  label TEXT NOT NULL,             -- "Grok", "ClaudeBot", "Googlebot"
  class TEXT NOT NULL,             -- ai-crawler, ai-assistant, search, monitor, other-bot, browser
  match TEXT NOT NULL,             -- contains | prefix | regex
  pattern TEXT NOT NULL,
  priority INTEGER NOT NULL DEFAULT 100,   -- lower wins when several match
  note TEXT, created_by INTEGER, created_at TEXT NOT NULL);
CREATE TABLE rollups_day (
  day TEXT NOT NULL, kind TEXT NOT NULL, dim TEXT NOT NULL, key TEXT NOT NULL,
  hits INTEGER NOT NULL, visitors INTEGER,
  PRIMARY KEY (day, kind, dim, key));
```

### Labels and grouping

A label is a name plus a class; a rule is one pattern that maps a User-Agent
string to a label. Several rules can carry the same label, which is how "Grok"
covers `xAI-Grok`, `GrokBot` and whatever comes next. Matching runs over the
rules by priority and takes the first hit; no rule means `unlabelled`.

The rules live in D1 and are applied in two places:

- **At write time** the Worker labels the hit into blob10/11 so the cheap
  day-by-day totals need no join. The rule table is cached in the isolate for
  60 s, so a new rule reaches new hits within a minute.
- **At query time** the admin's agent views group the last 90 days by the raw
  string (blob12), then apply the current rules in the Worker. So a label added
  today relabels the history, and the "Unlabelled" list is always what the
  current rules miss.

The seed list, shipped in the first migration and editable from the admin:

| Label | Class | Patterns |
|---|---|---|
| OpenAI | ai-crawler | `GPTBot`, `OAI-SearchBot` |
| ChatGPT (for a user) | ai-assistant | `ChatGPT-User` |
| Anthropic | ai-crawler | `ClaudeBot`, `Claude-SearchBot`, `anthropic-ai` |
| Claude (for a user) | ai-assistant | `Claude-User` |
| Perplexity | ai-crawler, ai-assistant | `PerplexityBot`, `Perplexity-User` |
| Grok | ai-crawler | `xAI`, `Grok` (to be checked against real strings once they arrive) |
| Google | search | `Googlebot`, `Google-Extended`, `GoogleOther` |
| Bing | search | `bingbot` |
| Apple | search | `Applebot`, `Applebot-Extended` |
| Meta | ai-crawler | `meta-externalagent`, `FacebookBot` |
| Amazon | ai-crawler | `Amazonbot` |
| ByteDance | ai-crawler | `Bytespider` |
| Common Crawl | ai-crawler | `CCBot` |
| Feed readers | other-bot | `Feedly`, `NetNewsWire`, `Miniflux` … |
| Monitors | monitor | `UptimeRobot`, `Pingdom` |
| Browsers | browser | a regex for Chrome, Safari, Firefox, Edge with `Mozilla/5.0` and no `bot` token |

The admin's Agents page shows, for the chosen range: hits by label and class,
by day; the paths each label fetched most (the AI crawlers' interest in
`llms-full.txt` against the posts); and the **Unlabelled** table, raw strings
with counts and a "Label as…" control that opens a new rule prefilled with the
string, where the admin picks `contains` on a chosen fragment, a class and an
existing or new label. A "Test" box runs any string through the rules.

Cloudflare's bot score and verified-bot category are Enterprise fields, so on
the free plan the User-Agent is the signal. Cloudflare's own AI Crawl Control
page for the zone stays as a second opinion, with its 24-hour window.

## The admin

### Access

- `/admin/` and everything under it is served only with a valid session; the
  Worker answers a redirect to `/admin/login` otherwise. `/api/admin/*`
  answers 401.
- **Registration.** `GET /admin/register` is available only while the admins
  table is empty, and is linked from the login page in that state ("No admin
  yet. Register the first one"). Recommended guard: a `ADMIN_SETUP_CODE`
  secret, set with `wrangler secret put` before the deploy and asked for on
  the form, so a stranger who reaches the page in the minutes before the
  developer does can't take the seat. Once one admin exists the route is
  gone; more admins are added from inside.
- **Passwords.** PBKDF2-SHA-256 through WebCrypto with a 16-byte salt, the
  iteration count stored with the hash so it can rise later. The free plan
  allows 10 ms of CPU per request, so the count is measured in step 3 and set
  to what fits with margin (Cloudflare's own example uses 100,000). At least
  12 characters, checked against the email.
- **Sessions.** A 32-byte random id, 30 days, carried in a cookie
  `admin_session`, `HttpOnly; Secure; SameSite=Strict; Path=/`. D1 keeps the
  id's SHA-256, so the table alone opens nothing. The cookie is
  only ever set for an admin, so readers still get none. Logout deletes the
  row. Every API write needs the `Origin` header to match the site.
- **Lockout.** Five failed logins on an email or a /24 lock it for 15 minutes
  (an IPv6 address counts by its /64). An unknown email costs the same hash
  as a known one, so timing doesn't tell which emails exist.
- **One-time passwords.** An admin added from inside gets a generated
  20-character password, shown once, and `admins.must_change_pw = 1`: until
  they choose their own, the API answers only `me`, `password`, `logout`
  and `signout-all`.

### Sections

| Section | What's on it |
|---|---|
| Overview | Visitors and page views for today, 7 days, 30 days; a 30-day chart; the five top pages, referrers and countries. |
| Pages | Page views and visitors per path over the range, with bounce. |
| Sources | Referrer hosts, with the UTM source and medium when present. |
| Visitors | Countries, devices, browsers. |
| Events | `download-mac`, `video-play`, by day and by page. |
| Agents | By label and class; paths per label; the Unlabelled table; the rules editor with Test. |
| History | Daily rollups older than 90 days, from D1. |
| Admins | Add an admin (email and a one-time password they must change), change own password, sign out everywhere. |

Every chart is drawn with inline SVG from JSON the API returns; no chart
library. The range picker is 24 h, 7 d, 30 d, 90 d and custom.

### The analytics API

`GET /api/admin/stats/<view>?from=…&to=…` returns JSON. The SQL is
Counterscale's, ported: visitors from `SUM(IF(double1=1,_sample_interval,0))`,
views from `SUM(_sample_interval)`, bounce from double2, grouped by the blob the
view asks for, with the time bound the SQL API requires. Reads go through the
Analytics SQL binding (Wrangler 4.145 and later; we deploy with 4.151), so no
account token is stored anywhere.

## Counting humans: the tracker

```html
<script src="../assets/js/count.js" defer data-site="ringshadow"></script>
```

On `DOMContentLoaded` it sends one `GET /c/collect?sid=ringshadow&p=/blog/x&h=ringshadow.com&r=<referrer host>`.
The collector answers a 1×1 GIF with the `Last-Modified` trick from
Counterscale for the daily visitor and bounce counts. `count.js` exports
`window.count(name)`; `home.js` calls it on the download picker's button and
`album.js` or the post page on a video's first `play`. The CSP stays as it is:
the script is `'self'`, the request is `connect-src 'self'`. Nothing is read
from or written to storage.

Localhost and `wrangler dev` are not counted (the host check), and a
`?nocount` query or `data-count="off"` on the page skips the beacon for the
admin's own visits.

## Counting agents: the Worker

For every request the Worker serves from the assets with a content type of
HTML, Atom, plain text or Markdown, it writes one `kind=server` point with the
raw User-Agent, the label and class from the cached rules, the path, the
referrer host, the country and the asset kind. The write is `writeDataPoint`,
fire and forget. Range requests, `HEAD` and 304s are not counted.

Guard rails on the free tier's 100,000 points a day: a per-isolate token bucket
of 20 points a second for `kind=server`, with a counter of dropped points
written as one `kind=meta` row a minute, so a crawler storm shows up as a
number rather than as silence.

## Keeping history: the rollup

Analytics Engine keeps three months. A cron at 02:10 UTC rolls the previous day
into `rollups_day`: totals, per path, per referrer host, per country, per
label and class, per event. A few hundred rows a day, well inside D1's free
100,000 writes. The History section reads D1; everything else reads Analytics
Engine. No R2.

## Steps

Each step ends in a deploy, since every step leaves the site working as before.

1. **The Worker in front.** `src/worker.js` passes everything to the assets.
   `.assetsignore` updated. Done when a crawl of every URL in `sitemap.xml`
   and the feed returns the same status, body and headers as the day before
   (the `_headers` CSP included), and the Cloudflare dashboard shows the
   Worker's requests.
2. **Server-side counting.** The dataset binding, the point writer, the seed
   rules as a JS table for now. Done when a `curl -A GPTBot` of a post shows
   up in the SQL within about a minute (measured 46 to 67 s: Analytics
   Engine's ingestion) labelled OpenAI, and an image fetch writes nothing.
3. **D1 and the admin's door.** The database, migration 0001, the register
   page while empty, login, sessions, lockout, logout, the gated `/admin/`
   shell, and the Admins section in it (the door's own management, so the
   developer can change the password right after registering). Done when
   the first admin is registered, the register route is gone, a wrong
   password five times locks for 15 minutes, the PBKDF2 cost is measured
   under the CPU limit, a reader's response carries no `Set-Cookie`, a
   second admin is added with a one-time password that must be changed on
   first login, an admin's own password is changed, and "sign out
   everywhere" clears every session row.
4. **The analytics section, server hits first.** The Analytics SQL binding,
   the stats API, the Agents section with labels, rules and the Unlabelled
   table, rules moved from the JS table into D1 with the migration's seed.
   Done when labelling a raw string from the Unlabelled table moves its
   history under the new label on the next load.
5. **Humans.** `count.js`, the collector with Counterscale's visitor and
   bounce logic, the two events, the tag on every page, Overview, Pages,
   Sources, Visitors and Events. Done when a visit from a fresh browser is
   one visitor and one view, the second page is a view and no bounce, a
   download click is one event, and the CSP report console is clean.
6. **History.** The cron rollup, the History section. Done when yesterday's
   totals in D1 equal the Analytics Engine query for the same day.
7. **Words and docs.** The footer: "No cookies for readers, first-party
   analytics without tracking across sites." A `/privacy` page saying what
   is stored. `website/README.md` updated: the Worker, the bindings, how to
   run `wrangler dev` with a local D1, how to add a migration. Done when the
   README's commands run as written.

Steps 1, 2 and 5 touch what readers see and get a check on the live site
after each deploy. Steps 3 and 4 need the developer once: to run the register
page, and, if the setup code is used, to set it.

## Limits that matter

| Limit | Free tier | Where we sit |
|---|---|---|
| Worker requests | 100,000 a day | pages, text files, admin and collector; assets skip it |
| Worker CPU | 10 ms a request | only the password hash comes near it |
| Analytics Engine writes | 100,000 points a day | one per page fetch plus one per human view; the token bucket caps a storm |
| Analytics Engine reads | 10,000 queries a day | the admin's views, a handful per page load |
| Retention | 3 months | the rollup keeps the rest in D1 |
| D1 | 5 M reads, 100,000 writes a day, 500 MB | sessions, rules, rollups: thousands, not millions |
| Cron triggers | free | one a day |

## What is decided and what is open

Decided by the developer on 2026-10-15: email and password login with the
first admin registering itself; labels editable live and groupable; the
vendored analytics inside the admin.

Open, with the plan's default in brackets:

- The setup code on the register page (recommended on).
- Whether the raw User-Agent of browsers is kept on server hits (plan: the
  class `browser` keeps only the family, since it isn't needed for labelling).
- Which events beyond the two (plan: only those two until asked).
- Whether to also allow or block crawlers per label from the admin (plan:
  no; Cloudflare's AI Crawl Control does that at the edge, and the admin
  links to it).

## Sources

- The research: the journalist's two Grok runs on 2026-10-15 (Counterscale,
  Cloudflare Web Analytics, Analytics Engine limits, D1 limits, the Analytics
  SQL binding, AI Crawl Control, bot fields by plan).
- benvinegar/counterscale v3.4.1: `packages/server/app/analytics/{collect,query,schema}.ts`,
  `packages/tracker/src/tracker.ts`, MIT.
- Cloudflare docs: Workers static assets (`run_worker_first`), Analytics
  Engine limits and sampling, the Analytics SQL binding, D1 pricing, Workers
  Free limits, AI Crawl Control.
