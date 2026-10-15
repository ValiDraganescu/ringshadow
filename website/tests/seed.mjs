// The seed rules as migration 0001 inserts them, for the tests.
import { readFileSync } from "node:fs";

const sql = readFileSync(new URL("../migrations/0001_admin.sql", import.meta.url), "utf8");
const row = /\('((?:[^']|'')*)', '([a-z-]+)', '(contains|prefix|regex)', '((?:[^']|'')*)', (\d+),/g;

export const SEED_RULES = [...sql.matchAll(row)].map((m, i) => ({
  id: i + 1,
  label: m[1].replace(/''/g, "'"),
  class: m[2],
  match: m[3],
  pattern: m[4].replace(/''/g, "'"),
  priority: Number(m[5]),
}));
