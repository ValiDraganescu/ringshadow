#!/usr/bin/env bash
# The admin's door, end to end, against a local `wrangler dev` with an empty
# local D1. It registers an admin, so never point it at the live site.
#
#   cd website
#   P=$(mktemp -d)   # a fresh local D1
#   pnpm dlx wrangler@4.151.0 d1 migrations apply ringshadow-admin --local --persist-to "$P"
#   pnpm dlx wrangler@4.151.0 dev --port 8792 --persist-to "$P" --var ADMIN_SETUP_CODE:local-code &
#   bash tests/smoke-admin.sh http://127.0.0.1:8792 local-code "$P"

set -u
BASE=${1:-http://127.0.0.1:8792}
CODE=${2:-local-code}
PERSIST=${3:?the --persist-to folder of the local D1}
ORIGIN=$BASE
FAILS=0
H=$(mktemp)

check() { # name expected actual
  if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected [$2], got [$3] $(head -c 200 "$H.body" 2>/dev/null)"; FAILS=$((FAILS + 1)); fi
}
# req METHOD PATH [JSON] [COOKIE] -> prints the status; headers land in $H, body in $H.body
req() {
  local args=(-s -o "$H.body" -D "$H" -w "%{http_code}" -X "$1")
  [ -n "${3:-}" ] && args+=(-H "Content-Type: application/json" -H "Origin: $ORIGIN" --data "$3")
  [ -n "${4:-}" ] && args+=(-H "Cookie: admin_session=$4")
  curl "${args[@]}" "$BASE$2"
}
# js KEY VALUE ... -> a JSON object (no inline braces: bash would expand them)
js() { python3 -c 'import json,sys; a=sys.argv[1:]; print(json.dumps(dict(zip(a[::2], a[1::2]))))' "$@"; }
cookie() { grep -i '^set-cookie: admin_session=' "$H" | sed -E 's/^[^=]*=([^;]*).*/\1/' | tr -d '\r'; }
field() { python3 -c "import json,sys; print(json.load(open(sys.argv[1])).get(sys.argv[2], ''))" "$H.body" "$1"; }
d1() { pnpm dlx wrangler@4.151.0 d1 execute ringshadow-admin --local --persist-to "$PERSIST" --json --command "$1" 2>/dev/null \
  | python3 -c "import json,sys; print(list(json.load(sys.stdin)[0]['results'][0].values())[0])"; }

A=first@example.org; APW='orbit lantern 1971'; APW2='orbit lantern 2026'
B=second@example.org; BPW='quiet harbour 3141'

check "a reader's page sets no cookie" 0 "$(curl -s -D - -o /dev/null "$BASE/" | grep -ci '^set-cookie')"
check "state: no admin yet" 200 "$(req GET /api/admin/state)"; check "hasAdmin false" False "$(field hasAdmin)"
check "the shell redirects without a session" 302 "$(req GET /admin/)"
check "to the login page" "/admin/login" "$(grep -i '^location:' "$H" | awk '{print $2}' | tr -d '\r')"
check "the API needs a session" 401 "$(req GET /api/admin/me)"
check "the register page while empty" 200 "$(req GET /admin/register)"
check "a write without the site's Origin is refused" 403 "$(curl -s -o /dev/null -w '%{http_code}' -X POST -H 'Content-Type: application/json' --data "$(js)" "$BASE/api/admin/register")"
check "a wrong setup code" 403 "$(req POST /api/admin/register "$(js code "nope" email "$A" password "$APW")")"
check "a short password" 400 "$(req POST /api/admin/register "$(js code "$CODE" email "$A" password "short")")"
check "register the first admin" 201 "$(req POST /api/admin/register "$(js code "$CODE" email "$A" password "$APW")")"
SA=$(cookie)
check "the session cookie's flags" 1 "$(grep -ci 'admin_session=.*HttpOnly; Secure; SameSite=Strict; Path=/; Max-Age=2592000' "$H")"
check "the register page is gone" 404 "$(req GET /admin/register)"
check "and so is the route" 404 "$(req POST /api/admin/register "$(js code "$CODE" email "x@example.org" password "$APW")")"
check "the shell with a session" 200 "$(req GET /admin/ '' "$SA")"
check "the shell is private" 1 "$(grep -ci '^cache-control: no-store' "$H")"
check "me" "$A" "$(req GET /api/admin/me '' "$SA" >/dev/null; field email)"

check "add a second admin" 201 "$(req POST /api/admin/admins "$(js email "$B")" "$SA")"
OTP=$(field password)
check "its one-time password is 20 characters" 20 "${#OTP}"
check "the same email twice" 409 "$(req POST /api/admin/admins "$(js email "$B")" "$SA")"
check "the second admin signs in with it" 200 "$(req POST /api/admin/login "$(js email "$B" password "$OTP")")"
SB=$(cookie)
check "and must change it" True "$(field mustChange)"
check "nothing else until then" 403 "$(req GET /api/admin/admins '' "$SB")"
check "the second admin changes it" 200 "$(req POST /api/admin/password "$(js current "$OTP" next "$BPW")" "$SB")"
check "then the admin list opens" 200 "$(req GET /api/admin/admins '' "$SB")"
check "the one-time password no longer works" 401 "$(req POST /api/admin/login "$(js email "$B" password "$OTP")")"
check "the second admin's own one does" 200 "$(req POST /api/admin/login "$(js email "$B" password "$BPW")")"

check "the first admin changes their password" 200 "$(req POST /api/admin/password "$(js current "$APW" next "$APW2")" "$SA")"
check "a wrong current password is refused" 403 "$(req POST /api/admin/password "$(js current "$APW" next "$APW2 x")" "$SA")"
check "the new password signs in" 200 "$(req POST /api/admin/login "$(js email "$A" password "$APW2")")"
SA2=$(cookie)

check "sessions before sign-out-everywhere > 1" 1 "$( [ "$(d1 'SELECT count(*) FROM sessions')" -gt 1 ] && echo 1 || echo 0)"
check "sign out everywhere" 200 "$(req POST /api/admin/signout-all "$(js)" "$SA2")"
check "every session row is gone" 0 "$(d1 'SELECT count(*) FROM sessions')"
check "the old session is refused" 401 "$(req GET /api/admin/me '' "$SB")"

check "sign in again" 200 "$(req POST /api/admin/login "$(js email "$A" password "$APW2")")"
SA3=$(cookie)
check "sign out" 200 "$(req POST /api/admin/logout "$(js)" "$SA3")"
check "that session is gone" 401 "$(req GET /api/admin/me '' "$SA3")"

for i in 1 2 3 4; do
  check "wrong password $i" 401 "$(req POST /api/admin/login "$(js email "$A" password "wrong password $i")")"
done
check "wrong password 5 locks" 429 "$(req POST /api/admin/login "$(js email "$A" password "wrong password 5")")"
check "the right password is locked out too" 429 "$(req POST /api/admin/login "$(js email "$A" password "$APW2")")"
UNTIL=$(field until)
MIN=$(python3 -c "import sys,datetime as d; u=d.datetime.fromisoformat(sys.argv[1].replace('Z','+00:00')); print(round((u-d.datetime.now(d.timezone.utc)).total_seconds()/60))" "$UNTIL")
check "for 15 minutes" 15 "$MIN"

rm -f "$H" "$H.body"
echo "$FAILS failed"
[ "$FAILS" = 0 ]
