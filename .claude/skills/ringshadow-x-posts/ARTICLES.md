# X Articles

An X Article is a long-form page on X: a title, a 5:2 header image, headings, lists, code blocks, tables and images. A post is the share that links to it. The first one was `docs/articles/ringshadow-release-builds.md` (2026-10-08). Read it for the shape.

Claude writes the markdown and makes the images, then saves the article as an Orchestrator article draft with one tool call. Orchestrator turns the markdown into the x.com editor's own content and copies the images into its drafts library. The developer opens x.com's article editor, picks the draft from the amber "Orchestrator drafts" button, and the browser extension loads the title, the body (headings, bold, code blocks, tables), the header image and the inline images through the editor itself, so X's own autosave stores it. The developer reviews it and presses Publish. Claude never drives x.com in the browser.

| Path | What |
|---|---|
| `docs/articles/<slug>.md` | The article, the source of truth |
| `docs/articles/<slug>/` | Its images: `thumb.jpg` (2000×800) and the inline ones |
| `scripts/article-art/` | The image kit: `shot.sh` (page to PNG) and `base.css` (HUD palette and fonts) |
| `scripts/article-art/examples/` | The first article's four pages, one per layout. Copy one and replace its content |

## 1. Write the markdown

Same voice and banned list as a post (SKILL.md). Then, for an article:

- **The facts are measured.** Every number comes from a log, a bill or a command's output, and the same number reads the same everywhere it appears. Give costs per month at real usage (for example 2 to 3 releases a week), with the per-run cost beside them. List the sources in an HTML comment at the end: everything from the first `<!--` on stays out of the article.
- **Reproducible.** Every step has the command that does it and a "Done when …" line. Code blocks hold real commands and real file contents. A reader who wants to do the same can follow along.
- **The file:** the H1 title (specific, with the payoff: "One Mac, One Rented Windows Box: …"), an optional `>` note to yourself (header image, inline images; Orchestrator leaves the `>` lines right under the title out of the body), then the body. A hook line, then 3 to 4 bold numbers, then the sections. A subheading every 3 to 5 paragraphs, the key line of each section in bold, short paragraphs. End with what's still open and a question to reply to.
- **The markdown Orchestrator reads:** call `x_article_format` for the exact dialect, what gets flattened and what gets refused. In short: `## ` headings, `> ` quotes, `- ` and `1. ` lists, `**bold**`, fenced code (with a language), `|` tables, and `![caption](<slug>/NAME.jpg)` alone on its line. The editor has no inline code, so backticks are dropped. Write `build_release.sh` and similar names so they read as plain text too. Links, italics, `###` and dividers are flattened, each with a warning.

Done when every number traces to a source in the closing comment, and a search finds no em dash, en dash or banned word.

## 2. Make the images

