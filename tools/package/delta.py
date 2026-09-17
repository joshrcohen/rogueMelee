"""Binary delta encoding shared by the portable mod packager."""
from pathlib import Path
import hashlib
import io
import struct

ROOT = Path(__file__).resolve().parents[2]

def delta(source, target):
    # Index aligned source blocks, then find them at arbitrary target offsets.
    # Copy runs survive linker address shifts; literals hold changed code.
    index = {}
    for pos in range(0, len(source) - 31, 16):
        index.setdefault(source[pos:pos+32], pos)
    out = io.BytesIO()
    out.write(b'ROGUED01' + hashlib.sha256(target).digest() + struct.pack('<I', len(target)))
    pos = literal = 0
    while pos < len(target):
        match = index.get(target[pos:pos+32])
        if match is None:
            pos += 1
            continue
        if literal < pos:
            out.write(b'\0' + struct.pack('<I', pos-literal) + target[literal:pos])
        size = 32
        while match+size < len(source) and pos+size < len(target) and source[match+size] == target[pos+size]:
            size += 1
        out.write(b'\1' + struct.pack('<II', match, size))
        pos += size
        literal = pos
    if literal < pos:
        out.write(b'\0' + struct.pack('<I', pos-literal) + target[literal:pos])
    out.write(b'\xff')
    return out.getvalue()
