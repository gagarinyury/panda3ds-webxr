#!/usr/bin/env python3
"""Runs every run/examples/*.3dsx headless for a number of frames and writes docs/examples-status.md (+ screenshots in run/shots)."""
import concurrent.futures, os, re, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUN = os.path.join(ROOT, 'run')
FRAMES = int(sys.argv[1]) if len(sys.argv) > 1 else 90
TIMEOUT = int(sys.argv[2]) if len(sys.argv) > 2 else 120
os.makedirs(os.path.join(RUN, 'shots'), exist_ok=True)

def run_one(name):
    base = os.path.join('shots', name)
    t0 = time.time()
    try:
        p = subprocess.run(['node', '../build-core/AlberHeadless.js', os.path.join('examples', name + '.3dsx'), str(FRAMES), base],
                           cwd=RUN, capture_output=True, text=True, timeout=TIMEOUT, errors='replace')
        out = (p.stdout or '') + (p.stderr or '')
        status = 'exit%d' % p.returncode
    except subprocess.TimeoutExpired as e:
        out = ((e.stdout or b'').decode(errors='replace') if isinstance(e.stdout, bytes) else (e.stdout or '')) + ((e.stderr or b'').decode(errors='replace') if isinstance(e.stderr, bytes) else (e.stderr or ''))
        status = 'timeout'
    lines = [l for l in out.splitlines() if len(l) < 400]
    fatal = next((l for l in lines if '[FATAL]' in l or 'C++ exception' in l or 'RuntimeError' in l or 'Aborted' in l), '')
    jit = sorted({re.sub(r'block at 0x[0-9a-f]+: ', '', l.split('] ', 1)[1]) for l in lines if l.startswith('[jit] block at')})
    done = next((l for l in lines if '[headless] done' in l), '')
    top = next((int(re.search(r'\((\d+) non-black', l).group(1)) for l in lines if 'top screen' in l and 'non-black' in l), None)
    bot = next((int(re.search(r'\((\d+) non-black', l).group(1)) for l in lines if 'bottom screen' in l and 'non-black' in l), None)
    ms = re.search(r'\(([\d.]+) ms/frame', done)
    for scr in ('top', 'bottom'):
        ppm = os.path.join(RUN, base + '-' + scr + '.ppm')
        if os.path.exists(ppm):
            subprocess.run([sys.executable, '-I', os.path.join(ROOT, 'scripts/ppm2png.py'), ppm, ppm[:-4] + '.png', '1'])
            os.remove(ppm)
    return dict(name=name, status=status, ran=bool(done), fatal=fatal.strip()[:140], jit='; '.join(jit)[:200], top=top, bot=bot,
                mspf=float(ms.group(1)) if ms else None, wall=time.time() - t0)

names = sorted(f[:-5] for f in os.listdir(os.path.join(RUN, 'examples')) if f.endswith('.3dsx'))
only = os.environ.get('ONLY')
if only: names = [n for n in names if re.search(only, n)]
with concurrent.futures.ThreadPoolExecutor(max_workers=int(os.environ.get('JOBS', '4'))) as ex:
    results = list(ex.map(run_one, names))

os.makedirs(os.path.join(ROOT, 'docs'), exist_ok=True)
with open(os.path.join(ROOT, 'docs/examples-status.md'), 'w') as f:
    f.write('# devkitPro 3ds-examples on Panda3DS + WASM JIT (headless, Null renderer, %d frames)\n\n' % FRAMES)
    ran = [r for r in results if r['ran']]
    f.write('%d of %d examples ran the requested frames without a fatal error.\n\n' % (len(ran), len(results)))
    f.write('| example | result | top px | bottom px | ms/frame | first problem | missing JIT ops |\n|---|---|---:|---:|---:|---|---|\n')
    for r in results:
        res = 'ran' if r['ran'] else r['status']
        f.write('| %s | %s | %s | %s | %s | %s | %s |\n' % (r['name'], res, r['top'] if r['top'] is not None else '', r['bot'] if r['bot'] is not None else '',
                '%.1f' % r['mspf'] if r['mspf'] else '', r['fatal'].replace('|', '/'), r['jit'].replace('|', '/')))
for r in results:
    print('%-44s %-9s top=%-6s bot=%-6s %s%s' % (r['name'], 'ran' if r['ran'] else r['status'], r['top'], r['bot'], r['fatal'][:90], (' JIT:' + r['jit']) if r['jit'] else ''))