Render HTML pages to PNG with headless Chrome, in the HUD look (ink #06121e, cyan #59d9ff, ice #ebf7ff, Barlow Condensed):

```bash
cp .claude/skills/ringshadow-x-posts/scripts/article-art/examples/bill.html <scratch>/art/chart.html
.claude/skills/ringshadow-x-posts/scripts/article-art/shot.sh <scratch>/art/chart.html 1600 770
sips -s format jpeg -s formatOptions 85 <scratch>/art/chart.png --out docs/articles/<slug>/chart.jpg
```

- **The thumbnail** is the scroll stopper: 2000×800 (5:2), a game frame on the right (`bg.jpg`, from a clip with ffmpeg `-ss T -frames:v 1`), the headline on the left over a fade to ink.
- **Inline images** at 1600 wide: a diagram of the setup, a table of what happened, a chart of the bill. Draw charts to scale from the article's own numbers. Put the numbers in the HTML as data, with a comment naming their source.
- Start each image from the example with the closest layout, and replace all of its text and numbers:
  - `thumb.html`: the 2000×800 header, a game frame with the headline over a fade.
  - `pipeline.html`: a flow diagram of boxes and arrows.
  - `rounds.html`: a table of attempts, each row coloured by its result.
  - `bill.html`: a stacked bar chart drawn to scale from data in the page.
- Read every PNG before using it: the text fits and the numbers match the article.
- Each inline image needs its own file name: the name marks where it goes. JPG, PNG or WebP under 5 MB.

## 3. Save it as an Orchestrator article draft

Publishing stays the developer's. Claude writes and checks the draft, and the developer presses Publish.

1. `x_create_article_draft` with `markdown_path: docs/articles/<slug>.md` and `header_image: docs/articles/<slug>/thumb.jpg`. Inline image paths resolve against the markdown file's folder.
2. Read the reply: the block count by type, the MARKDOWN entities (code blocks plus tables), one image marker per inline image, and the warnings. A refusal lists every problem at once: an image that's missing or inside a paragraph, a fence that's never closed, a duplicate image name.
3. Fix every warning that changes what the reader sees (inline code that now reads oddly, a link that should be spelled out, a `###` meant as `##`), then `x_update_article_draft` with the draft id to convert again. Unchanged images are kept.
4. Write the draft id into the article's `>` note (`Orchestrator draft: xd_…`) so the next session finds it.

Done when the counts match the article (code blocks plus tables, one marker per image) and no warning is left that changes what the reader sees.

## 4. The developer loads and publishes it

Tell the developer the draft is ready, with its id and counts, and how to load it:

1. Open x.com → Articles → Write (`https://x.com/compose/articles`), or the draft's edit link once it has one.
2. Click the amber **Orchestrator drafts** button (bottom right) and pick the article. The extension sets the title, loads the body, uploads the header and applies X's "Edit media" step, then inserts each inline image at its marker through Insert → Media and removes the marker.
3. The amber panel shows each step and the check: no markers left, no blank code block or table, one image per marker, the code/table count equal to Orchestrator's, and every paragraph equal to the draft. Within seconds the page shows "Last saved just now".
4. The developer opens Preview, scrolls through it (headings, bold, code blocks, tables, images) and presses Publish.

`x_get_draft` shows where it stands: `loaded` with `article.edit_url` (the x.com draft), then `published` with `article.published_url`. Publishing is recorded when the extension sees it. If it wasn't seen, the developer presses "Mark published" in the extension's popup.

**After an edit to the markdown:** `x_update_article_draft` again (the status goes back to `draft`, and the edit link stays). The developer opens the edit link and picks the draft again. The body is replaced and the inline images go in again. The header image stays.

### When the check isn't clean

| The panel says | What to do |
|---|---|
| An image failed, or a marker is left | The developer inserts that image by hand at its `[IMAGE: name]` line (Insert → Media), deletes the marker line, and presses "Run the check again". Never Backspace next to a table or code block after an upload: the key hits the block and empties it. |
| A code block or table went blank | Pick the draft again. The body is reloaded from Orchestrator. |
| "inserted, but not right after its marker" | Drag the image to its place, or delete it and insert it again from the end of the marker line. |
| The header slot was not found | The article already has a header, and it stays. Replace it by hand if it should change. |
| "article editor did not load" | The page was still loading. Wait 2 s and pick the draft again. |
| "Can't reach Orchestrator" | The Orchestrator app isn't running, or the extension isn't connected (its popup shows the connection). |

### How the editor stores an article

This is useful when something breaks. The editor is Draft.js, and the draft is its raw state. The block types are `unstyled`, `header-two`, `unordered-list-item`, `ordered-list-item`, `blockquote` and `atomic`. Bold is the inline style `BOLD`, and X's saved JSON spells it `Bold`. Code blocks, tables and LaTeX are all `atomic` blocks with one entity type, `MARKDOWN`, whose `data.markdown` is a fenced block or a markdown table. Images are `MEDIA` entities that point at an upload, which is why they go in through Insert → Media. Orchestrator's converter and the extension's loader are documented in Orchestrator's `Sources/OrchestratorCore/XDrafts/CLAUDE.md`.

## 5. The share post

The post that links the article follows SKILL.md, with three changes:

- It's at most 256 characters.
- It's plain text, so the developer can copy it.
- It has the article's one-line promise and its headline number ("about $15 a month").

Like every post, it needs the developer's yes to the exact text before it goes out (WORKFLOW.md). Once the article is `published`, draft the share post the same way as any post (WORKFLOW.md step 7): `x_create_draft` with the share text as post 1 and the article's link (`published_url` from `x_get_draft`) as the reply, post 2. The developer posts it from the composer's amber button.
