# /// script
# requires-python = ">=3.11"
# ///
"""Write the site's files for language models (https://llmstxt.org).

    uv run website/tools/llms.py

For every page it writes a clean Markdown copy next to it (index.html ->
index.md, blog/the-brood.html -> blog/the-brood.md, served at
/blog/the-brood.md), then llms.txt (the index: what Ringshadow is and a
link to each page's Markdown) and llms-full.txt (every page in one file).
Run it after every post or page change, before deploying.
"""
import html
import re
from html.parser import HTMLParser
from pathlib import Path

SITE = "https://ringshadow.com"
ROOT = Path(__file__).resolve().parent.parent

# Pages in the order llms.txt lists them; the blog's posts follow, newest first.
PAGES = ["index.html", "roadmap.html", "blog/index.html"]


def page_url(rel: str) -> str:
    if rel == "index.html":
        return SITE + "/"
    if rel.endswith("/index.html"):
        return f"{SITE}/{rel[:-len('index.html')]}"
    return f"{SITE}/{rel[:-5]}"


def md_url(rel: str) -> str:
    return f"{SITE}/{rel[:-5]}.md"


class ToMarkdown(HTMLParser):
    """Turns the <main> of one of our pages into Markdown. It knows only the
    tags the site uses; anything inside aria-hidden or svg is skipped."""

    BLOCK = {"p", "h1", "h2", "h3", "li", "blockquote", "figcaption", "tr", "summary"}

    def __init__(self, rel: str):
        super().__init__(convert_charrefs=True)
        self.rel = rel
        self.out: list[str] = []
        self.buf = ""
        self.in_main = False
        self.stack: list[str] = []
        self.skip_at: int | None = None
        self.href: list[str | None] = []
        self.list_depth = 0
        self.cells: list[str] | None = None
        self.header_row = False
        self.rows_done = 0
        self.meta_at: int | None = None
        self.meta = ""
        self.flat: list[bool] = []

    def url(self, u: str) -> str:
        if u.startswith(("http:", "https:", "mailto:", "#")):
            return u
        base = SITE + "/" + str(Path(self.rel).parent).replace(".", "").strip("/")
        full = (base.rstrip("/") + "/" + u) if not u.startswith("/") else SITE + u
        parts: list[str] = []
        for seg in full[len(SITE) + 1:].split("/"):
            if seg == "..":
                parts and parts.pop()
            elif seg not in ("", "."):
                parts.append(seg)
        path = "/".join(parts)
        if path.endswith("index.html"):
            path = path[: -len("index.html")]
        elif path.endswith(".html"):
            path = path[:-5]
        return f"{SITE}/{path}"

    def flush(self, kind: str = "p") -> None:
        text = re.sub(r"[ \t\n]+", " ", self.buf).strip()
        self.buf = ""
        if not text:
            return
        if kind == "h1":
            self.out.append(f"# {text}")
        elif kind == "h2":
            self.out.append(f"## {text}")
        elif kind == "h3":
            self.out.append(f"### {text}")
        elif kind == "li":
            self.out.append("  " * max(self.list_depth - 1, 0) + f"- {text}")
            return
        elif kind == "blockquote":
            self.out.append(f"> {text}")
        elif kind == "figcaption":
            self.out.append(f"*{text}*")
        else:
            self.out.append(text)
        self.out.append("")

    VOID = {"img", "br", "source", "input", "meta", "link", "hr", "track", "wbr"}

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if tag == "main":
            self.in_main = True
        if not self.in_main:
            return
        if tag not in self.VOID:
            self.stack.append(tag)
        if self.skip_at is not None:
            return
        if tag == "svg" or a.get("aria-hidden") == "true" or tag == "nav" or "more" in (a.get("class") or "").split():
            if tag not in self.VOID:
                self.skip_at = len(self.stack)
            return
        cls = a.get("class") or ""
        if tag in ("ul", "ol") and "ms-list" in cls:
            # The roadmap's milestones: each item is a section, not a bullet.
            self.flat.append(True)
        elif tag in ("ul", "ol"):
            self.flat.append(False)
            if self.buf.strip():
                self.flush("li" if self.list_depth else "p")
            if self.out and self.out[-1] != "" and not self.list_depth:
                self.out.append("")
            self.list_depth += 1
        elif tag == "div" and "ms-meta" in cls:
            self.flush()
            self.meta_at = len(self.stack)
        elif tag in ("h1", "h2", "h3"):
            if self.buf.strip():
                self.flush()
        elif tag == "a" and "card" in cls.split():
            # A blog card: its blocks become plain Markdown, the link a line after them.
            self.flush()
            self.href.append(("card:" + a.get("href", ""), len(self.buf)))
        elif tag == "a":
            self.href.append((a.get("href"), len(self.buf)))
            self.buf += "["
        elif tag in ("span", "time") and self.meta_at is not None:
            self.buf += " "
        elif tag == "img":
            alt = a.get("alt", "")
            if alt:
                link = self.href[-1][0] if self.href else None
                self.buf += f"![{alt}]({self.url(link or a['src'])})"
        elif tag == "video":
            self.buf += f"[Video: {a.get('aria-label', 'video')}]({self.url(a['src'])})"
        elif tag == "audio":
            self.buf += f"[Audio]({self.url(a['src'])}) "
        elif tag == "figcaption":
            self.flush()
        elif tag == "span" and a.get("class") == "cap":
            self.buf += " "
        elif tag == "br":
            self.buf += " "
        elif tag == "kbd":
            self.buf += "`"
        elif tag == "table":
            self.rows_done = 0
        elif tag == "tr":
            self.cells = []
        elif tag in ("td", "th"):
            self.header_row = tag == "th"
            self.buf = ""

    def handle_endtag(self, tag):
        if not self.in_main:
            return
        if tag in self.VOID:
            return
        depth = len(self.stack)
        if self.stack:
            self.stack.pop()
        if self.skip_at is not None:
            if depth == self.skip_at:
                self.skip_at = None
            return
        if tag == "main":
            self.flush()
            self.in_main = False
        elif tag in ("ul", "ol"):
            if self.flat and self.flat.pop():
                return
            if self.buf.strip():
                self.flush("li")
            self.list_depth -= 1
            if not self.list_depth:
                self.out.append("")
        elif tag == "div" and self.meta_at is not None and depth == self.meta_at:
            self.meta = re.sub(r"\s+", " ", self.buf).strip()
            self.buf = ""
            self.meta_at = None
        elif tag == "b" and self.list_depth:
            self.buf += ": "
        elif tag == "h3" and self.meta:
            self.buf += " (" + " · ".join(self.meta.split()) + ")"
            self.meta = ""
            self.flush("h3")
        elif tag == "li" and self.flat and self.flat[-1] and not self.list_depth:
            self.flush()
        elif tag == "a":
            link, at = self.href.pop() if self.href else (None, len(self.buf))
            if link and link.startswith("card:"):
                self.flush()
                self.out += [f"Read it: {self.url(link[5:])}", ""]
            elif self.buf.endswith("["):
                self.buf = self.buf[:-1]
            elif self.buf[at:].startswith("[!["):
                # A linked thumbnail: the image already points at the full picture.
                self.buf = self.buf[:at] + self.buf[at + 1:]
            else:
                self.buf += f"]({self.url(link)})" if link else "]"
        elif tag == "kbd":
            self.buf += "`"
        elif tag in ("td", "th") and self.cells is not None:
            self.cells.append(re.sub(r"\s+", " ", self.buf).strip())
            self.buf = ""
        elif tag == "tr" and self.cells is not None:
            self.out.append("| " + " | ".join(self.cells) + " |")
            if self.header_row and self.rows_done == 0:
                self.out.append("|" + "---|" * len(self.cells))
            self.rows_done += 1
            self.cells = None
        elif tag == "table":
            self.out.append("")
        elif tag in self.BLOCK and self.cells is None:
            self.flush(tag)

    def handle_data(self, data):
        if self.in_main and self.skip_at is None:
            self.buf += data


