"""Unit v5001, the tag functions AddATag, SaveTags and LoadTags, against libtpw_unit_SaveTags.dylib (tests/unit_SaveTags.cpp), main app
("SimTheme Park"), src/ui_StatusPanel.cpp:
  AddATag 0x10191ef8, SaveTags 0x1019231c, LoadTags 0x10192704

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments: LbFile_Read and LbFile_Write (the file's bytes come from the case; a write
is logged by its fields, the text up to its NUL as a length and a hash, because the stack bytes the original leaves in the padding and
after the NUL are not defined), OverrideTag, ShowTagPanel, the InterfaceWindow members (GetDims, SetFrame, getExtraParam, setExtraParam,
setAnimTimer, startAnimTimer), UI_PostMessage and SOUND_SendEvent. wcslen, wcsncpy and wcscpy run for real on both sides. The Mac swaps every
field of the file with SamsUtilities::USwap* (real code on the PowerPC side); the file is little-endian, so the host's swaps are no-ops and
the bytes the fakes see are the same.

The world of a case: the 10 static tags (window, active flag, payload text), the windows (a zeroed block each; serial at +0x600, a text
buffer at +0x400), gGameWindow, gAerialWindow, gTagFirstWindow, gTagCount, gMaxTags, the sound banks, the file bytes and the fakes' answer
queues. What is compared: the answer (a window by its serial), the event log, the active flags, the globals, byte 0x134 of each window and
each window's text buffer.

No fused operations. Not tested: a text of 256 characters or more (the original overruns its buffers), a negative tag count in a file.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SaveTags"
MASK = 0xFFFFFFFF

FN_ADD, FN_SAVE, FN_LOAD = 0x10191EF8, 0x1019231C, 0x10192704
SIZES = {FN_ADD: 720, FN_SAVE: 520, FN_LOAD: 440}
OWN = set(SIZES)

ADDR = dict(game=0x103949E4, aerial=0x103949D4, first=0x103949D0, count=0x1038CD74, max=0x102A8464, banks=0x102F7C94, tags=0x1038CD84)
HOST_LAYOUT = [8, 10]
COOKIE = 0x5151
WIN_SERIAL, WIN_TEXT, WIN_SIZE = 0x600, 0x400, 0x700


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
FILE = {"data": b"", "pos": 0}


def wid(e, p):
    p &= MASK
    return e.u32(p + WIN_SERIAL) if p else 0


def stub(addr, name, fn):
    STUBS.internal(addr, fn)


def st_override(e):
    L = e.cl
    p = e.r(3) & MASK
    raw = e.read(p, 0x25)
    idv, f4, text, tm, frame, f14, f18, f1c, f20, show = struct.unpack(">IiIih2xiiH2xiB", raw)
    L.rec("OverrideTag", (idv, f4, int(text != 0), tm, frame, f14, f18, f1c, f20, show))
    e.set_r(3, 1 if L.ret("override") else 0)


def st_show(e):
    e.cl.rec("ShowTagPanel", (wid(e, e.r(3)),))


def st_dims(e):
    e.cl.rec("GetDims", (wid(e, e.r(3)), int((e.r(4) & MASK) != 0), int((e.r(5) & MASK) != 0), s16(e.r(6))))
    e.set_r(3, 0)


def st_frame(e):
    e.cl.rec("SetFrame", (wid(e, e.r(3)), s32(e.r(4))))


def st_gep(e):
    L = e.cl
    w = e.r(3) & MASK
    idx = s16(e.r(4))
    L.rec("getExtraParam", (wid(e, w), idx))
    v = L.ret("gep%d" % idx)
    if idx == 2:
        e.set_r(3, (w + WIN_TEXT) & MASK if v == 1 else 0)
    else:
        e.set_r(3, v & MASK)


def st_sep(e):
    e.cl.rec("setExtraParam", (wid(e, e.r(3)), s16(e.r(4)), s32(e.r(5))))
    e.set_r(3, 0)


def st_sat(e):
    e.cl.rec("setAnimTimer", (wid(e, e.r(3)), s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, 0)


def st_start(e):
    e.cl.rec("startAnimTimer", (wid(e, e.r(3)), s32(e.r(4))))
    e.set_r(3, 1)


def st_post(e):
    e.cl.rec("UI_PostMessage", (wid(e, e.r(3)), s32(e.r(4)), wid(e, e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def st_sound(e):
    e.cl.rec("SOUND_SendEvent", tuple(s32(e.r(i)) for i in range(3, 9)))
    e.set_r(3, 0)


def st_read(e):
    cookie, buf, n = e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK
    e.cl.rec("LbFile_Read", (cookie, n))
    data = FILE["data"][FILE["pos"]:FILE["pos"] + n]
    e.write(buf, data + bytes(n - len(data)))
    FILE["pos"] += n


def st_write(e):
    cookie, buf, n = e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK
    d = e.read(buf, n)
    if n == 4:
        e.cl.rec("LbFile_Write", (cookie, 4, struct.unpack("<I", d)[0]))
        return
    h, k = 7, 0
    while k < 256 and struct.unpack_from("<H", d, 4 + 2 * k)[0] != 0:
        h = (h * 31 + struct.unpack_from("<H", d, 4 + 2 * k)[0]) & MASK
        k += 1
    u32 = lambda o: struct.unpack_from("<I", d, o)[0]
    u16 = lambda o: struct.unpack_from("<H", d, o)[0]
    e.cl.rec("LbFile_Write", (cookie, n, u32(0), k, h, u32(0x204), u16(0x208), u32(0x20C), u32(0x210), u16(0x214), u32(0x218)))


def rd_wide(e, a):
    out = []
    while True:
        u = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if u == 0:
            return out
        out.append(u)


def imp_swapblock(e):
    p, n = e.r(3) & MASK, e.r(4) & MASK
    d = bytearray(e.read(p, 2 * n))
    for k in range(0, 2 * n, 2):
        d[k], d[k + 1] = d[k + 1], d[k]
    e.write(p, bytes(d))


def imp_wcslen(e):
    e.set_r(3, len(rd_wide(e, e.r(3) & MASK)))


def imp_wcscpy(e):
    dst, src = e.r(3) & MASK, e.r(4) & MASK
    u = rd_wide(e, src)
    e.write(dst, struct.pack(">%dH" % (len(u) + 1), *u, 0))


def imp_wcsncpy(e):
    dst, src, n = e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK
    u = rd_wide(e, src)[:n]
    u = u + [0] * (n - len(u))
    e.write(dst, struct.pack(">%dH" % n, *u))


ERR = []
try:
    for _a, _f in ((0x10191D7C, st_override), (0x10190D0C, st_show), (0x101DBEB0, st_dims), (0x101DB7DC, st_frame), (0x101DD030, st_gep),
                   (0x101DCFD0, st_sep), (0x101DD5E0, st_sat), (0x101DD6B8, st_start), (0x101C9400, st_post), (0x100DD9FC, st_sound)):
        STUBS.internal(_a, _f)
    STUBS.imp("LbFile_Read__FPvPvUlPUl", st_read)
    STUBS.imp("LbFile_Write__FPvPCvUlPUl", st_write)
    STUBS.imp("_USwapBlock16__13SamsUtilitiesFPUsl", imp_swapblock)
    STUBS.imp("wcslen", imp_wcslen)
    STUBS.imp("wcscpy", imp_wcscpy)
    STUBS.imp("wcsncpy", imp_wcsncpy)
except Exception:
    import traceback
    ERR.append(traceback.format_exc()[-600:])


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptr(self, a, v):
        # a native pointer is 8 bytes on the host, 4 on the Mac
        self.write(a, struct.pack(self.fmt[0] + ("Q" if self.PTR8 else "I"), v))

    def rd_ptr(self, a):
        return struct.unpack(self.fmt[0] + ("Q" if self.PTR8 else "I"), self.read(a, 8 if self.PTR8 else 4))[0]

    def units(self, a, us):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(us), *us))

    def rd_units(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">I"
    PTR8 = False
    OFF_WIN, OFF_ACTIVE, OFF_PAY, OFF_F134 = 0, 4, 6, 0x134

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
        self.L = e.cl
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def tag_base(self, i):
        return ADDR["tags"] + i * 0x208

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def setfile(self, data):
        FILE["data"], FILE["pos"] = data, 0

    def call_add(self, p, text):
        a = self.alloc(0x28)
        self.write(a, struct.pack(">IiIih2xiiH2xiB", p["id"] & MASK, p["f4"], text, p["time"], p["frame"], p["f14"], p["f18"], p["f1c"],
                                  p["f20"], p["show"]))
        return T.call_ppc(self.e, FN_ADD, [a])[0] & MASK

    def call_save(self):
        T.call_ppc(self.e, FN_SAVE, [COOKIE])

    def call_load(self):
        T.call_ppc(self.e, FN_LOAD, [COOKIE])


class Host(Side):
    fmt = "<I"
    PTR8 = True
    OFF_WIN = 0

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.OFF_ACTIVE, self.OFF_PAY, self.OFF_F134 = lib.layout
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_st_addr(name.encode())

    def tag_base(self, i):
        return self.lib.tpw_st_tag(i)

    def alloc(self, n):
        # blocks are reused from case to case (the arena has no free): the k-th block of a case is the k-th of the last one, cleared
        k = self.count_alloc = getattr(self, "count_alloc", 0) + 1
        pool = self.lib.pool
        if k in pool and pool[k][1] >= n:
            C.memset(pool[k][0], 0, n)
            return pool[k][0]
        a = self.lib.tpw_st_alloc(n)
        pool[k] = (a, n)
        return a

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setfile(self, data):
        self.lib.tpw_st_setfile(data, len(data))

    def call_add(self, p, text):
        return self.lib.tpw_st_add(p["id"] & MASK, p["f4"], text, p["time"], p["frame"], p["f14"], p["f18"], p["f1c"], p["f20"], p["show"])

    def call_save(self):
        self.lib.tpw_st_save(COOKIE)

    def call_load(self):
        self.lib.tpw_st_load(COOKIE)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_st_addr.restype, lib.tpw_st_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_st_alloc.restype, lib.tpw_st_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_st_tag.restype, lib.tpw_st_tag.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_st_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_st_setfile.argtypes = [C.c_char_p, C.c_uint32]
    lib.tpw_st_add.restype = C.c_uint64
    lib.tpw_st_add.argtypes = [C.c_uint32, C.c_int32, C.c_uint64, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_uint32, C.c_int32,
                               C.c_uint32]
    lib.tpw_st_save.argtypes = [C.c_uint32]
    lib.tpw_st_load.argtypes = [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_st_layout(out)
    assert list(out)[:2] == HOST_LAYOUT, list(out)[:n]
    lib.layout = (out[0], out[1], out[2])
    lib.pool = {}
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def ser(s, a):
    return s.rd_word(a + WIN_SERIAL) if a else 0


def build(s, case):
    def window(serial):
        a = s.alloc(WIN_SIZE)
        s.words(a + WIN_SERIAL, [serial])
        s.units(a + WIN_TEXT, [0x100 + i for i in range(256)])
        return a

    s.wins = [window(i + 1) for i in range(10)]
    game = window(101) if case["game"] else 0
    aerial = window(102) if case["aerial"] else 0
    s.ptr(s.g("game"), game)
    s.ptr(s.g("aerial"), aerial)
    s.ptr(s.g("first"), s.wins[case["first"] - 1] if case["first"] else 0)
    s.words(s.g("count"), [case["count"]])
    s.words(s.g("max"), [case["max"]])
    s.words(s.g("banks"), case["banks"])
    for i in range(10):
        b = s.tag_base(i)
        s.ptr(b + s.OFF_WIN, s.wins[i])
        s.write(b + s.OFF_ACTIVE, struct.pack(s.fmt[0] + "h", case["active"][i]))
        t = case["ptext"][i]
        s.units(b + s.OFF_PAY, t + [0] + [0x5555] * (256 - len(t)))


def text_addr(s, t):
    if t is None:
        return 0
    a = s.alloc(0x400)
    s.units(a, t + [0])
    return a


def execute(s, case):
    op = case["op"]
    if op == "add":
        return ser(s, s.call_add(case["params"], text_addr(s, case["params"]["text"])))
    if op == "save":
        s.call_save()
        return 0
    s.setfile(case["file"])
    s.call_load()
    return 0


def observe(s, case, answer):
    return {
        "answer": answer,
        "log": s.log(),
        "count": s.rd_word(s.g("count")),
        "max": s.rd_word(s.g("max")),
        "first": ser(s, s.rd_ptr(s.g("first"))),
        "active": [struct.unpack(s.fmt[0] + "h", s.read(s.tag_base(i) + s.OFF_ACTIVE, 2))[0] for i in range(10)],
        "f134": [s.read(w + s.OFF_F134, 1) for w in s.wins],
        "text": [s.rd_units(w + WIN_TEXT, 256) for w in s.wins],
    }


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    if ERR:
        return {"exc": ERR[0]}
    try:
        return run(Ppc(e, case), case)
    except Exception:
        import traceback
        return {"E" + traceback.format_exc().strip().splitlines()[-1][-150:].replace("\n", "|").replace(" ", ""): 1}


def host(lib, case):
    try:
        return run(Host(lib, case), case)
    except Exception:
        import traceback
        return {"exc": traceback.format_exc()[-600:]}


# ---------------------------------------------------------------------------------------------------------------- case generators


def rtext(r, n=None):
    if n is None:
        n = r.choice([0, 1, 2, 5, 40, 120, 254, 255])
    return [r.randrange(1, 0x10000) for _ in range(n)]


def rparams(r):
    return {
        "id": r.getrandbits(32), "f4": s32(r.getrandbits(32)),
        "text": r.choice([None, [], rtext(r, 1), rtext(r), rtext(r), rtext(r, 255)]),
        "time": r.choice([0, -5, 1, 5000, 5001, 100000, s32(r.getrandbits(32)), r.randrange(-3, 12000)]),
        "frame": s16(r.getrandbits(16)), "f14": s32(r.getrandbits(32)), "f18": s32(r.getrandbits(32)), "f1c": r.getrandbits(16),
        "f20": s32(r.getrandbits(32)), "show": 0,
    }


def file_for(r, n):
    out = struct.pack("<I", n)
    for _ in range(n):
        p = rparams(r)
        t = p["text"] or []
        t = (t + [0] + [r.randrange(1, 0x10000) for _ in range(256)])[:256]
        out += struct.pack("<I256HihxxiiHxxi", p["id"] & MASK, *t, p["time"], p["frame"], p["f14"], p["f18"], p["f1c"], p["f20"])
    assert len(out) == 4 + n * 0x21C
    return out


class Case(dict):
    """A case that prints short (the judge cuts long lines, and the mismatches follow the case)."""

    def __repr__(self):
        return "<%s game=%s aerial=%s first=%s count=%s max=%s active=%s>" % (
            self["op"], self["game"], self["aerial"], self["first"], self["count"], self["max"], self["active"])


def base(r, op, **over):
    c = Case({
        "op": op,
        "game": r.random() < 0.9, "aerial": r.random() < 0.9,
        "first": r.choice([0, 0, 1, 5, 10]), "count": r.randrange(0, 11), "max": r.choice([10, 10, 10, 5, 3, 0, 11, -1]),
        "active": [r.choice([0, 0, 1, 1, -1, 0x100, 0x7FFF, -0x8000]) for _ in range(10)],
        "ptext": [rtext(r) for _ in range(10)],
        "banks": [r.getrandbits(16) for _ in range(8)],
        "params": rparams(r),
    })
    q = {"override": [r.choice([0, 1, 1, 1]) for _ in range(12)], "gep2": [r.choice([0, 1, 1]) for _ in range(12)]}
    for k in (1, 3, 4, 5, 6, 7, 8):
        q["gep%d" % k] = [r.getrandbits(32) for _ in range(12)]
    c["queues"] = q
    c["file"] = file_for(r, r.choice([0, 1, 2, 3]))
    c.update(over)
    return c


def add_case(r, over=None, **kw):
    c = base(r, "add", **kw)
    for k, v in (over or {}).items():
        c["queues"][k] = [v] * 12
    return c


def add_edges():
    r = random.Random(5001)
    yield add_case(r, game=False)
    yield add_case(r, aerial=False, game=True)
    yield add_case(r, {"override": 0}, game=True, aerial=True)
    for cnt, mx in ((5, 5), (6, 5), (4, 5), (-2, -1), (0, 0), (0, 1)):
        yield add_case(r, {"override": 1}, game=True, aerial=True, count=cnt, max=mx)
    yield add_case(r, {"override": 1}, game=True, aerial=True, max=10, count=0, active=[1] * 10)
    for i in range(10):
        act = [1] * i + [0] + [r.choice([0, 1]) for _ in range(9 - i)]
        yield add_case(r, {"override": 1}, game=True, aerial=True, max=10, count=0, active=act, first=r.choice([0, 3]))
    for t in (None, [], rtext(r, 1), rtext(r, 254), rtext(r, 255)):
        for g2 in (0, 1):
            c = add_case(r, {"override": 1, "gep2": g2}, game=True, aerial=True, max=10, count=0, active=[0] * 10)
            c["params"]["text"] = t
            yield c
    for tm in (0, 1, 5000, 5001, -5000, 0x7FFFFFFF, -0x80000000):
        c = add_case(r, {"override": 1}, game=True, aerial=True, max=10, count=0, active=[0] * 10)
        c["params"]["time"] = tm
        yield c


def add_random(r):
    return add_case(r)


def save_edges():
    r = random.Random(5002)
    yield base(r, "save", active=[0] * 10)
    yield base(r, "save", active=[1] * 10)
    for i in range(10):
        yield base(r, "save", active=[1 if j == i else 0 for j in range(10)])
    for n in (0, 1, 254, 255):
        yield base(r, "save", active=[1] * 10, ptext=[rtext(r, n) for _ in range(10)])
    yield base(r, "save", active=[-1, 0x100, 0x7FFF, -0x8000, 0, 0, 1, 1, 0, 1])


def save_random(r):
    return base(r, "save")


def load_edges():
    r = random.Random(5003)
    for n in (0, 1, 2, 3, 4):
        c = base(r, "load", game=True, aerial=True, max=10, count=0, active=[0] * 10)
        c["queues"]["override"] = [1]
        c["file"] = file_for(r, n)
        yield c
    c = base(r, "load", game=False)
    c["file"] = file_for(r, 2)
    yield c
    c = base(r, "load", game=True, aerial=True, max=2, count=0, active=[0] * 10)
    c["queues"]["override"] = [1]
    c["file"] = file_for(r, 4)
    yield c


def load_random(r):
    c = base(r, "load")
    if r.random() < 0.7:
        c.update(game=True, aerial=True)
        c["queues"]["override"] = [1]
    return c


TESTS = [
    FnTest("AddATag", ppc, host, edge=add_edges, rnd=add_random),
    FnTest("SaveTags", ppc, host, edge=save_edges, rnd=save_random),
    FnTest("LoadTags", ppc, host, edge=load_edges, rnd=load_random),
]
