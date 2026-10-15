// The admin's pages: sign in, register the first admin, and the shell.
// Talks to /api/admin/ with fetch; the CSP allows no form posts, so every
// form is sent from here.

(() => {
  const page = document.body.dataset.page;
  const $ = (sel, el = document) => el.querySelector(sel);

  async function api(path, body, method) {
    const init = body === undefined
      ? (method ? { method } : {})
      : { method: method || "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) };
    const r = await fetch("/api/admin/" + path, { credentials: "same-origin", ...init });
    let data = {};
    try {
      data = await r.json();
    } catch {
      // an empty or non-JSON answer
    }
    return { ok: r.ok, status: r.status, data };
  }

  function say(form, text, good = false) {
    const m = $(".msg", form);
    m.textContent = text || "";
    m.classList.toggle("good", good);
  }

  const values = (form) => Object.fromEntries(new FormData(form));

  function onSubmit(form, handler) {
    form.addEventListener("submit", async (e) => {
      e.preventDefault();
      const button = $("button[type=submit]", form);
      button.disabled = true;
      try {
        await handler(values(form));
      } catch {
        say(form, "The server didn't answer. Try again.");
      } finally {
        button.disabled = false;
      }
    });
  }

  const when = (iso) => (iso ? iso.slice(0, 16).replace("T", " ") + " UTC" : "never");

  // --- Sign in

  if (page === "login") {
    api("state").then(({ data }) => {
      if (data.hasAdmin === false) $(".first").hidden = false;
    });
    const form = $("#login");
    onSubmit(form, async (v) => {
      const { ok, data } = await api("login", { email: v.email, password: v.password });
      if (ok) location.assign("/admin/");
      else say(form, data.error || "Sign-in failed.");
    });
  }

  // --- The first admin

  if (page === "register") {
    const form = $("#register");
    onSubmit(form, async (v) => {
      if (v.password !== v.again) return say(form, "The two passwords differ.");
      const { ok, data } = await api("register", { code: v.code, email: v.email, password: v.password });
      if (ok) location.assign("/admin/");
      else say(form, data.error || "Registering failed.");
    });
  }

  // --- The shell

  const fmt = (x) => Number(x).toLocaleString("en-US");
  const el = (tag, props = {}, ...kids) => {
    const e = document.createElement(tag);
    for (const [k, v] of Object.entries(props)) {
      if (k === "class") e.className = v;
      else if (k === "data") Object.assign(e.dataset, v);
      else e[k] = v;
    }
    e.append(...kids);
    return e;
  };
  const row = (...cells) => el("tr", {}, ...cells.map((c) => (c instanceof Node ? el("td", {}, c) : el("td", { textContent: c }))));

  // A two-click button: the first click arms it, the second acts.
  function twoClick(button, armedText, act) {
    button.addEventListener("click", () => {
      if (button.dataset.armed) return act();
      button.dataset.armed = "1";
      button.textContent = armedText;
    });
  }

  if (page === "shell") {
    const signOut = async (path) => {
      await api(path, {});
      location.assign("/admin/login");
    };
    $("#signout").addEventListener("click", () => signOut("logout"));
    twoClick($("#signout-all"), "Click again: sign out everyone", () => signOut("signout-all"));

    // --- Admins

    async function loadAdmins() {
      const { ok, data } = await api("admins");
      if (!ok) return;
      $("#admin-list tbody").replaceChildren(...data.admins.map((a) =>
        row(a.email, when(a.created_at), when(a.last_login_at), a.must_change_pw ? "one-time" : "own")));
    }

    const add = $("#add");
    onSubmit(add, async (v) => {
      $(".otp").hidden = true;
      const { ok, data } = await api("admins", { email: v.email });
      if (!ok) return say(add, data.error || "Adding failed.");
      say(add, "");
      add.reset();
      $(".otp-email").textContent = data.email;
      $(".otp-pw").textContent = data.password;
      $(".otp").hidden = false;
      loadAdmins();
    });

    const pw = $("#password");
    onSubmit(pw, async (v) => {
      if (v.next !== v.again) return say(pw, "The two new passwords differ.");
      const { ok, data } = await api("password", { current: v.current, next: v.next });
      if (!ok) return say(pw, data.error || "Changing the password failed.");
      pw.reset();
      say(pw, "Password changed. Your other sessions are signed out.", true);
      if (document.body.classList.contains("must-change")) setTimeout(() => location.reload(), 900);
    });

    // --- Agents

    let range = { range: "7d" };
    const query = () => new URLSearchParams(range).toString();

    for (const b of document.querySelectorAll(".range button")) {
      b.addEventListener("click", () => {
        for (const o of document.querySelectorAll(".range button")) o.setAttribute("aria-pressed", String(o === b));
        $("#custom-range").hidden = b.dataset.range !== "custom";
        if (b.dataset.range !== "custom") {
          range = { range: b.dataset.range };
          loadAgents();
        }
      });
    }
    onSubmit($("#custom-range"), async (v) => {
      range = { range: "custom", from: v.from, to: v.to };
      await loadAgents();
    });

    const CLASS_ORDER = ["ai-crawler", "ai-assistant", "search", "monitor", "other-bot", "unlabelled"];
    const SVG = "http://www.w3.org/2000/svg";
    const svgEl = (tag, attrs) => {
      const e = document.createElementNS(SVG, tag);
      for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
      return e;
    };

    // Stacked bars, one per hour or day of the range, by class.
    function drawChart(data) {
      const svg = $("#agents-chart svg");
      const step = data.range.bucket === "hour" ? 3_600_000 : 86_400_000;
      const t0 = Math.floor(Date.parse(data.range.from) / step) * step;
      const t1 = Date.parse(data.range.to);
      const bySlot = new Map(data.series.map((s) => [Date.parse(s.t.replace(" ", "T") + "Z"), s.byClass]));
      const slots = [];
      for (let t = t0; t < t1; t += step) slots.push([t, bySlot.get(t) || {}]);
      const totals = slots.map(([, c]) => Object.values(c).reduce((a, b) => a + b, 0));
      const max = Math.max(1, ...totals);
      const W = 800, H = 220, left = 44, bottom = 22, top = 8;
      const bw = (W - left) / Math.max(1, slots.length);
      const kids = [
        svgEl("line", { x1: left, x2: W, y1: H - bottom, y2: H - bottom, class: "axis" }),
        Object.assign(svgEl("text", { x: left - 6, y: top + 10, class: "tick", "text-anchor": "end" }), { textContent: fmt(max) }),
        Object.assign(svgEl("text", { x: left - 6, y: H - bottom, class: "tick", "text-anchor": "end" }), { textContent: "0" }),
      ];
      slots.forEach(([t, byClass], i) => {
        let y = H - bottom;
        for (const k of CLASS_ORDER) {
          const v = byClass[k] || 0;
          if (!v) continue;
          const h = (v / max) * (H - bottom - top);
          y -= h;
          const r = svgEl("rect", { x: left + i * bw + 1, y, width: Math.max(1, bw - 2), height: h, class: "k-" + k });
          r.append(Object.assign(svgEl("title", {}), { textContent: `${new Date(t).toISOString().slice(0, step < 86_400_000 ? 13 : 10).replace("T", " ")}${step < 86_400_000 ? ":00" : ""} · ${k}: ${fmt(v)}` }));
          kids.push(r);
        }
      });
      const label = (t) => new Date(t).toISOString().slice(0, step < 86_400_000 ? 13 : 10).replace("T", " ") + (step < 86_400_000 ? ":00" : "");
      if (slots.length) {
        kids.push(Object.assign(svgEl("text", { x: left, y: H - 6, class: "tick" }), { textContent: label(slots[0][0]) }));
        kids.push(Object.assign(svgEl("text", { x: W, y: H - 6, class: "tick", "text-anchor": "end" }), { textContent: label(slots.at(-1)[0]) }));
      }
      svg.replaceChildren(...kids);
      const present = CLASS_ORDER.filter((k) => data.series.some((s) => s.byClass[k]));
      $("#agents-chart .legend").replaceChildren(...present.map((k) => el("span", { class: "key" }, el("i", { class: "k-" + k }), k)));
    }

    function labelAs(ua) {
      const form = $("#rule");
      form.reset();
      form.elements.id.value = "";
      form.elements.pattern.value = ua;
      $("#rule-title").textContent = "Label a string";
      $("#rule-cancel").hidden = false;
      say(form, "Cut the pattern down to the part that names the agent.");
      $("#rule-part").scrollIntoView({ behavior: "smooth", block: "start" });
      form.elements.pattern.focus();
    }

    async function loadAgents() {
      const summary = $("#agents .summary");
      summary.textContent = "Loading…";
      const { ok, data } = await api("stats/agents?" + query() + ($("#probes-only").checked ? "&probes=1" : ""));
      if (!ok) {
        summary.textContent = data.error || "The stats didn't load.";
        return;
      }
      summary.textContent = `${fmt(data.total)} agent fetches (${fmt(data.probes)} probes) · ${fmt(data.browsers)} browser page fetches` +
        (data.dropped ? ` · ${fmt(data.dropped)} not counted (the rate cap)` : "") +
        ` · ${data.range.from.slice(0, 16).replace("T", " ")} to ${data.range.to.slice(0, 16).replace("T", " ")} UTC`;
      drawChart(data);
      $("#agents-labels tbody").replaceChildren(...data.labels.map((l) => {
        const paths = el("details", {}, el("summary", { textContent: `${l.paths.length} path${l.paths.length === 1 ? "" : "s"}` }),
          el("ol", {}, ...l.paths.map((p) => el("li", {}, el("code", { textContent: p.path }),
            ...(p.status >= 400 ? [" ", el("span", { class: "status", textContent: String(p.status) })] : []), " " + fmt(p.hits)))));
        return row(l.label, l.class, fmt(l.hits), fmt(l.probes), fmt(l.strings), paths);
      }));
      $("#agents-unlabelled tbody").replaceChildren(...(data.unlabelled.length
        ? data.unlabelled.map((u) => {
          const b = el("button", { type: "button", class: "small", textContent: "Label as" });
          b.addEventListener("click", () => labelAs(u.ua));
          return row(el("code", { textContent: u.ua || "(no User-Agent)" }), fmt(u.hits), fmt(u.probes), b);
        })
        : [row("Every string has a label.", "", "", "")]));
    }

    $("#probes-only").addEventListener("change", () => loadAgents());

    async function loadRules() {
      const { ok, data } = await api("rules");
      if (!ok) return;
      $("#label-names").replaceChildren(...[...new Set(data.rules.map((r) => r.label))].map((l) => el("option", { value: l })));
      $("#rules tbody").replaceChildren(...data.rules.map((r) => {
        const edit = el("button", { type: "button", class: "small quiet", textContent: "Edit" });
        edit.addEventListener("click", () => {
          const form = $("#rule");
          for (const k of ["id", "label", "class", "match", "pattern", "priority", "note"]) form.elements[k].value = r[k] ?? "";
          $("#rule-title").textContent = "Change a rule";
          $("#rule-cancel").hidden = false;
          say(form, "");
          $("#rule-part").scrollIntoView({ behavior: "smooth", block: "start" });
        });
        const del = el("button", { type: "button", class: "small quiet", textContent: "Delete" });
        twoClick(del, "Delete?", async () => {
          await api("rules/" + r.id, undefined, "DELETE");
          await Promise.all([loadRules(), loadAgents()]);
        });
        return row(r.label, r.class, r.match, el("code", { textContent: r.pattern }), String(r.priority), el("span", { class: "acts" }, edit, del));
      }));
    }

    const ruleForm = $("#rule");
    const resetRule = () => {
      ruleForm.reset();
      ruleForm.elements.id.value = "";
      $("#rule-title").textContent = "A new rule";
      $("#rule-cancel").hidden = true;
    };
    $("#rule-cancel").addEventListener("click", () => {
      resetRule();
      say(ruleForm, "");
    });
    onSubmit(ruleForm, async (v) => {
      const body = { label: v.label, class: v.class, match: v.match, pattern: v.pattern, priority: Number(v.priority || 100), note: v.note };
      const { ok, data } = v.id ? await api("rules/" + v.id, body, "PUT") : await api("rules", body);
      if (!ok) return say(ruleForm, data.error || "Saving failed.");
      resetRule();
      say(ruleForm, `Saved: ${data.rule.match} "${data.rule.pattern}" is ${data.rule.label} (${data.rule.class}).`, true);
      await Promise.all([loadRules(), loadAgents()]);
    });

    const testForm = $("#test");
    onSubmit(testForm, async (v) => {
      const { ok, data } = await api("rules/test", { ua: v.ua });
      if (!ok) return say(testForm, data.error || "Testing failed.");
      say(testForm, data.rule
        ? `${data.label} (${data.class}), by rule ${data.rule.match} "${data.rule.pattern}", priority ${data.rule.priority}.`
        : "unlabelled: no rule matches.", true);
    });

    // --- Sections

    const VIEWS = ["agents", "admins"];
    const loaded = new Set();
    function show() {
      const want = location.hash.slice(1);
      const id = VIEWS.includes(want) ? want : "agents";
      for (const v of VIEWS) $("#" + v).hidden = v !== id;
      for (const a of document.querySelectorAll(".sections a")) {
        if (a.getAttribute("href") === "#" + id) a.setAttribute("aria-current", "page");
        else a.removeAttribute("aria-current");
      }
      if (loaded.has(id)) return;
      loaded.add(id);
      if (id === "agents") Promise.all([loadAgents(), loadRules()]);
      if (id === "admins") loadAdmins();
    }

    api("me").then(({ ok, data }) => {
      if (!ok) return location.assign("/admin/login");
      $(".who").textContent = data.email;
      if (data.mustChange) {
        document.body.classList.add("must-change");
        $("#must-change").hidden = false;
        $("#admins").hidden = false;
        for (const sel of ["#admin-list", "#admin-add", "#everywhere", ".sections"]) $(sel).hidden = true;
        return;
      }
      window.addEventListener("hashchange", show);
      show();
    });
  }
})();
