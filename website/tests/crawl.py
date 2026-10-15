#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# ///
"""Crawl the site and record what each address answers, to compare two runs.

For every page in sitemap.xml, its Markdown copy, the feed, the text files, a
few redirects, a 404 and a few static files, it records the status, the
headers that matter and a SHA-256 of the body. Volatile headers (date, ray id,
cache age) are left out.

  uv run website/tests/crawl.py https://ringshadow.com > before.json
  uv run website/tests/crawl.py https://ringshadow.com > after.json
  uv run website/tests/crawl.py --diff before.json after.json
"""

import hashlib
import json
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

SITE = Path(__file__).resolve().parent.parent
SKIP = {"date", "cf-ray", "age", "server-timing", "nel", "report-to", "alt-svc",
        "cf-cache-status", "server", "connection", "transfer-encoding",
        "x-request-id", "cf-placement"}
EXTRA = ["/feed.xml", "/blog/feed.xml", "/llms.txt", "/llms-full.txt", "/robots.txt",
         "/sitemap.xml", "/index.md", "/roadmap.md", "/blog/index.md",
         "/blog/the-brood.html", "/roadmap.html", "/blog", "/no-such-page",
         "/assets/css/ring.css", "/assets/js/home.js", "/assets/media/key-art.jpg",
         "/blog/media/the-brood/cover.jpg", "/README.md", "/wrangler.toml",
         "/tools/llms.py", "/src/worker.js", "/tests/crawl.py",
         "/migrations/0001_admin.sql"]


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


def paths():
    sitemap = (SITE / "sitemap.xml").read_text()
    out = []
    for loc in re.findall(r"<loc>([^<]+)</loc>", sitemap):
        p = re.sub(r"^https?://[^/]+", "", loc) or "/"
        out.append(p)
        md = "/index.md" if p == "/" else (p + "index.md" if p.endswith("/") else p + ".md")
        out.append(md)
    return list(dict.fromkeys(out + EXTRA))


def fetch(base, path):
    opener = urllib.request.build_opener(NoRedirect)
    req = urllib.request.Request(base + path, headers={"User-Agent": "ringshadow-crawl-check"})
    try:
        resp = opener.open(req, timeout=30)
        status, headers, body = resp.status, resp.headers, resp.read()
    except urllib.error.HTTPError as e:
        status, headers, body = e.code, e.headers, e.read()
    kept = {k.lower(): v for k, v in headers.items() if k.lower() not in SKIP}
    return {"status": status, "headers": dict(sorted(kept.items())),
            "sha256": hashlib.sha256(body).hexdigest(), "bytes": len(body)}


def diff(a_path, b_path):
    a, b = json.loads(Path(a_path).read_text()), json.loads(Path(b_path).read_text())
    bad = 0
    for path in sorted(set(a) | set(b)):
        if a.get(path) != b.get(path):
            bad += 1
            print(f"DIFF {path}")
            ra, rb = a.get(path) or {}, b.get(path) or {}
            for key in ("status", "sha256", "bytes"):
                if ra.get(key) != rb.get(key):
                    print(f"  {key}: {ra.get(key)} -> {rb.get(key)}")
            ha, hb = ra.get("headers", {}), rb.get("headers", {})
            for h in sorted(set(ha) | set(hb)):
                if ha.get(h) != hb.get(h):
                    print(f"  {h}: {ha.get(h)!r} -> {hb.get(h)!r}")
    print(f"{len(set(a) | set(b))} addresses, {bad} differ")
    return 1 if bad else 0


def main():
    if sys.argv[1:2] == ["--diff"]:
        sys.exit(diff(sys.argv[2], sys.argv[3]))
    base = sys.argv[1].rstrip("/")
    print(json.dumps({p: fetch(base, p) for p in paths()}, indent=1))


if __name__ == "__main__":
    main()