def convert(rel: str) -> tuple[str, str, str]:
    src = (ROOT / rel).read_text()
    og = re.search(r'<meta property="og:title" content="([^"]*)"', src)
    title = html.unescape(og.group(1) if og else re.search(r"<title>(.*?)</title>", src).group(1))
    desc = html.unescape(re.search(r'<meta name="description" content="([^"]*)"', src).group(1))
    p = ToMarkdown(rel)
    p.feed(src)
    lines = [l for l in p.out]
    h1 = next((l for l in lines if l.startswith("# ")), None)
    if h1:
        lines.remove(h1)
        if not og or rel == "index.html":
            pass
        else:
            title = h1[2:]
    body = re.sub(r"\n{3,}", "\n\n", "\n".join(lines)).strip()
    md = f"# {title}\n\n> {desc}\n\nSource: {page_url(rel)}\n\n{body}\n"
    return title, desc, md


def main() -> None:
    posts = re.findall(r'<a class="card panel" href="([a-z0-9-]+\.html)"', (ROOT / "blog/index.html").read_text())
    pages = PAGES + [f"blog/{p}" for p in posts]
    entries = []
    full = []
    for rel in pages:
        title, desc, md = convert(rel)
        (ROOT / (rel[:-5] + ".md")).write_text(md)
        entries.append((rel, title, desc))
        full.append(md)
    lines = [
        "# Ringshadow",
        "",
        "> Ringshadow is an open-source real-time strategy game for the Mac, built in Unreal Engine 5 by one developer and AI coding agents, and dedicated to the public domain (CC0). An AI commander runs your army and you can take over any unit and play it like a shooter. Alongside it an MMORPG mode is being built: you are one Prospector in a lasting world under a ringed gas giant.",
        "",
        "Every page below has a Markdown copy at its own address with `.md` added. The source code, art and docs are at https://github.com/ValiDraganescu/ringshadow. The blog's Atom feed is https://ringshadow.com/blog/feed.xml, and everything on this site in one file is https://ringshadow.com/llms-full.txt.",
        "",
        "## Pages",
        "",
    ]
    for rel, title, desc in entries[:3]:
        lines.append(f"- [{title}]({md_url(rel)}): {desc}")
    lines += ["", "## Blog posts (newest first)", ""]
    for rel, title, desc in entries[3:]:
        lines.append(f"- [{title}]({md_url(rel)}): {desc}")
    lines += ["", "## Optional", "", "- [Everything in one file](https://ringshadow.com/llms-full.txt)", "- [The repository](https://github.com/ValiDraganescu/ringshadow)", ""]
    (ROOT / "llms.txt").write_text("\n".join(lines))
    (ROOT / "llms-full.txt").write_text("\n\n---\n\n".join(full))
    for rel, *_ in entries:
        print(rel[:-5] + ".md")
    print("llms.txt, llms-full.txt")


if __name__ == "__main__":
    main()
