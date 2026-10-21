"""Unit v5148, Outbox::ReadMailData against libtpw_unit_Outbox_ReadMailData.dylib (tests/unit_Outbox_ReadMailData.cpp), main app
("SimTheme Park"), src/game/online/email.cpp:
  Outbox::ReadMailData 0x102049fc

A case is a world and a call. The Outbox is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets for the
original (the MSL strings as a pointer to a block whose word at +0xc points at the characters, the list as a Python model kept by the
fakes), and as the host library's Outbox object for the translation. The MSL string and list functions (ctor, assign, append, dtor,
list erase and insert) are modelled on the PowerPC side and are the host's own std::string and std::list. The stream (ifstream ctor,
open, close, getline, the three destructors, getloc / use_facet / widen through the pointer glue, the exception __throw) is faked on
both sides with the same events: ctor, open (mode, name), widen, getline (size, delimiter), close, destroy, throw (message). The lines
getline delivers come from a table, the stream's state bits, mask and buffer from the case's queues; sprintf, strlen, strcpy, new[] are
the std stubs, atoi is a C atoi.

The world of a case: the folder name mF00, the other strings, the three numbers and the list (so erasing is seen), the line table, the
queues (open result, the filebuf's open word, mask, buffer, widen answers, line indices, state bits, close result). What is compared:
the answer, the event log, the four strings, the list, the three numbers.

No fused operations. Not tested: lines longer than the Mac's 132 byte buffer (the original overruns its frame), a state that never ends
the read loop (the original would not stop either).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Outbox_ReadMailData"
MASK = 0xFFFFFFFF

FN = 0x102049fc
SIZES = {FN: 1836}
OWN = set(SIZES)

STR_OFFS = (0x00, 0x04, 0x08, 0x18)
LIST_OFF = 0x0C
INT_OFFS = (0x1C, 0x20, 0x24)


def s32(v):
    return T.s32(v)


def s8(v):
    v &= 0xFF
    return v - 256 if v & 0x80 else v


# ---------------------------------------------------------------------------------------------------------------- PPC model

class Model:
    lists = {}
    lines = []


def mkstr(e, text):
    """An MSL string: a pointer to a block whose word at +0xc points at the characters."""
    data = text.encode("latin-1")
    chars = e.alloc(len(data) + 1, data + b"\0")
    rep = e.alloc(0x10)
    e.write(rep + 0xC, struct.pack(">I", chars))
    return rep


def set_str(e, addr, text):
    e.write(addr, struct.pack(">I", mkstr(e, text)))


def get_str(e, addr):
    return e.cstr(e.u32(e.u32(addr) + 0xC))


def atoi(text):
    t = text.lstrip(" \t\n\v\f\r")
    sign = 1
    if t[:1] in ("+", "-"):
        sign = -1 if t[0] == "-" else 1
        t = t[1:]
    n = 0
    for ch in t:
        if not ch.isdigit():
            break
        n = n * 10 + int(ch)
    return s32(sign * n)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_str_empty(e):                 # basic_string(const allocator&) 0x1007363c
    set_str(e, e.r(3), "")


def st_assign_sub(e):                # assign(const string&, pos, n) 0x10001c80
    text = get_str(e, e.r(4))
    pos, n = e.r(5), s32(e.r(6))
    text = text[pos:] if n == -1 else text[pos:pos + n]
    set_str(e, e.r(3), text)


def st_append(e):                    # append(const char*, n) 0x1020100c
    text = get_str(e, e.r(3)) + e.read(e.r(4), e.r(5)).decode("latin-1")
    set_str(e, e.r(3), text)


def st_assign_ptr(e):                # assign(const char*, n) 0x101b2440
    set_str(e, e.r(3), e.read(e.r(4), e.r(5)).decode("latin-1"))


def st_str_ctor(e):                  # basic_string(const char*, const allocator&) 0x10002338
    set_str(e, e.r(3), e.cstr(e.r(4)))


def st_nothing(e):
    pass


def st_erase(e):                     # list::erase(first, last), here begin to end 0x102084d8
    Model.lists[e.r(4)] = []


def st_insert(e):                    # list::insert(pos, const string&) 0x101b25e0
    Model.lists.setdefault(e.r(4), []).append(get_str(e, e.r(6)))


def st_ios_ctor(e):                  # basic_ifstream() 0x1020801c: the ios part sits 0x4c after the stream
    L = e.cl
    s = e.r(3)
    L.rec("ctor", ())
    ios = s + 0x4C
    e.write(s, struct.pack(">I", ios))
    e.write(ios + 0x24, struct.pack(">I", L.ret("rdbuf") & MASK))
    e.write(ios + 0x34, struct.pack(">I", 0))
    e.write(ios + 0x38, struct.pack(">I", L.ret("mask") & MASK))


def st_open(e):                      # filebuf::open(name, mode) 0x10071c44
    L = e.cl
    L.rec("open", (s32(e.r(5)),), (e.cstr(e.r(4)),))
    e.write(e.r(3) + 0x24, struct.pack(">I", L.ret("openflag") & MASK))
    e.set_r(3, L.ret("open") & MASK)


def st_close(e):                     # filebuf::close() 0x10071b54
    L = e.cl
    L.rec("close", ())
    e.set_r(3, L.ret("close") & MASK)


def st_destroy(e):                   # ~filebuf 0x100700e4 (~istream and ~ios are silent)
    e.cl.rec("destroy", ())


def st_getline(e):                   # istream::getline(buf, n, delim) 0x102079f8
    L = e.cl
    s, buf, n, delim = e.r(3), e.r(4), s32(e.r(5)), s8(e.r(6))
    L.rec("getline", (n, delim))
    idx = L.ret("line")
    text = Model.lines[idx % len(Model.lines)] if Model.lines else ""
    data = text.encode("latin-1")[:n - 1]
    e.write(buf, data + b"\0")
    ios = e.u32(s)
    e.write(ios + 0x34, struct.pack(">I", (e.u32(ios + 0x34) | L.ret("state")) & MASK))
    e.set_r(3, s)


def st_widen(e):                     # the virtual widen through __ptr_glue 0x1023baa8
    L = e.cl
    L.rec("widen", (s32(e.r(4)),))
    e.set_r(3, L.ret("widen") & MASK)


def st_facet(e):                     # use_facet<ctype<char>>(locale) 0x10073934: a facet whose vtable has a slot at +0x1c (the glue is faked)
    vt = e.alloc(0x40)
    obj = e.alloc(0x10)
    e.write(obj, struct.pack(">I", vt))
    e.set_r(3, obj)


def st_refptr(e):                   # _RefCountedPtr<char>(char*) 0x100026ac
    e.write(e.r(3), struct.pack(">I", e.r(4)))


def st_throw(e):                     # __throw(type, object, vtable): the object's word at +4 is the _RefCountedPtr
    e.cl.rec("throw", (), (e.cstr(e.u32(e.r(4) + 4)),))


def st_atoi(e):
    e.set_r(3, atoi(e.cstr(e.r(3))) & MASK)


for addr, fn in ((0x1007363c, st_str_empty), (0x10001c80, st_assign_sub), (0x1020100c, st_append), (0x101b2440, st_assign_ptr),
                 (0x10002338, st_str_ctor), (0x1000221c, st_nothing), (0x102084d8, st_erase), (0x101b25e0, st_insert),
                 (0x1020801c, st_ios_ctor), (0x10071c44, st_open), (0x10071b54, st_close), (0x100700e4, st_destroy),
                 (0x102043a8, st_nothing), (0x1006ff50, st_nothing), (0x102079f8, st_getline), (0x1023baa8, st_widen),
                 (0x10072a34, st_nothing), (0x10073934, st_facet),(0x10071f34, st_nothing), (0x100026ac, st_refptr)):
    STUBS.internal(addr, fn)
STUBS.imp("atoi", st_atoi)
STUBS.imp("__throw", st_throw)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        Model.lists = {}
        Model.lines = list(case["lines"])
        self.obj = e.alloc(0x60)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def set_str(self, off, text):
        set_str(self.e, self.obj + off, text)

    def get_str(self, off):
        return get_str(self.e, self.obj + off)

    def set_int(self, i, v):
        self.e.write(self.obj + INT_OFFS[i], struct.pack(">I", v & MASK))

    def get_int(self, i):
        return s32(self.e.u32(self.obj + INT_OFFS[i]))

    def set_list(self, items):
        Model.lists[self.obj + LIST_OFF] = list(items)

    def get_list(self):
        return list(Model.lists.get(self.obj + LIST_OFF, []))

    def log(self):
        return self.L.log

    def call(self, index):
        r3, _, _ = T.call_ppc(self.e, FN, [self.obj, index & MASK])
        return r3 & MASK


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_ob_reset()
        for t in case["lines"]:
            lib.tpw_ob_line_add(t.encode("latin-1"))
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def set_str(self, off, text):
        self.lib.tpw_ob_set_str(off, text.encode("latin-1"))

    def get_str(self, off):
        buf = C.create_string_buffer(512)
        self.lib.tpw_ob_get_str(off, buf, 512)
        return buf.value.decode("latin-1")

    def set_int(self, i, v):
        self.lib.tpw_ob_set_int(i, s32(v))

    def get_int(self, i):
        return self.lib.tpw_ob_get_int(i)

    def set_list(self, items):
        for t in items:
            self.lib.tpw_ob_list_add(t.encode("latin-1"))

    def get_list(self):
        out = []
        for i in range(self.lib.tpw_ob_list_count()):
            buf = C.create_string_buffer(512)
            self.lib.tpw_ob_list_get(i, buf, 512)
            out.append(buf.value.decode("latin-1"))
        return out

    def log(self):
        return CL.host_end(self.lib)

    def call(self, index):
        return self.lib.tpw_ob_read(s32(index)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ob_set_str.argtypes = [C.c_int32, C.c_char_p]
    lib.tpw_ob_get_str.argtypes = [C.c_int32, C.c_char_p, C.c_uint32]
    lib.tpw_ob_get_str.restype = C.c_uint32
    lib.tpw_ob_set_int.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_ob_get_int.argtypes = [C.c_int32]
    lib.tpw_ob_get_int.restype = C.c_int32
    lib.tpw_ob_list_add.argtypes = [C.c_char_p]
    lib.tpw_ob_list_count.restype = C.c_uint32
    lib.tpw_ob_list_get.argtypes = [C.c_uint32, C.c_char_p, C.c_uint32]
    lib.tpw_ob_line_add.argtypes = [C.c_char_p]
    lib.tpw_ob_read.argtypes = [C.c_int32]
    lib.tpw_ob_read.restype = C.c_uint32
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def build(s, case):
    for off, text in zip(STR_OFFS, case["strs"]):
        s.set_str(off, text)
    for i, v in enumerate(case["ints"]):
        s.set_int(i, v)
    s.set_list(case["list"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "strs": [s.get_str(o) for o in STR_OFFS],
            "ints": [s.get_int(i) for i in range(3)], "list": s.get_list()}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["index"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

LINES = ["123", "-45", "0", "abc", "12ab", "  7", "", "2147483647", "-2147483648", "line text", "x" * 100, "+9", "5 6", "a.b@c.d", "99999"]
INDEXES = [0, 1, -1, 0x1234, 0x7FFFFFFF, -0x80000000, 255, 0xABCDEF]


def make(r, op=1, of=1, mask=None, rdbuf=1, close=1):
    n = r.randrange(0, 6)
    lines = [r.choice(LINES) for _ in range(r.randrange(1, 9))]
    zeros = r.randrange(0, 12)
    state = [0] * zeros + [r.choice([1, 3, 4, 5, 6, 7])]
    return {
        "index": r.choice(INDEXES) if r.random() < 0.6 else T.s32(r.getrandbits(32)),
        "strs": [r.choice(["", "Data:outbox", "a", "Mail:folder:x"]), r.choice(["", "old"]), r.choice(["", "old2"]), r.choice(["", "jpg"])],
        "ints": [r.choice([0, 5, -1, 77]) for _ in range(3)],
        "list": [r.choice(["a", "bb", ""]) for _ in range(n)],
        "lines": lines,
        "queues": {
            "open": [op], "openflag": [of], "mask": [r.choice([0, 1, 4, 5, 2, 8, 0xFFFF]) if mask is None else mask],
            "rdbuf": [rdbuf], "close": [close],
            "widen": [r.choice([10, 10, 65, 0, 127, 255]) for _ in range(r.randrange(1, 4))],
            "line": [r.randrange(0, 40) for _ in range(r.randrange(1, 12))],
            "state": state,
        },
    }


def edges():
    r = random.Random(4801)
    for op in (0, 1):
        for of in (0, 1):
            for mask in (0, 1, 2, 4, 5):
                for rdbuf in (0, 1):
                    for close in (0, 1):
                        yield make(r, op, of, mask, rdbuf, close)


def rnd(r):
    return make(r, r.choice([0, 1, 1, 1]), r.choice([0, 1, 1, 1]), None, r.choice([0, 1, 1, 1]), r.choice([0, 1, 1]))


TESTS = [
    FnTest("Outbox::ReadMailData", ppc, host, edge=edges, rnd=rnd),
]
