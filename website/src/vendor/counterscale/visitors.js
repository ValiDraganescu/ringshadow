// Ported from Counterscale v3.4.1 (MIT, see LICENSE and SOURCE.md):
// packages/server/app/analytics/collect.ts and app/lib/utils.ts.
//
// Cookieless visitor and session tracking, after
// https://notes.normally.com/cookieless-unique-visitor-counts/ : the
// browser's own HTTP cache keeps a Last-Modified date for a fixed URL; its
// day says whether this visitor was seen today and its seconds (1 to 3) how
// many hits came before.

function getMidnightDate() {
  const midnight = new Date();
  midnight.setHours(0, 0, 0, 0);
  return midnight;
}

function getNextLastModifiedDate(current) {
  // in case date is an 'Invalid Date'
  if (current && isNaN(current.getTime())) {
    current = null;
  }

  const midnight = getMidnightDate();

  // check if new day, if it is then set to midnight
  let next = current ? current : midnight;
  next = midnight.getTime() - next.getTime() > 0 ? midnight : next;

  // Next seconds value is the current seconds value + 1, capped at 3
  const currentSeconds = next.getSeconds();
  next.setSeconds(Math.min(3, currentSeconds + 1));

  return next;
}

/**
 * Bounce value from the hit count: 1 (bounce), -1 (anti-bounce), or 0 (normal).
 */
export function getBounceValue(hits) {
  if (hits === 1) {
    return 1; // First hit = bounce
  } else if (hits === 2) {
    return -1; // Second hit = anti-bounce
  } else {
    return 0; // Third+ hit = normal
  }
}

function checkVisitorSession(ifModifiedSince) {
  let newVisitor = true;

  if (ifModifiedSince) {
    // check today is a new day vs ifModifiedSince
    const today = new Date();
    const ifModifiedSinceDate = new Date(ifModifiedSince);
    if (
      today.getFullYear() === ifModifiedSinceDate.getFullYear() &&
      today.getMonth() === ifModifiedSinceDate.getMonth() &&
      today.getDate() === ifModifiedSinceDate.getDate()
    ) {
      // if ifModifiedSince is today, this is not a new visitor
      newVisitor = false;
    }
  }

  return { newVisitor };
}

/**
 * The hit count (1 first visit, 2 anti-bounce, 3 a regular view) from the
 * If-Modified-Since header, and the Last-Modified date to answer with.
 */
export function handleCacheHeaders(ifModifiedSince) {
  const { newVisitor } = checkVisitorSession(ifModifiedSince);
  const nextLastModifiedDate = getNextLastModifiedDate(ifModifiedSince ? new Date(ifModifiedSince) : null);

  let hits = newVisitor ? 1 : nextLastModifiedDate.getSeconds();

  // Cap the hit count at 3 to avoid exposing exact hit counts publicly
  if (hits > 3) {
    hits = 3;
  }

  return { hits, nextLastModifiedDate };
}

/**
 * The hit count from the collector's `ht` parameter, 1 to 3.
 */
export function hitsFromParam(ht) {
  let hits = parseInt(ht, 10);
  if (isNaN(hits) || hits <= 0) hits = 1;
  if (hits > 3) hits = 3;
  return hits;
}

/**
 * Only the major version: "141.0.7390.54" becomes "141.x.x.x".
 */
export function maskBrowserVersion(version) {
  if (!version) return version;

  const majorEnd = version.indexOf(".");

  if (majorEnd != -1) {
    version = version.substring(0, majorEnd) + version.slice(majorEnd).replaceAll(/\.[^.]+/g, ".x");
  }

  return version;
}
