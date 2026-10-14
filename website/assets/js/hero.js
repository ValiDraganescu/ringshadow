// The home page's hero: Ringshadow, the ringed gas giant, drawn live with
// three.js. Everything is a shader, nothing is a texture: the planet's bands
// turn at different speeds by latitude, a great storm turns inside them,
// meteorites strike it and send ripples across the clouds, the rings orbit
// (inner faster than outer) and nod slightly, Perseid-style showers and
// lone meteors cross the sky, and now and then a comet passes.
//
// Kept cheap: five draw calls, no textures, a 60 fps cap, a pixel ratio that
// drops when frames run long, and no frames at all while the hero is off
// screen or the tab is hidden. Without WebGL the key art is shown instead;
// under reduced motion the scene is drawn once and holds still.
import {
  WebGLRenderer, Scene, PerspectiveCamera, Group, Mesh,
  SphereGeometry, RingGeometry, BufferGeometry, InstancedBufferGeometry,
  BufferAttribute, InstancedBufferAttribute, ShaderMaterial,
  Vector3, NormalBlending, CustomBlending, AddEquation, OneFactor, ZeroFactor,
  DoubleSide, BackSide, DynamicDrawUsage,
} from "./vendor/three-hero.min.js";

const hero = document.querySelector(".hero");
const canvas = hero?.querySelector(".hero-gl");
const still = matchMedia("(prefers-reduced-motion: reduce)").matches;

function fallback() {
  hero.classList.add("hero-still");
  if (!hero.querySelector(".hero-art")) {
    const img = new Image(1408, 704);
    img.className = "hero-art";
    img.alt = "";
    img.src = "assets/media/key-art.jpg";
    hero.prepend(img);
  }
}

if (hero && canvas) {
  try { start(); } catch (e) { fallback(); }
}

