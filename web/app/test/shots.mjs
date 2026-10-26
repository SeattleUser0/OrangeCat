// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: a look at the page in a real browser against the mocked unit (a developer's tool, not part of run.mjs: it needs
// puppeteer, which the repository does not carry). It opens every tab, presses the buttons that read from the unit, and saves a
// screenshot of each, failing on any error the page logs.
//   (cd web && python3 -m http.server 8765) &
//   NODE_PATH=/path/with/puppeteer node web/app/test/shots.mjs http://localhost:8765/app/ OUT_DIR
// Revisions:
//   0.14.2: first version (Session D).
import { createRequire } from "node:module";
const require = createRequire(import.meta.url);
const puppeteer = require(process.env.PUPPETEER || "puppeteer");
const base = process.argv[2] || "http://localhost:8765/app/", out = process.argv[3] || ".";
const width = +(process.env.W || 1200);

const browser = await puppeteer.launch({ args: ["--no-sandbox"] });
const page = await browser.newPage();
await page.setViewport({ width, height: 900, deviceScaleFactor: 1 });
const errors = [];
page.on("pageerror", (e) => errors.push("pageerror: " + e.message));
page.on("console", (m) => { if (m.type() === "error") errors.push("console: " + m.text()); });
const click = async (text) => {
  const ok = await page.evaluate((t) => { const b = [...document.querySelectorAll("button")].find((x) => x.textContent.trim() === t && !x.disabled); if (b) b.click(); return !!b; }, text);
  if (!ok) errors.push("no button " + text);
  return ok;
};
const idle = async () => { for (let i = 0; i < 400; i++) { const busy = await page.evaluate(() => /Busy|Reading|Connecting|%/.test(document.getElementById("status").textContent) && !/ready|Ready/.test(document.getElementById("status").textContent)); if (!busy) break; await new Promise((r) => setTimeout(r, 100)); } await new Promise((r) => setTimeout(r, 400)); };
const tab = async (name) => { await page.evaluate((n) => { [...document.querySelectorAll("#tabs button")].find((b) => b.textContent === n).click(); }, name); await new Promise((r) => setTimeout(r, 200)); };
const shot = async (name) => { await page.screenshot({ path: `${out}/${name}.png`, fullPage: true }); };

await page.goto(base + "?mock", { waitUntil: "networkidle0" });
await shot("0-start");
await click("Connect");
for (let i = 0; i < 300; i++) { const t = await page.evaluate(() => document.getElementById("status").textContent); if (/Ready|ready/i.test(t)) break; await new Promise((r) => setTimeout(r, 100)); }
await shot("1-unit");
await tab("Sounds"); await shot("2-sounds");
await click("Read the library"); await idle(); await shot("2b-sounds-library");
await tab("Steps"); await click("Read the steps"); await idle(); await click("Read them from the unit"); await idle(); await shot("3-steps");
await tab("Arrange"); await click("Read the arrangement"); await idle();
await page.evaluate(() => { const s = [...document.querySelectorAll(".song select")][1]; s.value = "7"; s.dispatchEvent(new Event("change")); });
await new Promise((r) => setTimeout(r, 200)); await shot("4-arrange");
await tab("Project"); await click("Read"); await idle(); await shot("5-project");
await tab("Settings"); await shot("6-settings");
await browser.close();
console.log(errors.length ? "ERRORS:\n" + errors.join("\n") : "no page errors");
process.exit(errors.length ? 1 : 0);
