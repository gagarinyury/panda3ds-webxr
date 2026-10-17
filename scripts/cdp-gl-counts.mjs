// cdp-gl-counts.mjs <url> <startFrame> [frames=60] [port]: draws, UBO/vertex/index buffer uploads and bindBufferRange per frame (uboBytes is not reliable)
const [url, start, n = '60', port = '9333'] = process.argv.slice(2);
const base = `http://127.0.0.1:${port}`;
const tab = await (await fetch(`${base}/json/new?about:blank`, { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl); await new Promise((r) => (ws.onopen = r));
let id = 0; const pend = new Map();
ws.onmessage = (m) => { const d = JSON.parse(m.data); if (d.id && pend.has(d.id)) { pend.get(d.id)(d); pend.delete(d.id); } };
const call = (method, params = {}) => new Promise((r) => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const hook = `(() => { const P = WebGL2RenderingContext.prototype; window.__c = { draws: 0, ubo: 0, uboBytes: 0, vtx: 0, idx: 0, bindRange: 0 };
 for (const k of ['drawArrays','drawElements','drawArraysInstanced','drawElementsInstanced','drawRangeElements']) { const o = P[k]; P[k] = function () { __c.draws++; return o.apply(this, arguments); }; }
 const bd = P.bufferData, bsd = P.bufferSubData;
 const f = function (t, n) { if (t === 35345) { __c.ubo++; __c.uboBytes += n; } else if (t === 34962) __c.vtx++; else if (t === 34963) __c.idx++; };
 P.bufferData = function (t, d) { f(t, typeof d === 'number' ? d : d.byteLength); return bd.apply(this, arguments); };
 P.bufferSubData = function (t, o, d) { f(t, arguments.length >= 5 ? arguments[4] : d.byteLength); return bsd.apply(this, arguments); };
 const br = P.bindBufferRange; P.bindBufferRange = function () { __c.bindRange++; return br.apply(this, arguments); }; })();`;
await call('Page.enable'); await call('Page.addScriptToEvaluateOnNewDocument', { source: hook }); await call('Page.navigate', { url });
const ev = async (e) => (await call('Runtime.evaluate', { expression: e, returnByValue: true })).result.result.value;
const w = async (f) => { while (true) { const v = await ev('typeof frames==="number"?frames:0'); if (v >= f) return v; await new Promise((r) => setTimeout(r, 100)); } };
await w(+start); const a = JSON.parse(await ev('JSON.stringify({f: frames, c: __c})')); await w(+start + +n); const b = JSON.parse(await ev('JSON.stringify({f: frames, c: __c})'));
const fr = b.f - a.f, o = {}; for (const k in b.c) o[k] = +((b.c[k] - a.c[k]) / fr).toFixed(1);
console.log(JSON.stringify({ frames: fr, perFrame: o, uboUploadsPerDraw: +((b.c.ubo - a.c.ubo) / (b.c.draws - a.c.draws)).toFixed(2) }));
await fetch(`${base}/json/close/${tab.id}`); ws.close();