function start() {
  const renderer = new WebGLRenderer({ canvas, antialias: true, alpha: true, powerPreference: "low-power" });
  renderer.setClearColor(0x000000, 0);
  canvas.addEventListener("webglcontextlost", (e) => { e.preventDefault(); running = false; fallback(); });

  const scene = new Scene();
  // Glows add light. The canvas is transparent and premultiplied, so each
  // glow also adds its brightness to alpha: light with no alpha would vanish
  // over the empty sky, and alpha with no light would darken the page behind.
  const glow = {
    transparent: true, depthWrite: false, blending: CustomBlending, blendEquation: AddEquation,
    blendSrc: OneFactor, blendDst: OneFactor, blendSrcAlpha: OneFactor, blendDstAlpha: OneFactor,
  };
  const glowOut = "vec4 glowOut(vec3 c) { c = min(c, 1.0); return vec4(c, max(c.r, max(c.g, c.b))); }";
  const camera = new PerspectiveCamera(32, 2, 0.1, 100);

  // ---- Shared GLSL ----------------------------------------------------------

  const RIN = 1.28, ROUT = 2.32;
  const common = /* glsl */ `
    float hash(vec3 p) { p = fract(p * 0.3183099 + 0.1); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }
    float noise(vec3 x) {
      vec3 i = floor(x), f = fract(x); f = f * f * (3.0 - 2.0 * f);
      return mix(mix(mix(hash(i), hash(i + vec3(1,0,0)), f.x), mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
                 mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x), mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y), f.z);
    }
    float fbm(vec3 p) { float a = 0.5, s = 0.0; for (int i = 0; i < 4; i++) { s += a * noise(p); p = p * 2.03 + vec3(1.7, 9.2, 3.1); a *= 0.5; } return s; }
    vec3 sph(float lon, float lat) { float c = cos(lat); return vec3(c * cos(lon), sin(lat), c * sin(lon)); }
    // How much of a ripple of frequency f (radians per unit) survives when one
    // pixel spans fw units: fine bands fade out before they can alias.
    float aa(float f, float fw) { return 1.0 - smoothstep(0.5, 1.5, f * fw); }
    // The rings' opacity by radius: bands, a faint inner ring, the dark gap.
    // fw is the radius one pixel spans (0 where there are no pixels to fit).
    float ringDensity(float r, float fw) {
      if (r < ${RIN.toFixed(2)} || r > ${ROUT.toFixed(2)}) return 0.0;
      float x = (r - ${RIN.toFixed(2)}) / ${(ROUT - RIN).toFixed(2)};
      float d = 0.55 + 0.22 * aa(41.0, fw) * sin(r * 41.0) + 0.14 * aa(97.0, fw) * sin(r * 97.0 + 1.3) + 0.09 * aa(173.0, fw) * sin(r * 173.0);
      d *= smoothstep(0.0, 0.05, x) * (1.0 - smoothstep(0.9, 1.0, x));
      // The gap: when it is thinner than a pixel, wider and fainter by the same amount.
      float gw = max(0.018, fw * 1.5);
      d *= 1.0 - 0.9 * (0.018 / gw) * (1.0 - smoothstep(0.0, gw, abs(r - 1.94)));
      d *= mix(0.3, 1.0, smoothstep(1.42, 1.56, r));
      return clamp(d, 0.0, 1.0);
    }
  `;

  // ---- The planet -----------------------------------------------------------

  // Bands drift at their own speed by latitude (rad/s); keep in step with omega() below.
  const omega = (lat) => 0.035 + 0.012 * Math.sin(lat * 11.0);

  const planetUniforms = {
    uTime: { value: 0 },
    uSun: { value: new Vector3(-1, 0.35, 0.55).normalize() },
    uRingN: { value: new Vector3(0, 1, 0) },
    uImpact: { value: new Vector3(0, 0, -1000) }, // band longitude, latitude, time of impact
    uStorm: { value: new Vector3(-0.36, 2.2, 0) },  // latitude, band longitude
  };

  const planetMat = new ShaderMaterial({
    uniforms: planetUniforms,
    vertexShader: /* glsl */ `
      varying vec3 vL; varying vec3 vN; varying vec3 vW;
      void main() {
        vL = position;
        vec4 w = modelMatrix * vec4(position, 1.0);
        vW = w.xyz;
        vN = normalize(mat3(modelMatrix) * normal);
        gl_Position = projectionMatrix * viewMatrix * w;
      }`,
    fragmentShader: /* glsl */ `
      uniform float uTime; uniform vec3 uSun; uniform vec3 uRingN; uniform vec3 uImpact; uniform vec3 uStorm;
      varying vec3 vL; varying vec3 vN; varying vec3 vW;
      ${common}
      float omega(float lat) { return 0.035 + 0.012 * sin(lat * 11.0); }
      float adiff(float a, float b) { return atan(sin(a - b), cos(a - b)); }
      vec3 bands(float b, float n) {
        float s1 = 0.5 + 0.5 * sin(b);
        float s2 = 0.5 + 0.5 * sin(b * 2.3 + 1.7);
        float s3 = 0.5 + 0.5 * sin(b * 5.1 + 0.4 + n * 2.0);
        vec3 c = mix(vec3(0.07, 0.27, 0.32), vec3(0.27, 0.68, 0.67), s1);
        c = mix(c, vec3(0.93, 0.84, 0.64), smoothstep(0.55, 0.95, s2) * 0.8);
        c = mix(c, vec3(0.89, 0.52, 0.24), smoothstep(0.72, 1.0, s3) * 0.75);
        return c;
      }
      void main() {
        vec3 p = normalize(vL);
        float lat = asin(clamp(p.y, -1.0, 1.0));
        float lq = atan(p.z, p.x) - omega(lat) * uTime;   // longitude in the band's own frame

        // The storm: an oval vortex riding its band, turning on itself.
        vec2 sd = vec2(adiff(lq, uStorm.y) * cos(lat), lat - uStorm.x);
        float e = length(sd / vec2(0.30, 0.13));
        float sw = 1.0 - smoothstep(0.0, 1.7, e);
        float ang = sw * sw * 5.0 + uTime * 0.5 * sw;
        vec2 sr = mat2(cos(ang), -sin(ang), sin(ang), cos(ang)) * sd;
        float latw = mix(lat, uStorm.x + sr.y, sw);
        float lqw = mix(lq, uStorm.y + sr.x / cos(lat), sw);

        vec3 P = sph(lqw, latw);
        float n = fbm(P * vec3(3.0, 11.0, 3.0));
        float n2 = fbm(P * vec3(10.0, 34.0, 10.0));
        vec3 col = bands(latw * 8.5 + n * 1.7 + n2 * 0.25, n2);
        float core = 1.0 - smoothstep(0.5, 1.05, e);
        col = mix(col, vec3(0.84, 0.40, 0.24) * (0.8 + 0.4 * n2), core * 0.85);
        col = mix(col, vec3(0.96, 0.88, 0.72), (1.0 - smoothstep(1.0, 1.3, e)) * (1.0 - core) * 0.45);

        // A meteorite strike: a flash, a fireball, ripples running out, a dark scar.
        float rip = 0.0, flash = 0.0;
        float tI = uTime - uImpact.z;
        if (tI > 0.0 && tI < 80.0) {
          float d = acos(clamp(dot(sph(lq, lat), sph(uImpact.x, uImpact.y)), -1.0, 1.0));
          float x = d - 0.11 * tI;
          rip = sin(x * 55.0) * exp(-x * x * 90.0) * exp(-tI * 0.08);
          float x2 = d - 0.05 * tI;
          rip += 0.6 * sin(x2 * 85.0) * exp(-x2 * x2 * 170.0) * exp(-tI * 0.11);
          flash = exp(-d * d * 1400.0) * exp(-tI * 2.4);
          float plume = exp(-d * d * 280.0) * exp(-tI * 0.35);
          float scar = (1.0 - smoothstep(0.025, 0.1, d)) * smoothstep(0.0, 4.0, tI) * exp(-tI * 0.035);
          col = mix(col, vec3(0.13, 0.08, 0.07), scar * 0.65);
          col += vec3(1.0, 0.5, 0.2) * plume * 0.9;
        }

        vec3 N = normalize(vN);
        vec3 V = normalize(cameraPosition - vW);
        float nl = dot(N, uSun);
        float lit = smoothstep(-0.1, 0.35, nl) * (1.0 + 0.3 * rip);
        // The rings' shadow on the clouds.
        float sh = 1.0;
        float den = dot(uSun, uRingN);
        if (abs(den) > 1e-3) {
          float t = -dot(vW, uRingN) / den;
          if (t > 0.0) sh = 1.0 - 0.78 * ringDensity(length(vW + uSun * t), 0.0);
        }
        float nv = max(dot(N, V), 0.0);
        vec3 c = col * (0.025 + lit * sh * 1.08) * pow(nv, 0.4);
        c += vec3(0.33, 0.74, 0.86) * pow(1.0 - nv, 3.0) * smoothstep(-0.25, 0.6, nl) * 0.6;
        c += vec3(0.62, 0.82, 0.9) * max(rip, 0.0) * 0.05 * lit;
        c += vec3(1.0, 0.86, 0.62) * flash * 4.0;
        gl_FragColor = vec4(c, 1.0);
      }`,
  });

  const system = new Group();          // tilt of the giant's axis
  system.rotation.set(0.2, 0, -0.28);
  scene.add(system);
  const planet = new Mesh(new SphereGeometry(1, 96, 64), planetMat);
  system.add(planet);

  // A thin glow around the disc.
  const halo = new Mesh(new SphereGeometry(1.04, 64, 32), new ShaderMaterial({
    uniforms: { uSun: planetUniforms.uSun },
    vertexShader: /* glsl */ `
      varying vec3 vN; varying vec3 vW;
      void main() { vec4 w = modelMatrix * vec4(position, 1.0); vW = w.xyz; vN = normalize(mat3(modelMatrix) * normal);
        gl_Position = projectionMatrix * viewMatrix * w; }`,
    fragmentShader: /* glsl */ `
      uniform vec3 uSun; varying vec3 vN; varying vec3 vW;
      ${glowOut}
      void main() {
        vec3 V = normalize(cameraPosition - vW);
        // Strongest at the planet's edge (|N.V| ~ 0.28 there), gone at the halo's own edge.
        float g = pow(smoothstep(0.0, 0.28, abs(dot(normalize(vN), V))), 2.5);
        float lit = smoothstep(-0.3, 0.7, dot(normalize(vN), uSun));
        gl_FragColor = glowOut(vec3(0.4, 0.82, 0.95) * g * 0.35 * lit);
      }`,
    side: BackSide, ...glow,
  }));
  system.add(halo);

  // ---- The rings ------------------------------------------------------------

  const ringUniforms = { uTime: planetUniforms.uTime, uSun: planetUniforms.uSun, uRingN: planetUniforms.uRingN };
  const rings = new Mesh(new RingGeometry(RIN, ROUT, 256, 1), new ShaderMaterial({
    uniforms: ringUniforms,
    vertexShader: /* glsl */ `
      varying vec3 vL; varying vec3 vW;
      void main() { vL = position; vec4 w = modelMatrix * vec4(position, 1.0); vW = w.xyz;
        gl_Position = projectionMatrix * viewMatrix * w; }`,
    fragmentShader: /* glsl */ `
      uniform float uTime; uniform vec3 uSun; uniform vec3 uRingN;
      varying vec3 vL; varying vec3 vW;
      ${common}
      void main() {
        float r = length(vL.xy);
        float a = atan(vL.y, vL.x) - 0.25 * pow(r, -1.5) * uTime;   // inner rings orbit faster
        vec2 cs = vec2(cos(a), sin(a));
        float fw = fwidth(r);
        float clump = mix(0.5, noise(vec3(r * 38.0, cs * 7.0)), aa(38.0 * 6.28, fw));
        float fine = mix(0.5, noise(vec3(r * 240.0, cs * 2.5)), aa(240.0 * 6.28, fw));
        float dens = ringDensity(r, fw) * (0.8 + 0.34 * clump) * (0.88 + 0.24 * fine);
        // The giant's shadow on the rings.
        float b = dot(vW, uSun), c = dot(vW, vW) - 1.0, disc = b * b - c;
        float shadow = b < 0.0 ? 1.0 - 0.92 * smoothstep(0.0, 0.06, disc) : 1.0;
        vec3 rc = mix(vec3(0.6, 0.66, 0.67), vec3(0.96, 0.9, 0.79), smoothstep(1.5, 2.2, r)) * (0.88 + 0.24 * fine);
        vec3 col = rc * (0.55 + 1.1 * abs(dot(uRingN, uSun))) * shadow;
        gl_FragColor = vec4(col, dens * 0.92);
      }`,
    side: DoubleSide, transparent: true, depthWrite: false, blending: NormalBlending,
  }));
  const ringPivot = new Group();
  ringPivot.add(rings);
  rings.rotation.x = -Math.PI / 2;
  system.add(ringPivot);

  // ---- Meteors (screen space, one instanced draw) ---------------------------

  const MAX = 48;
  const quad = (geo) => {
    geo.setAttribute("corner", new BufferAttribute(new Float32Array([0, -1, 1, -1, 1, 1, 0, 1]), 2));
    geo.setIndex([0, 1, 2, 0, 2, 3]);
    return geo;
  };
  const mGeo = quad(new InstancedBufferGeometry());
  mGeo.instanceCount = MAX;
  const attr = (name, size) => {
    const a = new InstancedBufferAttribute(new Float32Array(MAX * size), size);
    a.setUsage(DynamicDrawUsage);
    mGeo.setAttribute(name, a);
    return a;
  };
  const aStart = attr("aStart", 2), aVel = attr("aVel", 2), aTime = attr("aTime", 4), aColor = attr("aColor", 4);
  for (let i = 0; i < MAX; i++) aTime.array[i * 4] = -1e6;
  const res = { value: [1, 1] };
  const meteors = new Mesh(mGeo, new ShaderMaterial({
    uniforms: { uTime: planetUniforms.uTime, uRes: res },
    vertexShader: /* glsl */ `
      attribute vec2 corner; attribute vec2 aStart; attribute vec2 aVel; attribute vec4 aTime; attribute vec4 aColor;
      uniform float uTime; uniform vec2 uRes;
      varying vec2 vC; varying float vFade; varying vec3 vCol;
      void main() {
        float tt = uTime - aTime.x;
        if (tt < 0.0 || tt > aTime.y) { gl_Position = vec4(2.0, 2.0, 2.0, 1.0); return; }
        vec2 head = aStart + aVel * tt;
        vec2 tail = head - aVel * min(tt, aTime.z);
        vec2 dir = normalize(aVel), nrm = vec2(-dir.y, dir.x);
        vec2 p = mix(tail, head, corner.x) + nrm * corner.y * aTime.w + dir * corner.x * aTime.w * 2.0;
        vec2 ndc = p / uRes * 2.0 - 1.0;
        gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
        float u = tt / aTime.y;
        vFade = smoothstep(0.0, 0.12, u) * (aColor.a > 0.5 ? 1.0 : 1.0 - smoothstep(0.65, 1.0, u));
        vC = corner; vCol = aColor.rgb;
      }`,
    fragmentShader: /* glsl */ `
      varying vec2 vC; varying float vFade; varying vec3 vCol;
      ${glowOut}
      void main() {
        float across = 1.0 - abs(vC.y);
        float a = pow(vC.x, 3.0) * across * across * vFade;
        float head = smoothstep(0.9, 1.0, vC.x) * across * vFade;
        gl_FragColor = glowOut(vCol * (a + head * 1.5));
      }`,
    ...glow, depthTest: false,
    side: DoubleSide, // screen-space quads: y is flipped, so their winding is too
  }));
  meteors.frustumCulled = false;
  meteors.renderOrder = 10;
  scene.add(meteors);

  let slot = 0;
  function meteor(x, y, vx, vy, t0, life, trail, width, r, g, b, impactor = 0) {
    const i = slot; slot = (slot + 1) % MAX;
    aStart.array.set([x, y], i * 2); aVel.array.set([vx, vy], i * 2);
    aTime.array.set([t0, life, trail, width], i * 4); aColor.array.set([r, g, b, impactor], i * 4);
    for (const a of [aStart, aVel, aTime, aColor]) { a.clearUpdateRanges(); a.needsUpdate = true; }
  }

  // ---- The comet (in the world, far behind the giant) -----------------------
  // One quad from the head along the anti-sun direction, widened on screen.
  // It is depth-tested, so the giant hides it, and drawn before the rings, so
  // they pass in front of it.

  const comet = {
    head: { value: new Vector3(0, 0, -1000) },
    tail: { value: planetUniforms.uSun.value.clone().negate() },
    len: { value: 2.8 }, wid: { value: 40 }, alpha: { value: 0 },
  };
  const cometMesh = new Mesh(quad(new BufferGeometry()), new ShaderMaterial({
    uniforms: { uRes: res, uHead: comet.head, uTail: comet.tail, uLen: comet.len, uWid: comet.wid, uAlpha: comet.alpha },
    vertexShader: /* glsl */ `
      attribute vec2 corner; uniform vec2 uRes; uniform vec3 uHead, uTail; uniform float uLen, uWid;
      varying vec2 vUV;
      void main() {
        float u = corner.x * 1.1 - 0.1;   // a little room ahead of the head for the coma
        mat4 vp = projectionMatrix * viewMatrix;
        vec4 h = vp * vec4(uHead, 1.0), e = vp * vec4(uHead + uTail * uLen, 1.0);
        vec2 dir = normalize((e.xy / e.w - h.xy / h.w) * uRes + 1e-6), nrm = vec2(-dir.y, dir.x);
        vec4 c = vp * vec4(uHead + uTail * uLen * u, 1.0);
        vec2 px = nrm * corner.y * uWid * (0.35 + 0.65 * max(u, 0.0));
        gl_Position = vec4(c.xy + px / uRes * 2.0 * c.w, c.z, c.w);
        vUV = vec2(u, corner.y);
      }`,
    fragmentShader: /* glsl */ `
      uniform float uAlpha; varying vec2 vUV;
      ${glowOut}
      void main() {
        float u = vUV.x, v = vUV.y, uc = clamp(u, 0.0, 1.0);
        float edge = 1.0 - smoothstep(0.7, 1.0, abs(v));
        // Dust: a soft fan, widening and fading along the tail.
        float s = 0.16 + 0.5 * uc;
        float dust = exp(-v * v / (s * s)) * pow(1.0 - uc, 2.2) * smoothstep(-0.02, 0.08, u);
        // Ion tail: narrower and bluish.
        float ion = exp(-v * v / 0.01) * pow(1.0 - uc, 1.3) * smoothstep(0.0, 0.12, u);
        // Coma and nucleus round the head (u = 0).
        float r = length(vec2(u * 12.0, v * 0.9));
        float coma = exp(-r * r * 5.0), nucleus = exp(-r * r * 50.0);
        vec3 col = vec3(1.0, 0.92, 0.78) * dust * 0.5 + vec3(0.5, 0.78, 1.0) * ion * 0.3
                 + vec3(0.75, 0.95, 1.0) * coma * 0.6 + vec3(1.0) * nucleus;
        gl_FragColor = glowOut(col * edge * uAlpha);
      }`,
    ...glow, side: DoubleSide,
  }));
  cometMesh.frustumCulled = false;
  cometMesh.renderOrder = -1;
  scene.add(cometMesh);

  // ---- Layout ---------------------------------------------------------------

  // Crisp from the start; when frames run long the ratio drops, but never
  // below one pixel per CSS pixel (lower makes stair-stepped edges).
  const maxPr = Math.min(devicePixelRatio || 1, 2), minPr = Math.min(devicePixelRatio || 1, 1);
  let W = 1, H = 1, pr = maxPr;
  function layout() {
    const r = hero.getBoundingClientRect();
    W = Math.max(1, Math.round(r.width)); H = Math.max(1, Math.round(r.height));
    renderer.setPixelRatio(pr);
    renderer.setSize(W, H, false);
    res.value[0] = W; res.value[1] = H;
    comet.wid.value = Math.max(24, 0.05 * H);
    const aspect = W / H;
    camera.aspect = aspect;
    // Wide screens: the giant to the right of the title. Tall ones: above it.
    const wide = aspect >= 1.1;
    const d = wide ? 4.4 : 6.2;
    const hh = Math.tan((camera.fov * Math.PI) / 360) * d, hw = hh * aspect;
    camera.position.set(wide ? -0.4 * hw : 0, wide ? -0.1 * hh : -0.36 * hh, d);
    camera.updateProjectionMatrix();
    camera.updateMatrixWorld();
    if (still) draw();
  }

  // ---- Events: showers, comets, strikes -------------------------------------

  const rnd = (a, b) => a + Math.random() * (b - a);
  let t = 0;
  let nextSporadic = 1.5, nextShower = rnd(6, 14), nextComet = 3, nextImpact = 8;
  let cometOn = null;
  const pending = [];

  function shootingStar(at, bright = 1) {
    // Perseids: all from one radiant, high up to the left.
    const rx = -0.15 * W, ry = -0.25 * H;
    const x = rnd(0.05, 1.05) * W, y = rnd(0.0, 0.55) * H;
    let dx = x - rx, dy = y - ry; const l = Math.hypot(dx, dy); dx /= l; dy /= l;
    const speed = rnd(700, 1400) * Math.max(0.6, W / 1500);
    const g = rnd(0.85, 1);
    meteor(x, y, dx * speed, dy * speed, at, rnd(0.35, 0.8), rnd(0.1, 0.17), rnd(0.8, 1.7) * bright, 0.9 * g, 1, 0.92 * g);
  }

  function strike() {
    // A point on the lit face, turned toward us, in the bands' own frame.
    planet.updateMatrixWorld();
    const view = camera.position.clone().normalize();
    for (let k = 0; k < 30; k++) {
      const lat = rnd(-0.9, 0.9), lon = rnd(-Math.PI, Math.PI);
      const local = new Vector3(Math.cos(lat) * Math.cos(lon), Math.sin(lat), Math.cos(lat) * Math.sin(lon));
      const world = local.clone().applyMatrix4(planet.matrixWorld);
      const toCam = camera.position.clone().sub(world).normalize();
      if (world.dot(toCam) < 0.55 || world.dot(planetUniforms.uSun.value) < 0.15) continue;
      const life = 1.2, hitAt = t + life;
      const scr = world.clone().project(camera);
      const ex = (scr.x * 0.5 + 0.5) * W, ey = (-scr.y * 0.5 + 0.5) * H;
      const ang = rnd(-2.6, -1.9), dist = rnd(0.16, 0.26) * H;
      const vx = (-Math.cos(ang) * dist) / life, vy = (-Math.sin(ang) * dist) / life;
      meteor(ex - vx * life, ey - vy * life, vx, vy, t, life, 0.55, 2.2, 1, 0.72, 0.42, 1);
      // Its longitude at the moment it lands, in the frame the shader uses.
      pending.push({ at: hitAt, lq: lon - omega(lat) * hitAt, lat });
      return;
    }
  }

  function events(dt) {
    if (t >= nextSporadic) { shootingStar(t); nextSporadic = t + rnd(1.5, 5); }
    if (t >= nextShower) {
      const n = Math.round(rnd(8, 18));
      for (let i = 0; i < n; i++) shootingStar(t + rnd(0, 3.5), rnd(1, 1.4));
      nextShower = t + rnd(25, 50);
    }
    if (t >= nextImpact) { strike(); nextImpact = t + rnd(28, 55); }
    for (let i = pending.length - 1; i >= 0; i--) {
      if (t >= pending[i].at) {
        planetUniforms.uImpact.value.set(pending[i].lq, pending[i].lat, pending[i].at);
        pending.splice(i, 1);
      }
    }
    if (!cometOn && t >= nextComet) {
      // Far behind the giant (z -7), right to left or back, about 2.5 minutes.
      const rtl = Math.random() < 0.6, y0 = rnd(0.6, 1.9), y1 = y0 - rnd(0.6, 1.8);
      cometOn = { t0: t, dur: rnd(140, 180), x0: rtl ? 7.5 : -9, x1: rtl ? -9 : 7.5, y0, y1 };
    }
    if (cometOn) {
      const u = (t - cometOn.t0) / cometOn.dur;
      if (u >= 1) { cometOn = null; comet.alpha.value = 0; nextComet = t + rnd(90, 180); }
      else {
        comet.head.value.set(cometOn.x0 + (cometOn.x1 - cometOn.x0) * u, cometOn.y0 + (cometOn.y1 - cometOn.y0) * u, -7);
        comet.alpha.value = Math.min(1, u * 8, (1 - u) * 8) * 0.9;
      }
    }
  }

  // ---- The loop -------------------------------------------------------------

  const ringN = new Vector3();
  function draw() {
    planetUniforms.uTime.value = t;
    ringPivot.rotation.x = 0.016 * Math.sin(t * 0.33);
    ringPivot.rotation.z = 0.01 * Math.sin(t * 0.21 + 1.2);
    ringPivot.updateMatrixWorld(true);
    ringN.set(0, 0, 1).transformDirection(rings.matrixWorld);
    planetUniforms.uRingN.value.copy(ringN);
    renderer.render(scene, camera);
  }

  let running = false, last = 0, acc = 0, frames = 0, shown = false;
  function frame(now) {
    if (!running) return;
    requestAnimationFrame(frame);
    const ms = now - last;
    if (ms < 15) return;                 // 60 fps at most, also on 120 Hz screens
    last = now;
    const dt = Math.min(ms / 1000, 0.05);
    t += dt;
    events(dt);
    draw();
    if (!shown) { shown = true; hero.classList.add("hero-live"); }
    // Adapt the resolution to what the machine keeps up with.
    acc += ms; frames++;
    if (frames === 90) {
      const avg = acc / frames;
      if (avg > 26 && pr > minPr) { pr = Math.max(minPr, pr * 0.85); layout(); }
      else if (avg < 18 && pr < maxPr) { pr = Math.min(maxPr, pr * 1.15); layout(); }
      acc = 0; frames = 0;
    }
  }
  function run(on) {
    on = on && !document.hidden && !still;
    if (on === running) return;
    running = on;
    if (on) { last = performance.now(); requestAnimationFrame(frame); }
  }

  new ResizeObserver(layout).observe(hero);
  layout();
  if (still) {
    t = 40;
    draw();
    hero.classList.add("hero-live");
    return;
  }
  let onScreen = true;
  new IntersectionObserver(([e]) => { onScreen = e.isIntersecting; run(onScreen); }).observe(hero);
  document.addEventListener("visibilitychange", () => run(onScreen));
  run(true);
}
