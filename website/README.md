# The Ringshadow website

The game's static website: a home page (`index.html`), a roadmap
(`roadmap.html`) and a blog (`blog/`). It is plain HTML, CSS and JavaScript with no build step,
no framework and no server side, so this folder deploys as it is.

| Path | What |
|---|---|
| `index.html` | The home page: the pitch, how you play, the units, the MMORPG mode, how it's made, CC0. |
| `roadmap.html` | The roadmap: every milestone a moon on the giant's ring, DONE, NEXT and LATER. |
| `assets/css/` | `site.css` (shared: palette, fonts, navigation), `home.css`, `roadmap.css`. |
| `assets/js/` | `stars.js` (the starfield), `home.js` (clips play only on screen), `roadmap.js` (the ring). |
| `assets/fonts/` | Barlow Condensed and Exo 2 as woff2, with their SIL Open Font License texts. |
| `assets/media/` | Screenshots and clips from `docs/media/` (the GIFs re-encoded as MP4) and the key art from `art/splash/`. |
| `_headers` | Caching and security headers for Cloudflare Pages. |
| `blog/` | The blog: `index.html` (newest first), one page per post, `feed.xml` (Atom), `media/<post>/` (its album). |
| `assets/css/blog.css`, `assets/js/album.js` | The blog's look, and the albums' lightbox (keys, swipes, captions). |
| `tools/album.py` | Copies pictures into an album, resized and recompressed (`uv run`). |

The palette is the game's HUD (`unreal/Source/Autocraft/AcHudStyle.h`):
glass `#030912`, cyan `#59d9ff`, ice `#ccf2ff`. Everything is self-hosted:
no CDN, no web fonts from elsewhere, no trackers, no analytics.

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
shell (navigation, footer, scripts). Then add a card to `blog/index.html`
and an entry to `blog/feed.xml`, both newest first. The feed's links are
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
Cloudflare Pages allows 25 MB a file and 20,000 files.

## Preview locally

From the repository root, either:

```sh
uv run --no-project python -m http.server 8765 -d website
pnpm dlx serve website
```

Then open http://localhost:8765/ (or the address `serve` prints).

## Deploy to Cloudflare Pages (later)

Create a Pages project from the GitHub repo with:

- Build command: none
- Build output directory: `website`

Or upload the folder directly: `pnpm dlx wrangler pages deploy website`.
`_headers` is read by Pages on deploy.
