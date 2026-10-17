// cdp-bufmem.mjs <url> <seconds> [port=9333]: counts GPU buffer bytes allocated through bufferData (per buffer, latest size wins)
const [url, secs = '60', port = '9333'] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const tab = await (await fetch(`${base}/json/new?about:blank`, { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pend = new Map();
ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d); pend.delete(d.id); } };
const call = (method, params = {}) => new Promise((r) => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const hook = `(() => {
  const P = WebGL2RenderingContext.prototype, bound = {}, size = new Map();
  const bb = P.bindBuffer, bd = P.bufferData, db = P.deleteBuffer;
  P.bindBuffer = function (t, b) { bound[t] = b; return bb.call(this, t, b); };
  P.bufferData = function (t, d) { const b = bound[t]; if (b) size.set(b, typeof d === 'number' ? d : d.byteLength === undefined ? 0 : (arguments.length >= 5 ? arguments[4] : d.byteLength)); return bd.apply(this, arguments); };
  P.deleteBuffer = function (b) { size.delete(b); return db.call(this, b); };
  window.__bufmem = () => { let tot = 0, mx = 0; for (const v of size.values()) { tot += v; if (v > mx) mx = v; } return { buffers: size.size, MB: +(tot / 1048576).toFixed(1), maxKB: Math.round(mx / 1024) }; };
})();`;
await call('Page.enable'); await call('Page.addScriptToEvaluateOnNewDocument', { source: hook });
await call('Page.navigate', { url });
const t0 = Date.now();
while (Date.now() - t0 < +secs * 1000) {
  await new Promise((r) => setTimeout(r, 10000));
  const r = await call('Runtime.evaluate', { expression: `JSON.stringify({frames: typeof frames==='number'?frames:-1, ...(window.__bufmem?window.__bufmem():{})})`, returnByValue: true });
  console.log(Math.round((Date.now() - t0) / 1000) + 's', r.result.result.value);
}
await fetch(`${base}/json/close/${tab.id}`); ws.close();
