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

    // --- The range, shared by every view but Overview and Admins

    let range = { range: "7d" };
    const query = () => new URLSearchParams(range).toString();
    const span = (r) => `${r.from.slice(0, 16).replace("T", " ")} to ${r.to.slice(0, 16).replace("T", " ")} UTC`;

    for (const b of document.querySelectorAll(".range button")) {
      b.addEventListener("click", () => {
        for (const o of document.querySelectorAll(".range button")) o.setAttribute("aria-pressed", String(o === b));
        $("#custom-range").hidden = b.dataset.range !== "custom";
        if (b.dataset.range !== "custom") {
          range = { range: b.dataset.range };
          reload();
        }
      });
    }
    onSubmit($("#custom-range"), async (v) => {
      range = { range: "custom", from: v.from, to: v.to };
      await reload();
    });

    // --- People

    const SVG = "http://www.w3.org/2000/svg";
    const svgEl = (tag, attrs) => {
      const e = document.createElementNS(SVG, tag);
      for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
      return e;
    };

    const pct = (a, b) => (b ? Math.round((100 * a) / b) + "%" : "–");
    const fill = (sel, list, withBounce = false) =>
      $(sel + " tbody").replaceChildren(...(list.length
        ? list.map((r) => withBounce
          ? row(el("code", { textContent: r.key || "(none)" }), fmt(r.views), fmt(r.visitors), pct(r.bounces, r.visitors))
          : row(r.key || "(direct or none)", fmt(r.views), fmt(r.visitors)))
        : [row("Nothing yet.", "", "")]));

    // Views as bars, visitors as narrower bars inside them, one per bucket.
    function drawPeople(svg, series, bucket) {
      const W = 800, H = 220, left = 44, bottom = 22, top = 8;
      const max = Math.max(1, ...series.map((s) => s.views));
      const bw = (W - left) / Math.max(1, series.length);
      const y = (v) => H - bottom - (v / max) * (H - bottom - top);
      const label = (t) => (bucket === "hour" ? t.slice(0, 13) + ":00" : t.slice(0, 10));
      const kids = [
        svgEl("line", { x1: left, x2: W, y1: H - bottom, y2: H - bottom, class: "axis" }),
        Object.assign(svgEl("text", { x: left - 6, y: top + 10, class: "tick", "text-anchor": "end" }), { textContent: fmt(max) }),
        Object.assign(svgEl("text", { x: left - 6, y: H - bottom, class: "tick", "text-anchor": "end" }), { textContent: "0" }),
      ];
      series.forEach((s, i) => {
        const x = left + i * bw;
        const g = svgEl("g", {});
        g.append(Object.assign(svgEl("title", {}), { textContent: `${label(s.t)} · ${fmt(s.views)} views · ${fmt(s.visitors)} visitors` }));
        if (s.views) g.append(svgEl("rect", { x: x + 1, y: y(s.views), width: Math.max(1, bw - 2), height: H - bottom - y(s.views), class: "k-views" }));
        if (s.visitors) g.append(svgEl("rect", { x: x + bw * 0.25, y: y(s.visitors), width: Math.max(1, bw * 0.5), height: H - bottom - y(s.visitors), class: "k-visitors" }));
        kids.push(g);
      });
      if (series.length) {
        kids.push(Object.assign(svgEl("text", { x: left, y: H - 6, class: "tick" }), { textContent: label(series[0].t) }));
        kids.push(Object.assign(svgEl("text", { x: W, y: H - 6, class: "tick", "text-anchor": "end" }), { textContent: label(series.at(-1).t) }));
      }
      svg.replaceChildren(...kids);
    }

    async function people(view, summarySel) {
      const summary = summarySel && $(summarySel);
      if (summary) summary.textContent = "Loading…";
      const { ok, data } = await api(`stats/${view}?` + query());
      if (!ok) {
        if (summary) summary.textContent = data.error || "The stats didn't load.";
        return null;
      }
      if (summary) summary.textContent = span(data.range);
      return data;
    }

    async function loadOverview() {
      const { ok, data } = await api("stats/overview");
      if (!ok) return;
      for (const k of ["today", "week", "month"]) {
        $(`.t-${k}-visitors`).textContent = fmt(data[k].visitors);
        $(`.t-${k}-views`).textContent = fmt(data[k].views);
      }
      drawPeople($("#overview-chart svg"), data.series, "day");
      fill("#ov-pages", data.pages);
      fill("#ov-referrers", data.referrers);
      fill("#ov-countries", data.countries);
    }

    async function loadPages() {
      const data = await people("pages", "#pages .summary");
      if (data) fill("#pages-table", data.pages, true);
    }

    async function loadSources() {
      const data = await people("sources", "#sources .summary");
      if (!data) return;
      fill("#src-referrers", data.referrers);
      fill("#src-utm-source", data.utmSources);
      fill("#src-utm-medium", data.utmMedia);
    }

    async function loadVisitors() {
      const data = await people("visitors", "#visitors .summary");
      if (!data) return;
      fill("#vis-countries", data.countries);
      fill("#vis-devices", data.devices);
      fill("#vis-browsers", data.browsers);
    }

    async function loadEvents() {
      const data = await people("events", "#events .summary");
      if (!data) return;
      $("#events-list").replaceChildren(...(data.events.length ? data.events.map((e) => {
        const svg = svgEl("svg", { viewBox: "0 0 800 120", role: "img", "aria-label": `${e.event} over time` });
        const max = Math.max(1, ...e.series.map((s) => s.count));
        const bw = 800 / Math.max(1, e.series.length);
        svg.append(...e.series.filter((s) => s.count).map((s) => {
          const i = e.series.indexOf(s), h = (s.count / max) * 110;
          const r = svgEl("rect", { x: i * bw + 1, y: 120 - h, width: Math.max(1, bw - 2), height: h, class: "k-views" });
          r.append(Object.assign(svgEl("title", {}), { textContent: `${s.t} · ${fmt(s.count)}` }));
          return r;
        }));
        const pages = el("table", { class: "list" }, el("thead", {}, el("tr", {}, el("th", { textContent: "Page" }), el("th", { textContent: "Count" }))),
          el("tbody", {}, ...e.pages.map((p) => row(el("code", { textContent: p.path }), fmt(p.count)))));
        return el("div", { class: "part" }, el("h3", { textContent: `${e.event} · ${fmt(e.total)}` }), el("figure", { class: "chart" }, svg), pages);
      }) : [el("p", { textContent: "No events in this range." })]));
    }

    // --- Requests

    const reqForm = $("#requests-filter");
    let reqPage = 0;
    const STATUS_ORDER = ["2xx", "3xx", "4xx", "5xx", "unknown"];

    function drawStatus(svg, series, bucket) {
      const W = 800, H = 220, left = 44, bottom = 22, top = 8;
      const totals = series.map((s) => Object.values(s.byClass).reduce((a, b) => a + b, 0));
      const max = Math.max(1, ...totals);
      const bw = (W - left) / Math.max(1, series.length);
      const label = (t) => (bucket === "hour" ? t.slice(0, 13) + ":00" : t.slice(0, 10));
      const kids = [
        svgEl("line", { x1: left, x2: W, y1: H - bottom, y2: H - bottom, class: "axis" }),
        Object.assign(svgEl("text", { x: left - 6, y: top + 10, class: "tick", "text-anchor": "end" }), { textContent: fmt(max) }),
        Object.assign(svgEl("text", { x: left - 6, y: H - bottom, class: "tick", "text-anchor": "end" }), { textContent: "0" }),
      ];
      series.forEach((s, i) => {
        let y = H - bottom;
        for (const k of STATUS_ORDER) {
          const v = s.byClass[k] || 0;
          if (!v) continue;
          const h = (v / max) * (H - bottom - top);
          y -= h;
          const r = svgEl("rect", { x: left + i * bw + 1, y, width: Math.max(1, bw - 2), height: h, class: "k-" + k });
          r.append(Object.assign(svgEl("title", {}), { textContent: `${label(s.t)} · ${k}: ${fmt(v)}` }));
          kids.push(r);
        }
      });
      if (series.length) {
        kids.push(Object.assign(svgEl("text", { x: left, y: H - 6, class: "tick" }), { textContent: label(series[0].t) }));
        kids.push(Object.assign(svgEl("text", { x: W, y: H - 6, class: "tick", "text-anchor": "end" }), { textContent: label(series.at(-1).t) }));
      }
      svg.replaceChildren(...kids);
      const present = STATUS_ORDER.filter((k) => series.some((s) => s.byClass[k]));
      $("#requests-chart .legend").replaceChildren(...present.map((k) => el("span", { class: "key" }, el("i", { class: "k-" + k }), k)));
    }

    async function loadRequests() {
      const v = values(reqForm);
      const q = new URLSearchParams(Object.fromEntries(Object.entries(v).filter(([, x]) => x)));
      q.set("page", String(reqPage));
      const summary = $("#requests .summary");
      summary.textContent = "Loading…";
      const { ok, data } = await api("requests?" + q);
      if (!ok) {
        summary.textContent = data.error || "The log didn't load.";
        return;
      }
      summary.textContent = `${span(data.range)} · page ${data.page + 1}`;
      if (data.series) drawStatus($("#requests-chart svg"), data.series, data.range.bucket);
      $("#requests-log tbody").replaceChildren(...(data.rows.length ? data.rows.map((r) => {
        const st = el("span", { class: "status s" + String(r.status).charAt(0), textContent: r.status ? String(r.status) : "?" });
        const flags = [r.admin ? "admin" : "", r.sample > 1 ? "×" + r.sample : "", r.kind === "server" ? "counted" : ""].filter(Boolean).join(" ");
        return row(r.t.slice(5), r.method || "–", el("code", { textContent: r.path }), st, fmt(r.bytes), fmt(r.ms), r.source || "–",
          `${r.label} (${r.class})`, el("code", { class: "ua", textContent: r.ua, title: r.ua }), flags);
      }) : [row("No requests match.", "", "", "", "", "", "", "", "", "")]));
      $("#requests-newer").disabled = data.page === 0;
      $("#requests-older").disabled = !data.more;
    }
    reqForm.addEventListener("submit", (e) => {
      e.preventDefault();
      reqPage = 0;
      loadRequests();
    });
    $("#requests-newer").addEventListener("click", () => {
      reqPage = Math.max(0, reqPage - 1);
      loadRequests();
    });
    $("#requests-older").addEventListener("click", () => {
      reqPage += 1;
      loadRequests();
    });

    // --- History

    const historyForm = $("#history-range");
    async function loadHistory() {
      const v = values(historyForm);
      const q = new URLSearchParams(Object.fromEntries(Object.entries(v).filter(([, x]) => x)));
      const summary = $("#history .summary");
      summary.textContent = "Loading…";
      const { ok, data } = await api("history?" + q);
      if (!ok) {
        summary.textContent = data.error || "The history didn't load.";
        return;
      }
      summary.textContent = `${data.from} to ${data.to} · ${fmt(data.days.length)} day${data.days.length === 1 ? "" : "s"} rolled up`;
      drawPeople($("#history-chart svg"), data.days.map((d) => ({ t: d.day, views: d.views, visitors: d.visitors })), "day");
      $("#history-days tbody").replaceChildren(...(data.days.length
        ? [...data.days].reverse().map((d) => row(d.day, fmt(d.views), fmt(d.visitors), fmt(d.agents), fmt(d.events), fmt(d.requests), fmt(d.errors)))
        : [row("No rollups yet: the first runs at 02:10 UTC.", "", "", "", "", "", "")]));
      const tops = (sel, list = [], third = (r) => fmt(r.visitors ?? 0)) =>
        $(sel + " tbody").replaceChildren(...(list.length ? list.map((r) => row(r.key || "(none)", fmt(r.hits), third(r))) : [row("Nothing yet.", "", "")]));
      tops("#hist-path", data.top["page.path"]);
      tops("#hist-referrer", data.top["page.referrer"]);
      tops("#hist-country", data.top["page.country"]);
      tops("#hist-label", data.top["server.label"], () => "");
      tops("#hist-event", data.top["event.event"], () => "");
    }
    historyForm.addEventListener("submit", (e) => {
      e.preventDefault();
      loadHistory();
    });

    // --- Agents

    const CLASS_ORDER = ["ai-crawler", "ai-assistant", "search", "monitor", "other-bot", "unlabelled"];

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
            " ", el("span", { class: "status s" + String(p.status).charAt(0), textContent: p.status ? String(p.status) : "?",
              title: p.status ? "" : "fetched before the status was recorded (09:28 UTC, 15 October 2026)" }), " " + fmt(p.hits)))));
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

    const VIEWS = {
      overview: loadOverview,
      pages: loadPages,
      sources: loadSources,
      visitors: loadVisitors,
      events: loadEvents,
      agents: () => Promise.all([loadAgents(), loadRules()]),
      requests: () => Promise.all([loadRequests(), loadRules()]),
      history: loadHistory,
      admins: loadAdmins,
    };
    const RANGED = ["pages", "sources", "visitors", "events", "agents"];
    let loaded = new Set();
    const current = () => (location.hash.slice(1) in VIEWS ? location.hash.slice(1) : "overview");
    function show() {
      const id = current();
      for (const v of Object.keys(VIEWS)) $("#" + v).hidden = v !== id;
      $("#range-bar").hidden = !RANGED.includes(id);
      for (const a of document.querySelectorAll(".sections a")) {
        if (a.getAttribute("href") === "#" + id) a.setAttribute("aria-current", "page");
        else a.removeAttribute("aria-current");
      }
      if (loaded.has(id)) return;
      loaded.add(id);
      VIEWS[id]();
    }
    // A new range: the ranged views load again when next shown.
    function reload() {
      loaded = new Set([...loaded].filter((v) => !RANGED.includes(v)));
      show();
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
