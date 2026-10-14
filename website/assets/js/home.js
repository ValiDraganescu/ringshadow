// The home page. Loaded in <head> without defer so the "js" class is set
// before the first paint: the hidden starting states of the entrances apply
// only under html.js, so with no script everything is simply there.
document.documentElement.classList.add("js");

addEventListener("DOMContentLoaded", () => {
  const still = matchMedia("(prefers-reduced-motion: reduce)").matches;
  const $$ = (s, el = document) => [...el.querySelectorAll(s)];

  // Entrances: each part arrives once, as its section comes into view.
  // A clipped element can't be seen by the observer, so a clip reveal watches its parent.
  const targets = [...$$(".reveal, .units, .blocks, .diagram"), ...$$(".shutter").map((el) => el.parentElement)];
  if (!("IntersectionObserver" in window)) {
    for (const el of targets) el.classList.add("is-in");
  } else {
    const io = new IntersectionObserver((entries) => {
      for (const e of entries) if (e.isIntersecting) { e.target.classList.add("is-in"); io.unobserve(e.target); }
    }, { rootMargin: "0px 0px -10% 0px", threshold: 0.08 });
    for (const el of targets) io.observe(el);
  }

  // Clips play only while on screen, and only the tab that is showing.
  // Under reduced motion they stay paused on their first frame, with controls.
  const videos = $$("main video");
  const hiddenPanel = (v) => v.classList.contains("panel-media") && !v.classList.contains("on");
  if (still) {
    for (const v of videos) { v.removeAttribute("autoplay"); v.pause(); v.controls = true; }
  } else {
    const seen = new IntersectionObserver((entries) => {
      for (const e of entries) {
        const v = e.target;
        v.dataset.seen = e.isIntersecting ? "1" : "";
        if (e.isIntersecting && !hiddenPanel(v)) {
          if (v.preload === "none") v.preload = "auto";
          v.play().catch(() => {});
        } else v.pause();
      }
    }, { rootMargin: "120px" });
    for (const v of videos) seen.observe(v);
  }

  // Three ways to play: tabs switch the plate's picture and the words.
  const tabs = $$(".tabs [role=tab]");
  const select = (tab, focus) => {
    for (const t of tabs) {
      const on = t === tab;
      t.setAttribute("aria-selected", on);
      t.tabIndex = on ? 0 : -1;
      document.getElementById(t.getAttribute("aria-controls")).hidden = !on;
    }
    for (const m of $$(".show-plate .panel-media")) {
      const on = m.dataset.panel === tab.dataset.tab;
      m.classList.toggle("on", on);
      if (m.tagName === "VIDEO" && !still) on && m.dataset.seen ? m.play().catch(() => {}) : m.pause();
    }
    if (focus) tab.focus();
  };
  for (const [i, t] of tabs.entries()) {
    t.addEventListener("click", () => select(t));
    t.addEventListener("keydown", (e) => {
      const step = { ArrowRight: 1, ArrowLeft: -1 }[e.key];
      if (step) { e.preventDefault(); select(tabs[(i + step + tabs.length) % tabs.length], true); }
    });
  }
  if (tabs.length) select(tabs[0]);

  // A cockpit for each: one frame, its picture changes every few seconds
  // while it's on screen. A click or a tap shows the next one at once.
  const stack = document.querySelector(".frame-stack");
  if (stack) {
    const imgs = $$("img", stack);
    const cap = document.querySelector(".frame-cap");
    let i = 0, timer = 0;
    const show = (n) => {
      imgs[i].classList.remove("on");
      i = (n + imgs.length) % imgs.length;
      imgs[i].classList.add("on");
      cap.textContent = imgs[i].dataset.cap;
    };
    const run = (on) => {
      clearInterval(timer);
      if (on && !still) timer = setInterval(() => show(i + 1), 3000);
    };
    stack.addEventListener("click", () => { show(i + 1); run(true); });
    stack.addEventListener("keydown", (e) => {
      if (e.key === "Enter" || e.key === " ") { e.preventDefault(); show(i + 1); run(true); }
    });
    new IntersectionObserver(([e]) => run(e.isIntersecting)).observe(stack);
  }

  // The download: the build for the visitor's computer, picked from the
  // user agent, switchable. The links are the 0.1.1 release's own files.
  const rel = "https://github.com/ValiDraganescu/ringshadow/releases/download/v0.1.1/";
  const builds = {
    mac: ["Ringshadow-0.1.1-mac.zip", "Mac", "Needs macOS 14 or newer and an M1 or later."],
    win: ["Ringshadow-0.1.1-windows.zip", "Windows", "Needs Windows 10 or 11 and a DirectX 12 graphics card."],
    linux: ["Ringshadow-0.1.1-linux.tar.gz", "Linux", "Needs an x64 PC with glibc and a Vulkan driver."],
  };
  const seg = $$(".seg [role=radio]");
  const go = document.querySelector(".dl-go");
  const need = document.querySelector(".dl-need");
  const pick = (os, focus) => {
    const [file, name, needs] = builds[os];
    for (const b of seg) {
      const on = b.dataset.os === os;
      b.setAttribute("aria-checked", on);
      b.tabIndex = on ? 0 : -1;
      if (on && focus) b.focus();
    }
    go.href = rel + file;
    go.textContent = `Download for ${name}`;
    need.textContent = needs;
  };
  if (go) {
    const ua = navigator.userAgent;
    pick(/Windows/.test(ua) ? "win" : /Linux/.test(ua) && !/Android/.test(ua) ? "linux" : "mac");
    for (const [i, b] of seg.entries()) {
      b.addEventListener("click", () => pick(b.dataset.os));
      b.addEventListener("keydown", (e) => {
        const step = { ArrowRight: 1, ArrowDown: 1, ArrowLeft: -1, ArrowUp: -1 }[e.key];
        if (step) { e.preventDefault(); pick(seg[(i + step + seg.length) % seg.length].dataset.os, true); }
      });
    }
  }

  const copy = document.querySelector(".dl-copy");
  copy?.addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(document.querySelector(".dl-code code").textContent);
      copy.textContent = "Copied";
    } catch { copy.textContent = "Select it"; }
    setTimeout(() => (copy.textContent = "Copy"), 1600);
  });
});
