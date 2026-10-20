"""Unit v51049, eight small functions, against libtpw_unit_std_locale_Dtor_locale.dylib (tests/unit_std_locale_Dtor_locale.cpp), main app
("SimTheme Park"), src/std_locale_Dtor_locale.cpp:
  std::locale::~locale 0x10070d44, std::basic_filebuf<char>::imbue 0x10070bf4, std::basic_filebuf<char>::pbackfail 0x100715b4,
  DisableRideHighlight 0x100672e4, ESP_GetRandomSpritesFrom 0x1007c900, ToggleProceduralTexturing 0x100634f8,
  DestroyDynamicMeshSetInstance 0x10063264, CAdvisor::SetPlayingFor 0x1000c7fc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit (UnhighlightRide, PT_Enable/PT_Disable, CorrectOrientationFlagsForDiffProceduralMode, CWorld::GetRand, CClock::ReadFixed,
DisableMeshDraw, DestroyEngineMesh, MReleaseFreeF, the locale impl destructor, operator delete, std::locale assign, getloc, use_facet and the
codecvt virtual always_noconv) is a fake on both sides that records its arguments (objects as labels) and answers from the case.

The world of a case: the blocks (locale, counts, impl, facet, filebuf, mesh set ...) are allocated first, then written with Mac offsets on the
PowerPC side and the host's offsets (native pointers) on the host side. What is compared: the answer, the event log, and every word or
byte the function may write (addresses made into labels or offsets).

No fused operations. Not tested: ESP_GetRandomSpritesFrom with a group count of 0 (divwu by 0 is undefined on the hardware; the host would
trap), pbackfail with the get pointer above the start and an invalid address (both sides would write through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_locale_Dtor_locale"
MASK = 0xFFFFFFFF

F_LDTOR, F_IMBUE, F_PBACK = 0x10070D44, 0x10070BF4, 0x100715B4
F_DISABLE, F_ESP, F_TOGGLE, F_DESTROY, F_PLAYING = 0x100672E4, 0x1007C900, 0x100634F8, 0x10063264, 0x1000C7FC
SIZES = {F_LDTOR: 172, F_IMBUE: 244, F_PBACK: 88, F_DISABLE: 92, F_ESP: 84, F_TOGGLE: 164, F_DESTROY: 132, F_PLAYING: 72}
OWN = set(SIZES)

# the Mac's addresses of the globals (the asm's TOC comments)
ADDR = dict(rideflag=0x102D1F04, hl=0x102C6CE8, groups=0x1029D368, world=0x1037EBFC, opts=0x102B2B48, settings=0x1038071C,
            clock=0x1037F048)

# the memory blocks of a case: name -> size (allocated before the log is made, so their addresses can be labelled)
BLOCKS = [("loc", 0x10), ("cnt", 0x10), ("imp", 0x10), ("facet", 0x40), ("vt", 0x40), ("fb", 0x40), ("world", 0x20), ("set", 0x100),
          ("mesh", 0x100), ("shared", 0x40), ("malloc", 0x10), ("salloc", 0x10), ("buf", 0x40)]
LABELS = dict(loc=1, cnt=2, imp=3, facet=4, fb=5, world=7, set=8, mesh=9, shared=10, malloc=11, salloc=12)
ADV_PPC_SIZE = 0x1800

LAY = []        # the host layout list (tpw_loc_layout), read once in bind
CUR = {}        # the addresses the PowerPC fakes hand out in the running case


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(e, name, args=()):
    e.cl.rec(name, tuple(args))


def st_unhighlight(e):
    rec(e, "Unhighlight")


def st_pt_enable(e):
    rec(e, "PT_Enable")


def st_pt_disable(e):
    rec(e, "PT_Disable")


def st_correct(e):
    rec(e, "Correct", (s32(e.r(3)),))


def st_disable_draw(e):
    rec(e, "DisableMeshDraw", (e.cl.lab(e.r(3)),))


def st_destroy_mesh(e):
    rec(e, "DestroyEngineMesh", (e.cl.lab(e.r(3)),))


def st_release(e):
    rec(e, "MReleaseFreeF", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


def st_getrand(e):
    rec(e, "GetRand", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("GetRand") & MASK)


def st_readfixed(e):
    rec(e, "ReadFixed", (1 if e.r(3) == ADDR["clock"] else 0,))
    e.set_r(3, e.cl.ret("ReadFixed") & MASK)


def st_getloc(e):
    rec(e, "Getloc", (e.cl.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">II", CUR["imp"], CUR["tcnt"]))


def st_usefacet(e):
    p = e.r(3)
    rec(e, "UseFacet", (e.cl.lab(e.u32(p)), e.cl.lab(e.u32(p + 4))))
    e.set_r(3, CUR["facet"])


def st_impdtor(e):
    rec(e, "ImpDtor", (e.cl.lab(e.r(3)), s32(e.r(4))))


def st_delete(e):
    rec(e, "Delete", (e.cl.lab(e.r(3)),))


def st_assign(e):
    rec(e, "LocaleAssign", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def st_noconv(e):
    rec(e, "Noconv", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("Noconv") & MASK)


STUBS.internal(0x1006D0F8, st_unhighlight)
STUBS.internal(0x102391D8, st_pt_enable)
STUBS.internal(0x102391A8, st_pt_disable)
STUBS.internal(0x100B7FA0, st_correct)
STUBS.internal(0x100653A4, st_disable_draw)
STUBS.internal(0x10065004, st_destroy_mesh)
STUBS.internal(0x10064C54, st_release)
STUBS.internal(0x1013B1E4, st_getrand)
STUBS.internal(0x1014733C, st_readfixed)
STUBS.internal(0x10070E1C, st_getloc)
STUBS.internal(0x1007245C, st_usefacet)
STUBS.internal(0x100737A0, st_impdtor)
STUBS.internal(0x10238218, st_delete)
STUBS.internal(0x10072128, st_assign)
STUBS.internal(0x1023BAA8, st_noconv)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def alloc_blocks(self):
        self.A = {name: self.alloc(size) for name, size in BLOCKS}
        self.adv = self.alloc(self.advsize)

    def mp(self, w):
        """A word that may be (the 32-bit word of) an address of the case, as a label."""
        a = self.w(self.A["buf"])
        if a <= w < a + 0x40:
            return 0x7D000000 + (w - a)
        for i, (name, size) in enumerate(BLOCKS):
            a = self.w(self.A[name])
            if a <= w < a + size:
                return 0x7E000000 + (i << 8) + (w - a)
        return w

    def tmpcnt(self, case):
        return 0 if case.get("tnull") else self.w(self.A["cnt"])


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    advsize = ADV_PPC_SIZE

    def __init__(self, e, case):
        self.e = e
        self.alloc_blocks()
        CUR.update(imp=self.A["imp"], tcnt=self.tmpcnt(case), facet=self.A["facet"])
        # (the Mac's 32-bit words are the addresses themselves)
        fixed = {self.A[n]: l for n, l in LABELS.items()}
        fixed[self.A["fb"] + 0x1C] = 6
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)
        self.grp_stride, self.grp_cnt = 12, 8
        self.o_mesh, self.o_salloc, self.o_shared, self.o_f00, self.o_malloc, self.o_inst = 0x24, 0x34, 0x3C, 0, 0x24, 0x1C
        self.o_start, self.o_dur = 0xD8, 0xDC

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def w(self, addr):
        return addr

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.advsize = LAY[8] + 16
        self.alloc_blocks()
        self.base = lib.tpw_loc_addr(b"arenabase")      # the Mac's 32-bit words are offsets from here
        lib.tpw_loc_set(self.w(self.A["imp"]), self.tmpcnt(case), self.w(self.A["facet"]))
        labels = {l: self.A[n] for n, l in LABELS.items()}
        labels[6] = self.A["fb"] + 0x1C
        CL.host_begin(lib, labels, case["queues"])
        self.grp_stride, self.grp_cnt = LAY[0], LAY[1]
        self.o_mesh, self.o_salloc, self.o_shared, self.o_f00, self.o_malloc, self.o_inst = LAY[2], LAY[3], LAY[4], LAY[5], LAY[6], LAY[7]
        self.o_start, self.o_dur = LAY[9], LAY[10]

    def done(self):
        pass

    def w(self, addr):
        return addr - self.base if addr else 0

    def g(self, name):
        return self.lib.tpw_loc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_loc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        L = self.lib
        if fn == F_LDTOR:
            return L.tpw_loc_dtor(args[0], s32(args[1]))
        if fn == F_IMBUE:
            return L.tpw_loc_imbue(args[0], args[1]) or 0
        if fn == F_PBACK:
            return L.tpw_loc_pbackfail(args[0], s32(args[1])) & MASK
        if fn == F_DISABLE:
            return L.tpw_loc_disable(s32(args[0])) or 0
        if fn == F_ESP:
            return L.tpw_loc_esp(s32(args[0])) & MASK
        if fn == F_TOGGLE:
            return L.tpw_loc_toggle() & MASK
        if fn == F_DESTROY:
            return L.tpw_loc_destroy(args[0], args[1], args[2] & MASK) & MASK
        return L.tpw_loc_playing(args[0], args[1] & MASK) or 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    lib.tpw_loc_addr.restype, lib.tpw_loc_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_loc_alloc.restype, lib.tpw_loc_alloc.argtypes = u64, [u32]
    lib.tpw_loc_set.restype, lib.tpw_loc_set.argtypes = None, [u64, u64, u64]
    lib.tpw_loc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_loc_dtor.restype, lib.tpw_loc_dtor.argtypes = u64, [u64, i32]
    lib.tpw_loc_imbue.restype, lib.tpw_loc_imbue.argtypes = None, [u64, u64]
    lib.tpw_loc_pbackfail.restype, lib.tpw_loc_pbackfail.argtypes = i32, [u64, i32]
    lib.tpw_loc_disable.restype, lib.tpw_loc_disable.argtypes = None, [i32]
    lib.tpw_loc_esp.restype, lib.tpw_loc_esp.argtypes = i32, [i32]
    lib.tpw_loc_toggle.restype, lib.tpw_loc_toggle.argtypes = i32, []
    lib.tpw_loc_destroy.restype, lib.tpw_loc_destroy.argtypes = i32, [u64, u64, u32]
    lib.tpw_loc_playing.restype, lib.tpw_loc_playing.argtypes = None, [u64, u32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_loc_layout(out)
    LAY[:] = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    A, op = s.A, case["op"]
    if op == "dtor":
        s.words(A["loc"], [s.w(A["imp"]), 0 if case["lnull"] else s.w(A["cnt"])])
        s.words(A["cnt"], [case["cnt"]])
    elif op == "imbue":
        s.words(A["fb"], case["fb"])
        s.words(A["cnt"], [case["cnt"]])
        s.words(A["loc"], [0x1111, 0x2222])
        s.words(A["facet"], [s.w(A["vt"])])
    elif op == "pback":
        words = list(case["fb"])
        for idx, key in ((1, "eback"), (2, "gptr")):
            kind, v = case[key]
            words[idx] = s.w(A["buf"] + v) if kind == "b" else v
        words[9] = case["file"]
        s.words(A["fb"], words)
        s.write(A["buf"], bytes((i * 7 + 3) & 0xFF for i in range(0x40)))
    elif op == "disable":
        s.words(s.g("rideflag"), [case["rideflag"]])
        s.ptr(s.g("hl"), A["set"] if case["hl"] else 0)
    elif op == "esp":
        s.ptr(s.g("world"), A["world"])
        for i, n in enumerate(case["counts"]):
            s.words(s.g("groups") + i * s.grp_stride + s.grp_cnt, [n])
    elif op == "toggle":
        s.words(s.g("opts"), [case["opts"]])
        s.words(s.g("settings"), [case["renderer"]])
    elif op == "destroy":
        s.ptr(A["set"] + s.o_mesh, A["mesh"])
        s.ptr(A["set"] + s.o_salloc, A["salloc"])
        s.ptr(A["set"] + s.o_shared, A["shared"])
        s.words(A["mesh"] + s.o_f00, [case["f00"]])
        s.ptr(A["mesh"] + s.o_malloc, A["malloc"])
        s.words(A["shared"] + s.o_inst, [case["inst"]])
    else:
        s.write(s.adv, bytes([0xA5]) * s.advsize)
        s.words(s.adv + s.o_start, [0x11111111])
        s.words(s.adv + s.o_dur, [0x22222222])


def execute(s, case):
    A, op = s.A, case["op"]
    if op == "dtor":
        this = 0 if case["null"] else A["loc"]
        return int(s.call(F_LDTOR, [this, case["flag"]]) == this)
    if op == "imbue":
        s.call(F_IMBUE, [A["fb"], A["loc"]])      # void: r3 is left over
        return 0
    if op == "pback":
        return s.call(F_PBACK, [A["fb"], case["c"]])
    if op == "disable":
        s.call(F_DISABLE, [case["flag"]])         # void
        return 0
    if op == "esp":
        return s.call(F_ESP, [case["idx"]])
    if op == "toggle":
        return s.call(F_TOGGLE, [])
    if op == "destroy":
        return s.call(F_DESTROY, [A["set"], 0x1234, case["flags"]])
    s.call(F_PLAYING, [s.adv, case["length"]])    # void
    return 0


def observe(s, case, answer):
    A, op = s.A, case["op"]
    out = {"answer": answer, "log": s.log()}
    if op == "dtor":
        out["cnt"] = s.rd_word(A["cnt"])
        out["loc"] = [s.mp(w) for w in s.rd_words(A["loc"], 2)]
    elif op == "imbue":
        out["cnt"] = s.rd_word(A["cnt"])
        out["fb"] = [s.mp(w) for w in s.rd_words(A["fb"], 15)]
        out["fbtail"] = s.read(A["fb"] + 0x3F, 1)       # the byte imbue writes (word 15 is the other endianness)
    elif op == "pback":
        out["fb"] = [s.mp(w) for w in s.rd_words(A["fb"], 16)]
        out["buf"] = s.read(A["buf"], 0x40)
    elif op == "disable":
        out["rideflag"] = s.rd_word(s.g("rideflag"))
    elif op == "toggle":
        out["opts"] = s.rd_word(s.g("opts"))
    elif op == "destroy":
        out["inst"] = s.rd_word(A["shared"] + s.o_inst)
        out["f00"] = s.rd_word(A["mesh"] + s.o_f00)
    elif op == "playing":
        raw = s.read(s.adv, s.advsize)
        keep = bytearray(raw)
        for o in (s.o_start, s.o_dur):
            keep[o:o + 4] = b"\xA5" * 4
        out["start"] = s.rd_word(s.adv + s.o_start)
        out["dur"] = s.rd_word(s.adv + s.o_dur)
        out["rest_untouched"] = bytes(keep) == bytes([0xA5]) * s.advsize
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL32 = [0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x12345678]


def r32(r):
    return r.choice(SPECIAL32) if r.random() < 0.2 else r.getrandbits(32)


def mk(r, op, **kw):
    c = {"op": op, "queues": {"GetRand": [r32(r)], "ReadFixed": [r32(r)], "Noconv": [r.randrange(0x200)]}}
    c.update(kw)
    return c


def small_words(r, n=16):
    return [r.randrange(0x10000) for _ in range(n)]


# ---- std::locale::~locale
FLAGS = [-1, 0, 1, 5, 0x7FFF, 0x8000, 0x10000, 0x10001, 0x18001, -32768, 123]
CNTS = [1, 2, 0, 0x80000000, 0xFFFFFFFF]


def dtor_case(r, null=None, lnull=None, cnt=None, flag=None):
    return mk(r, "dtor", null=r.random() < 0.1 if null is None else null, lnull=r.random() < 0.2 if lnull is None else lnull,
              cnt=r.choice(CNTS) if cnt is None else cnt, flag=r.choice(FLAGS) if flag is None else flag)


def dtor_edges():
    r = random.Random(101)
    for null in (False, True):
        for lnull in (False, True):
            for cnt in CNTS:
                for flag in FLAGS:
                    yield dtor_case(r, null, lnull, cnt, flag)


# ---- imbue
NOCONV = [0, 1, 0x100, 0x1FF, 0xFFFFFFFF, 2]


def imbue_case(r, tnull=None, cnt=None):
    c = mk(r, "imbue", tnull=r.random() < 0.2 if tnull is None else tnull, cnt=r.choice(CNTS) if cnt is None else cnt,
           fb=small_words(r))
    if r.random() < 0.5:
        c["queues"]["Noconv"] = [r.choice(NOCONV)]
    return c


def imbue_edges():
    r = random.Random(102)
    for tnull in (False, True):
        for cnt in CNTS:
            for nc in NOCONV:
                c = imbue_case(r, tnull, cnt)
                c["queues"]["Noconv"] = [nc]
                yield c


# ---- pbackfail
CS = [-1, 0, 0x41, 0x80, 0xFF, 0x100, -2, 0x7FFFFFFF, 0x1FF, 0x7F]
POSNS = [(("b", 4), ("b", 5)), (("b", 5), ("b", 5)), (("b", 6), ("b", 5)), (("b", 0), ("b", 1)), (("b", 0), ("b", 0x30)),
         (("a", 0x90000000), ("a", 0x10)), (("a", 0x10), ("a", 0x10)), (("a", 0xFFFFFFFF), ("a", 0)), (("a", 0), ("a", 0)),
         (("b", 0x3F), ("b", 0x3F)), (("b", 0x10), ("b", 0x20))]


def pback_case(r, pos=None, file=None, c=None):
    pos = r.choice(POSNS) if pos is None else pos
    return mk(r, "pback", eback=pos[0], gptr=pos[1], file=r.choice([0, 0x1234, 1]) if file is None else file,
              c=r.choice(CS) if c is None else c, fb=small_words(r))


def pback_edges():
    r = random.Random(103)
    for pos in POSNS:
        for file in (0, 0x1234):
            for c in CS:
                yield pback_case(r, pos, file, c)


# ---- DisableRideHighlight
def disable_case(r, flag=None, hl=None, rideflag=None):
    return mk(r, "disable", flag=r.choice([0, 1, -1, 5, 0x10000]) if flag is None else flag, hl=r.random() < 0.5 if hl is None else hl,
              rideflag=r.choice([0, 1, 7, 0xFFFFFFFF]) if rideflag is None else rideflag)


def disable_edges():
    r = random.Random(104)
    for flag in (0, 1, -1, 5, 0x10000):
        for hl in (False, True):
            for rideflag in (0, 1, 7):
                yield disable_case(r, flag, hl, rideflag)


# ---- ESP_GetRandomSpritesFrom
COUNTS = [1, 2, 3, 5, 7, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x10000]


def esp_case(r, idx=None, rand=None):
    c = mk(r, "esp", idx=r.randrange(14) if idx is None else idx, counts=[r.choice(COUNTS) for _ in range(14)])
    if rand is not None:
        c["queues"]["GetRand"] = [rand]
    return c


def esp_edges():
    r = random.Random(105)
    for idx in range(14):
        for rand in (0, 3, 4, 0xFFFFFFFF, 0x80000000, 0x12345678):
            yield esp_case(r, idx, rand)


# ---- ToggleProceduralTexturing
def toggle_case(r, opts=None, renderer=None):
    if opts is None:
        opts = (r.getrandbits(32) & ~0x6000) | r.choice([0, 0x2000, 0x4000, 0x6000])
    return mk(r, "toggle", opts=opts, renderer=r.choice([0, 1, 2, -1, 5]) if renderer is None else renderer)


def toggle_edges():
    r = random.Random(106)
    for opts in (0, 0x2000, 0x4000, 0x6000, 0xFFFFFFFF, 0xFFFF9FFF, 0xFFFFBFFF, 0xFFFFDFFF, 0x12342000):
        for renderer in (0, 1, 2, -1, 5):
            yield toggle_case(r, opts, renderer)


# ---- DestroyDynamicMeshSetInstance
def destroy_case(r, f00=None, inst=None):
    return mk(r, "destroy", f00=r32(r) if f00 is None else f00, inst=r32(r) if inst is None else inst, flags=r32(r))


def destroy_edges():
    r = random.Random(107)
    for f00 in (0, 1, 2, 3, 0xFFFFFFFF, 0xFFFFFFFE):
        for inst in (0, 1, 0x80000000, 5):
            yield destroy_case(r, f00, inst)


# ---- CAdvisor::SetPlayingFor
def playing_case(r, length=None, now=None):
    c = mk(r, "playing", length=r32(r) if length is None else length)
    if now is not None:
        c["queues"]["ReadFixed"] = [now]
    return c


def playing_edges():
    r = random.Random(108)
    for now in (0, 5, 0x80000000, 0xFFFFFFFF):
        for length in (0, 1, 0xFFFFFFFF, 0x80000000):
            yield playing_case(r, length, now)


TESTS = [
    FnTest("std::locale::~locale", ppc, host, edge=dtor_edges, rnd=dtor_case),
    FnTest("std::basic_filebuf<char,std::char_traits<char>>::imbue", ppc, host, edge=imbue_edges, rnd=imbue_case),
    FnTest("std::basic_filebuf<char,std::char_traits<char>>::pbackfail", ppc, host, edge=pback_edges, rnd=pback_case),
    FnTest("DisableRideHighlight", ppc, host, edge=disable_edges, rnd=disable_case),
    FnTest("ESP_GetRandomSpritesFrom", ppc, host, edge=esp_edges, rnd=esp_case),
    FnTest("ToggleProceduralTexturing", ppc, host, edge=toggle_edges, rnd=toggle_case),
    FnTest("DestroyDynamicMeshSetInstance", ppc, host, edge=destroy_edges, rnd=destroy_case),
    FnTest("CAdvisor::SetPlayingFor", ppc, host, edge=playing_edges, rnd=playing_case),
]
