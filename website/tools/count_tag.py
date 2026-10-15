#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# ///
"""Put the counter's tag (assets/js/count.js) into every page that lacks it.

The tag goes into <head>, right after the page's last stylesheet link, with
a path relative to the page like the site's other links:

  <script src="../assets/js/count.js" defer data-site="ringshadow"></script>

Pages: index.html, roadmap.html, privacy.html and every page in blog/. The
404 page and the admin are left out. Run it after adding a post; it changes nothing on a page
that has the tag already.

  uv run website/tools/count_tag.py           # tag what lacks it
  uv run website/tools/count_tag.py --check   # exit 1 if a page lacks it
"""

import re
import sys
from pathlib import Path

SITE = Path(__file__).resolve().parent.parent


def pages():
    return [SITE / "index.html", SITE / "roadmap.html", SITE / "privacy.html", *sorted((SITE / "blog").glob("*.html"))]


def tag_for(page: Path) -> str:
    depth = len(page.relative_to(SITE).parts) - 1
    return f'<script src="{"../" * depth}assets/js/count.js" defer data-site="ringshadow"></script>'


def main():
    check = "--check" in sys.argv[1:]
    missing = []
    for page in pages():
        html = page.read_text()
        if "assets/js/count.js" in html:
            continue
        missing.append(page.relative_to(SITE))
        if check:
            continue
        links = list(re.finditer(r'^([ \t]*)<link rel="stylesheet"[^>]*>\n', html, re.M))
        if not links:
            sys.exit(f"{page.relative_to(SITE)}: no stylesheet link to put the tag after")
        last = links[-1]
        html = html[: last.end()] + last.group(1) + tag_for(page) + "\n" + html[last.end():]
        page.write_text(html)
    if check:
        for p in missing:
            print(f"no counter tag: {p}")
        sys.exit(1 if missing else 0)
    print(f"tagged {len(missing)} page(s)" + (": " + ", ".join(map(str, missing)) if missing else ""))


if __name__ == "__main__":
    main()
