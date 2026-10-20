"""The Unicode string tag needed by EntryMsg's delete-player prompt (unit w43)."""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_LoginMsg"
STUBS = CL.Stubs()
STUBS.real.add(0x100092f0)


def setup(runner):
    T.install_std_stubs(runner.emu)
    runner.lib.tpw_lm_tag.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32, C.c_int16, C.c_uint16, C.c_uint32, C.POINTER(C.c_uint32)]
    runner.lib.tpw_lm_tag.restype = C.c_int32


def ppc(e, case):
    saved = STUBS.activate(e)
    e.cl = CL.PpcLog({})
    try:
        text, ident, magic, width, precision, flags = case
        raw = None if text is None else text.encode("utf-16-be") + b"\0\0"
        pointer = e.alloc(len(raw), raw) if raw is not None else 0
        buffer = e.alloc(4, bytes(4))
        e.cl.labels[buffer] = 10
        vtable = T.persist_alloc(e, 0x20)
        callback = T.fake_function(e, CL.recorder("AppendUnicode", "LSu"))
        e.write(vtable + 0x18, struct.pack(">I", callback))
        e.write(buffer, struct.pack(">I", vtable))
        tag = e.alloc(16, struct.pack(">4I", magic, 0, ident, pointer))
        params = e.alloc(12, struct.pack(">IhHI", 0, width, precision, flags))
        result = T.call_ppc(e, 0x100092f0, [tag, buffer, params])[0]
        state = [e.u32(tag), e.u32(tag + 8), int(e.u32(tag + 12) == pointer), int(e.u32(params) == 0),
                 T.s16(struct.unpack(">H", e.read(params + 4, 2))[0]) & 0xffffffff,
                 struct.unpack(">H", e.read(params + 6, 2))[0], e.u32(params + 8)]
        after_text = list(struct.unpack(">%dH" % (len(raw) // 2), e.read(pointer, len(raw)))) if raw is not None else []
        return dict(result=T.s32(result), state=state, text=after_text, log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    text, ident, magic, width, precision, flags = case
    raw = None if text is None else text.encode("utf-16-le") + b"\0\0"
    buffer = C.create_string_buffer(raw) if raw is not None else None
    CL.host_begin(lib, {})
    state = (C.c_uint32 * 7)()
    result = lib.tpw_lm_tag(C.cast(buffer, C.c_void_p) if buffer is not None else None, ident, magic, width, precision, flags, state)
    after_text = list(struct.unpack("<%dH" % (len(raw) // 2), bytes(buffer)[:len(raw)])) if raw is not None else []
    return dict(result=result, state=list(state), text=after_text, log=CL.host_end(lib))


def random_case(r):
    text = "".join(r.choice("abcXYZ \u00e9\u0416") for _ in range(r.randrange(120)))
    return (text if r.randrange(5) else None, r.getrandbits(32), r.getrandbits(32), r.randrange(-32768, 32768),
            r.randrange(65536), r.getrandbits(32))


TESTS = [FnTest("TbUniStringVTag::Format", ppc, host,
                edge=lambda: [(text, 4, 0x5641524d, -1, 0, 0) for text in (None, "", "Anna", "x" * 120, "\u0416")],
                rnd=random_case)]
