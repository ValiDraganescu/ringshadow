# The Ringshadow website

The game's website: a home page (`index.html`), a roadmap
(`roadmap.html`), a blog (`blog/`) and a privacy page (`privacy.html`). It is
plain HTML, CSS and JavaScript with no build step and no framework, so this
folder deploys as it is. A small Worker (`src/`) stands in front of the
files: it counts readers and agents, and it runs the admin (see "Analytics
and the admin").

| Path | What |
|---|---|
| `index.html` | The home page: how you play, the twelve units as a carousel of cockpits, leveling, the world, the MMORPG mode, how it's made, the download. |
| `roadmap.html` | The roadmap: every milestone a moon on the giant's ring, DONE, NEXT and LATER. |
| `assets/css/` | `site.css` (the fonts, and the roadmap's look), `ring.css` (the look the home page and the blog share: palette, type, the band shape, buttons, the bar, the footer, entrances), `home.css`, `blog.css`, `roadmap.css`. |
| `assets/js/` | `ring.js` (the `js` class and the entrances, home and blog), `stars.js` (the starfield), `home.js` (entrances, tabs, the roster carousel, the download picker; clips play only on screen), `hero.js` (the live sky, three.js in `vendor/`), `roadmap.js` (the ring). |
| `assets/fonts/` | Barlow Condensed and Exo 2 as woff2, with their SIL Open Font License texts. |
| `assets/media/` | Screenshots and clips from `docs/media/` (the GIFs re-encoded as MP4) and the key art from `art/splash/`. |
| `_headers`, `wrangler.toml`, `.assetsignore`, `404.html`, `robots.txt`, `sitemap.xml` | Hosting on Cloudflare: see "Deploy to Cloudflare". |
| `blog/` | The blog: `index.html` (newest first), one page per post, `feed.xml` (Atom), `media/<post>/` (its album). |
| `assets/css/blog.css`, `assets/js/album.js` | The blog's look, and the albums' lightbox (keys, swipes, captions). |
| `tools/album.py` | Copies pictures into an album, resized and recompressed (`uv run`). |
| `llms.txt`, `llms-full.txt`, `*.md` | For language models; written by `tools/llms.py`, never by hand. |
| `privacy.html` | What the site counts, where it is kept and for how long. |
| `assets/js/count.js`, `tools/count_tag.py` | The readers' counter, and the tool that puts its tag in every page. |
| `src/`, `admin/`, `migrations/`, `tests/` | The Worker, the admin's pages, the admin's D1 tables, the tests: see "Analytics and the admin". |

The palette is the game's HUD (`unreal/Source/Autocraft/AcHudStyle.h`):
glass `#030912`, cyan `#59d9ff`, ice `#ccf2ff`. Everything is self-hosted:
no CDN, no web fonts from elsewhere, no third-party scripts. The site
counts its own readers, without cookies (see "Analytics and the admin").

## The home page's design

One shape runs the home page: the dark band the giant's ring throws across
Ashfall Reach, a parallelogram slanted 12 degrees (`ring.css`, `.cut` and the
buttons). It masks every picture, shapes the buttons and tabs and splits the
page: the sunlit half, in the sand of the game's desert, is the strategy game
you can play today; the half under the ring, in night and stars, is the
MMORPG still being built. The leveling diagram's 216 marks are the real
upgrade count; `tools/` has no generator for it, the SVG paths live in
`index.html`.

House rules, from the taste skill the page was rebuilt with: headlines on two
lines or fewer at every width, no small labels above headings, no gradients,
glows or thin outlines, two typefaces (Barlow Condensed for display, Exo 2 for
text), five type sizes. Its checks are `lint.mjs` and `inspect.mjs` in the
`design-taste-frontend` skill; run both after a change to the home page.
The CSP allows no inline script or `style=""`, so staggered entrances are
classes (`.s1`–`.s3`) and the `js` class comes from `home.js`, loaded in
`<head>` without `defer`.

## The hero

The home page's hero is drawn live with three.js (`assets/js/hero.js`). It
draws the giant with its storm, bands that turn faster at the equator,
meteorite strikes that ripple across the clouds, rings that orbit (inner
faster) and nod slightly, Perseid showers and lone meteors, and now and then
a comet that takes about two and a half minutes to cross, far behind the
giant (the giant hides it, the rings pass in front of it). Everything is a shader, nothing is a texture. Five draw calls.

It is built to cost little. The script loads as a module after the page
paints. Frames stop while the hero is off screen or the tab is hidden, and
never run faster than 60 a second. The pixel ratio starts at the screen's (2
at most) and drops when frames run long, but never below 1: lower makes
stair-stepped edges. Ring detail finer than a pixel fades out (`fwidth`) so
it can't shimmer. Nothing allocates per frame. Under
`prefers-reduced-motion` it draws one still frame. Without WebGL it shows
`assets/media/key-art.jpg`, and without JavaScript the `<noscript>` image.

three.js r186.1 (MIT, `assets/js/vendor/three.LICENSE.txt`) is vendored as
one tree-shaken file, `assets/js/vendor/three-hero.min.js` (520 KB, about
133 KB gzipped), so the CSP stays `self` only. To rebuild it after changing
the imports, list them in `tools/three-entry.js` and run, in an empty
scratch folder with that file copied in as `entry.js`:

```sh
pnpm add three@0.186.1 esbuild@0.25.10
pnpm exec esbuild entry.js --bundle --format=esm --minify --legal-comments=eof --target=es2020 --outfile=three-hero.min.js
```

Glows (meteors, the comet, the halo) blend additively into a transparent
canvas and write their brightness to alpha as well, so they show over the
page's own starfield.

## The roadmap

The list in `roadmap.html` is the source: each `<li class="ms">` with a
`data-status` of `done`, `next` or `later` becomes a moon, in order. To
change the roadmap, edit the list; the ring follows. Its facts come from
`docs/mmorpg.md`, `docs/mmorpg-log.md` and the git history. Don't put a
date on anything that isn't done.

On a screen 901 px or wider with motion allowed, the giant stays in view
and the ring turns with the scroll. On phones and under
`prefers-reduced-motion` the page is a plain list under a still picture.

## The blog

The blog tells how the game is being made, as it happens. A journalist
agent keeps it: a post for each feature worth showing, written from
`docs/mmorpg-log.md` and the milestone docs, with an album of the workers'
own pictures. Only what is merged goes in as fact; anything else is marked
COMING. The roadmap's moons follow the same milestones.

A post is a plain HTML page in `blog/`: copy the newest one and keep its
shell (navigation, footer, scripts). The head is the title, then one line
`date · milestone`, then the lede; there is no label above the title. Pictures
sit in `<div class="cut r16">` frames, voices in `.voice`, coming work in
`.coming`, and nothing gets an outline or a panel. Then add an entry (`<li
class="reveal"><a class="card entry">`: picture, title, date line, summary) to `blog/index.html`
and an entry to `blog/feed.xml`, both newest first. Every text on the site
goes through the humanizer skill (`/humanizer`, github.com/blader/humanizer)
before it ships: no staged contrasts, one-line closers, run-ups, bold
labels or dashes, and nothing added that the log doesn't say.

A new post needs the counter's tag: run `uv run website/tools/count_tag.py`
(it tags every page that lacks it; `--check` only lists them). After any
page changes, run `uv run website/tools/llms.py`. It writes the
files for language models (llmstxt.org): a Markdown copy of every page at
its address plus `.md` (`/blog/the-brood.md`), `llms.txt` (what Ringshadow
is, and a link to each page's Markdown) and `llms-full.txt` (all of it in
one file). New posts are picked up from the blog index's entries. The feed's links are
relative, so it works at whatever address the site gets.

An album is a `<section class="album">` holding a list of links to the full
pictures, each with its thumbnail inside and a `<span class="cap">`
caption. Without JavaScript the links open the pictures; with it,
`album.js` opens a lightbox: arrows, Home and End, Esc, swipes.
`class="album wide"` shows the pictures one per row, for wide strips.

Pictures come in through the album tool, which only reads its sources
(never move or delete a worker's file):

```sh
uv run website/tools/album.py website/blog/media/<post> SRC:01-name SRC:02-name
uv run website/tools/album.py website/blog/media/<post> --cover SRC
```

It writes `NAME.webp` (at most 1600 px wide, under 300 KB) and
`NAME-t.webp` (640 px), and `cover.jpg` (1200 px) for the card and
`og:image`. Video and sound go through ffmpeg from nix: video at 960 px,
H.264 CRF 28 with `+faststart`, `preload="none"`; narration as mono AAC at
64 kbit/s.

### Media budget

The site was 4.9 MB before the blog. A post's album should stay under
about 2.5 MB of full pictures (the 14-picture Brood album is 2.3 MB); a
reader only loads thumbnails until they open one. One video per post at
most, about 4 MB. The whole site should stay under 50 MB; on 2026-10-14
it is 15 MB, 9.7 MB of it blog media (the showreel is 4 MB of that).
Cloudflare allows 25 MB a file and 20,000 files.

## Preview locally

From the repository root, either:

```sh
uv run --no-project python -m http.server 8765 -d website
pnpm dlx serve website
```

Then open http://localhost:8765/ (or the address `serve` prints).

## Deploy to Cloudflare

The site's address is **https://ringshadow.com** (on the user's Cloudflare
account). Every page carries it: `<link rel="canonical">`, `og:url` and an
absolute `og:image` (link previews on X need the full URL), and so do
`sitemap.xml`, `robots.txt` and the blog's feed. Cloudflare serves `/blog/the-brood`
for `blog/the-brood.html` and redirects the `.html` address there, so the
canonical addresses have no `.html`; links inside the site stay relative and
work either way. A new post adds its own address in the same places, and a
line in `sitemap.xml`.

The site is **live at https://ringshadow.com** since 2026-10-14. Cloudflare
runs it as a Worker that serves this folder's files ("static assets"; new
Pages projects are Workers now). Since 2026-10-15 the Worker has a script,
`src/worker.js`, which runs first for the pages and the text files; files
under `assets/` and `blog/media/` go straight to the assets. Wrangler
bundles the script on deploy, so the site still has no build step.

