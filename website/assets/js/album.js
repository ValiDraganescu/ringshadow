// Albums: every `.album` is a grid of thumbnails linking to the full
// pictures. With this script a click opens them in a lightbox instead:
// arrow keys and swipes step through the album, Esc closes. Without it the
// links still open each picture on its own.
(() => {
  const albums = [...document.querySelectorAll(".album")];
  if (!albums.length || !window.HTMLDialogElement) return;

  const lb = document.createElement("dialog");
  lb.className = "lb";
  lb.setAttribute("aria-label", "Picture viewer");
  lb.innerHTML = `
    <div class="lb-stage">
      <img alt="">
      <button class="lb-prev" type="button" aria-label="Previous picture">&#8249;</button>
      <button class="lb-next" type="button" aria-label="Next picture">&#8250;</button>
      <button class="lb-close" type="button" aria-label="Close">&#215;</button>
    </div>
    <div class="lb-bar"><span class="lb-count"></span><p class="lb-cap"></p></div>`;
  document.body.append(lb);
  const img = lb.querySelector("img");
  const cap = lb.querySelector(".lb-cap");
  const count = lb.querySelector(".lb-count");

  let items = [], at = 0;

  const show = (i) => {
    at = (i + items.length) % items.length;
    const a = items[at];
    img.src = a.href;
    img.alt = a.querySelector("img")?.alt || "";
    cap.textContent = a.parentElement.querySelector(".cap")?.textContent || img.alt;
    count.textContent = `${at + 1} / ${items.length}`;
    // Warm the neighbours so stepping is instant.
    for (const d of [1, -1]) new Image().src = items[(at + d + items.length) % items.length].href;
  };

  for (const album of albums) {
    const links = [...album.querySelectorAll(".album-grid a")];
    links.forEach((a, i) => a.addEventListener("click", (e) => {
      if (e.metaKey || e.ctrlKey || e.shiftKey || e.button !== 0) return;
      e.preventDefault();
      items = links;
      show(i);
      lb.showModal();
    }));
  }

  lb.querySelector(".lb-prev").addEventListener("click", () => show(at - 1));
  lb.querySelector(".lb-next").addEventListener("click", () => show(at + 1));
  lb.querySelector(".lb-close").addEventListener("click", () => lb.close());
  lb.addEventListener("keydown", (e) => {
    if (e.key === "ArrowRight") { e.preventDefault(); show(at + 1); }
    else if (e.key === "ArrowLeft") { e.preventDefault(); show(at - 1); }
    else if (e.key === "Home") { e.preventDefault(); show(0); }
    else if (e.key === "End") { e.preventDefault(); show(items.length - 1); }
  });
  // A click on the dark around the picture closes it.
  let swiped = false;
  lb.addEventListener("click", (e) => {
    if (swiped) { swiped = false; return; }
    if (e.target === lb || e.target.classList.contains("lb-stage")) lb.close();
  });
  lb.addEventListener("close", () => { img.removeAttribute("src"); items[at]?.focus(); });

  // Swipes: a mostly sideways drag of 40 px or more steps the album.
  let x0 = null, y0 = 0;
  const stage = lb.querySelector(".lb-stage");
  stage.addEventListener("pointerdown", (e) => { x0 = e.clientX; y0 = e.clientY; swiped = false; });
  stage.addEventListener("pointerup", (e) => {
    if (x0 === null) return;
    const dx = e.clientX - x0, dy = e.clientY - y0;
    x0 = null;
    if (Math.abs(dx) > 40 && Math.abs(dx) > Math.abs(dy) * 1.5) { swiped = true; show(at + (dx < 0 ? 1 : -1)); }
  });
  stage.addEventListener("pointercancel", () => { x0 = null; });
})();

// Videos with controls (the posts' showreels): the first play of each is one
// video-play event for the site's counter (count.js).
for (const video of document.querySelectorAll("video[controls]")) {
  video.addEventListener("play", () => window.count?.("video-play"), { once: true });
}
