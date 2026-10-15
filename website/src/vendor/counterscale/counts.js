// Ported from Counterscale v3.4.1 (MIT, see LICENSE and SOURCE.md):
// packages/server/app/analytics/query.ts.

/**
 * Given counts { views, visitors, bounces } and a row grouped by the visitor
 * and bounce columns ({ count, isVisitor, isBounce }), accumulate views,
 * visitors and bounces.
 */
export function accumulateCountsFromRowResult(counts, row) {
  if (row.isVisitor == 1) {
    counts.visitors += Number(row.count);
  }
  if (row.isBounce && row.isBounce != 0) {
    // bounce is either 1 or -1
    counts.bounces += Number(row.count) * row.isBounce;
  }
  counts.views += Number(row.count);
}

/**
 * Fix negative bounce values coming from sparse values: a bucket can hold
 * the -1 of a visit whose +1 fell in an earlier bucket. Go back in time and
 * amend the last bucket with bounces, so no bucket shows a negative bounce
 * rate. The buckets must be sorted; each is [key, counts].
 */
export function fixNegativeBounces(sortedRows) {
  for (let i = 1; i < sortedRows.length; i++) {
    const current = sortedRows[i][1];
    // if the current value of bounces is negative, find the last non-zero bucket and decrement
    if (current.bounces < 0) {
      for (let j = i - 1; j >= 0; j--) {
        const prev = sortedRows[j][1];
        if (prev.bounces > 0) {
          prev.bounces += current.bounces;
          current.bounces = 0; // zero-out current bucket
          break;
        }
      }
    }
  }
  return sortedRows;
}
