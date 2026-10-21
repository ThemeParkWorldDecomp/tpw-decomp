"""Unit v51072, the recorder stand-ins, the visitor panel stand-ins, three destructors, a constructor and some key table actions, against
libtpw_unit_CActionRecorder_ProcessAction_3.dylib (tests/unit_CActionRecorder_ProcessAction_3.cpp), main app
("SimTheme Park"), src/game/ActionRecorder.cpp, src/game/ExitErr.cpp, src/game/InGame.cpp, src/game/keyboard.cpp, src/game/ui/ui_visitorStatus.cpp:
  CActionRecorder::ProcessAction 0x10146524 (count, text), GetNextCameraFileEntry 0x10145360, GetNextTimingFileEntry 0x10145318,
  SaveRecordingToDisk 0x10143cac, std::basic_string<wchar_t>::~basic_string 0x101468f8, bfl::FileSystem::CMemFile::~CMemFile 0x101437e0,
  VisitorStatusPanel::Remove / Locate / KidGone / DeInitWindow / DeInitStatusWindow / DeInitViewWindow / InitViewWindow, CWorldSavvy::GetMap,
  CWorld::_CThingArrayEntry::_CThingArrayEntry, TbTList<...ThingId...>::~TbTList, __sinit_InGame_cpp, __sinit_ExitErr_cpp, cheat_cleanup_park,
  show_script, LoadPtclLib, recreate_recording (3 speeds), save_recording, pause.

A case is a block of random words (0x200 bytes, one arena block) plus the globals the function reads. The block is built twice by one piece
of Python code: in emulator memory (big-endian) and in the host library's memory (little-endian). Callees outside the unit are fakes on both
sides that record the same events; pointers into the block are recorded as offsets from the block, the known globals as labels.

What is compared: the answer (for the functions that answer), the event log and the block (or the global object written). Not tested: the
vtable words the Mac's destructors store (the host's objects have their own); the r3 answer of the functions the host declares void; the
deletion of a string whose representation is NULL (the original crashes); the Ptr32 words are compared as "set or not" because the host
holds arena offsets where the Mac holds addresses. No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CActionRecorder_ProcessAction_3"
MASK = 0xFFFFFFFF
P = "tpw_v51072_"

# op: (name, address, size)
OPS = {
    "pa": ("CActionRecorder::ProcessAction", 0x10146524, 60),
    "gnc": ("CActionRecorder::GetNextCameraFileEntry", 0x10145360, 8),
    "gnt": ("CActionRecorder::GetNextTimingFileEntry", 0x10145318, 8),
    "srd": ("CActionRecorder::SaveRecordingToDisk", 0x10143cac, 8),
    "wsd": ("std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>::~basic_string", 0x101468f8, 188),
    "mfd": ("bfl::FileSystem::CMemFile::~CMemFile", 0x101437e0, 104),
    "rem": ("VisitorStatusPanel::Remove", 0x1014356c, 36),
    "loc": ("VisitorStatusPanel::Locate", 0x1014350c, 44),
    "kid": ("VisitorStatusPanel::KidGone", 0x101434a0, 48),
    "diw": ("VisitorStatusPanel::DeInitWindow", 0x10143084, 4),
    "disw": ("VisitorStatusPanel::DeInitStatusWindow", 0x10142aa0, 4),
    "dvw": ("VisitorStatusPanel::DeInitViewWindow", 0x10142110, 36),
    "ivw": ("VisitorStatusPanel::InitViewWindow", 0x101420d0, 4),
    "gm": ("CWorldSavvy::GetMap", 0x10142098, 12),
    "tae": ("CWorld::_CThingArrayEntry::_CThingArrayEntry", 0x10142008, 28),
    "tld": ("TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId,unsigned_short>,tag_IDX_TYPE<IDXTYPEUniqueNameThingId,unsigned_short>>::~TbTList",
            0x10141e34, 100),
    "sin": ("__sinit_InGame_cpp", 0x1014b5cc, 56),
    "sex": ("__sinit_ExitErr_cpp", 0x1014bf38, 20),
    "ccp": ("cheat_cleanup_park", 0x1014c250, 52),
    "ss": ("show_script", 0x1014c4e0, 8),
    "lp": ("LoadPtclLib", 0x1014c50c, 8),
    "rr": ("recreate_recording", 0x1014c538, 48),
    "rrr": ("recreate_recording_real_time", 0x1014c594, 48),
    "rrm": ("recreate_recording_medium_speed", 0x1014c5f8, 48),
    "sr": ("save_recording", 0x1014c660, 8),
    "pause": ("pause", 0x1014c690, 48),
}
OWN = {a for (_, a, _) in OPS.values()}
RETURNS = {"gm", "ccp", "ss", "lp", "sr", "rr", "rrr", "rrm", "pause"}
NOARGS = {"sin", "sex", "ccp", "ss", "lp", "sr", "rr", "rrr", "rrm", "pause"}

ADDR = dict(recorder=0x1037ec44, game=0x1037f134, font=0x1037f1a0, fontNode=0x1037f1b4, exitErr=0x1037f1c0, world=0x1037ebfc,
            map=0x1037ec08, staffThing=0x103947a4)
LABELS = {"recorder": 1, "game": 2, "font": 3, "fontNode": 4}
HOST_LAYOUT = [12, 0x5c, 12]
BASE = [0]


def s32(v):
    return T.s32(v)


def rel(v):
    v &= MASK
    return -1 if v == 0 else s32(v - BASE[0])


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def fake(addr, name, args):
    def fn(e):
        e.cl.rec(name, tuple(args(e)))
    STUBS.internal(addr, fn)


def ctrace_args(e):
    text = e.cstr(e.r(4))
    total = sum(ord(c) if isinstance(c, str) else c for c in text)
    return [e.r(3) & MASK, s32(e.r(5)), s32(e.r(6)), total]


fake(0x1006fcbc, "CTRACE", ctrace_args)
fake(0x10144e2c, "ReplayRecording", lambda e: [e.cl.lab(e.r(3)), s32(e.r(4))])
fake(0x10149a74, "TogglePause", lambda e: [e.cl.lab(e.r(3)), e.r(4) & 0xFF])
fake(0x10101554, "CleanUpPark", lambda e: [s32(e.r(3) - e.u32(ADDR["world"]))])
fake(0x10168ed8, "InterfaceFont", lambda e: [e.cl.lab(e.r(3))])
fake(0x101c0690, "ShowStaffPanel", lambda e: [e.r(3) & MASK])
fake(0x10181864, "DestroySpritePreview", lambda e: [])
fake(0x10193774, "CloseWindow", lambda e: [rel(e.r(3))])
fake(0x10192f9c, "ObjectGone", lambda e: [rel(e.r(3)), e.r(4) & MASK])
fake(0x10239868, "CFileDtor", lambda e: [rel(e.r(3))])
fake(0x10238218, "delete", lambda e: [rel(e.r(3))])
fake(0x10141f1c, "RemoveAll", lambda e: [rel(e.r(3))])
fake(0x10239940, "RegisterGlobalObject", lambda e: [e.cl.lab(e.r(3)), e.cl.lab(e.r(5)), 1 if (e.r(4) & MASK) == 0x102673f0 else 0])


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


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {ADDR[k]: v for k, v in LABELS.items()}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def setbase(self, a):
        BASE[0] = a

    def ptr(self, a):
        return a

    def gptr(self, name, value):
        self.words(ADDR[name], [value])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, args):
        r3, _, _ = T.call_ppc(self.e, OPS[op][1], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {v: self.g(k) for k, v in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return getattr(self.lib, P + "addr")(name.encode())

    def alloc(self, n):
        return getattr(self.lib, P + "alloc")(n)

    def setbase(self, a):
        getattr(self.lib, P + "set_base")(a)

    def ptr(self, a):
        return getattr(self.lib, P + "ptr")(a) if a else 0

    def gptr(self, name, value):
        self.write(self.g(name), struct.pack("<Q", value & MASK))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        r = getattr(self.lib, P + op)(*args)
        return None if r is None else r & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, u32, i16, u8 = C.c_uint64, C.c_int32, C.c_uint32, C.c_int16, C.c_uint8
    getattr(lib, P + "addr").restype, getattr(lib, P + "addr").argtypes = u64, [C.c_char_p]
    getattr(lib, P + "alloc").restype, getattr(lib, P + "alloc").argtypes = u64, [u32]
    getattr(lib, P + "set_base").argtypes = [u64]
    getattr(lib, P + "ptr").restype, getattr(lib, P + "ptr").argtypes = u32, [u64]
    getattr(lib, P + "layout").argtypes = [C.POINTER(C.c_int64)]
    sig = {"pa": [u64, i32, u8, u64], "wsd": [u64, i16], "mfd": [u64, i16], "tld": [u64, i16], "kid": [u64, u32]}
    for op in OPS:
        f = getattr(lib, P + op)
        f.argtypes = sig.get(op, [] if op in NOARGS else [u64])
        f.restype = (u32 if op == "gm" else i32) if op in RETURNS else None
    out = (C.c_int64 * 32)()
    n = getattr(lib, P + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

BLOCKOPS = {"pa", "gnc", "gnt", "srd", "rem", "kid", "diw", "disw", "dvw", "ivw", "tae", "loc"}


def build(s, case):
    op = case["op"]
    b = s.alloc(0x200)
    s.blk = b
    s.setbase(b)
    s.words(b, case["fill"])
    if op == "wsd":
        rep = b + 0x20
        s.words(rep + 8, [case["refs"]])
        s.words(rep + 0xC, [s.ptr(b + 0x40) if case["buf"] else 0])
        s.words(b, [s.ptr(rep)])
    elif op == "kid":
        s.words(b + 0xC, [case["w"]])
    elif op == "loc":
        s.gptr("staffThing", case["thing"])
    elif op == "gm":
        s.gptr("map", case["map"])
    elif op == "ccp":
        s.gptr("world", b)
    elif op == "sex":
        s.write(s.g("exitErr"), case["bytes"])


def execute(s, case):
    op, b = case["op"], s.blk
    if op == "pa":
        return s.call(op, [b, case["action"], case["count"], case["text"]])
    if op == "wsd":
        return s.call(op, [0 if case["null"] else b, case["flag"]])
    if op in ("mfd", "tld"):
        return s.call(op, [0 if case["null"] else b + 0x10, case["flag"]])
    if op == "kid":
        return s.call(op, [b, case["thing"]])
    if op == "gm":
        return s.call(op, [case["self"]])
    if op == "tae":
        return s.call(op, [b + 4])
    if op in NOARGS:
        return s.call(op, [])
    return s.call(op, [b])


def observe(s, case, answer):
    op, b = case["op"], s.blk
    out = {"answer": answer if op in RETURNS else None, "log": s.log()}
    if op == "wsd":
        rep = b + 0x20
        out["refs"] = s.rd_word(rep + 8)
        out["head"] = s.rd_words(rep, 2)
        out["str"] = s.rd_word(b) != 0
        out["rest"] = s.rd_words(b + 0x60, 0x68)
    elif op in ("mfd", "tld"):
        out["mem"] = s.rd_words(b, 4) + s.rd_words(b + 0x18, 0x7A)     # the vtable word (Mac 4 bytes, host 8 bytes) at +0x10 is skipped
    elif op == "sex":
        out["mem"] = s.read(s.g("exitErr"), 0x5C)
    elif op in BLOCKOPS:
        out["mem"] = s.rd_words(b, 0x80)
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

W32 = [0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100, 0x1FF, 0xFF]
FLAGS = [0, 1, 0xFFFFFFFF, 0x7FFF, 0x8000, 0x10000, 0x18001, 0x10001, 2]


def rw(r):
    return r.choice(W32) if r.random() < 0.4 else r.getrandbits(32)


def mk(r, op, **kw):
    c = {"op": op, "queues": {}, "fill": [r.getrandbits(32) for _ in range(128)]}
    c.update(kw)
    return c


def params(op, r, edge=None):
    """The case parameters of one op: an edge index (a fixed combination) or random."""
    if op == "pa":
        return dict(action=rw(r), count=rw(r), text=rw(r)) if edge is None else dict(action=W32[edge % 9], count=W32[(edge * 4) % 9], text=0)
    if op == "wsd":
        refs = [0, 1, 2, 3, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 5]
        if edge is None:
            return dict(refs=r.choice(refs), flag=r.choice(FLAGS), buf=r.random() < 0.7, null=r.random() < 0.1)
        return dict(refs=refs[edge % 8], flag=FLAGS[(edge // 8) % 9], buf=bool((edge // 72) % 2), null=False)
    if op in ("mfd", "tld"):
        if edge is None:
            return dict(flag=r.choice(FLAGS), null=r.random() < 0.15)
        return dict(flag=FLAGS[edge % 9], null=edge >= 9)
    if op == "kid":
        if edge is None:
            return dict(w=r.choice([0, 0, 1, 0x80000000, r.getrandbits(32)]), thing=rw(r))
        return dict(w=[0, 1, 0x80000000, 0xFFFFFFFF][edge % 4], thing=W32[edge % 9])
    if op == "loc":
        return dict(thing=rw(r)) if edge is None else dict(thing=W32[edge % 9])
    if op == "gm":
        return dict(map=rw(r), self=rw(r)) if edge is None else dict(map=W32[edge % 9], self=0)
    if op == "sex":
        return dict(bytes=bytes(r.getrandbits(8) for _ in range(0x5C)) if edge is None else bytes([0xFF if edge else 0] * 0x5C))
    return {}


def make(op):
    def edges():
        r = random.Random(5000 + len(op))
        n = {"pa": 9, "wsd": 144, "mfd": 18, "tld": 18, "kid": 9, "loc": 9, "gm": 9, "sex": 2}.get(op, 2)
        for i in range(n):
            yield mk(r, op, **params(op, r, i))

    def rnd(r):
        return mk(r, op, **params(op, r))
    return edges, rnd


TESTS = []
for _op, (_name, _a, _s) in OPS.items():
    _e, _r = make(_op)
    TESTS.append(FnTest(_name, ppc, host, edge=_e, rnd=_r))
