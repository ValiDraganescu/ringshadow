// The home page's clips play only while on screen. Under reduced motion
// they stay paused on their first frame, with controls to start them.
(() => {
  const still = matchMedia("(prefers-reduced-motion: reduce)").matches;
  const videos = document.querySelectorAll("video");
  if (still) {
    for (const v of videos) { v.removeAttribute("autoplay"); v.pause(); v.controls = true; }
    return;
  }
  const seen = new IntersectionObserver((entries) => {
    for (const e of entries) {
      const v = e.target;
      if (e.isIntersecting) {
        if (v.preload === "none") v.preload = "auto";
        v.play().catch(() => {});
      } else v.pause();
    }
  }, { rootMargin: "120px" });
  for (const v of videos) seen.observe(v);
})();
