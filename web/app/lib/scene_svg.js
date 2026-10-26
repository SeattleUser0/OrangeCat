// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: an engine's picture drawn from its scene bytes, as SVG. A port of Session C's interpreter (tests/ui_scene.h,
// branch ui-design; docs/orangecat/UI_DESIGN.md "The scene format"), op for op: the same stream, the same bindings, the same
// groups (pivot, turn, move), the same REP, HIDE and SCOPE, in the same 240 x 132 design box. The unit draws pixels on its canvas;
// this draws SVG lines and circles with the same coordinates, so the page shows the unit's picture, moving with the same sources.
//
//   sceneSvg(bytes, input) -> SVG markup. input: {s: {K1..K4, ENV, HIT, GATE, LFO, NOTE, TIME, WOB, PH1..PH4} (Q8 as the unit's),
//                                                 col: [INK, K1, K2, K3, K4, TRK, DIM, BG] (CSS colours), scope: 64 values -256..256}
// Revisions:
//   0.14.2: first version (Session D), from ui_scene.h of ui-design 776adb5.

export const SO = { END: 0, LINE: 1, PLINE: 2, PGON: 3, RECT: 4, RRECT: 5, CIRC: 6, DISC: 7, ARC: 8, FILL: 9, SCOPE: 10, PUSH: 11, POP: 12, REP: 13, STYLE: 14, BIND: 31 };
export const SS = ["K1", "K2", "K3", "K4", "ENV", "HIT", "GATE", "LFO", "NOTE", "TIME", "WOB", "PH1", "PH2", "PH3", "PH4"];
const SF = { X: 0, Y: 1, X2: 2, Y2: 3, R: 4, A0: 5, A1: 6, ANG: 7, CNT: 8, DX: 9, DY: 10, HIDE: 11, P0X: 16, P0Y: 32 };
const s8 = (v) => (v > 127 ? v - 256 : v);
/* sine and cosine of a in 1/256 turn, Q12 (the interpreter's quarter wave, so a turned part lands where the unit's does) */
const SINQ = [0, 101, 201, 301, 401, 501, 601, 700, 799, 897, 995, 1092, 1189, 1285, 1380, 1474, 1567, 1660, 1751, 1842, 1931, 2019,
  2106, 2191, 2276, 2359, 2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229, 3290, 3349, 3406, 3461,
  3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822, 3857, 3889, 3920, 3948, 3973, 3996, 4017, 4036, 4052, 4065, 4076, 4085, 4091, 4095, 4096];
function sin(a) { a &= 255; return a < 64 ? SINQ[a] : a < 128 ? SINQ[128 - a] : a < 192 ? -SINQ[a - 128] : -SINQ[256 - a]; }
const cos = (a) => sin(a + 64);
/* bytes an op takes after its op byte */
function len(p, i) {
  switch (p[i] & 31) {
    case SO.LINE: case SO.RECT: case SO.FILL: case SO.SCOPE: return 4;
    case SO.RRECT: case SO.ARC: return 5;
    case SO.CIRC: case SO.DISC: case SO.REP: return 3;
    case SO.PUSH: return 2;
    case SO.STYLE: return 1;
    case SO.BIND: return 3;
    case SO.PLINE: case SO.PGON: return 1 + 2 * p[i + 1];
    default: return 0;
  }
}

