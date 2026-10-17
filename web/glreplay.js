// glreplay.js: rebuilds the GL objects of a frame recorded by glrec.js and returns a function that replays its calls (shared by probe-glreplay.html and index.html?rp=1)
function makeReplayer(gl, data, skip, cpu, heapMode) {
  const objs = new Map(data.objs.map((x) => [x.id, x])), real = new Map(), zeros = new Map();
  const build = (id) => {
    if (real.has(id)) return real.get(id);
    const x = objs.get(id); if (!x) return null; const m = x.m; let r = null;
    switch (x.type) {
      case 'Shader': r = gl.createShader(m.stype); gl.shaderSource(r, m.src); gl.compileShader(r); break;
      case 'Program': r = gl.createProgram(); (m.shaders || []).forEach((s) => gl.attachShader(r, build(s))); (m.attribs || []).forEach(([i, n]) => gl.bindAttribLocation(r, i, n)); gl.linkProgram(r); (m.ubb || []).forEach(([i, b]) => gl.uniformBlockBinding(r, i, b)); break;
      case 'UniformLocation': r = gl.getUniformLocation(build(m.prog), m.name); break;
      case 'Texture': r = gl.createTexture(); if (m.w) { gl.bindTexture(m.target, r); if (m.storage) gl.texStorage2D(m.target, m.levels || 1, m.ifmt, m.w, m.h); else gl.texImage2D(m.target, 0, m.ifmt, m.w, m.h, 0, m.format, m.type, null); for (const [k, v] of Object.entries(m.params || {})) gl.texParameteri(m.target, +k, v); } break;
      case 'Buffer': r = gl.createBuffer(); if (m.size) { gl.bindBuffer(m.target, r); gl.bufferData(m.target, m.size, m.usage || gl.STREAM_DRAW); } break;
      case 'Renderbuffer': r = gl.createRenderbuffer(); if (m.w) { gl.bindRenderbuffer(gl.RENDERBUFFER, r); if (m.samples) gl.renderbufferStorageMultisample(gl.RENDERBUFFER, m.samples, m.ifmt, m.w, m.h); else gl.renderbufferStorage(gl.RENDERBUFFER, m.ifmt, m.w, m.h); } break;
      case 'Framebuffer': r = gl.createFramebuffer(); gl.bindFramebuffer(gl.FRAMEBUFFER, r);
        for (const [att, a] of Object.entries(m.att || {})) { if (a.kind === 'tex') gl.framebufferTexture2D(gl.FRAMEBUFFER, +att, a.textarget, build(a.tex), a.level); else gl.framebufferRenderbuffer(gl.FRAMEBUFFER, +att, gl.RENDERBUFFER, build(a.rb)); } break;
      case 'VertexArrayObject': r = gl.createVertexArray(); break;
      case 'Query': r = gl.createQuery(); break;
      case 'Sampler': r = gl.createSampler(); break;
      default: r = null;
    }
    real.set(id, r); return r;
  };
  for (const id of objs.keys()) build(id);
  gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  const T = { Float32Array, Uint8Array, Uint16Array, Uint32Array, Int32Array, Int16Array, Int8Array, Float64Array };
  const dec = (a) => {
    if (a === null || typeof a !== 'object') return a;
    if (Array.isArray(a)) return a.map(dec);
    if (a.o !== undefined) return real.get(a.o) || null;
    if (a.z !== undefined) { let z = zeros.get(a.z); if (!z) { z = new Uint8Array(a.z); zeros.set(a.z, z); } return z; }
    if (a.t !== undefined) { const s = atob(a.b), u = new Uint8Array(s.length); for (let i = 0; i < s.length; i++) u[i] = s.charCodeAt(i); return new T[a.t](u.buffer, 0, u.length / T[a.t].BYTES_PER_ELEMENT); }
    return a;
  };
  // decode once (typed data is shared between iterations), keep the creating calls separate so they run only on the first pass
  const calls = data.calls.filter((c) => !skip.has(c[0])).map((c) => ({ n: c[0], a: c[1].map(dec), ret: c[2] }));
  if (heapMode) {   // the Emscripten way: every upload is a view of one big memory plus (srcOffset, length) - the data is copied into that memory first
    const mem = new WebAssembly.Memory({ initial: 4096 }), heap = new Uint8Array(mem.buffer); let top = 4096;
    const put = (v) => { top = (top + 15) & ~15; const n = v.byteLength; heap.set(new Uint8Array(v.buffer, v.byteOffset, n), top); const o = top; top += n; return [o, n]; };
    for (const c of calls) {
      if (c.n === 'bufferData' && ArrayBuffer.isView(c.a[1])) { const [o, n] = put(c.a[1]); c.a = [c.a[0], heap, c.a[2], o, n]; }
      else if (c.n === 'bufferSubData' && ArrayBuffer.isView(c.a[2])) { const [o, n] = put(c.a[2]); c.a = [c.a[0], c.a[1], heap, o, n]; }
      else if (/^uniform/.test(c.n)) { c.a = c.a.map((x) => { if (!ArrayBuffer.isView(x)) return x; const [o, n] = put(x); return new x.constructor(mem.buffer, o, n / x.BYTES_PER_ELEMENT); }); }
    }
  }
  let first = true;
  const prof = {}; let profOn = false;
  const f = function frame() {
    const t0 = performance.now(); if (cpu) while (performance.now() - t0 < cpu) { }
    for (let i = 0; i < calls.length; i++) {
      const c = calls[i];
      if (c.ret !== undefined) { if (!first) continue; const r = gl[c.n](...c.a); real.set(c.ret, r); continue; }
      if (profOn) { const a = performance.now(); gl[c.n](...c.a); const e = prof[c.n] || (prof[c.n] = { n: 0, ms: 0 }); e.n++; e.ms += performance.now() - a; } else gl[c.n](...c.a);
    }
    first = false;
  };
  f.prof = prof; f.setProf = (v) => { profOn = v; };
  return f;
}
