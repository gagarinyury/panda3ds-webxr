// cdp-trace.mjs <url> <out-prefix> [startFrame=9900] [traceSec=6] [port=9333]
// Waits until the emulator page reaches <startFrame>, records a Chromium trace (CDP Tracing) for <traceSec> seconds, saves <out-prefix>.json
// (open it in https://ui.perfetto.dev or chrome://tracing) and prints a summary: busy/idle time of the renderer main thread and the
// longest trace events on it (this shows what the thread waits for: GL command buffer flush, swap, timers, GC...).
import fs from 'node:fs';
const [url, prefix, startFrame = '9900', traceSec = '6', port = '9333'] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const tab = await (await fetch(`${base}/json/new?${encodeURIComponent(url)}`, { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pend = new Map(); const events = []; let traceDone;
const done = new Promise((r) => (traceDone = r));
ws.onmessage = (m) => { const d = JSON.parse(m.data);
  if (d.id && pend.has(d.id)) { pend.get(d.id)(d); pend.delete(d.id); }
  else if (d.method === 'Tracing.dataCollected') events.push(...d.params.value);
  else if (d.method === 'Tracing.tracingComplete') traceDone(); };
const call = (method, params = {}) => new Promise((r) => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (expression) => (await call('Runtime.evaluate', { expression, returnByValue: true })).result.result.value;
await call('Emulation.setDeviceMetricsOverride', { width: 1500, height: 520, deviceScaleFactor: 1, mobile: false });
const t0 = Date.now();
while (Date.now() - t0 < 900000) { const f = await ev('typeof frames === "number" ? frames : 0'); if (f >= +startFrame) break; if ((Date.now() - t0) % 20000 < 600) console.log('PROGRESS frame=' + f); await new Promise((r) => setTimeout(r, 500)); }
console.log('TRACING at frame ' + await ev('frames'));
await call('Tracing.start', { transferMode: 'ReportEvents', categories: 'toplevel,blink,cc,gpu,viz,v8,devtools.timeline,disabled-by-default-gpu.service,disabled-by-default-devtools.timeline', options: 'record-until-full' });
await new Promise((r) => setTimeout(r, +traceSec * 1000));
await call('Tracing.end'); await traceDone;
fs.writeFileSync(prefix + '.json', JSON.stringify(events));
console.log('TRACE saved ' + events.length + ' events -> ' + prefix + '.json');
const meta = events.filter((e) => e.ph === 'M' && e.name === 'thread_name');
const names = new Map(meta.map((e) => [e.pid + ':' + e.tid, e.args.name]));
const X = events.filter((e) => e.ph === 'X' && e.dur);
const byThread = new Map();
for (const e of X) { const k = e.pid + ':' + e.tid; const o = byThread.get(k) || { busy: 0, ev: new Map() }; o.busy += e.dur; const s = o.ev.get(e.name) || { n: 0, us: 0 }; s.n++; s.us += e.dur; o.ev.set(e.name, s); byThread.set(k, o); }
const span = Math.max(...X.map((e) => e.ts + e.dur)) - Math.min(...X.map((e) => e.ts));
const rows = [...byThread.entries()].map(([k, v]) => ({ k, name: names.get(k) || '?', top: v })).filter((r) => /CrRendererMain|CrGpuMain|VizCompositor|Compositor|GpuMain/.test(r.name));
for (const r of rows) {
  const top = [...r.top.ev.entries()].filter(([n]) => n !== 'ThreadControllerImpl::RunTask' && n !== 'ThreadControllerImpl::DoWork').sort((a, b) => b[1].us - a[1].us).slice(0, 8);
  console.log(`THREAD ${r.name} (${r.k}) toplevel-busy ${(r.top.busy / 1000).toFixed(0)} ms of ${(span / 1000).toFixed(0)} ms`);
  for (const [n, s] of top) console.log(`   ${(s.us / 1000).toFixed(1).padStart(8)} ms  x${String(s.n).padStart(5)}  ${n}`);
}
console.log('DONE');
await fetch(`${base}/json/close/${tab.id}`); ws.close(); process.exit(0);