export function sceneSvg(bytes, input, { title = "" } = {}) {
  const p = bytes, W = p[1] || 240, H = p[2] || 132;
  const src = (b) => { const v = input.s[SS[b.src & 0x7F]] ?? 0; return b.src & 0x80 ? v - 128 : v; };
  const out = [];
  let weight = 1, depth = 0, rep = 1, rdx = 0, rdy = 0, nb = 0;
  const xf = [{ px: 0, py: 0, tx: 0, ty: 0, c: 4096, s: 0, ang: 0 }, null, null];
  let binds = [];
  const mod = (f, hideBox) => {
    let o = 0;
    for (const b of binds) {
      const v = src(b);
      if (b.field === SF.HIDE) { if (b.depth >= 0 ? v < 2 * b.depth : v > -2 * b.depth) hideBox.h = true; }
      else if (b.field === f) o += f === SF.ANG ? Math.trunc(2 * b.depth * v / 256) : Math.trunc(b.depth * v / 256);
    }
    return o;
  };
  const map = (x, y) => {                       /* design px through the groups, outermost last */
    let X = x, Y = y;
    for (let d = depth; d >= 1; d--) {
      const g = xf[d], rx = X - g.px, ry = Y - g.py;
      X = g.px + (rx * g.c - ry * g.s) / 4096 + g.tx;
      Y = g.py + (rx * g.s + ry * g.c) / 4096 + g.ty;
    }
    return [X, Y];
  };
  const rot = () => { let a = 0; for (let d = 1; d <= depth; d++) a += xf[d].ang; return a; };
  const f2 = (v) => Math.round(v * 100) / 100;
  const stroke = (c) => `stroke="${c}" stroke-width="${weight}" fill="none" stroke-linecap="round" stroke-linejoin="round"`;
  const line = (a, b, c) => out.push(`<line x1="${f2(a[0])}" y1="${f2(a[1])}" x2="${f2(b[0])}" y2="${f2(b[1])}" ${stroke(c)}/>`);
  const arc = (cx, cy, r, a0, a1, c) => {
    if (a1 <= a0) a1 += 256;
    const [mx, my] = map(cx, cy), turn = rot(), pts = [];
    for (let a = a0; ; a += 8) {
      if (a > a1) a = a1;
      pts.push([mx + r * cos(a + turn) / 4096, my + r * sin(a + turn) / 4096]);
      if (a === a1) break;
    }
    out.push(`<polyline points="${pts.map((q) => f2(q[0]) + "," + f2(q[1])).join(" ")}" ${stroke(c)}/>`);
  };
  let i = 4;
  while (i < p.length && p[i] !== SO.END) {
    const op = p[i] & 31, c = input.col[p[i] >> 5] ?? "#fff", n = len(p, i), hb = { h: false };
    if (op === SO.BIND) { if (nb < 4) { binds.push({ src: p[i + 1], field: p[i + 2], depth: s8(p[i + 3]) }); nb++; } i += 4; continue; }
    if (op === SO.STYLE) { weight = p[i + 1]; i += 2; continue; }
    if (op === SO.REP) {
      rep = p[i + 1] + mod(SF.CNT, hb); rdx = s8(p[i + 2]) + mod(SF.DX, hb); rdy = s8(p[i + 3]) + mod(SF.DY, hb);
      if (hb.h || rep < 0) rep = 0;
      binds = []; nb = 0; i += 4; continue;
    }
    if (op === SO.PUSH) {
      if (depth < 2) {
        const a = mod(SF.ANG, hb);
        xf[++depth] = { px: p[i + 1], py: p[i + 2], tx: mod(SF.X, hb), ty: mod(SF.Y, hb), c: cos(a), s: sin(a), ang: a };
      }
      binds = []; nb = 0; i += 3; continue;
    }
    if (op === SO.POP) { if (depth) depth--; i++; continue; }
    for (let k = 0; k < rep && !hb.h; k++) {
      const dx = k * rdx + mod(SF.X, hb), dy = k * rdy + mod(SF.Y, hb);
      if (hb.h) break;
      switch (op) {
        case SO.LINE:
          line(map(p[i + 1] + dx, p[i + 2] + dy), map(p[i + 3] + dx + mod(SF.X2, hb), p[i + 4] + dy + mod(SF.Y2, hb)), c);
          break;
        case SO.PLINE: case SO.PGON: {
          const m = p[i + 1], pts = [];
          for (let j = 0; j < m; j++) pts.push(map(p[i + 2 + 2 * j] + dx + mod(SF.P0X + j, hb), p[i + 3 + 2 * j] + dy + mod(SF.P0Y + j, hb)));
          out.push(`<${op === SO.PGON && m > 2 ? "polygon" : "polyline"} points="${pts.map((q) => f2(q[0]) + "," + f2(q[1])).join(" ")}" ${stroke(c)}/>`);
          break;
        }
        case SO.RECT: case SO.FILL: case SO.RRECT: {
          const w = p[i + 3] + mod(SF.X2, hb), h = p[i + 4] + mod(SF.Y2, hb);
          if (w <= 0 || h <= 0) break;
          if (op === SO.RRECT) {
            let r = p[i + 5] + mod(SF.R, hb);
            r = Math.max(0, Math.min(r, w / 2, h / 2));
            const X = p[i + 1] + dx, Y = p[i + 2] + dy;
            line(map(X + r, Y), map(X + w - 1 - r, Y), c); line(map(X + r, Y + h - 1), map(X + w - 1 - r, Y + h - 1), c);
            line(map(X, Y + r), map(X, Y + h - 1 - r), c); line(map(X + w - 1, Y + r), map(X + w - 1, Y + h - 1 - r), c);
            if (r > 0) { arc(X + r, Y + r, r, 128, 192, c); arc(X + w - 1 - r, Y + r, r, 192, 256, c); arc(X + w - 1 - r, Y + h - 1 - r, r, 0, 64, c); arc(X + r, Y + h - 1 - r, r, 64, 128, c); }
            break;
          }
          const [x0, y0] = map(p[i + 1] + dx, p[i + 2] + dy);   /* (RECT and FILL only move: upright) */
          out.push(op === SO.FILL ? `<rect x="${f2(x0)}" y="${f2(y0)}" width="${w}" height="${h}" fill="${c}"/>`
            : `<rect x="${f2(x0 + 0.5)}" y="${f2(y0 + 0.5)}" width="${w - 1}" height="${h - 1}" ${stroke(c)}/>`);
          break;
        }
        case SO.CIRC: case SO.DISC: {
          const d = p[i + 3] + mod(SF.R, hb);
          if (d < 1) break;
          const [x0, y0] = map(p[i + 1] + dx, p[i + 2] + dy);
          out.push(op === SO.DISC ? `<circle cx="${f2(x0)}" cy="${f2(y0)}" r="${f2(d / 2)}" fill="${c}"/>` : `<circle cx="${f2(x0)}" cy="${f2(y0)}" r="${f2(Math.max(0.5, d / 2 - weight / 2))}" ${stroke(c)}/>`);
          break;
        }
        case SO.ARC: {
          const r = p[i + 3] + mod(SF.R, hb), a0 = p[i + 4] + mod(SF.A0, hb), a1 = p[i + 5] + mod(SF.A1, hb);
          if (r > 0 && a0 !== a1) arc(p[i + 1] + dx, p[i + 2] + dy, r, a0, a1, c);
          break;
        }
        case SO.SCOPE: {
          const w = p[i + 3], h = p[i + 4], pts = [];
          for (let j = 0; j < 64; j++) pts.push(map(p[i + 1] + dx + Math.trunc(j * w / 63), p[i + 2] + dy + Math.trunc(h / 2) - Math.trunc((input.scope?.[j] ?? 0) * Math.trunc(h / 2) / 256)));
          out.push(`<polyline points="${pts.map((q) => f2(q[0]) + "," + f2(q[1])).join(" ")}" ${stroke(c)}/>`);
          break;
        }
        default: break;
      }
    }
    rep = 1; rdx = rdy = 0; binds = []; nb = 0;
    i += 1 + n;
  }
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${W} ${H}" role="img" aria-label="${title}" class="scene">${out.join("")}</svg>`;
}
