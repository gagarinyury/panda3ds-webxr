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
        self.send_header('Cache-Control', 'no-store'); super().end_headers()
    def do_POST(self):
        n = int(self.headers.get('Content-Length', 0)); body = self.rfile.read(n)
        name = self.path.split('name=')[-1].split('&')[0]
        if self.path.startswith('/report-png'): open(os.path.join(OUT, name + '.png'), 'wb').write(body)
        else: open(os.path.join(OUT, name + '.json'), 'wb').write(body); print('REPORT', name, body[:300].decode(errors='replace'), flush=True)
        self.send_response(200); self.end_headers(); self.wfile.write(b'ok')
    def log_message(self, *a): pass

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8099
    http.server.ThreadingHTTPServer(('127.0.0.1', port), H).serve_forever()
