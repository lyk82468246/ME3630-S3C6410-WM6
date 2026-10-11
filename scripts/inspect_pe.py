"""Inspect CE PE headers/imports/exports without executing the binary."""
import hashlib
import json
import struct
import sys
from pathlib import Path

def inspect(path):
    data = Path(path).read_bytes()
    u16 = lambda o: struct.unpack_from('<H', data, o)[0]
    u32 = lambda o: struct.unpack_from('<I', data, o)[0]
    pe = u32(0x3c)
    assert data[pe:pe+4] == b'PE\0\0'
    opt = pe+24
    sections = []
    for i in range(u16(pe+6)):
        o = opt+u16(pe+20)+40*i
        sections.append((u32(o+12), max(u32(o+8), u32(o+16)), u32(o+20)))
    def offset(rva):
        for base, size, raw in sections:
            if base <= rva < base+size:
                return raw+rva-base
        if rva < u32(opt+60):
            return rva
        raise ValueError(f'Unmapped RVA {rva:x}')
    def string(rva):
        o = offset(rva)
        return data[o:data.index(0, o)].decode('ascii', errors='replace')
    imports = {}
    rva = u32(opt+104)
    if rva:
        o = offset(rva)
        while any(data[o:o+20]):
            dll = string(u32(o+12))
            thunk = offset(u32(o) or u32(o+16))
            names = []
            while u32(thunk):
                value = u32(thunk)
                names.append('#'+str(value & 0xffff) if value & 0x80000000 else string(value+2))
                thunk += 4
            imports[dll] = names
            o += 20
    exports = {}
    rva = u32(opt+96)
    if rva:
        o = offset(rva)
        ordinal_base, count, named = u32(o+16), u32(o+20), u32(o+24)
        funcs, names, ords = offset(u32(o+28)), offset(u32(o+32)), offset(u32(o+36))
        for i in range(named):
            index = u16(ords+2*i)
            exports[string(u32(names+4*i))] = ordinal_base+index
    return dict(path=str(path), sha256=hashlib.sha256(data).hexdigest(), machine=hex(u16(pe+4)),
                os_version=[u16(opt+40),u16(opt+42)], subsystem=u16(opt+68),
                subsystem_version=[u16(opt+48),u16(opt+50)], imports=imports, exports=exports)

if __name__ == '__main__':
    print(json.dumps([inspect(p) for p in sys.argv[1:]], indent=2))
