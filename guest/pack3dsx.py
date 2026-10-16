# Wraps a raw ARM code blob (entry point at offset 0) into a minimal .3dsx: no relocations, no rodata/data.
import struct, sys
code = open(sys.argv[1], 'rb').read()
code += b'\0' * (-len(code) % 4)
hdr = b'3DSX' + struct.pack('<HHIIIIII', 0x20, 8, 0, 0, len(code), 0, 0, 0)
open(sys.argv[2], 'wb').write(hdr + struct.pack('<6I', 0, 0, 0, 0, 0, 0) + code)
print(f'wrote {sys.argv[2]}: {len(code)} bytes of code')
