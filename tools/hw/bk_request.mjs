// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
//
// (Round 32, the unit's "Unexpected backup reply" at 50 %) A request function for web/fm1backup.js captureBackup over a Linux PC's MIDI
// bridge. The page's OcLink.ask takes the first reply with the command's number: after a timeout and its retry, the first try's late reply
// answers the retry and the retry's own reply then answers the NEXT request (another offset): captureBackup throws "Unexpected backup
// reply". Here a BACKUP_GET reply (66: id, rc, offset, ...) is taken only when its id and offset are the request's; any other is dropped (a
// late duplicate of a read is harmless). Reads only: BACKUP_LIST, BACKUP_GET and INFO are asked again on a timeout, nothing else.
const RESEND = new Set([1, 65, 66]);
const r32 = (a, o) => (a[o] | a[o + 1] << 7 | a[o + 2] << 14 | a[o + 3] << 21 | a[o + 4] << 28) >>> 0;

export function matchedRequest(input, output, { minTimeout = 2000, minRetries = 2 } = {}) {
  let pending = null;
  const fits = (p, d) => {
    if (p.cmd !== d[0]) return false;
    if (p.cmd !== 66) return true;
    const a = d.slice(1);
    return a[0] === p.args[0] && a.length >= 7 && r32(a, 2) === r32(p.args, 1);
  };
  input.onmidimessage = (e) => {
    const d = e.data;
    if (d.length < 6 || d[0] !== 240 || d[1] !== 125 || d[2] !== 70 || d[3] !== 76 || d[d.length - 1] !== 247) return;
    const body = Array.from(d.slice(4, -1));
    if (pending && fits(pending, body)) { const p = pending; pending = null; clearTimeout(p.timer); p.resolve(body.slice(1)); }
  };
  return async ([cmd, args], { timeout = 1500, retries = 0 } = {}) => {
    const tries = 1 + (RESEND.has(cmd) ? Math.max(retries, minRetries) : retries), wait = Math.max(timeout, minTimeout);
    for (let t = 0; t < tries; t++) {
      const r = await new Promise((resolve) => {
        const timer = setTimeout(() => { pending = null; resolve(null); }, wait);
        pending = { cmd, args, timer, resolve };
        output.send([240, 125, 70, 76, cmd, ...args, 247]);
      });
      if (r) return r;
    }
    throw new Error(`no reply to editor command ${cmd} after ${tries} tries`);
  };
}
