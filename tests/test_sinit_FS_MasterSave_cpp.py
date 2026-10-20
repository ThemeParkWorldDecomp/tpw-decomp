"""Unit v51117: FS_MasterSave.cpp's static initialiser, five online callbacks, the keyboard actions, against
libtpw_unit_sinit_FS_MasterSave_cpp.dylib (tests/unit_sinit_FS_MasterSave_cpp.cpp), main app, src/FS_Online_gametype_std___tree_insert_node_at_3_sinit_FS_MasterSave_cpp.cpp:
  __sinit_FS_MasterSave_cpp 0x1016f35c, the News/UploadServer/RequestFTP/GotoPlayerByID callbacks, SatelliteOnlineParkInfoPanelCancelCallback,
  dbgchan0..9 and the other one-line actions, save_hoardings, save_complete_hoardings.

A case is a world and a call, built by one piece of Python in both memories. The callees (string constructor, __register_global_object,
the online system's constructor and callbacks, the download globe's virtual, the two hoarding writers) are fakes on both sides that
record their arguments. The world: gOnlineSystem (NULL or a dummy object), the wide text at the string pool + 0x5f, the lobby's screens
with a globe. Compared: the answer of the int functions, gOnlineSystem as null/dummy/new, and the event log.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_FS_MasterSave_cpp"
MASK = 0xFFFFFFFF

SINIT = 0x1016F35C
FN = {
    "FSOnlineNewsConnectionCallback": 0x10171DFC,
    "SatelliteOnlineParkInfoPanelCancelCallback": 0x10174B6C,
    "FSOnlineUploadServerConnectionCallback": 0x10175748,
    "RequestFTPPostLoginCallbackFn": 0x10175D3C,
    "GotoPlayerByIDFTPPostLoginCallbackFn": 0x10175FAC,
}
SIMPLE = [
    ("dbgchan0", 0x1014FB54), ("dbgchan1", 0x1014FB7C), ("dbgchan2", 0x1014FBA4), ("dbgchan3", 0x1014FBCC),
    ("dbgchan4", 0x1014FBF4), ("dbgchan5", 0x1014FC1C), ("dbgchan6", 0x1014FC44), ("dbgchan7", 0x1014FC6C),
    ("dbgchan8", 0x1014FC94), ("dbgchan9", 0x1014FCBC), ("debug_track_cell_dirs", 0x1014F9F8),
    ("debug_hoarding_cell_neighbs", 0x1014F9BC), ("debug_track_cell_neighbs", 0x1014F984), ("debug_cell_dirs", 0x1014F954),
    ("debug_cell_neighbs", 0x1014F920), ("force_ride_fix", 0x1014F8F0), ("force_ride_condemned", 0x1014F8BC),
    ("force_ride_worn", 0x1014F7B0), ("toggle_cell_flash", 0x1014F62C), ("next_debug_text_mode", 0x1014F5F8),
    ("set_allow_loops_no_research", 0x1014F5BC), ("select_debug_person", 0x1014F588),
    ("save_complete_hoardings", 0x1014F43C), ("save_hoardings", 0x1014F3EC),
]
SIMPLE_ADDR = dict(SIMPLE)
OWN = {SINIT} | set(FN.values()) | set(SIMPLE_ADDR.values())

STRING_CTOR, REGISTER, NEW_ONLINE_CTOR, PTR_GLUE = 0x10238800, 0x10239940, 0x1016F7D0, 0x1023BAA8
NEWS_CB, UPLOAD_CB, REQ_CB, GOTO_CB = 0x10172030, 0x101753DC, 0x10175F24, 0x10176180
WRITE_COMPLETE, WRITE_HOARD = 0x10095E54, 0x10095B20
DESTROY_WSTRING = 0x103BCBB8

ADDR = dict(online=0x10380AB0, dummy=0x10380AC0, globe=0x10380AE0, vtable=0x10380B00, screens=0x10380B40, lobby=0x102E49B8,
            pool=0x1024C95E, node=0x10380AA4, string=0x10380A90)
HOST_LAYOUT = [24]      # CLBScreenSystem::mDownloadGlobe (the Mac's is 0x0c)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_register(e):
    L = e.cl
    L.rec("Register", (L.lab(e.r(3)), L.lab(e.r(5)), 1 if e.r(4) == DESTROY_WSTRING else 0))


def stub_glue(e):
    L = e.cl
    L.rec("DownloadGlobeSlot4c", (L.lab(e.r(3)),))


def toc(fn):
    """A glue saves the TOC at 0x14(r1) and the caller reloads r2 from there."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


