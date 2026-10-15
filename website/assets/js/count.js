// The site's own counter, first-party and cookieless (docs/website-analytics.md,
// "Counting humans"). On load: one request to /c/cache, whose Last-Modified
// the browser's own HTTP cache keeps (the visit counter), then one to
// /c/collect with the path, the referrer's host and that counter.
// count(name) sends an event. Nothing is read from or written to storage.
// Off on other hosts, with ?nocount, and on a page with data-count="off".

(() => {
  const site = document.currentScript?.dataset.site;
  const off = !site || location.hostname !== "ringshadow.com" || new URLSearchParams(location.search).has("nocount") ||
    document.documentElement.dataset.count === "off" || document.body?.dataset.count === "off";

  function referrer() {
    try {
      const host = new URL(document.referrer).host;
      return host === location.host ? "" : host;
    } catch {
      return "";
    }
  }

  function send(extra) {
    const q = new URLSearchParams({ sid: site, h: location.host, p: location.pathname, r: referrer(), ...extra });
    fetch("/c/collect?" + q, { credentials: "omit", keepalive: true }).catch(() => {});
  }

  window.count = (name) => {
    if (!off) send({ e: name });
  };
  if (off) return;

  const utm = new URLSearchParams(location.search);
  const view = () =>
    fetch("/c/cache?sid=" + encodeURIComponent(site), { credentials: "omit" })
      .then((r) => r.json())
      .then((d) => d.ht, () => 1)
      .then((ht) => send({ ht: String(ht || 1), us: utm.get("utm_source") || "", um: utm.get("utm_medium") || "" }));

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", view);
  else view();
})();
