#!/usr/bin/env python3
"""ios-watch.py <timeout-sec> app-args...

Launches PandaWeb on the iPhone and prints one line per *event* the moment it happens (for use under Monitor):
  LOCKED   - the phone is locked, launch refused
  START / FIRST_FRAME / PROGRESS (every 60 frames) / DONE / ERROR / WEBCONTENT_KILLED
  STALL    - no PANDA line for STALL_SEC seconds; shows the last crumb (guest pc, kind) and the last frame number
  LOOP     - the same guest pc keeps showing up in crumbs (the guest/host is spinning there)
  END      - the console closed (exit code)
Full raw log: /tmp/ios-watch-last.txt
"""
import subprocess, sys, time, json, threading, queue, collections, os
UDID = os.environ.get('IOS_UDID') or sys.exit('set IOS_UDID to the device UDID (xcrun devicectl list devices)')
BUNDLE = os.environ.get('IOS_BUNDLE', 'com.example.pandaweb')
STALL_SEC = float(os.environ.get('STALL_SEC', 5))
tmo = float(sys.argv[1]); args = sys.argv[2:]
raw = open('/tmp/ios-watch-last.txt', 'w')
subprocess.run(['pkill', '-f', 'devicectl device process launch'], capture_output=True)   # no overlapping launches
p = subprocess.Popen(['xcrun', 'devicectl', 'device', 'process', 'launch', '--device', UDID, '--console', '--terminate-existing', BUNDLE, '--'] + args,
                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace', bufsize=1)
q = queue.Queue()
threading.Thread(target=lambda: [q.put(l) for l in p.stdout] or q.put(None), daemon=True).start()
def out(*a): print(*a, flush=True)

last_tick_print = 0; done_at = None; t0 = time.time(); last_line = t0; last_frame = -1; last_crumb = None; stalled = False; got_any = False
seen_logs = {}
pcs = collections.deque(maxlen=12); loop_reported = set(); crumbs = 0
while True:
    now = time.time()
    if done_at and now - done_at > 1.0 and not any('playlist' in a for a in sys.argv):
        p.kill(); break
    if now - t0 > tmo:
        out('TIMEOUT after %.0fs (killing console); last frame %d, last crumb %s' % (now - t0, last_frame, last_crumb)); p.kill(); break
    try: l = q.get(timeout=0.5)
    except queue.Empty:
        if got_any and not stalled and now - last_line > STALL_SEC:
            stalled = True
            out('STALL no output for %.0fs: last frame %d, last crumb %s, crumbs seen %d' % (now - last_line, last_frame, last_crumb, crumbs))
        continue
    if l is None: break
    raw.write(l); raw.flush()
    if 'Locked' in l or 'could not be, unlocked' in l:
        out('LOCKED the phone is locked - unlock it'); continue
    if 'PANDA|' not in l:
        if 'ERROR' in l or 'failed' in l.lower(): out('LAUNCH-MSG ' + l.strip()[:200])
        continue
    try: m = json.loads(l.split('PANDA|', 1)[1])
    except Exception: continue
    got_any = True
    t = m.get('type')
    if t not in ('native', 'heartbeat', 'tick'):   # the app's own heartbeats keep coming while the page is hung - they must not reset the stall timer
        last_line = now
        if stalled: out('RESUMED after stall'); stalled = False
    if t == 'crumb':
        crumbs += 1; last_crumb = {'k': m['k'], 'pc': '%08x' % m['pc'], 'x': m['x']}
        if m['k'] == 2:
            pcs.append(m['pc'])
            if len(pcs) == pcs.maxlen:
                c = collections.Counter(pcs).most_common(1)[0]
                if c[1] >= 10 and c[0] not in loop_reported: loop_reported.add(c[0]); out('LOOP guest pc %08x in %d of the last %d dispatch crumbs' % (c[0], c[1], len(pcs)))
    elif t == 'frame':
        last_frame = m.get('n', last_frame)
        if last_frame == 1: out('FIRST_FRAME %.1fs after launch ms=%s' % (now - t0, m.get('ms')))
        elif last_frame % 60 == 0: out('PROGRESS frame %d ms=%s' % (last_frame, m.get('ms')))
    elif t == 'tick':
        if now - last_tick_print >= 5:
            last_tick_print = now
            out('TICK frame %s speed %s fps  emu %s ms  loop %s Hz  modules %s  wasm %s MB' % (m.get('frames'), m.get('speed'), m.get('emuMs'), m.get('loopHz'), m.get('blocks'), m.get('wasmMB')))
    elif t == 'shot':
        import base64
        png = '/tmp/shot-%s.png' % m.get('name', 'run')
        open(png, 'wb').write(base64.b64decode(m['png'].split(',', 1)[1])); out('SHOT saved ' + png)
    elif t == 'playlist_start': out('PLAYLIST START %s items' % m.get('count'))
    elif t == 'playlist_item': out('%s %s/%s %s :: %s %s (%ss)' % ('OK  ' if m.get('result') == 'DONE' else 'FAIL', m.get('i'), m.get('of'), m.get('name'), m.get('result'), m.get('detail', '')[:150], m.get('sec')))
    elif t == 'playlist_done': out('PLAYLIST DONE: %s ok, %s failed of %s' % (m.get('ok'), m.get('failed'), m.get('count')))
    elif t == 'launch': out('START ' + json.dumps(m)[:200])
    elif t == 'webcontent_terminated': out('WEBCONTENT_KILLED ' + json.dumps(m)[:200])
    elif t in ('error',): out('ERROR ' + json.dumps(m)[:300])
    elif t == 'done':
        got_any = False   # finished: no more stall alarms
        done_at = now      # single-run mode: do not wait for devicectl to close the console
        out('DONE fps=%s avg=%sms worst=%sms modules=%s jitKB=%s wasmMB=%s' % (m.get('fps'), m.get('avgMs'), m.get('worstMs'), m.get('blocks'), m.get('jitKB'), m.get('wasmMB')))
    elif t in ('stress',):
        if m.get('i', 0) % 2048 == 0: out('STRESS %s' % json.dumps(m))
    elif t == 'log':
        msg = str(m.get('msg', ''))
        if any(w in msg.lower() for w in ('fail', 'error', 'unimplemented', 'fatal', 'warning', 'not found', 'abort', 'exception')):
            key = msg[:80]
            seen_logs[key] = seen_logs.get(key, 0) + 1
            if seen_logs[key] <= 2: out('LOG ' + msg[:200])
rc = p.poll()
out('END exit=%s total %.0fs; last frame %d, last crumb %s' % (rc, time.time() - t0, last_frame, last_crumb))
