// The roadmap's ring. The milestones in the list are the source: each one
// becomes a moon on the ring around the giant. In orbit mode (a wide
// screen, motion allowed) the scroll turns the ring so the milestone being
// read swings to the front, and the giant's night side grows as the list
// moves from the done past into the future. Otherwise the ring stands
// still with "now" at the front, over the plain list.
(() => {
  const svg = document.querySelector(".giant");
  if (!svg) return;
  const NS = "http://www.w3.org/2000/svg";
  const items = [...document.querySelectorAll(".ms")];
  const N = items.length;
  const STEP = (15 * Math.PI) / 180;
  const RX = 462, RY = 111;
  const TILT = (-16 * Math.PI) / 180;
  const RADIUS = { done: 13, next: 12, later: 10 };

  const back = svg.querySelector(".moons-back");
  const front = svg.querySelector(".moons-front");
  const tag = svg.querySelector(".tag");
  const tagLine = svg.querySelector(".tag-line");
  const tagText = svg.querySelector(".tag-text");
  const stage = document.querySelector(".stage");
  const night0 = svg.querySelector("#night .t0");
  const night1 = svg.querySelector("#night .t1");
  const roEra = document.querySelector(".ro-era");
  const roCount = document.querySelector(".ro-count");
  const roStatus = document.querySelector(".ro-status");
  const orbitMode = matchMedia("(min-width: 901px) and (prefers-reduced-motion: no-preference)");

  const el = (name, attrs) => {
    const e = document.createElementNS(NS, name);
    for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
    return e;
  };

  // The moons.
  const moons = items.map((item, i) => {
    const status = item.dataset.status;
    const g = el("g", { class: `moon ${status}` });
    const r = RADIUS[status];
    g.append(el("circle", { class: "halo", r }), el("circle", { class: "body", r }));
    g.addEventListener("click", () =>
      item.scrollIntoView({ block: "center", behavior: orbitMode.matches ? "smooth" : "auto" }));
    return { g, item, status, base: -i * STEP, r, side: null };
  });
  const lastDone = items.findLastIndex((m) => m.dataset.status === "done");

  // "Now": a marker on the track between the last done moon and the first next one.
  const now = el("g", { class: "now-mark" });
  now.append(el("polygon", { points: "0,-7 6,0 0,7 -6,0" }), Object.assign(el("text", { x: 0, y: -16, "text-anchor": "middle", transform: "rotate(16)" }), { textContent: "Now" }));
  const nowBase = -(lastDone + 0.5) * STEP;

  const place = (g, angle, record) => {
    const z = Math.sin(angle);
    const x = RX * Math.cos(angle), y = RY * z;
    const s = 0.72 + 0.42 * (z + 1) / 2;
    const side = z < 0 ? back : front;
    if (record.side !== side) { side.append(g); record.side = side; }
    g.setAttribute("transform", `translate(${x.toFixed(1)} ${y.toFixed(1)}) scale(${s.toFixed(3)})`);
    g.style.opacity = (0.5 + 0.5 * (z + 1) / 2).toFixed(3);
    return { x, y, s };
  };
  const nowRecord = { side: null };

  // Where each milestone's middle sits on the page, for the scroll.
  let centers = [];
  const measure = () => {
    centers = items.map((m) => {
      const b = m.getBoundingClientRect();
      return b.top + scrollY + b.height / 2;
    });
  };
  const scrollIndex = () => {
    const v = scrollY + innerHeight * 0.5;
    if (v <= centers[0]) return 0;
    for (let i = 0; i < N - 1; i++)
      if (v < centers[i + 1]) return i + (v - centers[i]) / (centers[i + 1] - centers[i]);
    return N - 1;
  };

  let shown = -1;
  let active = -1;
  const draw = (f) => {
    const rot = Math.PI / 2 + f * STEP;
    for (const m of moons) place(m.g, m.base + rot, m);
    place(now, nowBase + rot, nowRecord);

    // The night side grows from the past (mostly lit) to the future (mostly dark).
    const p = f / (N - 1);
    const t0 = 0.72 - 0.56 * p;
    night0.setAttribute("offset", t0.toFixed(3));
    night1.setAttribute("offset", (t0 + 0.14).toFixed(3));
    stage.style.setProperty("--sun", (1 - 0.75 * p).toFixed(3));

    const a = Math.round(f);
    if (a !== active) {
      active = a;
      items.forEach((m, i) => m.classList.toggle("active", i === a && orbitMode.matches));
      moons.forEach((m, i) => m.g.classList.toggle("active", i === a && orbitMode.matches));
      const m = moons[a];
      roEra.textContent = m.item.closest(".era").dataset.era;
      roCount.textContent = `${String(a + 1).padStart(2, "0")} / ${N}`;
      roStatus.textContent = m.status;
      roStatus.dataset.s = m.status;
      tagText.textContent = `${m.item.dataset.short} · ${m.status}`;
    }
    // The label hangs under the active moon, in the svg's own (untilted) frame.
    if (orbitMode.matches) {
      const ang = moons[a].base + rot;
      const lx = RX * Math.cos(ang), ly = RY * Math.sin(ang);
      const x = lx * Math.cos(TILT) - ly * Math.sin(TILT);
      const y = lx * Math.sin(TILT) + ly * Math.cos(TILT);
      tagLine.setAttribute("x1", x.toFixed(1)); tagLine.setAttribute("x2", x.toFixed(1));
      tagLine.setAttribute("y1", (y + 18).toFixed(1)); tagLine.setAttribute("y2", (y + 52).toFixed(1));
      tagText.setAttribute("x", x.toFixed(1)); tagText.setAttribute("y", (y + 74).toFixed(1));
      tagText.setAttribute("text-anchor", "middle");
      tag.setAttribute("opacity", "1");
    } else tag.setAttribute("opacity", "0");
  };

  // Orbit mode: ease toward the scroll's position.
  let current = 0, target = 0, running = false;
  const tick = () => {
    const d = target - current;
    current = Math.abs(d) < 0.002 ? target : current + d * 0.12;
    if (current !== shown) { draw(current); shown = current; }
    if (current !== target) requestAnimationFrame(tick);
    else running = false;
  };
  const onScroll = () => {
    if (!orbitMode.matches) return;
    target = scrollIndex();
    if (!running) { running = true; requestAnimationFrame(tick); }
  };

  const setMode = () => {
    measure();
    active = -1;
    if (orbitMode.matches) {
      current = target = scrollIndex();
      draw(current);
    } else {
      // Still: "now" at the front, between the done moons and the next ones.
      items.forEach((m) => m.classList.remove("active"));
      draw(lastDone + 0.5);
      const done = moons.filter((m) => m.status === "done").length;
      const next = moons.filter((m) => m.status === "next").length;
      roEra.textContent = "Ringshadow roadmap";
      roCount.textContent = `${done} done`;
      roStatus.textContent = `${next} next · ${N - done - next} later`;
      roStatus.dataset.s = "next";
    }
  };

  setMode();
  addEventListener("scroll", onScroll, { passive: true });
  addEventListener("resize", () => { setMode(); });
  orbitMode.addEventListener("change", setMode);
  // Fonts and images change the list's heights once they arrive.
  document.fonts?.ready.then(() => { measure(); onScroll(); });
  addEventListener("load", () => { measure(); onScroll(); });
})();
