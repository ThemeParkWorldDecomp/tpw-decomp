"""Batch 3 boundary fakes. Callees outside the 18-function scope are not translated.
Fakes record every argument in order and mutate accessible state to expose forwarding
mistakes. Logical fields compare across native pointer/vtable layout changes.
"""
import ctypes as C
import struct
import tpwtest as T

VP, U32, I32, U64 = C.c_void_p, C.c_uint32, C.c_int32, C.c_uint64

def F(lib, name, ret, *args):
    f = lib[name]
    f.restype, f.argtypes = ret, list(args)
    return f

class Event(C.Structure):
    _fields_ = [("kind", I32), ("allocBefore", I32), ("obj", VP), ("arg", VP),
                ("args", U32 * 4), ("words", U32 * 20), ("bytes", C.c_ubyte * 256),
                ("text", C.c_char * 64)]

def label(ptr, bases):
    if not ptr:
        return None
    for name, start, size in bases:
        if start <= ptr < start + size:
            return (name, ptr - start)
    return "temporary"

def begin_host(lib, c):
    for name in ("tpw_batch3_on", "tpw_batch3_n", "tpw_alloc_log_on", "tpw_alloc_n"):
        I32.in_dll(lib, name).value = 0
    for name, val in (("tpw_batch3_mask", c.get("mask", 0)), ("tpw_batch3_answer", c.get("answer", 0)),
                      ("tpw_batch3_clock", c.get("clock", 0)), ("tpw_batch3_count", c.get("count", 0)),
                      ("tpw_batch3_result", c.get("result", 0))):
        U32.in_dll(lib, name).value = val
    I32.in_dll(lib, "tpw_batch3_read_size").value = c.get("read_size", 4)
    (C.c_ubyte * 4).in_dll(lib, "tpw_batch3_data")[:] = c.get("data", b"\0" * 4)[:4]
    VP.in_dll(lib, "tpw_batch3_colour_context").value = None

def start_host(lib):
    I32.in_dll(lib, "tpw_batch3_on").value = 1
    I32.in_dll(lib, "tpw_alloc_log_on").value = 1

def end_host(lib, bases):
    n = I32.in_dll(lib, "tpw_batch3_n").value
    trace = []
    for e in (Event * 128).in_dll(lib, "tpw_batch3_log")[:n]:
        # No function in this scope allocates before a callee. Destruction may delete
        # only after the final callee, so this also checks call/delete ordering.
        assert e.allocBefore == 0
        obj, arg = label(e.obj, bases), label(e.arg, bases)
        if e.kind == 1:
            ev = (1, obj, bytes(e.bytes[:5]))
        elif e.kind == 2:
            ev = (2, obj, list(e.args[:3]), bytes(e.bytes[:250]))
        elif e.kind == 3:
            ev = (3,)
        elif e.kind == 4:
            ev = (4, e.text.decode())
        elif e.kind == 5:
            ev = (5, obj, arg, e.args[0])
        elif e.kind == 6:
            ev = (6, obj, arg, e.args[0], bytes(e.bytes[:e.args[0]]))
        elif e.kind in (7, 8):
            ev = (e.kind, obj, arg, list(e.words[:5]))
        elif e.kind in (9, 10):
            ev = (e.kind, obj, list(e.words[:19 if e.kind == 9 else 3]))
        elif e.kind == 11:
            ev = (11, obj, e.words[0])
        else:
            ev = (e.kind, obj, arg, e.words[0], e.text.decode(), e.args[0])
        trace.append(ev)
    n = I32.in_dll(lib, "tpw_alloc_n").value
    kinds, ptrs, sizes = (I32 * 1024).in_dll(lib, "tpw_alloc_kind"), (U64 * 1024).in_dll(lib, "tpw_alloc_ptr"), (U64 * 1024).in_dll(lib, "tpw_alloc_size")
    allocs = [({1: "new", 2: "new[]", 3: "delete", 4: "delete[]"}[kinds[i]],
               label(ptrs[i], bases), sizes[i]) for i in range(n)]
    I32.in_dll(lib, "tpw_batch3_on").value = 0
    I32.in_dll(lib, "tpw_alloc_log_on").value = 0
    return dict(trace=trace, allocs=allocs)

