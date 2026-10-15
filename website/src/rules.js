// The agent rules in D1 (agent_labels): loaded for labelling, cached in the
// isolate for 60 s so a new rule reaches new hits within a minute; and the
// admin's rules API (list, add, change, delete, test).

import { CLASSES, compileRules, matchRule } from "./agents.js";

const CACHE_MS = 60_000;
const MATCHES = ["contains", "prefix", "regex"];
let cache = { at: 0, compiled: null };

export async function loadRules(env, now = Date.now()) {
  if (cache.compiled && now - cache.at < CACHE_MS) return cache.compiled;
  try {
    const { results } = await env.ADMIN.prepare("SELECT id, label, class, match, pattern, priority FROM agent_labels").all();
    cache = { at: now, compiled: compileRules(results) };
  } catch (err) {
    console.error("rules failed to load", err);
    if (!cache.compiled) return compileRules([]);
  }
  return cache.compiled;
}

// After a change in this isolate; other isolates catch up within a minute.
export function forgetRules() {
  cache.at = 0;
}

// Why a rule can't be saved, or null.
export function ruleProblem(r) {
  if (!r || typeof r !== "object") return "Bad request.";
  const label = String(r.label || "").trim();
  const pattern = String(r.pattern || "");
  if (!label || label.length > 80) return "A label needs 1 to 80 characters.";
  if (label.toLowerCase() === "unlabelled") return "\"unlabelled\" is what no rule means; pick another label.";
  if (!CLASSES.includes(r.class)) return "Pick a class: " + CLASSES.join(", ") + ".";
  if (!MATCHES.includes(r.match)) return "Pick a match: contains, prefix or regex.";
  if (!pattern.trim() || pattern.length > 300) return "A pattern needs 1 to 300 characters.";
  if (r.match === "regex") {
    try {
      new RegExp(pattern, "i");
    } catch {
      return "That regex doesn't compile.";
    }
  }
  const priority = Number(r.priority ?? 100);
  if (!Number.isInteger(priority) || priority < 0 || priority > 100_000) return "Priority is a whole number from 0 to 100,000.";
  return null;
}

function clean(r) {
  return {
    label: String(r.label).trim(),
    class: r.class,
    match: r.match,
    pattern: String(r.pattern),
    priority: Number(r.priority ?? 100),
    note: r.note ? String(r.note).slice(0, 300) : null,
  };
}

const ruleColumns = "id, label, class, match, pattern, priority, note, created_by, created_at";

export async function listRules(env) {
  const { results } = await env.ADMIN.prepare(`SELECT ${ruleColumns} FROM agent_labels ORDER BY priority, label, id`).all();
  return results;
}

export async function addRule(env, body, adminId) {
  const problem = ruleProblem(body);
  if (problem) return { status: 400, body: { error: problem } };
  const r = clean(body);
  const row = await env.ADMIN.prepare(
    `INSERT INTO agent_labels (label, class, match, pattern, priority, note, created_by, created_at)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?) RETURNING ${ruleColumns}`,
  ).bind(r.label, r.class, r.match, r.pattern, r.priority, r.note, adminId, new Date().toISOString()).first();
  forgetRules();
  return { status: 201, body: { rule: row } };
}

export async function changeRule(env, id, body) {
  const problem = ruleProblem(body);
  if (problem) return { status: 400, body: { error: problem } };
  const r = clean(body);
  const row = await env.ADMIN.prepare(
    `UPDATE agent_labels SET label = ?, class = ?, match = ?, pattern = ?, priority = ?, note = ?
      WHERE id = ? RETURNING ${ruleColumns}`,
  ).bind(r.label, r.class, r.match, r.pattern, r.priority, r.note, id).first();
  if (!row) return { status: 404, body: { error: "No such rule." } };
  forgetRules();
  return { status: 200, body: { rule: row } };
}

export async function deleteRule(env, id) {
  const r = await env.ADMIN.prepare("DELETE FROM agent_labels WHERE id = ?").bind(id).run();
  if (!r.meta.changes) return { status: 404, body: { error: "No such rule." } };
  forgetRules();
  return { status: 200, body: { ok: true } };
}

// What the current rules make of a string, and which rule decided.
export async function testRules(env, ua) {
  forgetRules();
  const rule = matchRule(String(ua || ""), await loadRules(env));
  return rule
    ? { label: rule.label, class: rule.class, rule: { id: rule.id, match: rule.match, pattern: rule.pattern, priority: rule.priority } }
    : { label: "unlabelled", class: "unlabelled", rule: null };
}