| File | What |
|---|---|
| `wrangler.toml` | The Worker `ringshadow`: `src/worker.js` in front of this folder, `404.html` for unknown paths, `.html` dropped from addresses, the bindings and the cron (see "Analytics and the admin"), on the custom domain ringshadow.com only (no workers.dev, no preview URLs). |
| `.assetsignore` | Leaves the README, `tools/`, `src/`, `migrations/`, `tests/` and the config out of the upload. |
| `_headers` | Security headers (CSP self only) and browser caching. |
| `404.html` | The not-found page. |
| `robots.txt`, `sitemap.xml` | For search engines. |

**Caching.** Cloudflare's edge caches every file (`cf-cache-status: HIT`)
and a deploy replaces them all at once, so there is nothing to purge. In
the browser, per `_headers`: fonts a year (immutable), pictures, video and
sound a week, CSS and JS an hour. HTML is always revalidated
(`max-age=0, must-revalidate` with an ETag), so a new post shows at once.
File names carry no hashes, so a changed picture or stylesheet gets a new
name rather than relying on the cache to expire.

**Deploy** every website change that is ready for production, right after
its commit (the user, 2026-10-14), then check the live page. Drafts stay
undeployed. The login is once, in the browser:

```sh
cd website
pnpm dlx wrangler@4.151.0 login
pnpm dlx wrangler@4.151.0 deploy --message "what changed"
```