STUBS.internal(STRING_CTOR, toc(CL.recorder("WStringCtor", "NS")))
STUBS.internal(REGISTER, toc(stub_register))
STUBS.internal(NEW_ONLINE_CTOR, toc(CL.recorder("OnlineCtor", "N")))
STUBS.internal(NEWS_CB, toc(CL.recorder("NewsConnectionCallback", "Liiuu")))
STUBS.internal(UPLOAD_CB, toc(CL.recorder("UploadServerConnectionCallback", "Liiuu")))
STUBS.internal(REQ_CB, toc(CL.recorder("RequestFTPPostLoginCallback", "Liu")))
STUBS.internal(GOTO_CB, toc(CL.recorder("GotoPlayerByIDFTPPostLoginCallback", "Liu")))
STUBS.internal(PTR_GLUE, toc(stub_glue))
STUBS.internal(WRITE_COMPLETE, toc(CL.recorder("WriteComplete", "")))
STUBS.internal(WRITE_HOARD, toc(CL.recorder("WriteHoardings", "")))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def text(self, a, chars):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", c) for c in chars + [0]))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["dummy"]: 1, ADDR["node"]: 2, ADDR["globe"]: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def wptr(self, a, v):
        self.words(a, [v & MASK])

    def rptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def set_lobby(self):
        self.words(ADDR["globe"], [ADDR["vtable"]])
        self.words(ADDR["vtable"] + 0x4C, [0x10260000])
        self.words(ADDR["screens"] + 0xC, [ADDR["globe"]])
        self.words(ADDR["lobby"], [ADDR["screens"]])

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, name, case):
        a, b, c, d = case["args"]
        if name == "__sinit_FS_MasterSave_cpp":
            T.call_ppc(self.e, SINIT, [])
            return None
        if name == "FSOnlineNewsConnectionCallback" or name == "FSOnlineUploadServerConnectionCallback":
            T.call_ppc(self.e, FN[name], [a & MASK, b & MASK, c, d])
            return None
        if name in FN and name != "SatelliteOnlineParkInfoPanelCancelCallback":
            T.call_ppc(self.e, FN[name], [a & MASK, c])
            return None
        if name == "SatelliteOnlineParkInfoPanelCancelCallback":
            T.call_ppc(self.e, FN[name], [])
            return None
        r3, _, _ = T.call_ppc(self.e, SIMPLE_ADDR[name], [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("dummy"), 2: self.g("node"), 3: self.g("globe")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v51117_addr(name.encode())

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def set_lobby(self):
        self.lib.tpw_v51117_setlobby()

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, case):
        a, b, c, d = case["args"]
        r = self.lib.tpw_v51117_call(OPS[name], T.s32(a), T.s32(b), c, d)
        return r & MASK if name in SIMPLE_ADDR else None


OPS = {"__sinit_FS_MasterSave_cpp": 0, "FSOnlineNewsConnectionCallback": 1, "FSOnlineUploadServerConnectionCallback": 2,
       "RequestFTPPostLoginCallbackFn": 3, "GotoPlayerByIDFTPPostLoginCallbackFn": 4,
       "SatelliteOnlineParkInfoPanelCancelCallback": 5}
for i, (n, _) in enumerate(SIMPLE):
    OPS[n] = 6 + i


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v51117_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "call").restype, getattr(lib, p + "call").argtypes = C.c_uint32, [C.c_int32, C.c_int32, C.c_int32, C.c_uint32, C.c_uint32]
    getattr(lib, p + "setlobby").argtypes = []
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.wptr(s.g("online"), s.g("dummy") if case["pre"] else 0)
    s.set_lobby()
    s.text(s.g("pool") + 0x5F, case["text"])


def observe(s, case, answer):
    v = s.rptr(s.g("online"))
    state = 0 if v == 0 else (1 if v == s.g("dummy") else 2)
    return {"answer": answer, "log": s.log(), "online": state}


def make_run(name):
    def run(s, case):
        try:
            build(s, case)
            return observe(s, case, s.call(name, case))
        finally:
            s.done()

    return (lambda e, case: run(Ppc(e, case), case)), (lambda lib, case: run(Host(lib, case), case))


def base(r, pre=None):
    return {"queues": {"WriteComplete": [r.choice([0, 1])], "WriteHoardings": [r.choice([0, 1])]},
            "pre": r.choice([False, True]) if pre is None else pre,
            "text": [r.choice([0x41, 0x25, 0x6C, 0x20AC, 0x7A]) for _ in range(r.randrange(0, 9))],
            "args": [r.choice([0, 1, -1, r.randrange(-2**31, 2**31)]), r.choice([0, 1, -1, r.randrange(-2**31, 2**31)]),
                     r.choice([0, r.randrange(0, 2**32)]), r.choice([0, r.randrange(0, 2**32)])]}


def edges(seed):
    def gen():
        r = random.Random(seed)
        for pre in (False, True, False, True):
            yield base(r, pre)
    return gen


def rnd(r):
    return base(r)


TESTS = []
for _name in ["__sinit_FS_MasterSave_cpp"] + list(FN) + [n for n, _ in SIMPLE]:
    _p, _h = make_run(_name)
    TESTS.append(FnTest(_name, _p, _h, edge=edges(len(TESTS) + 7), rnd=rnd))
