#!/usr/bin/env python3
"""serve-site.py [port] [dir]: static server for the shared copy (ios/Site) with Cache-Control: no-store, so a phone always gets the newest page.
Default port 8791, default dir ios/Site. Put a tunnel (cloudflared tunnel --url http://127.0.0.1:8791) in front of it to show the page to somebody."""
import sys, os
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
port = int(sys.argv[1]) if len(sys.argv) > 1 else 8791
root = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'ios', 'Site')
class H(SimpleHTTPRequestHandler):
    def __init__(self, *a, **k): super().__init__(*a, directory=root, **k)
    def end_headers(self):
        self.send_header('Cache-Control', 'no-store'); super().end_headers()
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, '.wasm': 'application/wasm', '.js': 'text/javascript'}
ThreadingHTTPServer(('127.0.0.1', port), H).serve_forever()
