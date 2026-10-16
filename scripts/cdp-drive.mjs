// cdp-drive.mjs <url> <outPrefix> <steps> [width=1500] [height=520] [port=9333]
// Drives the emulator page like a player: <steps> is a comma list of  wait:<sec>  shot  and button names (A,B,X,Y,Start,Select,Up,Down,Left,Right,L,R).
// A button is held 250 ms through the page's own press(); `shot` saves <outPrefix>-<n>.png. Prints the diagnostics text after each shot.
import fs from 'node:fs';
const [url, prefix, steps, width = '1500', height = '520', port = '9333'] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const tab = await (await fetch(`${base}/json/new?${encodeURIComponent(url)}`, { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pend = new Map();
ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d); pend.delete(d.id); } };
const call = (method, params = {}) => new Promise((r) => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (expression) => (await call('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true })).result.result.value;
await call('Emulation.setDeviceMetricsOverride', { width: +width, height: +height, deviceScaleFactor: 1, mobile: false });
let n = 0;
for (const st of steps.split(',')) {
  if (st.startsWith('wait:')) { await new Promise((r) => setTimeout(r, +st.slice(5) * 1000)); continue; }
  if (st === 'shot') { const s = await call('Page.captureScreenshot', { format: 'png' }); fs.writeFileSync(`${prefix}-${String(++n).padStart(2, '0')}.png`, Buffer.from(s.result.data, 'base64')); console.log(n, await ev("document.getElementById('counters').innerText.replace(/\\n/g, ' | ').slice(0, 520)")); continue; }
  if (st === 'dump') { console.log('JIT totals:', await ev('JSON.stringify({ blocks: Module.jit.impls[0].count, wasmKB: Math.round(Module.jit.impls[0].bytes / 1024), msTotal: Module.jit.t.slice(0, 3).map((x) => Math.round(x)), frames })')); continue; }
  await ev(`(async () => { press('${st}', true); await new Promise((r) => setTimeout(r, 250)); press('${st}', false); })()`);
  console.log('pressed', st);
}
await fetch(`${base}/json/close/${tab.id}`); ws.close();
