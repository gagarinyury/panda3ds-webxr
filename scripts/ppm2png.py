# Converts a binary PPM (P6) to PNG, optionally scaled by an integer factor. Usage: ppm2png.py in.ppm out.png [scale]
import struct, sys, zlib
src, dst = sys.argv[1], sys.argv[2]
k = int(sys.argv[3]) if len(sys.argv) > 3 else 1
d = open(src, 'rb').read()
parts = d.split(b'\n', 3)
w, h = map(int, parts[1].split()); px = parts[3]
rows = []
for y in range(h):
    row = px[y * w * 3:(y + 1) * w * 3]
    big = b''.join(row[i:i + 3] * k for i in range(0, len(row), 3))
    rows += [b'\x00' + big] * k
raw = b''.join(rows)
def chunk(t, b): return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
open(dst, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w * k, h * k, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))
