// The site's Worker: it stands in front of the static assets (this folder).
// For now it hands every request to the assets unchanged; see
// docs/website-analytics.md for what it grows into.

export default {
  async fetch(request, env) {
    return env.ASSETS.fetch(request);
  },
};
