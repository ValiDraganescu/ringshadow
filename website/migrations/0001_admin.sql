-- The admin's own tables (docs/website-analytics.md, "D1: the admin's own tables").
-- Apply: cd website && pnpm dlx wrangler@4.151.0 d1 migrations apply ringshadow-admin --remote

CREATE TABLE admins (
  id INTEGER PRIMARY KEY,
  email TEXT UNIQUE NOT NULL,
  pw_hash TEXT NOT NULL,
  pw_salt TEXT NOT NULL,
  pw_iters INTEGER NOT NULL,
  must_change_pw INTEGER NOT NULL DEFAULT 0,  -- 1 for a one-time password set by another admin
  created_at TEXT NOT NULL,
  last_login_at TEXT);

-- id is the SHA-256 of the cookie's 32 random bytes, so the table alone opens nothing.
CREATE TABLE sessions (
  id TEXT PRIMARY KEY,
  admin_id INTEGER NOT NULL REFERENCES admins(id),
  created_at TEXT NOT NULL,
  expires_at TEXT NOT NULL,
  ua TEXT);
CREATE INDEX sessions_admin ON sessions(admin_id);

CREATE TABLE login_attempts (
  key TEXT PRIMARY KEY,            -- email or /24 of the IP (/64 for IPv6), hashed
  fails INTEGER NOT NULL,
  locked_until TEXT);

CREATE TABLE agent_labels (
  id INTEGER PRIMARY KEY,
  label TEXT NOT NULL,             -- "Grok", "ClaudeBot", "Googlebot"
  class TEXT NOT NULL,             -- ai-crawler, ai-assistant, search, monitor, other-bot, browser
  match TEXT NOT NULL,             -- contains | prefix | regex
  pattern TEXT NOT NULL,
  priority INTEGER NOT NULL DEFAULT 100,   -- lower wins when several match
  note TEXT,
  created_by INTEGER,
  created_at TEXT NOT NULL);

CREATE TABLE rollups_day (
  day TEXT NOT NULL,
  kind TEXT NOT NULL,
  dim TEXT NOT NULL,
  key TEXT NOT NULL,
  hits INTEGER NOT NULL,
  visitors INTEGER,
  PRIMARY KEY (day, kind, dim, key));

-- The seed rules: the same list as SEED_RULES in src/agents.js.
INSERT INTO agent_labels (label, class, match, pattern, priority, note, created_at) VALUES
  ('OpenAI', 'ai-crawler', 'contains', 'GPTBot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('OpenAI', 'ai-crawler', 'contains', 'OAI-SearchBot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('ChatGPT (for a user)', 'ai-assistant', 'contains', 'ChatGPT-User', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Anthropic', 'ai-crawler', 'contains', 'ClaudeBot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Anthropic', 'ai-crawler', 'contains', 'Claude-SearchBot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Anthropic', 'ai-crawler', 'contains', 'anthropic-ai', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Claude (for a user)', 'ai-assistant', 'contains', 'Claude-User', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Perplexity', 'ai-crawler', 'contains', 'PerplexityBot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Perplexity', 'ai-assistant', 'contains', 'Perplexity-User', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Grok', 'ai-crawler', 'contains', 'xAI', 100, 'seed; check against real strings', '2026-10-15T00:00:00Z'),
  ('Grok', 'ai-crawler', 'contains', 'Grok', 100, 'seed; check against real strings', '2026-10-15T00:00:00Z'),
  ('Google', 'search', 'contains', 'Googlebot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Google', 'search', 'contains', 'Google-Extended', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Google', 'search', 'contains', 'GoogleOther', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Bing', 'search', 'contains', 'bingbot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Apple', 'search', 'contains', 'Applebot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Apple', 'search', 'contains', 'Applebot-Extended', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Meta', 'ai-crawler', 'contains', 'meta-externalagent', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Meta', 'ai-crawler', 'contains', 'FacebookBot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Amazon', 'ai-crawler', 'contains', 'Amazonbot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('ByteDance', 'ai-crawler', 'contains', 'Bytespider', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Common Crawl', 'ai-crawler', 'contains', 'CCBot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Feed readers', 'other-bot', 'contains', 'Feedly', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Feed readers', 'other-bot', 'contains', 'NetNewsWire', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Feed readers', 'other-bot', 'contains', 'Miniflux', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Feed readers', 'other-bot', 'contains', 'Feedbin', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Feed readers', 'other-bot', 'contains', 'Inoreader', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Feed readers', 'other-bot', 'contains', 'NewsBlur', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Feed readers', 'other-bot', 'contains', 'FreshRSS', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Monitors', 'monitor', 'contains', 'UptimeRobot', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Monitors', 'monitor', 'contains', 'Pingdom', 100, 'seed', '2026-10-15T00:00:00Z'),
  ('Browsers', 'browser', 'regex', '^(?!.*(bot|crawl|spider))Mozilla/5\.0 .*(Chrome|Safari|Firefox|Edg)/', 1000, 'seed', '2026-10-15T00:00:00Z');
