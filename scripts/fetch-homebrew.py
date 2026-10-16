#!/usr/bin/env python3
"""fetch-homebrew.py [repo ...]: downloads open-source 3DS homebrew (.3dsx) listed in Universal-DB into run/homebrew/.

Source list: https://db.universal-team.net/data/full.json (scraped from the projects' GitHub releases).
Writes run/homebrew/SOURCES.txt (repo, license, url, size, sha256). run/homebrew/ is git-ignored; the binaries are only ever run inside the emulator.
"""
import hashlib, json, os, sys, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'run', 'homebrew')
DEFAULT = ['PrBoom-Plus-3DS', 'GameYob-3DS', 'snes9x_3ds', 'Minicraft3DS', 'Xash3DS', 'Yahtzee3DS', 'Snake-for-3DS', '3DEins', 'atari800', 'ClassiCube']
want = sys.argv[1:] or DEFAULT
os.makedirs(OUT, exist_ok=True)
db = json.load(urllib.request.urlopen('https://db.universal-team.net/data/full.json', timeout=60))
entries = [e for e in db if '3DS' in (e.get('systems') or [])]
lines = []
for key in want:
    e = next((x for x in entries if (x.get('github') or '').split('/')[-1].lower() == key.lower()), None)
    if not e:
        print('NOT IN DB:', key); continue
    dl = [(k, v) for k, v in (e.get('downloads') or {}).items() if k.endswith('.3dsx')]
    if not dl:
        print('NO .3dsx:', key); continue
    name, info = dl[0]
    dest = os.path.join(OUT, key + '.3dsx')
    data = urllib.request.urlopen(info['url'], timeout=120).read()
    open(dest, 'wb').write(data)
    sha = hashlib.sha256(data).hexdigest()
    lines.append('%s | %s | %s | %d bytes | sha256 %s' % (e['github'], e.get('license') or 'no license stated', info['url'], len(data), sha))
    print('OK  %-18s %8d bytes  license=%s  (%s)' % (key, len(data), e.get('license') or '-', e['github']))
open(os.path.join(OUT, 'SOURCES.txt'), 'a').write('\n'.join(lines) + '\n')
