// cdp-dispatch-stats.mjs <url> <startFrame> [frames=120] [port]: dispatcher returns (jit_js_run) and guest ticks per frame, from Module.jit.s (needs the counters in dynarmic a32_interface.cpp)
const [url, start, n = '120', port = '9333'] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const tab = await (await fetch(`${base}/json/new?${encodeURIComponent(url)}`, { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pend = new Map();
ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d); pend.delete(d.id); } };
const call = (method, params = {}) => new Promise((r) => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async (e) => (await call('Runtime.evaluate', { expression: e, returnByValue: true })).result.result.value;
const snap = () => ev('JSON.stringify({f: frames, s: Module.jit.s, b: Module.jit.impls[0].count})');
const w = async (f) => { while ((await ev('frames')) < f) await new Promise((r) => setTimeout(r, 100)); };
await w(+start); const a = JSON.parse(await snap()); await w(+start + +n); const b = JSON.parse(await snap());
const fr = b.f - a.f, d = b.s[0] - a.s[0], t = b.s[1] - a.s[1], runs = b.s[2] - a.s[2];
console.log(JSON.stringify({ frames: fr, runCallsPerFrame: +(runs / fr).toFixed(1), dispatchesPerFrame: +(d / fr).toFixed(0), ticksPerFrame: +(t / fr).toFixed(0), ticksPerDispatch: +(t / d).toFixed(1), newBlocks: b.b - a.b, msInRunPerFrame: +((b.s[3] - a.s[3]) / fr).toFixed(2), msInCallbacksPerFrame: +((b.s[4] - a.s[4]) / fr).toFixed(2), guestMsPerFrame: +((b.s[3] - a.s[3] - b.s[4] + a.s[4]) / fr).toFixed(2), nsPerTick: +(1e6 * (b.s[3] - a.s[3] - b.s[4] + a.s[4]) / t).toFixed(1) }));
await fetch(`${base}/json/close/${tab.id}`); ws.close();
