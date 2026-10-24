"""Unit v5333, CoasterPanelEndTrack, against libtpw_unit_CoasterPanelEndTrack.dylib (tests/unit_CoasterPanelEndTrack.cpp), main app
("SimTheme Park"), src/game/ui/ui_coasterPanel.cpp:
  CoasterPanelEndTrack 0x1019c158

A case is a world and a call. Callees outside the unit are fakes on both sides that record their events: operator new (size), the
CMMDefault and CMMCoaster constructors (new labels), CMouse::AssignMouseMode (labels) and the mode's GetMode virtual (via __ptr_glue),
which answers from the queue "GetMode".

The world of a case: gMouse (NULL or a persistent pre-existing mouse), gMouseMode (NULL or a persistent object with a vtable), the two
mouse words with random values. Compared: the event log, the two words, and gMouse as NULL / the pre-existing one / a new object.

With the port fixes on (unit coaster64; the port fix in CoasterPanelEndTrack, src/game/ui/ui_coasterPanel.cpp), the translation allocates the
mouse, the default mode and the coaster mode with sizeof each where the Mac asks for 1, 8 and 8. That one difference is mapped back: a host
allocation of exactly the sizeof the unit reports for the object it is made for (the constructor event after it names the object, none the
mouse) is logged as the Mac's size. Everything else is compared as with the fixes off.

No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CoasterPanelEndTrack"
MASK = 0xFFFFFFFF

FN = 0x1019C158
SIZES = {FN: 344}
OWN = set(SIZES)

NEW = 0x102384B8
CTOR_DEFAULT = 0x1007EA78
CTOR_COASTER = 0x1007FD14
ASSIGN = 0x1007E2E0
PTR_GLUE = 0x1023BAA8

ADDR = dict(gMouse=0x102DC1F4, gMouseMode=0x102DC1F0, w1E0=0x102DC1E0, w1E4=0x102DC1E4)
HOST_LAYOUT = []

PERSIST = {}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_new(e):
    size = e.r(3) & MASK
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size if size else 4))


def stub_glue(e):
    L = e.cl
    L.rec("GetMode", ())
    e.set_r(3, L.ret("GetMode") & MASK)


STUBS.internal(NEW, stub_new)
STUBS.internal(CTOR_DEFAULT, CL.recorder("CMMDefault", "N"))
STUBS.internal(CTOR_COASTER, CL.recorder("CMMCoaster", "N"))
STUBS.internal(ASSIGN, CL.recorder("AssignMouseMode", "LL"))
STUBS.internal(PTR_GLUE, stub_glue)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    vt = T.persist_alloc(e, 0x40)
    PERSIST["pre"] = T.persist_alloc(e, 16)
    PERSIST["mode"] = T.persist_alloc(e, 16, struct.pack(">I", vt) + bytes(12))


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({PERSIST["pre"]: 1, PERSIST["mode"]: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_ptrs(self, mouse, mode):
        self.words(ADDR["gMouse"], [PERSIST["pre"] if mouse else 0])
        self.words(ADDR["gMouseMode"], [PERSIST["mode"] if mode else 0])

    def gmouse(self):
        v = self.rd_words(ADDR["gMouse"], 1)[0]
        return "null" if v == 0 else "pre" if v == PERSIST["pre"] else "new"

    def call(self):
        T.call_ppc(self.e, FN, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: lib.tpw_cpet_addr(b"premouse"), 2: lib.tpw_cpet_addr(b"fakemode")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cpet_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        log = CL.host_end(self.lib)
        return mac_sizes(self.lib, log) if self.lib.port_fix else log

    def set_ptrs(self, mouse, mode):
        self.lib.tpw_cpet_set(mouse, mode)

    def gmouse(self):
        v = self.lib.tpw_cpet_get_gmouse()
        return "null" if v == 0 else "pre" if v == self.lib.tpw_cpet_addr(b"premouse") else "new"

    def call(self):
        self.lib.tpw_cpet_run()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cpet_addr.restype, lib.tpw_cpet_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cpet_set.restype, lib.tpw_cpet_set.argtypes = None, [C.c_uint32, C.c_uint32]
    lib.tpw_cpet_get_gmouse.restype, lib.tpw_cpet_get_gmouse.argtypes = C.c_uint64, []
    lib.tpw_cpet_run.restype, lib.tpw_cpet_run.argtypes = None, []
    lib.tpw_cpet_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cpet_portfix.restype, lib.tpw_cpet_portfix.argtypes = C.c_int32, []
    lib.tpw_cpet_new_sizes.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cpet_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.port_fix = lib.tpw_cpet_portfix() == 1
    n = lib.tpw_cpet_new_sizes(out)
    mouse, default, coaster = list(out)[:n]
    # the object an allocation is for (the name of the constructor event after it, None the mouse): (the host's sizeof, the Mac's size)
    lib.new_sizes = {None: (mouse, 1), "CMMDefault": (default, 8), "CMMCoaster": (coaster, 8)}
    lib.bound = True


def mac_sizes(lib, log):
    """The host log with the port fixes' allocation sizes (sizeof the object) turned back into the Mac's."""
    out = list(log)
    for i, (name, ints, texts) in enumerate(out):
        if name != "new":
            continue
        after = out[i + 1][0] if i + 1 < len(out) and out[i + 1][0] in lib.new_sizes else None
        host_size, mac_size = lib.new_sizes[after]
        if ints == (host_size,):
            out[i] = (name, (mac_size,), texts)
    return out


def run(s, case):
    try:
        s.set_ptrs(case["mouse"], case["mode"])
        s.words(s.g("w1E0"), [case["w0"]])
        s.words(s.g("w1E4"), [case["w1"]])
        s.call()
        return {"log": s.log(), "w0": s.rd_words(s.g("w1E0"), 1), "w1": s.rd_words(s.g("w1E4"), 1), "gmouse": s.gmouse()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, mouse, mode, answer):
    return {"mouse": mouse, "mode": mode, "w0": r.randrange(1 << 32), "w1": r.randrange(1 << 32),
            "queues": {"GetMode": [answer]}}


def edges():
    r = random.Random(5333)
    for mouse in (0, 1):
        for mode in (0, 1):
            for answer in (8, 0, 1, 7, 9, -1 & MASK):
                yield mk(r, mouse, mode, answer)


def rnd(r):
    return mk(r, r.choice([0, 1]), r.choice([0, 1]), r.choice([8, 8, 0, 1, r.randrange(1 << 32)]))


TESTS = [
    FnTest("CoasterPanelEndTrack", ppc, host, edge=edges, rnd=rnd),
]
