// cdp-cpuprof.mjs <url> <startFrame> <sec> [port=9333]: V8 sampling profile (self time per function) after the emulator reaches startFrame
const [url, startFrame, secs = '8', port = '9333'] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const tab = await (await fetch(`${base}/json/new?${encodeURIComponent(url)}`, { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pend = new Map();
ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d); pend.delete(d.id); } };
const call = (method, params = {}) => new Promise((r) => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (e) => (await call('Runtime.evaluate', { expression: e, returnByValue: true })).result.result.value;
while ((await ev('typeof frames==="number"?frames:0')) < +startFrame) await new Promise((r) => setTimeout(r, 500));
await call('Profiler.enable'); await call('Profiler.setSamplingInterval', { interval: 100 }); await call('Profiler.start');
await new Promise((r) => setTimeout(r, +secs * 1000));
const prof = (await call('Profiler.stop')).result.profile;
const self = new Map(); const dt = prof.timeDeltas; const byId = new Map(prof.nodes.map((n) => [n.id, n]));
let total = 0;
prof.samples.forEach((s, i) => { const n = byId.get(s); const cf = n.callFrame; const key = (cf.functionName || '(anon)') + ' ' + (cf.url ? cf.url.split('/').pop() : '') ; self.set(key, (self.get(key) || 0) + dt[i]); total += dt[i]; });
const rows = [...self.entries()].sort((a, b) => b[1] - a[1]);
const groups = { 'wasm-function (JIT blocks + Panda, no names)': 0, 'idle/program/gc': 0, other: 0 };
for (const [k, v] of rows) { if (/^wasm-function|^\d+ |^\(anon\) .*wasm/.test(k) || k.startsWith('wasm')) groups['wasm-function (JIT blocks + Panda, no names)'] += v; else if (/^\((idle|program|garbage collector)\)/.test(k)) groups['idle/program/gc'] += v; else groups.other += v; }
console.log('total ms', (total / 1000).toFixed(0));
for (const [k, v] of Object.entries(groups)) console.log(k, (100 * v / total).toFixed(1) + '%');
for (const [k, v] of rows.slice(0, 40)) console.log((100 * v / total).toFixed(1).padStart(5) + '%', k.slice(0, 110));
await fetch(`${base}/json/close/${tab.id}`); ws.close();