`pnpm dlx wrangler@4.151.0 dev` serves the folder locally the way Cloudflare
does (port 8787): the 404 page, the redirects, the headers, and the Worker
with a local D1 (see below).

**The domain** (zone settings, not in this folder; set 2026-10-14 with the
`cf` CLI, run as `pnpm dlx cf`): Always Use HTTPS on (http answers 301 to
https), minimum TLS 1.2, Automatic HTTPS Rewrites on; Brotli, HTTP/3 and
TLS 1.3 were on already. `www.ringshadow.com` is a proxied placeholder
record (AAAA `100::`) and a redirect rule (phase
`http_request_dynamic_redirect`) answers it with a 301 to
`https://ringshadow.com` plus the path and query. HSTS can follow once the
site has run on HTTPS for a while.

Limits that matter: 25 MiB a file, 20,000 files (the site is 15 MB in about
155 files), 100 `_headers` rules.

## Analytics and the admin

The site counts its readers and the agents that fetch it, first-party and
without cookies for readers, and has an admin page at
https://ringshadow.com/admin/ behind an email and password. The design and
its reasons are in `docs/website-analytics.md`; what readers are told is in
`privacy.html`.

| Path | What |
|---|---|
| `src/worker.js` | Routing: `/c/` the collector, `/api/admin/` the admin's API, `/admin/` its pages (only with a session), everything else the assets. Every answer is logged as one point (the request log); the counted page and text fetches are the agents' numbers. The nightly rollup. |
| `src/hits.js`, `src/agents.js`, `src/rules.js` | A server hit's data point; the agent rules (in D1, cached a minute) that label a User-Agent. |
| `src/collect.js`, `assets/js/count.js` | The readers' counter and its collector. |
| `src/vendor/counterscale/` | Counterscale's visitor and bounce logic (MIT), with its licence and source commit. |
| `src/admin.js`, `src/auth.js` | The admin's door (register, sign in, sessions, lockout) and its API. |
| `src/stats.js`, `src/rollup.js` | The stats queries and views; the nightly rollup and History. |
| `admin/` | The admin's pages, script and stylesheet, and `world.json`, the Visitors map. |
| `tools/world_map.py` | Makes `admin/world.json` from Natural Earth (`uv run`). |
| `migrations/` | The admin's D1 tables, applied in order. |
| `tests/` | Node tests, the crawl comparison and the admin's smoke test. |

