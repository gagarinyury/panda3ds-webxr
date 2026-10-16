#!/usr/bin/env python3
"""Runs examples in a real browser (default Safari) through the local report server: scripts/web-examples.py [browser] name1 name2 ...
Needs scripts/serve-web.py running on 8765. Prints one line per example and writes run/web-reports/<browser>-<name>.json/png."""
import json, os, subprocess, sys, time
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'run', 'web-reports')
browser = sys.argv[1]
names = sys.argv[2:]
frames = os.environ.get('FRAMES', '90')
for n in names:
    key = '%s-%s' % (browser.lower(), n)
    f = os.path.join(OUT, key + '.json')
    if os.path.exists(f): os.remove(f)
    url = 'http://127.0.0.1:8765/web/index.html?rom=/run/examples/%s.3dsx&frames=%s&report=1&name=%s' % (n, frames, key)
    subprocess.run(['open', '-g', '-a', browser, url])
    t0 = time.time()
    while not os.path.exists(f) and time.time() - t0 < 120: time.sleep(0.5)
    if os.path.exists(f):
        time.sleep(0.5); r = json.load(open(f))
        print('%-44s frames=%-4d avg=%6.2f ms worst=%6.1f ms top=%-5d bot=%-5d errors=%s' % (n, r['frames'], r['avgMs'], r['worstMs'], r['topLit'], r['bottomLit'], r['errors'][:2]), flush=True)
    else:
        print('%-44s NO REPORT (timeout)' % n, flush=True)
    subprocess.run(['osascript', '-e', 'tell application "%s" to close front document' % browser], capture_output=True)
