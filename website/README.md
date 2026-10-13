# The Ringshadow website

The game's static website: a home page (`index.html`) and a roadmap
(`roadmap.html`). It is plain HTML, CSS and JavaScript with no build step,
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
