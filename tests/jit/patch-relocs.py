# Resolves intra-object BL relocations (the test object is not linked) by patching the raw code.
import os, re, struct, subprocess, sys
obj, binf, syms, mode = sys.argv[1:5]
llvm = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../emsdk/upstream/bin/llvm-readobj')
text = subprocess.run([llvm, '-r', obj], capture_output=True, text=True).stdout
symoff = {l.split()[0]: int(l.split()[1], 16) for l in open(syms) if l.strip()}
data = bytearray(open(binf, 'rb').read())
in_text = False; n = 0
for line in text.splitlines():
    if 'Section' in line: in_text = '.rel.text' in line
    m = re.match(r'\s*0x([0-9A-F]+) (R_ARM_CALL|R_ARM_THM_CALL|R_ARM_JUMP24|R_ARM_THM_JUMP24) (\S+)', line)
    if in_text and m:
        off, kind, sym = int(m.group(1), 16), m.group(2), m.group(3)
        tgt = symoff[sym]
        if kind in ('R_ARM_CALL', 'R_ARM_JUMP24'):
            ins = struct.unpack_from('<I', data, off)[0]
            imm = ((tgt - (off + 8)) >> 2) & 0xFFFFFF
            struct.pack_into('<I', data, off, (ins & 0xFF000000) | imm)
        else:  # Thumb BL pair
            d = tgt - (off + 4)
            hi = 0xF000 | ((d >> 12) & 0x7FF); lo = 0xF800 | ((d >> 1) & 0x7FF)
            struct.pack_into('<HH', data, off, hi, lo)
        n += 1
open(binf, 'wb').write(data)
print(f'  patched {n} call relocations')
