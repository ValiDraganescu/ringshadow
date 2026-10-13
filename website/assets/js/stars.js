// The starfield behind both pages: three layers of stars that drift apart
// with the scroll (parallax) and twinkle. Still under reduced motion.
(() => {
  const canvas = document.querySelector("canvas.stars");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  const still = matchMedia("(prefers-reduced-motion: reduce)");

  // A seeded random, so the sky is the same on every visit.
  let seed = 7;
  const rand = () => ((seed = (seed * 16807) % 2147483647) - 1) / 2147483646;

  const layers = [
    { count: 260, size: 0.7, speed: 0.04, alpha: 0.55 },
    { count: 110, size: 1.1, speed: 0.1, alpha: 0.75 },
    { count: 36, size: 1.7, speed: 0.22, alpha: 0.95 },
  ];
  const tints = ["#ebf7ff", "#ccf2ff", "#8cd9f2", "#ffe2a8"];
  const stars = [];
  for (const [i, l] of layers.entries())
    for (let n = 0; n < l.count; n++)
      stars.push({
        layer: i, x: rand(), y: rand(),
        r: l.size * (0.6 + rand() * 0.8),
        tint: tints[Math.floor(rand() * (rand() < 0.85 ? 3 : 4))],
        phase: rand() * Math.PI * 2, rate: 0.4 + rand() * 1.4,
      });

  let w = 0, h = 0, dpr = 1;
  const resize = () => {
    dpr = Math.min(devicePixelRatio || 1, 2);
    w = innerWidth; h = innerHeight;
    canvas.width = Math.round(w * dpr); canvas.height = Math.round(h * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  };

  const draw = (t) => {
    ctx.clearRect(0, 0, w, h);
    const scroll = scrollY;
    for (const s of stars) {
      const l = layers[s.layer];
      const span = h * 1.6;
      let y = (s.y * span - scroll * l.speed) % span;
      if (y < 0) y += span;
      if (y > h + 4) continue;
      const tw = still.matches ? 1 : 0.65 + 0.35 * Math.sin(t * 0.001 * s.rate + s.phase);
      ctx.globalAlpha = l.alpha * tw;
      ctx.fillStyle = s.tint;
      ctx.beginPath();
      ctx.arc(s.x * w, y, s.r, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.globalAlpha = 1;
  };

  let queued = false;
  const frame = (t) => {
    queued = false;
    draw(t);
    if (!still.matches && !document.hidden) loop();
  };
  const loop = () => { if (!queued) { queued = true; requestAnimationFrame(frame); } };

  resize();
  addEventListener("resize", () => { resize(); loop(); });
  addEventListener("scroll", loop, { passive: true });
  document.addEventListener("visibilitychange", loop);
  still.addEventListener("change", loop);
  loop();
})();
