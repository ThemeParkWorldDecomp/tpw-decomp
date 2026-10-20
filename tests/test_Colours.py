"""Byte-addressed colour ramps, aliased selection pointers, ctor and palette order."""
import ctypes as C
import random
import tpwtest as T
from batch3_support import *
from batch3_support import setup as common_setup

GLOBAL = 0x102dc08c
OFFSETS = [i * 0x38 + j * 5 for i in range(5) for j in range(10)]
PTRS = [0x34, 0x6c, 0xa4, 0xdc, 0x114, 0x118]
DEFAULT = [5, 15, 25, 35, 49, 40]

def case(r):
    return dict(data=r.randbytes(260), extra=r.randbytes(30), palette=r.randbytes(1024),
                targets=[r.randrange(56) for _ in range(6)], mask=r.getrandbits(32), answer=r.getrandbits(32))

def edges():
    r = random.Random(31); out = []
    for targets in (DEFAULT, [50, 51, 52, 53, 54, 55], [50] * 6, [49] * 6,
                    [40, 5, 15, 25, 35, 49], [5] * 6):
        for v in (0, 255):
            c = case(r); c.update(targets=targets, data=bytes([v]) * 260, extra=bytes([v]) * 30)
            out.append(c)
    return out

def setup(runner):
    common_setup(runner); e = runner.emu
    def colour(e):
        a = e.r(3); raw = e.read(a, 5)
        e.b3_trace.append((1, pl(e, a), raw)); e.write(a, bytes(b ^ (e.b3_case["mask"] & 255) for b in raw))
        e.set_r(3, a)
    T.stub_internal(e, "__ct__8TbColourFv", colour)
    def palette(e):
        ptr, red, green, blue = [e.r(i) for i in range(3, 7)]
        assert ptr == e.data_import("_lbGlobalPalette")
        snapshot = b"".join(e.read(e.b3_colour_context + i * 0x38, 50) for i in range(5))
        e.b3_trace.append((2, pl(e, ptr), [red, green, blue], snapshot))
        e.set_r(3, (red * 3 + green * 5 + blue * 7 + e.b3_case["answer"]) & 255)
    e.stub("LbPalette_FindColour__FPC9TbPaletteUcUcUc", palette)
    def construct(e, call):
        a, ctor, dtor, size, count = [e.r(i) for i in range(3, 8)]
        assert (e.u32(ctor), dtor, size, count) == (e.func("__ct__8TbColourFv"), 0, 5, 10)
        for i in range(count):
            call(ctor, a + 5 * i, 1)
    T.stub_cb(e, "__construct_array", construct)

def ppc(sinit):
    def run(e, c):
        obj = GLOBAL if sinit else e.alloc(0x11c)
        extra = e.alloc(30, c["extra"]); palette = e.data_import("_lbGlobalPalette")
        e.write(palette, c["palette"])
        for i in range(5):
            e.write(obj + i * 0x38, c["data"][i * 52:i * 52 + 52])
        colours = [obj + off for off in OFFSETS] + [extra + i * 5 for i in range(6)]
        for off, target in zip(PTRS, c["targets"]): put(e, obj + off, [colours[target]])
        bases = [("colour%d" % i, a, 5) for i, a in enumerate(colours)] + [("palette", palette, 1024)]
        begin_ppc(e, c, bases); e.b3_colour_context = obj
        T.call_ppc(e, e.func("__sinit_Colours_cpp" if sinit else "Calibrate__8CColoursFv"), [] if sinit else [obj])
        assert not e.registered
        return dict(data=b"".join(e.read(obj + i * 0x38, 52) for i in range(5)), extra=e.read(extra, 30),
                    palette=e.read(palette, 1024), pointers=[pl(e, e.u32(obj + off)) for off in PTRS],
                    trace=e.b3_trace, allocs=allocs(e))
    return run

def host(sinit):
    def run(lib, c):
        begin_host(lib, c)
        obj = F(lib, "tpw_Colours_global" if sinit else "tpw_Colours_create", VP)()
        data = C.create_string_buffer(c["data"], 260); extra = C.create_string_buffer(c["extra"], 30)
        F(lib, "tpw_Colours_seed", None, VP, VP)(obj, data)
        palette = F(lib, "tpw_Colours_palette", VP)(); C.memmove(palette, c["palette"], 1024)
        colours = [F(lib, "tpw_Colours_colour", VP, VP, I32)(obj, i) for i in range(50)] + [C.addressof(extra) + 5 * i for i in range(6)]
        for i, target in enumerate(c["targets"]): F(lib, "tpw_Colours_set_pointer", None, VP, I32, VP)(obj, i, colours[target])
        bases = [("colour%d" % i, a, 5) for i, a in enumerate(colours)] + [("palette", palette, 1024)]
        VP.in_dll(lib, "tpw_batch3_colour_context").value = obj; start_host(lib)
        if sinit: F(lib, "tpw_sinit_Colours", None)()
        else: F(lib, "tpw_Colours_Calibrate", None, VP)(obj)
        F(lib, "tpw_Colours_read", None, VP, VP)(obj, data)
        out = dict(data=data.raw, extra=extra.raw, palette=C.string_at(palette, 1024),
                   pointers=[label(F(lib, "tpw_Colours_pointer", VP, VP, I32)(obj, i), bases) for i in range(6)], **end_host(lib, bases))
        if not sinit: F(lib, "tpw_Colours_release", None, VP)(obj)
        return out
    return run

TESTS = [T.FnTest("CColours::Calibrate", ppc(False), host(False), edge=edges, rnd=case),
         T.FnTest("__sinit_Colours_cpp", ppc(True), host(True), edge=edges, rnd=case)]
