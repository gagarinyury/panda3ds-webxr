#!/usr/bin/env python3
"""safari-ab.py name=querystring ... : opens each variant of web/index.html in real Safari (WebKit) on this Mac, one after another (60 s each),
reads the page's /live telemetry (run/web-reports/live-<name>.jsonl) and prints loop / paint rate, tick cost and speed after frame 600.
Needs scripts/serve-web.py running on 8777. Closes the Safari window after each run."""
import json, os, subprocess, sys, time, statistics as S
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'run', 'web-reports')
BASE = 'rom=/run/commercial/sm3dl.3ds&gl=1' + ('&' + os.environ['ROUTE'] if os.environ.get('ROUTE') else '')   # ROUTE: e.g. ff=9800&fscript=A:900,A:1200 (presses A by frame number; fast-forwards to frame ff)
SECS = float(os.environ.get('SECS', 55))
for arg in sys.argv[1:]:
    name, q = arg.split('=', 1)
    path = os.path.join(OUT, f'live-{name}.jsonl')
    if os.path.exists(path): os.remove(path)
    page = 'web/index.html'
    if q.startswith('PAGE:'): page, q = q[5:].split('?', 1)[0], q.split('?', 1)[1]
    url = f'http://127.0.0.1:8777/{page}?{BASE}&name={name}&{q}' if page.endswith('index.html') else f'http://127.0.0.1:8777/{page}?name={name}&{q}'
    subprocess.run(['open', '-a', 'Safari', url]); time.sleep(SECS)
    subprocess.run(['osascript', '-e', 'tell application "Safari" to close front window'], capture_output=True)
    rows = []
    try: rows = [json.loads(l) for l in open(path) if l.strip()]
    except Exception: pass
    t = [r for r in rows if r.get('type') == 'tick' and r.get('frames', 0) > 600 and r.get('ticksFr') is not None]
    def fmt(g):
        if not g: return 'none'
        f = lambda k: round(S.mean(r[k] for r in g if r.get(k) is not None), 1)
        return f"n={len(g):3d} speed {f('speed'):5} loop {f('loopHz'):5} tick {f('tickMs'):5} emu {f('emuMs'):5} ticks/fr {round(S.mean(r['ticksFr'] for r in g)/1000)}k"
    light = [r for r in t if r['ticksFr'] < 400000]; heavy = [r for r in t if r['ticksFr'] >= 400000 and r['speed'] < 80]
    print(f"{name:12s} light: {fmt(light)}\n{'':12s} heavy: {fmt(heavy)}", flush=True)
