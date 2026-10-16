// cdp-shot.mjs <url> <out.png> [waitMs=8000] [width=1600] [height=900] [port=9333]
// Opens <url> in a fresh tab of the Chrome that was started with --remote-debugging-port=<port>, waits, saves a PNG screenshot of the page and the page's text (#stats, #counters), then closes the tab.
import fs from 'node:fs';
const [url, out, waitMs = '8000', width = '1600', height = '900', port = '9333'] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const tab = await (await fetch(`${base}/json/new?${encodeURIComponent(url)}`, { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pend = new Map();
ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d); pend.delete(d.id); } };
const call = (method, params = {}) => new Promise((r) => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
await call('Emulation.setDeviceMetricsOverride', { width: +width, height: +height, deviceScaleFactor: 1, mobile: false });
await call('Page.bringToFront');
await new Promise((r) => setTimeout(r, +waitMs));
const shot = await call('Page.captureScreenshot', { format: 'png' });
fs.writeFileSync(out, Buffer.from(shot.result.data, 'base64'));
const txt = await call('Runtime.evaluate', { expression: "document.getElementById('stats').textContent + ' | ' + document.getElementById('counters').innerText.replace(/\\n/g, ' ')", returnByValue: true });
console.log(txt.result.result.value);
await fetch(`${base}/json/close/${tab.id}`);
ws.close();