def begin_ppc(e, c, bases):
    e.b3_case, e.b3_bases, e.b3_trace = c, bases, []
    e.alloc_log, e.registered = [], []

def pl(e, a):
    return label(a, e.b3_bases)

def words(e, a, n):
    return list(struct.unpack(">%dI" % n, e.read(a, n * 4)))

def put(e, a, values):
    e.write(a, struct.pack(">%dI" % len(values), *values))

def font_values(e, a):
    w = words(e, a, 6)
    return w[:4] + w[5:]  # Mac vptr at +0x10

def font_fake(e, kind):
    a, arg = e.r(3), e.r(4) if kind == 7 else 0
    if kind == 8:
        assert T.s16(e.r(4)) == -1
    e.b3_trace.append((kind, pl(e, a), pl(e, arg), font_values(e, a)))
    for off in (0, 4, 8, 12, 20):
        put(e, a + off, [e.u32(a + off) ^ e.b3_case["mask"]])
    e.set_r(3, a)

def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    def clock(e):
        e.b3_trace.append((3,)); e.set_r(3, e.b3_case["clock"])
    e.stub("LbTime_GetClock__Fv", clock)
    T.stub_internal(e, "UDebug__13SamsUtilitiesFe", lambda e: e.b3_trace.append((4, e.cstr(e.r(3)))))
    def io(e, write):
        result, file, buf, size, count = [e.r(i) for i in range(3, 8)]
        assert size == 4
        trace = (6, pl(e, file), pl(e, buf), size, e.read(buf, size)) if write else (5, pl(e, file), pl(e, buf), size)
        e.b3_trace.append(trace)
        if not write:
            n = e.b3_case.get("read_size", 4)
            e.write(buf, e.b3_case["data"][:n])
        put(e, result, [e.b3_case["result"]]); put(e, count, [e.b3_case["count"]])
    e.stub("LbFile_Read__FPvPvUlPUl", lambda e: io(e, False))
    e.stub("LbFile_Write__FPvPCvUlPUl", lambda e: io(e, True))
    T.stub_internal(e, "__ct__20InterfaceFontWrapperFP7TbIFont", lambda e: font_fake(e, 7))
    T.stub_internal(e, "__dt__20InterfaceFontWrapperFv", lambda e: font_fake(e, 8))
    def params(e):
        assert T.s16(e.r(4)) == -1
        a = e.r(3); w = words(e, a + 4, 19)
        e.b3_trace.append((9, pl(e, a), w))
        put(e, a + 4, [x ^ e.b3_case["mask"] for x in w]); e.set_r(3, a)
    e.stub("__dt__18TbFontRenderParamsFv", params)
    def manager(e):
        assert T.s16(e.r(4)) == -1
        a = e.r(3); w = [e.u32(a + off) for off in (0, 8, 12)]
        e.b3_trace.append((10, pl(e, a), w))
        for off, value in zip((0, 8, 12), w):
            put(e, a + off, [value ^ e.b3_case["mask"]])
        e.set_r(3, a)
    e.stub("__dt__21TbTextResourceManagerFv", manager)
    def serial(e, kind):
        file, a, name, flags = [e.r(i) for i in range(3, 7)]
        e.b3_trace.append((kind, pl(e, file), pl(e, a), e.u32(a), e.cstr(name), flags))
        put(e, a, [e.u32(a) ^ e.b3_case["mask"]]); e.set_r(3, e.b3_case["answer"])
    T.stub_internal(e, "LOADVAR<Ul>__FPvPUlPCci", lambda e: serial(e, 12))
    T.stub_internal(e, "SAVEVAR<Ul>__FPvPUlPCci", lambda e: serial(e, 13))
    def world(e):
        a = e.r(3); e.b3_trace.append((11, pl(e, a), e.u32(a)))
        put(e, a, [e.u32(a) ^ e.b3_case["mask"]]); e.set_r(3, e.b3_world)
    T.stub_internal(e, "GetWorld__11CWorldSavvyCFv", world)

def allocs(e):
    return [(k, pl(e, a), n) for k, a, n in e.alloc_log]
