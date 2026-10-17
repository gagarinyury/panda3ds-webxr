// glrec.js: records the WebGL calls of ONE emulated frame (the first one at or after `recstart` with at least `recdraws` draw calls) together with the
// metadata needed to rebuild every GL object it touches (shader sources, program links, texture / buffer / renderbuffer sizes, framebuffer attachments),
// then posts it as JSON to /report?name=glframe. web/probe-glreplay.html replays it in a bare page, with no emulator, to measure what the browser + GPU
// make of the real stream of calls. Load with `?glrec=1&recstart=1500&recdraws=100`. Texture contents are replaced by zeros (they do not change the cost).
(() => {
  const P = WebGL2RenderingContext.prototype;
  const params = new URLSearchParams(location.search);
  const START = +(params.get('recstart') || 1500), MIN_DRAWS = +(params.get('recdraws') || 100);
  const ids = new WeakMap(); let nextId = 1; const meta = new Map();   // object -> id; id -> {type, m}
  const idOf = (o, type) => { let i = ids.get(o); if (!i) { i = nextId++; ids.set(o, i); meta.set(i, { type: type || o.constructor.name.replace(/^WebGL/, ''), m: {} }); } return i; };
  const M = (o) => meta.get(idOf(o)).m;
  const bound = { tex: [], unit: 0, buf: {}, fbo: {}, rb: null };
  let rec = null, done = false, referenced = null, draws = 0;
  const SKIP = /^(get|is|checkFramebufferStatus)/, KEEP_GET = /^getUniformLocation$/;
  const bytesFor = (w, h) => Math.max(16, w * h * 16);   // generous: the data is thrown away at replay
  const enc = (name, a, i, args) => {
    if (a === null || a === undefined || typeof a === 'number' || typeof a === 'boolean' || typeof a === 'string') return a;
    if (ArrayBuffer.isView(a)) {
      if (/^(texImage|texSubImage|readPixels|compressedTex)/.test(name)) return { z: bytesFor(args[3] || 64, args[4] || 64) };   // content irrelevant
      let bytes;
      if (a.byteLength > (8 << 20)) {   // Emscripten passes the whole heap plus (srcOffset, length) for buffer uploads
        const es = a.BYTES_PER_ELEMENT, off = args[3] | 0, len = args[4];   // bufferData(target, view, usage, srcOffset, length) / bufferSubData(target, dstOffset, view, srcOffset, length)
        bytes = new Uint8Array(a.buffer, a.byteOffset + off * es, (len === undefined ? 0 : len) * es);
      } else bytes = new Uint8Array(a.buffer, a.byteOffset, a.byteLength);
      let s = ''; const c = bytes.slice(); for (let k = 0; k < c.length; k += 0x8000) s += String.fromCharCode.apply(null, c.subarray(k, k + 0x8000));
      return { t: a.constructor.name, b: btoa(s) };
    }
    if (Array.isArray(a)) return a.map((x) => (typeof x === 'object' && x ? enc(name, x, i, args) : x));
    if (typeof a === 'object') { const id = idOf(a); referenced.add(id); return { o: id }; }
    return a;
  };
  const MAXARGS = { bufferData: 3, bufferSubData: 3, texImage2D: 9, texSubImage2D: 9 };   // drop the srcOffset / length tail after the data was resolved
  for (const k of Object.getOwnPropertyNames(P)) {
    let orig; try { orig = P[k]; } catch (e) { continue; } if (typeof orig !== 'function' || k === 'constructor') continue;
    const isGet = SKIP.test(k) && !KEEP_GET.test(k);
    P[k] = function (...a) {
      const r = orig.apply(this, a);
      // ---- always-on metadata ----
      switch (k) {
        case 'createShader': meta.get(idOf(r, 'Shader')).m.stype = a[0]; break;
        case 'shaderSource': M(a[0]).src = a[1]; break;
        case 'createProgram': idOf(r, 'Program'); (M(r).shaders = []); M(r).attribs = []; M(r).ubb = []; break;
        case 'attachShader': M(a[0]).shaders.push(idOf(a[1])); break;
        case 'bindAttribLocation': M(a[0]).attribs.push([a[1], a[2]]); break;
        case 'uniformBlockBinding': M(a[0]).ubb.push([a[1], a[2]]); break;
        case 'getUniformLocation': if (r) { const i = idOf(r, 'UniformLocation'); meta.get(i).m.prog = idOf(a[0]); meta.get(i).m.name = a[1]; } break;
        case 'activeTexture': bound.unit = a[0] - 0x84C0; break;
        case 'bindTexture': (bound.tex[bound.unit] || (bound.tex[bound.unit] = {}))[a[0]] = a[1]; break;
        case 'texImage2D': case 'texStorage2D': {
          const t = (bound.tex[bound.unit] || {})[a[0]]; if (!t) break; const m = M(t);
          if (k === 'texStorage2D') { m.target = a[0]; m.storage = true; m.levels = a[1]; m.ifmt = a[2]; m.w = a[3]; m.h = a[4]; }
          else if (a[1] === 0 && typeof a[3] === 'number') { m.target = a[0]; m.ifmt = a[2]; m.w = a[3]; m.h = a[4]; m.format = a[6]; m.type = a[7]; }
          break;
        }
        case 'texParameteri': { const t = (bound.tex[bound.unit] || {})[a[0]]; if (t) (M(t).params || (M(t).params = {}))[a[1]] = a[2]; break; }
        case 'bindBuffer': bound.buf[a[0]] = a[1]; break;
        case 'bufferData': { const b = bound.buf[a[0]]; if (b) { const m = M(b); m.target = a[0]; m.usage = a[2]; m.size = typeof a[1] === 'number' ? a[1] : (arguments.length >= 5 ? a[4] * (a[1].BYTES_PER_ELEMENT || 1) : a[1].byteLength); } break; }
        case 'bindFramebuffer': bound.fbo[a[0]] = a[1]; break;
        case 'bindRenderbuffer': bound.rb = a[1]; break;
        case 'renderbufferStorage': case 'renderbufferStorageMultisample': {
          if (bound.rb) { const m = M(bound.rb), ms = k === 'renderbufferStorageMultisample'; m.samples = ms ? a[1] : 0; m.ifmt = a[ms ? 2 : 1]; m.w = a[ms ? 3 : 2]; m.h = a[ms ? 4 : 3]; }
          break;
        }
        case 'framebufferTexture2D': { const f = bound.fbo[a[0]] || bound.fbo[0x8D40]; if (f && a[3]) (M(f).att || (M(f).att = {}))[a[1]] = { kind: 'tex', tex: idOf(a[3]), level: a[4], textarget: a[2] }; break; }
        case 'framebufferRenderbuffer': { const f = bound.fbo[a[0]] || bound.fbo[0x8D40]; if (f && a[3]) (M(f).att || (M(f).att = {}))[a[1]] = { kind: 'rb', rb: idOf(a[3]) }; break; }
        case 'createTexture': case 'createBuffer': case 'createFramebuffer': case 'createRenderbuffer': case 'createVertexArray': case 'createQuery': case 'createSampler': idOf(r); break;
        default: break;
      }
      // ---- frame recording ----
      if (rec && !isGet) {
        if (/^draw/.test(k)) draws++;
        const args = a.map((x, i) => enc(k, x, i, a));
        const keep = MAXARGS[k] ? args.slice(0, MAXARGS[k]) : args;
        const e = [k, keep]; if (r && typeof r === 'object') { const id = idOf(r); referenced.add(id); e.push(id); }
        rec.push(e);
      }
      return r;
    };
  }
  const closure = (ids0) => {   // objects the frame needs, with everything they were built from
    const out = new Set(); const add = (i) => { if (!i || out.has(i)) return; out.add(i); const o = meta.get(i); if (!o) return; const m = o.m;
      (m.shaders || []).forEach(add); if (m.prog) add(m.prog); Object.values(m.att || {}).forEach((x) => { add(x.tex); add(x.rb); }); };
    ids0.forEach(add); return [...out].map((i) => ({ id: i, type: meta.get(i).type, m: meta.get(i).m }));
  };
  window.__glrec = {
    begin(frames) { if (!done && frames >= START && !rec) { rec = []; referenced = new Set(); draws = 0; } },
    end(frames) {
      if (!rec) return;
      if (draws < MIN_DRAWS) { rec = null; return; }
      done = true; const calls = rec; rec = null;
      const counts = {}; calls.forEach((c) => { counts[c[0]] = (counts[c[0]] || 0) + 1; });
      const out = { frame: frames, draws, calls, objs: closure(referenced), counts };
      const body = JSON.stringify(out);
      window.__glrecInfo = { frame: frames, draws, nCalls: calls.length, nObjs: out.objs.length, bytes: body.length, counts };
      const nat = window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.panda;
      if (nat) nat.postMessage(JSON.stringify({ type: 'log', name: 'glrec', msg: '[glrec] recorded frame ' + frames + ': ' + draws + ' draws, ' + calls.length + ' calls, ' + body.length + ' bytes, ' + out.objs.length + ' objects' }));
      try {   // keep it in this origin's IndexedDB: probe-glreplay.html?file=idb reads it back (works inside the iPhone app, where there is no server)
        const rq = indexedDB.open('glrec', 1); rq.onupgradeneeded = () => rq.result.createObjectStore('frames');
        rq.onsuccess = () => { const tx = rq.result.transaction('frames', 'readwrite'); tx.objectStore('frames').put(body, 'glframe'); tx.oncomplete = () => { window.__glrecSent = true; if (nat) nat.postMessage(JSON.stringify({ type: 'log', name: 'glrec', msg: '[glrec] saved to IndexedDB' })); }; };
      } catch (e) { }
      fetch('/report?name=glframe', { method: 'POST', body }).catch(() => {});
    },
  };
})();