The Worker's bindings, all in `wrangler.toml`:

| Binding | What |
|---|---|
| `ASSETS` | This folder. |
| `HITS` | The Analytics Engine dataset `ringshadow_hits2`: one point per page view, event, or page and text fetch. Kept three months. |
| `HITS_SQL` | The Analytics SQL binding that reads it, with no API token. It reads the live data even under `wrangler dev`. |
| `ADMIN` | The D1 database `ringshadow-admin`: admins, sessions, lockouts, agent rules, daily rollups. |
| cron `10 2 * * *` | Rolls the previous day into D1 at 02:10 UTC. |

**Run it locally** with a local D1, from this folder:

```sh
pnpm dlx wrangler@4.151.0 d1 migrations apply ringshadow-admin --local
pnpm dlx wrangler@4.151.0 dev --var ADMIN_SETUP_CODE:local-code
```

Then open http://localhost:8787/admin/register and register with the code
`local-code`; the local D1 lives in `.wrangler/`, which git and the upload
both leave out. Locally the collector counts nothing (it counts only the
host ringshadow.com), and the stats read the live dataset.

**Add a migration**, from this folder: create it, write the SQL, apply it
locally, test, then apply it to the live database before deploying code
that needs it.

```sh
pnpm dlx wrangler@4.151.0 d1 migrations create ringshadow-admin add_something
pnpm dlx wrangler@4.151.0 d1 migrations apply ringshadow-admin --local
pnpm dlx wrangler@4.151.0 d1 migrations apply ringshadow-admin --remote
```

**Test**, from the repository root. The crawl compares two runs, before and
after a deploy; the smoke test runs the admin's door end to end against a
local `wrangler dev` with an empty local D1 (its header says how), never
against the live site.

```sh
node --test "website/tests/*.test.mjs"
uv run website/tests/crawl.py https://ringshadow.com > before.json
uv run website/tests/crawl.py https://ringshadow.com > after.json
uv run website/tests/crawl.py --diff before.json after.json
```

Our own checks send a User-Agent starting with `ringshadow-` (the crawl
does), and the Worker never counts those. To look at the raw data from the
terminal, the `cf` CLI's login can read it:

```sh
pnpm dlx cf sql query "SELECT blob1, COUNT(*) FROM events.analyticsEngine.ringshadow_hits2 WHERE timestamp > NOW() - INTERVAL '1' DAY GROUP BY blob1" --scope-account f510a16043d1521697b8974165f9c78d
```

The Visitors map's shapes come from Natural Earth (naturalearthdata.com,
v5.1.2, 1:110m admin-0 countries, and 1:50m for the dots of small places),
which is public domain. `tools/world_map.py` fetches it at that release,
checks its SHA-256 and writes `admin/world.json`; run it only to change the
release or the projection.

The first admin registered on 2026-10-15 with a one-time setup code, and the
`ADMIN_SETUP_CODE` secret was deleted after. If the admins table is ever
emptied, set it again with `pnpm dlx wrangler@4.151.0 secret put
ADMIN_SETUP_CODE` before registering. More admins are added from the
Admins section.
