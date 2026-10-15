// The admin's pages: sign in, register the first admin, and the shell.
// Talks to /api/admin/ with fetch; the CSP allows no form posts, so every
// form is sent from here.

(() => {
  const page = document.body.dataset.page;
  const $ = (sel, el = document) => el.querySelector(sel);

  async function api(path, body) {
    const init = body === undefined
      ? {}
      : { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) };
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

  if (page === "shell") {
    const signOut = async (path) => {
      await api(path, {});
      location.assign("/admin/login");
    };
    $("#signout").addEventListener("click", () => signOut("logout"));

    const all = $("#signout-all");
    all.addEventListener("click", () => {
      if (all.dataset.armed) return signOut("signout-all");
      all.dataset.armed = "1";
      all.textContent = "Click again: sign out everyone";
    });

    async function loadAdmins() {
      const { ok, data } = await api("admins");
      if (!ok) return;
      const body = $("#admin-list tbody");
      body.replaceChildren(...data.admins.map((a) => {
        const tr = document.createElement("tr");
        for (const text of [a.email, when(a.created_at), when(a.last_login_at), a.must_change_pw ? "one-time" : "own"]) {
          const td = document.createElement("td");
          td.textContent = text;
          tr.append(td);
        }
        return tr;
      }));
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

    api("me").then(({ ok, data }) => {
      if (!ok) return location.assign("/admin/login");
      $(".who").textContent = data.email;
      $("#admins").hidden = false;
      if (data.mustChange) {
        document.body.classList.add("must-change");
        $("#must-change").hidden = false;
        for (const id of ["#admin-list", "#admin-add", "#everywhere", ".sections"]) $(id).hidden = true;
        return;
      }
      loadAdmins();
    });
  }
})();
