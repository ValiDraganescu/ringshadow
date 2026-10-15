// The admin's door: password hashing, the password rule, sessions' tokens,
// the lockout keys. Pure WebCrypto, so the tests run it under Node too.
// See docs/website-analytics.md, "Access".

// PBKDF2-SHA-256 iterations for new hashes, set to fit the free plan's 10 ms
// of CPU with margin. Measured on the live Worker 2026-10-15 (cpuTime from
// wrangler tail): 100,000 cost 17 to 32 ms a login, over the limit; a
// password change hashes twice, so it gets the tighter budget. The count is
// stored with each hash, so changing it later only affects new passwords.
export const PW_ITERS = 10_000;
export const MIN_PW = 12;
export const MAX_PW = 256;
export const SESSION_DAYS = 30;
export const LOCK_FAILS = 5;
export const LOCK_MINUTES = 15;
export const COOKIE = "admin_session";

const enc = new TextEncoder();

export function toHex(bytes) {
  return [...new Uint8Array(bytes)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

export function fromHex(hex) {
  const out = new Uint8Array(hex.length / 2);
  for (let i = 0; i < out.length; i++) out[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
  return out;
}

export function toB64url(bytes) {
  return btoa(String.fromCharCode(...new Uint8Array(bytes))).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

export function randomBytes(n) {
  return crypto.getRandomValues(new Uint8Array(n));
}

export async function sha256Hex(text) {
  return toHex(await crypto.subtle.digest("SHA-256", enc.encode(text)));
}

// Equal in time that doesn't depend on where two byte strings differ.
export function sameBytes(a, b) {
  if (a.length !== b.length) return false;
  let d = 0;
  for (let i = 0; i < a.length; i++) d |= a[i] ^ b[i];
  return d === 0;
}

export async function pbkdf2(password, saltBytes, iterations) {
  const key = await crypto.subtle.importKey("raw", enc.encode(password), "PBKDF2", false, ["deriveBits"]);
  const bits = await crypto.subtle.deriveBits({ name: "PBKDF2", hash: "SHA-256", salt: saltBytes, iterations }, key, 256);
  return new Uint8Array(bits);
}

// A new hash: { pw_hash, pw_salt, pw_iters }, hex strings and the count.
export async function hashPassword(password, iterations = PW_ITERS) {
  const salt = randomBytes(16);
  const hash = await pbkdf2(password, salt, iterations);
  return { pw_hash: toHex(hash), pw_salt: toHex(salt), pw_iters: iterations };
}

export async function verifyPassword(password, row) {
  const hash = await pbkdf2(password, fromHex(row.pw_salt), row.pw_iters);
  return sameBytes(hash, fromHex(row.pw_hash));
}

export function normalEmail(email) {
  return String(email || "").trim().toLowerCase();
}

export function validEmail(email) {
  return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email) && email.length <= 254;
}

// Why a password is refused, or null: at least 12 characters, at most 256,
// and not the email or its name part.
export function passwordProblem(password, email) {
  const pw = String(password || "");
  const n = [...pw].length;
  if (n < MIN_PW) return `The password needs at least ${MIN_PW} characters.`;
  if (n > MAX_PW) return `The password can have at most ${MAX_PW} characters.`;
  const low = pw.toLowerCase();
  const mail = normalEmail(email);
  const name = mail.split("@")[0];
  if (mail && low.includes(mail)) return "The password can't contain the email.";
  if (name.length >= 3 && low.includes(name)) return "The password can't contain the email's name.";
  return null;
}

// A one-time password for a new admin: 20 characters with no look-alikes.
export function oneTimePassword() {
  const abc = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  const out = [];
  for (const b of randomBytes(40)) {
    if (b < 220) out.push(abc[b % abc.length]); // 220 = 4 * 55: no modulo bias
    if (out.length === 20) break;
  }
  return out.join("");
}

// A session: the cookie carries 32 random bytes; D1 keeps their SHA-256.
export async function newSessionToken() {
  const token = toB64url(randomBytes(32));
  return { token, id: await sha256Hex(token) };
}

export function sessionCookie(token, maxAgeSeconds) {
  return `${COOKIE}=${token}; HttpOnly; Secure; SameSite=Strict; Path=/; Max-Age=${maxAgeSeconds}`;
}

export function readCookie(request, name = COOKIE) {
  const header = request.headers.get("Cookie") || "";
  for (const part of header.split(";")) {
    const [k, ...v] = part.trim().split("=");
    if (k === name) return v.join("=");
  }
  return "";
}

// The network a login comes from: the /24 of an IPv4 address, the /64 of an
// IPv6 one.
export function ipNetwork(ip) {
  const s = String(ip || "");
  const v4 = s.match(/^(\d+)\.(\d+)\.(\d+)\.\d+$/);
  if (v4) return `${v4[1]}.${v4[2]}.${v4[3]}.0/24`;
  if (s.includes(":")) {
    const [head, tail = ""] = s.split("::");
    const h = head ? head.split(":") : [];
    const t = s.includes("::") && tail ? tail.split(":") : [];
    const full = s.includes("::") ? [...h, ...Array(8 - h.length - t.length).fill("0"), ...t] : h;
    return full.slice(0, 4).map((x) => parseInt(x || "0", 16).toString(16)).join(":") + "::/64";
  }
  return s || "unknown";
}

export async function lockKeys(email, ip) {
  return [await sha256Hex("email:" + normalEmail(email)), await sha256Hex("ip:" + ipNetwork(ip))];
}

export const isoNow = (ms = Date.now()) => new Date(ms).toISOString();
