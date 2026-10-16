// cdp-watch.mjs <url> [doneRegex=REPORT SENT|ERROR] [timeoutSec=600] [port=9333] [stallSec=40]
// Opens <url> in the debug Chrome, polls every 5 s: prints the page's #o text as it grows (probe pages) plus the emulator frame counter,
// and ends by itself when the text matches doneRegex, on timeout, or when the frame counter has not moved for stallSec seconds (a hang).
// Output lines are one event each, meant for Monitor: "PROGRESS frame=N (+k)", "LINE <new text>", "STALL ...", "DONE", "TIMEOUT".
const [url, doneRe = 'REPORT SENT|ERROR', timeoutSec = '600', port = '9333', stallSec = '40'] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const tab = await (await fetch(`${base}/json/new?${encodeURIComponent(url)}`, { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pend = new Map();
ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d); pend.delete(d.id); } };
const call = (method, params = {}) => new Promise((r) => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
await call('Emulation.setDeviceMetricsOverride', { width: 1500, height: 1100, deviceScaleFactor: 1, mobile: false });
const probe = `(() => { const o = document.getElementById('o'); let fr = -1; try { const w = document.getElementById('f') ? document.getElementById('f').contentWindow : window; fr = w.eval('typeof frames === "number" ? frames : -1'); } catch (e) {} return JSON.stringify({ fr, text: o ? o.textContent : '' }); })()`;
const t0 = Date.now(); let seen = 0, lastFr = -2, lastMove = Date.now(), end = 'TIMEOUT';
while (Date.now() - t0 < +timeoutSec * 1000) {
  await new Promise((r) => setTimeout(r, 5000));
  let st; try { st = JSON.parse((await call('Runtime.evaluate', { expression: probe, returnByValue: true })).result.result.value); } catch (e) { console.log('STALL page not answering: ' + e); end = 'STALL'; break; }
  if (st.fr !== lastFr) { if (st.fr >= 0) console.log(`PROGRESS frame=${st.fr} t=${Math.round((Date.now() - t0) / 1000)}s`); lastFr = st.fr; lastMove = Date.now(); }
  if (st.text.length > seen) { for (const l of st.text.slice(seen).split('\n').filter(Boolean)) console.log('LINE ' + l.slice(0, 400)); seen = st.text.length; }
  if (new RegExp(doneRe).test(st.text)) { end = 'DONE'; break; }
  if (Date.now() - lastMove > +stallSec * 1000) { console.log(`STALL frame counter stuck at ${st.fr} for ${stallSec}s`); end = 'STALL'; break; }
}
console.log(end);
await fetch(`${base}/json/close/${tab.id}`); ws.close(); process.exit(0);
