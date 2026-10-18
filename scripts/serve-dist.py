#!/usr/bin/env python3
import http.server, os, sys

sys.stdout.reconfigure(line_buffering=True)
sys.stderr.reconfigure(line_buffering=True)

DIST = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'dist')

class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        '.wasm': 'application/wasm',
        '.js': 'text/javascript',
        '.3dsx': 'application/octet-stream',
        '.3ds': 'application/octet-stream',
    }
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIST, **kwargs)

    def end_headers(self):
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Access-Control-Allow-Origin', '*')
        super().end_headers()

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8088
    server = http.server.ThreadingHTTPServer(('0.0.0.0', port), Handler)
    print(f"Serving {DIST} on http://localhost:{port}")
    server.serve_forever()
