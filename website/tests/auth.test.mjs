// node --test "website/tests/*.test.mjs"
import assert from "node:assert/strict";
import { test } from "node:test";
import {
  hashPassword, ipNetwork, lockKeys, newSessionToken, oneTimePassword, passwordProblem,
  readCookie, sessionCookie, sha256Hex, verifyPassword,
} from "../src/auth.js";

test("a password hashes and verifies, and the wrong one doesn't", async () => {
  const row = await hashPassword("correct horse battery", 1000);
  assert.equal(row.pw_iters, 1000);
  assert.equal(row.pw_salt.length, 32);
  assert.equal(row.pw_hash.length, 64);
  assert.equal(await verifyPassword("correct horse battery", row), true);
  assert.equal(await verifyPassword("correct horse batterz", row), false);
  const again = await hashPassword("correct horse battery", 1000);
  assert.notEqual(again.pw_salt, row.pw_salt);
});

test("the password rule", () => {
  assert.match(passwordProblem("short", "a@b.co"), /12/);
  assert.equal(passwordProblem("twelve chars", "dev@example.org"), null);
  assert.match(passwordProblem("my dev@example.org pw", "dev@example.org"), /email/);
  assert.match(passwordProblem("xxDEVELOPERxxyy", "developer@example.org"), /name/);
  assert.equal(passwordProblem("abcdefghijklmnop", "ab@example.org"), null); // a name under 3 letters isn't checked
  assert.match(passwordProblem("x".repeat(257), "a@b.co"), /256/);
});

test("one-time passwords: 20 characters, no look-alikes, all different", () => {
  const seen = new Set();
  for (let i = 0; i < 200; i++) {
    const p = oneTimePassword();
    assert.match(p, /^[a-km-zA-HJ-NP-Z2-9]{20}$/);
    assert.doesNotMatch(p, /[lIO01]/);
    seen.add(p);
  }
  assert.equal(seen.size, 200);
});

test("sessions: the cookie holds the token, D1 its hash", async () => {
  const { token, id } = await newSessionToken();
  assert.equal(token.length, 43);
  assert.equal(id, await sha256Hex(token));
  const cookie = sessionCookie(token, 2592000);
  assert.equal(cookie, `admin_session=${token}; HttpOnly; Secure; SameSite=Strict; Path=/; Max-Age=2592000`);
  const req = new Request("https://ringshadow.com/admin/", { headers: { Cookie: `other=1; admin_session=${token}` } });
  assert.equal(readCookie(req), token);
  assert.equal(readCookie(new Request("https://ringshadow.com/")), "");
});

test("lockout networks: /24 for IPv4, /64 for IPv6", async () => {
  assert.equal(ipNetwork("203.0.113.77"), "203.0.113.0/24");
  assert.equal(ipNetwork("2001:db8:abcd:12:1:2:3:4"), "2001:db8:abcd:12::/64");
  assert.equal(ipNetwork("2001:db8::1"), "2001:db8:0:0::/64");
  assert.equal(ipNetwork("::1"), "0:0:0:0::/64");
  assert.equal(ipNetwork(""), "unknown");
  const [a] = await lockKeys(" Dev@Example.org ", "203.0.113.1");
  const [b, net] = await lockKeys("dev@example.org", "203.0.113.200");
  assert.equal(a, b);
  assert.equal(net, (await lockKeys("x@y.z", "203.0.113.9"))[1]);
});
