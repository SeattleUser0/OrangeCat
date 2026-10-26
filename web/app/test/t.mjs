// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app tests: the one checker every test file uses (the same line format as web/test_web.mjs).
export const T = { failed: 0, passed: 0 };
export function ok(cond, what) {
  console.log(`${what.padEnd(100)} ${cond ? "ok" : "FAIL"}`);
  if (cond) T.passed++; else T.failed++;
  return !!cond;
}
export const eqBytes = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
