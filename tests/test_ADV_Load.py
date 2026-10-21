"""Unit v5685, the advisor's part of the save file, against libtpw_unit_ADV_Load.dylib (tests/unit_ADV_Load.cpp), main app
("SimTheme Park"), src/game/advisor/Advisor.cpp:
  ADV_Save 0x10009788, ADV_Load 0x10009cb8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides: LbFile_Read (answers the bytes of the case's file, queue "fb"), LbFile_Write (records the bytes as words),
the platform's _USwapCopy32 / _USwapDelete (copies; the file is little-endian) and malloc / free / _USwapBlock32 (done for real; the
original's in-binary swap helpers run as the original on the PowerPC).

The world of a case: the advisor state (0x168 bytes on the Mac, as words; the host's has a native 64-bit data pointer, so its words are
written at the host's offsets), the data pointer (none, the static buffer, or a heap block), the static buffer's in-use flag and words,
and for ADV_Load the file. What is compared: the event log (reads, writes with every written word, copies and deletes), the state's words,
what the data pointer points to (none / static buffer / the file's own word / heap), the flag, the static buffer, the loaded data.

No fused operations. Not tested: ADV_Load with a state length other than 0x168 (the original swaps the fields the file did not provide,
which the host, where the swaps are no-ops, cannot repeat), a negative lip-sync size, a size that is not a multiple of 4; free() of the
old data pointer cannot be seen from the host (the real free runs, and the test hands it a real malloc block), and the malloc / free
calls are not in the log for the same reason; the data pointer's value is compared as a class (address differs between the two memories).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ADV_Load"
MASK = 0xFFFFFFFF

FN_SAVE, FN_LOAD = 0x10009788, 0x10009cb8
SIZES = {FN_SAVE: 808, FN_LOAD: 708}
OWN = set(SIZES)

READ, WRITE, FREE, MALLOC = 0x10238848, 0x10238878, 0x10238a70, 0x10239c58
SWAP_BLOCK, SWAP_COPY, SWAP_DELETE = 0x10239a78, 0x10239b68, 0x10239b80

ADDR = dict(state=0x102b354c, inuse=0x102b36b4, sbuf=0x102b36b8)
HOST_LAYOUT = [0x140, 0x148, 0x134, 0x168, 0x170]
FILE = 0x12345670           # the file handle: never dereferenced
TAG = 0x5f564441
PTR = 80                    # the data pointer's word index in the image


def s32(v):
    return T.s32(v)


def wchunks(data):
    """The written bytes as events: word index, then up to ten words (the pointer word of a state image as 0 or 1)."""
    n = len(data) // 4
    words = list(struct.unpack("<%dI" % n, data[:4 * n]))
    if len(data) == 0x168:
        words[PTR] = 1 if words[PTR] else 0
    for i in range(n):
        yield (i, words[i])


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_read(e):
    """LbFile_Read(sret, file, buffer, length, transferred)"""
    L = e.cl
    n, buf, xfer = e.r(6), e.r(5), e.r(7)
    L.rec("read", (L.lab(e.r(4)), n, 1 if xfer else 0))
    if n:
        data = L.file[L.pos:L.pos + n]
        L.pos += len(data)
        e.write(buf, data + bytes(n - len(data)))
    if xfer:
        e.write(xfer, struct.pack(">I", n))
    e.write(e.r(3), struct.pack(">I", 0))


def stub_write(e):
    """LbFile_Write(sret, file, buffer, length, transferred)"""
    L = e.cl
    n, buf, xfer = e.r(6), e.r(5), e.r(7)
    L.rec("write", (L.lab(e.r(4)), n, 1 if xfer else 0))
    data = e.read(buf, n) if n else b""
    for chunk in wchunks(data):
        L.rec("wd", chunk)
    if xfer:
        e.write(xfer, struct.pack(">I", n))
    e.write(e.r(3), struct.pack(">I", 0))


def stub_swap_block(e):
    a, n = e.r(3), s32(e.r(4))
    if n > 0:
        words = struct.unpack(">%dI" % n, e.read(a, 4 * n))
        e.write(a, struct.pack("<%dI" % n, *words))


def stub_swap_copy(e):
    a, n = e.r(3), s32(e.r(4))
    e.cl.rec("USwapCopy32", (n & MASK,))
    n = max(n, 0)
    words = struct.unpack(">%dI" % n, e.read(a, 4 * n)) if n else ()
    out = e.alloc(max(4 * n, 4))
    if n:
        e.write(out, struct.pack("<%dI" % n, *words))
    e.set_r(3, out)


def stub_swap_delete(e):
    e.cl.rec("USwapDelete", (0,))


def stub_malloc(e):
    e.set_r(3, e.alloc(max(e.r(3), 4)))


def stub_free(e):
    pass


STUBS.internal(READ, stub_read)
STUBS.internal(WRITE, stub_write)
STUBS.internal(SWAP_BLOCK, stub_swap_block)
STUBS.internal(SWAP_COPY, stub_swap_copy)
STUBS.internal(SWAP_DELETE, stub_swap_delete)
STUBS.internal(MALLOC, stub_malloc)
STUBS.internal(FREE, stub_free)


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
        self.L = e.cl = CL.PpcLog({FILE: 1}, case["queues"])
        self.L.file, self.L.pos = case["file"], 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def so(self, off):
        return ADDR["state"] + off

    def alloc(self, n):
        return self.e.alloc(n)

    def malloc(self, n):
        return self.e.alloc(max(n, 4))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def get_ptr(self):
        return self.rd_word(self.so(0x140))

    def set_ptr(self, v):
        self.words(self.so(0x140), [v])

    def log(self):
        return self.L.log

    def call(self, fn):
        T.call_ppc(self.e, fn, [FILE])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.fb = case["file"]
        CL.host_begin(lib, {1: FILE}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_adv_addr(name.encode())

    def so(self, off):
        return self.g("state") + (off if off <= 0x140 else off + 4)

    def alloc(self, n):
        return self.lib.tpw_adv_alloc(n)

    def malloc(self, n):
        return self.lib.tpw_adv_malloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def get_ptr(self):
        return struct.unpack("<Q", self.read(self.so(0x140), 8))[0]

    def set_ptr(self, v):
        self.write(self.so(0x140), struct.pack("<Q", v))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn):
        self.file = C.create_string_buffer(bytes(self.fb) + b"\0", len(self.fb) + 1)
        self.lib.tpw_adv_setfile(self.file, len(self.fb))
        (self.lib.tpw_adv_load if fn == FN_LOAD else self.lib.tpw_adv_save)(FILE)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_adv_"
    for name in ("addr", "alloc", "malloc"):
        getattr(lib, p + name).restype = C.c_uint64
    lib.tpw_adv_addr.argtypes = [C.c_char_p]
    lib.tpw_adv_alloc.argtypes = [C.c_uint32]
    lib.tpw_adv_malloc.argtypes = [C.c_uint32]
    lib.tpw_adv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_adv_setfile.argtypes = [C.c_char_p, C.c_uint32]
    lib.tpw_adv_load.argtypes = [C.c_uint64]
    lib.tpw_adv_save.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_adv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    st = case["state"]
    s.words(s.so(0), st[:80])
    s.words(s.so(0x144), st[81:90])
    kind = case["p0"]
    if kind == "none":
        s.set_ptr(0)
    elif kind == "static":
        s.set_ptr(s.g("sbuf"))
    else:
        h = s.malloc(4 * len(case["hdata"]) + 16)
        s.words(h, case["hdata"])
        s.set_ptr(h)
    s.words(s.g("inuse"), [case["inuse"]])
    s.words(s.g("sbuf"), case["sbuf"])


def execute(s, case):
    s.call(FN_LOAD if case["op"] == "load" else FN_SAVE)


def pclass(s, case):
    p = s.get_ptr()
    if p == 0:
        return "N"
    if p == s.g("sbuf"):
        return "S"
    if case["v"] and p == case["v"]:
        return "V"
    return "H"


def observe(s, case):
    st = s.rd_words(s.so(0), 80) + s.rd_words(s.so(0x144), 9)
    size = s32(st[77])
    cls = pclass(s, case)
    out = {"log": s.log(), "state": st, "class": cls, "inuse": s.rd_word(s.g("inuse")), "sbuf": s.rd_words(s.g("sbuf"), 250)}
    if case["op"] == "load" and cls in ("S", "H") and 0 < size <= 1300:
        out["data"] = s.rd_words(s.get_ptr(), size // 4)
    return out


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def base(r, op):
    st = rwords(r, 90)
    return {"op": op, "state": st, "p0": "none", "hdata": [], "inuse": r.choice([0, 1]), "sbuf": rwords(r, 250), "v": 0,
            "file": b"", "queues": {"fb": [0]}}


def rwords(r, n):
    return [r.choice([0, 1, MASK, r.getrandbits(32), r.getrandbits(32)]) for _ in range(n)]


def load_case(r, magic=TAG, size=None, p0=None, inuse=None, v=None):
    c = base(r, "load")
    if p0 is None:
        p0 = r.choice(["none", "static", "heap"])
    if size is None:
        size = r.choice([0, 4, 8, 400, 996, 1000, 1004, 4 * r.randrange(1, 300)])
    if inuse is not None:
        c["inuse"] = inuse
    c["p0"] = p0
    c["hdata"] = rwords(r, 4)
    image = rwords(r, 90)
    if v is None:
        v = r.choice([0, r.getrandbits(32) | 1])
    image[PTR] = v
    c["v"] = v
    data = struct.pack("<II", magic, 0x168) + struct.pack("<90I", *image) + struct.pack("<I", size & MASK)
    data += b"".join(struct.pack("<I", r.getrandbits(32)) for _ in range(size // 4))
    c["file"] = data
    return c


def save_case(r, p0=None, size=None):
    c = base(r, "save")
    if p0 is None:
        p0 = r.choice(["none", "static", "heap"])
    if size is None:
        size = 4 * r.choice([0, 1, 2, r.randrange(1, 250)])
    c["p0"] = p0
    c["state"][77] = size
    if p0 == "heap":
        c["hdata"] = rwords(r, size // 4 + 2 + r.randrange(4))
    return c


def load_edges():
    r = random.Random(5685)
    for magic in (TAG ^ 1, TAG ^ 0x10000, TAG ^ 0x80000000, 0, TAG + 1):
        for p0 in ("none", "static", "heap"):
            yield load_case(r, magic=magic, p0=p0)
    for p0 in ("none", "static", "heap"):
        for inuse in (0, 1):
            for size in (0, 4, 8, 996, 1000, 1004, 40):
                yield load_case(r, p0=p0, inuse=inuse, size=size)
    yield load_case(r, v=0, size=0)
    yield load_case(r, v=0x12345678, size=0)
    yield load_case(r, v=0x12345678, size=8, inuse=1)


def load_random(r):
    return load_case(r)


def save_edges():
    r = random.Random(9788)
    for p0 in ("none", "static", "heap"):
        for size in (0, 4, 8, 400, 996, 1000):
            yield save_case(r, p0=p0, size=size)


def save_random(r):
    return save_case(r)


TESTS = [
    FnTest("ADV_Save", ppc, host, edge=save_edges, rnd=save_random),
    FnTest("ADV_Load", ppc, host, edge=load_edges, rnd=load_random),
]
