#!/usr/bin/env python3
"""Tiny static server for the browser build (correct .wasm MIME, no caching) that also collects page reports: POST /report, /report-png."""
import http.server, json, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'run', 'web-reports')
os.makedirs(OUT, exist_ok=True)

class H(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, '.wasm': 'application/wasm', '.js': 'text/javascript', '.3dsx': 'application/octet-stream', '.3ds': 'application/octet-stream', '.cxi': 'application/octet-stream', '.cci': 'application/octet-stream'}
    def __init__(self, *a, **k): super().__init__(*a, directory=ROOT, **k)
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store'); self.send_header('Accept-Ranges', 'bytes'); super().end_headers()
    def do_GET(self):
        rng = self.headers.get('Range')
        path = self.translate_path(self.path.split('?')[0])
        if rng and rng.startswith('bytes=') and os.path.isfile(path):   # Range support: the page reads big game images in 1 MB pieces
            size = os.path.getsize(path); a, _, b = rng[6:].partition('-')
            start = int(a) if a else max(0, size - int(b)); end = min(int(b), size - 1) if (a and b) else size - 1
            if start >= size: self.send_response(416); self.send_header('Content-Range', 'bytes */%d' % size); self.end_headers(); return
            self.send_response(206); self.send_header('Content-Type', self.guess_type(path)); self.send_header('Content-Range', 'bytes %d-%d/%d' % (start, end, size))
            self.send_header('Content-Length', str(end - start + 1)); self.end_headers()
            with open(path, 'rb') as f:
                f.seek(start); left = end - start + 1
                while left > 0:
                    d = f.read(min(1 << 20, left))
                    if not d: break
                    self.wfile.write(d); left -= len(d)
            return
        super().do_GET()
    def do_POST(self):
        n = int(self.headers.get('Content-Length', 0)); body = self.rfile.read(n)
        name = self.path.split('name=')[-1].split('&')[0]
        if self.path.startswith('/live'):
            with open(os.path.join(OUT, 'live-' + name + '.jsonl'), 'ab') as f: f.write(body + b'\n')
            self.send_response(204); self.end_headers(); return
        if self.path.startswith('/report-png'): open(os.path.join(OUT, name + '.png'), 'wb').write(body)
        else: open(os.path.join(OUT, name + '.json'), 'wb').write(body); print('REPORT', name, body[:300].decode(errors='replace'), flush=True)
        self.send_response(200); self.end_headers(); self.wfile.write(b'ok')
    def log_message(self, *a): pass

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8099
    http.server.ThreadingHTTPServer(('127.0.0.1', port), H).serve_forever()
