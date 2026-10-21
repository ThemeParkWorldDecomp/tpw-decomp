"""Unit v51066, thirteen small functions against libtpw_unit_CSwitch_ModeSwitch.dylib (tests/unit_CSwitch_ModeSwitch.cpp), engine program,
src/engine/DirectDraw.cpp, src/engine/DrawPrimitiveOpenGL.cpp, src/engine/EngineUtil.cpp, src/engine/Switch.cpp, src/engine/tb_hfield.cpp, src/engine/tb_init.cpp:
  CSwitch::ModeSwitch 0x10032aa4, CSwitch::RezSwitch 0x10032b00, ToggleFullScreen 0x1003281c, IsAlignedTo64Bit 0x100391c8,
  SamsUtilities::UDebug 0x10043b20, SOldCsTexture::SOldCsTexture 0x1001eb94, COpenGLPrintf::~COpenGLPrintf 0x10046bdc,
  __sinit_tb_init_cpp 0x100141bc, __sinit_tb_hfield_cpp 0x1001271c, PT_Disable 0x1000be98, PT_Enable 0x1000be5c,
  USwapInplace16<unsigned_short> 0x10006218, USwapInplace32<unsigned_int> 0x100061d0.

A case is a world and a call, built twice by one piece of Python code (emulator memory big-endian for the original, the host library's
memory for the translation). CSwitch::Do, COpenGLPrintf::Close and operator delete are fakes on both sides recording their arguments.

Compared: the answer, the event log, the object words / globals / blocks the function may change. UDebug only stores registers into the
caller's frame (nothing the host can observe): the answer and the log are compared. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_CSwitch_ModeSwitch"
MASK = 0xFFFFFFFF

MODESWITCH, REZSWITCH, TOGGLE, ALIGNED, UDEBUG = 0x10032AA4, 0x10032B00, 0x1003281C, 0x100391C8, 0x10043B20
DTOR, SINIT_INIT, SINIT_HF, PT_DISABLE, PT_ENABLE = 0x10046BDC, 0x100141BC, 0x1001271C, 0x1000BE98, 0x1000BE5C
OLDCS, SWAP16, SWAP32 = 0x1001EB94, 0x10006218, 0x100061D0
OWN = {MODESWITCH: 48, REZSWITCH: 44, TOGGLE: 56, ALIGNED: 28, UDEBUG: 36, OLDCS: 16, DTOR: 92, SINIT_INIT: 28, SINIT_HF: 24,
       PT_DISABLE: 20, PT_ENABLE: 28, SWAP16: 12, SWAP32: 12}
CALLEE_DO, CALLEE_CLOSE, CALLEE_DELETE = 0x100328AC, 0x10046C64, 0x1004BC88

ADDR = dict(gei_Switch=0x10173180, gei_TextureSettings=0x10165FC8, gei_ParameterSettings=0x100AF128, gei_HFieldOne=0x100AF120)
HOST_LAYOUT = [4, 8, 12, 0, 8, 16]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_do(e):
    L = e.cl
    L.rec("Do", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("Do") & MASK)


def stub_close(e):
    L = e.cl
    L.rec("Close", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Close") & MASK)


def stub_delete(e):
    # The host's operator delete is the arena's (cannot record): the PowerPC side counts, observe() checks the count against the rule.
    e.cl.dels = getattr(e.cl, "dels", []) + [e.r(3)]


STUBS.internal(CALLEE_DO, stub_do)
STUBS.internal(CALLEE_CLOSE, stub_close)
STUBS.internal(CALLEE_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def put(self, a, v):
        self.words(a, [v])

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    alloc_heap = alloc

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK

    def pair(self, a):
        return self.rd_words(a, 3)

    def prefill_pair(self, a, x, y, canary):
        self.words(a, [x, y, canary])


class Host(Side):
    fmt = "<I"

    def put(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cs_alloc(n)

    def alloc_heap(self, n):
        return self.lib.tpw_cs_new(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        l = self.lib
        a = list(args)
        if entry == MODESWITCH:
            l.tpw_cs_modeswitch(a[0])
        elif entry == REZSWITCH:
            l.tpw_cs_rezswitch(a[0], s32(a[1]), s32(a[2]))
        elif entry == TOGGLE:
            return l.tpw_cs_toggle() & MASK
        elif entry == ALIGNED:
            return l.tpw_cs_aligned(a[0]) & MASK
        elif entry == UDEBUG:
            l.tpw_cs_udebug(a[0], a[1], a[2])
        elif entry == OLDCS:
            l.tpw_cs_oldcs(a[0])
        elif entry == DTOR:
            return l.tpw_cs_dtor(a[0], s32(a[1]))
        elif entry == SINIT_INIT:
            l.tpw_cs_sinit_init()
        elif entry == SINIT_HF:
            l.tpw_cs_sinit_hfield()
        elif entry == PT_DISABLE:
            l.tpw_cs_pt_disable()
        elif entry == PT_ENABLE:
            l.tpw_cs_pt_enable()
        elif entry == SWAP16:
            l.tpw_cs_swap16(a[0])
        elif entry == SWAP32:
            l.tpw_cs_swap32(a[0])
        return 0

    def pair(self, a):
        x, y = struct.unpack("<QQ", self.read(a, 16))
        return [x, y, self.rd_words(a + 16, 1)[0]]

    def prefill_pair(self, a, x, y, canary):
        self.write(a, struct.pack("<QQI", x, y, canary))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cs_addr.restype, lib.tpw_cs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cs_alloc.restype, lib.tpw_cs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cs_new.restype, lib.tpw_cs_new.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cs_modeswitch.argtypes = [C.c_uint64]
    lib.tpw_cs_rezswitch.argtypes = [C.c_uint64, C.c_int32, C.c_int32]
    lib.tpw_cs_toggle.restype = C.c_uint32
    lib.tpw_cs_aligned.restype, lib.tpw_cs_aligned.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_cs_udebug.argtypes = [C.c_uint32] * 3
    lib.tpw_cs_oldcs.argtypes = [C.c_uint64]
    lib.tpw_cs_dtor.restype, lib.tpw_cs_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_cs_swap16.argtypes = [C.c_uint64]
    lib.tpw_cs_swap32.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def run(s, case):
    try:
        return observe(s, case)
    finally:
        s.done()


def observe(s, case):
    op = case["op"]
    res = {}
    if op in ("mode", "rez"):
        obj = s.alloc(12)
        s.words(obj, case["w"])
        if op == "mode":
            s.call(MODESWITCH, [obj])
        else:
            s.call(REZSWITCH, [obj, case["a"], case["b"]])
        res["obj"] = s.rd_words(obj, 3)
    elif op == "toggle":
        obj = s.alloc(12)
        s.words(obj, case["w"])
        s.put(s.g("gei_Switch"), obj if case["have"] else 0)
        res["answer"] = s.call(TOGGLE, [])
        res["obj"] = s.rd_words(obj, 3)
    elif op == "aligned":
        cell = s.alloc(8)
        s.put(cell, case["v"])
        res["answer"] = s.call(ALIGNED, [cell]) & 0xFF
        res["cell"] = s.rd_words(cell, 1)
    elif op == "udebug":
        s.call(UDEBUG, [0x1000, 1, 2, 3, 4, 5, 6, 7])    # r3 comes back unchanged on the PowerPC, void on the host
    elif op == "oldcs":
        p = s.alloc(32)
        s.prefill_pair(p, case["x"], case["y"], 0xDEADBEEF)
        s.call(OLDCS, [p])
        res["obj"] = s.pair(p)
    elif op == "dtor":
        gone = case["have"] and s32(case["flag"] << 16 & MASK) > 0
        p = s.alloc_heap(32)
        s.write(p, case["bytes"])
        ptr = p if case["have"] else 0
        r = s.call(DTOR, [ptr, case["flag"]])
        res["same"] = r == ptr
        res["bytes"] = b"" if gone else s.read(p, 32)    # a deleted block is not read back
        expect = [p] if gone else []
        if isinstance(s, Ppc):
            assert getattr(s.L, "dels", []) == expect, (getattr(s.L, "dels", []), expect)
    elif op == "sinit_init":
        a = s.g("gei_ParameterSettings")
        s.words(a, case["w"])
        s.call(SINIT_INIT, [])
        res["w"] = s.rd_words(a, 5)
    elif op == "sinit_hf":
        a = s.g("gei_HFieldOne")
        s.words(a, [case["v"]])
        s.call(SINIT_HF, [])
        res["w"] = s.rd_words(a, 1)
    elif op in ("pt_en", "pt_dis"):
        a = s.g("gei_TextureSettings")
        s.words(a, case["w"])
        s.call(PT_ENABLE if op == "pt_en" else PT_DISABLE, [])
        res["w"] = s.rd_words(a, 2)
    elif op in ("swap16", "swap32"):
        p = s.alloc(16)
        s.write(p, case["bytes"])
        s.call(SWAP16 if op == "swap16" else SWAP32, [p + 4])
        res["bytes"] = s.read(p, 16)
    res["log"] = s.log()
    return res


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

WORDS = [0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x12345678, 8, 7, 0x10000000, 4]


def rw(r):
    return r.choice(WORDS) if r.random() < 0.5 else r.getrandbits(32)


def base(r, op):
    return {"op": op, "queues": {"Do": [r.choice([0, 1])], "Close": [r.choice([0, 1])]}}


def mk(op, fill):
    def case(r, **kw):
        c = base(r, op)
        fill(r, c)
        c.update(kw)
        return c
    return case


def fill_sw(r, c):
    c["w"] = [rw(r), rw(r), rw(r)]
    c["a"], c["b"] = rw(r), rw(r)
    c["have"] = r.random() < 0.7


def fill_aligned(r, c):
    c["v"] = r.getrandbits(32) if r.random() < 0.5 else r.getrandbits(29) << 3 | r.choice([0, 0, 1, 2, 4, 7])


def fill_oldcs(r, c):
    c["x"], c["y"] = r.getrandbits(32), r.getrandbits(32)


def fill_dtor(r, c):
    c["bytes"] = bytes(r.getrandbits(8) for _ in range(32))
    c["flag"] = r.choice([0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0x18001, r.randrange(-40, 40)]) & MASK
    c["have"] = r.random() < 0.8


def fill_init(r, c):
    c["w"] = [rw(r) for _ in range(5)]
    c["v"] = rw(r)


def fill_pt(r, c):
    c["w"] = [r.choice([0, 4, 0x10000000, 0x10000004, 0x17A, 0xFFFFFFFF, 0xEFFFFFFB]) if r.random() < 0.7 else r.getrandbits(32),
              rw(r)]


def fill_swap(r, c):
    c["bytes"] = bytes(r.getrandbits(8) for _ in range(16))


def tests():
    spec = [
        ("CSwitch::ModeSwitch", "mode", fill_sw, 1),
        ("CSwitch::RezSwitch", "rez", fill_sw, 2),
        ("ToggleFullScreen", "toggle", fill_sw, 3),
        ("IsAlignedTo64Bit", "aligned", fill_aligned, 4),
        ("SamsUtilities::UDebug", "udebug", lambda r, c: None, 5),
        ("SOldCsTexture::SOldCsTexture", "oldcs", fill_oldcs, 6),
        ("COpenGLPrintf::~COpenGLPrintf", "dtor", fill_dtor, 7),
        ("__sinit_tb_init_cpp", "sinit_init", fill_init, 8),
        ("__sinit_tb_hfield_cpp", "sinit_hf", fill_init, 9),
        ("PT_Disable", "pt_dis", fill_pt, 10),
        ("PT_Enable", "pt_en", fill_pt, 11),
        ("SamsUtilities::USwapInplace16<unsigned_short>", "swap16", fill_swap, 12),
        ("SamsUtilities::USwapInplace32<unsigned_int>", "swap32", fill_swap, 13),
    ]
    out = []
    for name, op, fill, seed in spec:
        gen = mk(op, fill)

        def edges(gen=gen, op=op, seed=seed):
            r = random.Random(seed)
            yield gen(r)
            if op == "toggle":
                yield gen(r, have=False)
                yield gen(r, have=True)
            if op == "dtor":
                for flag in (0, 1, MASK, 0x7FFF, 0x8000, 0x10000, 0x10001):
                    for have in (True, False):
                        yield gen(r, flag=flag, have=have)
            if op == "aligned":
                for v in (0, 7, 8, 4, 1, 0xFFFFFFF8, 0xFFFFFFFF):
                    yield gen(r, v=v)
            if op in ("pt_en", "pt_dis"):
                for f in (0, 4, 0x10000000, 0x10000004, 0xFFFFFFFB, 0xFFFFFFFF):
                    yield gen(r, w=[f, 5])
            if op == "swap16":
                yield gen(r, bytes=bytes(range(16)))
            if op == "swap32":
                yield gen(r, bytes=bytes(range(16)))

        out.append(FnTest(name, ppc, host, edge=edges, rnd=lambda r, gen=gen: gen(r)))
    return out


TESTS = tests()
