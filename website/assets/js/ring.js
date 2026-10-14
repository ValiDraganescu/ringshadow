// Shared by the home page and the blog. Loaded in <head> without defer so
// the "js" class is set before the first paint: the hidden starting states
// of the entrances apply only under html.js, so with no script everything
// is simply there.
document.documentElement.classList.add("js");

addEventListener("DOMContentLoaded", () => {
  // Entrances: each part arrives once, as it comes into view. A clipped
  // element can't be seen by the observer, so a clip reveal watches its parent.
  const $$ = (s) => [...document.querySelectorAll(s)];
  const targets = [...$$(".reveal, .track, .blocks, .diagram"), ...$$(".shutter").map((el) => el.parentElement)];
  if (!("IntersectionObserver" in window)) {
    for (const el of targets) el.classList.add("is-in");
    return;
  }
  const io = new IntersectionObserver((entries) => {
    for (const e of entries) if (e.isIntersecting) { e.target.classList.add("is-in"); io.unobserve(e.target); }
  }, { rootMargin: "0px 0px -10% 0px", threshold: 0.08 });
  for (const el of targets) io.observe(el);
});
