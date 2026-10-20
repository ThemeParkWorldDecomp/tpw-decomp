"""Unit v51120, twenty small ltms functions, against libtpw_unit_IsDelimiter.dylib (tests/unit_IsDelimiter.cpp), main app
("SimTheme Park"), src/ltms_textformatter_rest_IsDelimiter.cpp (and, tested where they are, IsDelimiter in
src/ltms_textformatter_rest.cpp, SetHorizontalJustification and the vectors' tear_down in src/ltms_textformatter.cpp, TbITable::AddRef
in src/TbITableResource.cpp):
  IsDelimiter 0x100068c4, __sinit_TbIResourceIndex_cpp 0x10004068, TbTextFormatter::SetHorizontalJustification 0x1000632c,
  the destructors of the three vectors 0x100060ec 0x10005fb0 0x10005e7c, CCharInfoList 0x10006060, TbIUnicodeToMB 0x10005674,
  TbIMBToUnicode 0x100032a8, TbIRLE4bitFont 0x10004150, the two TbRefReleaseSmartPtr 0x1000514c 0x10004f64, std::_MSLstring 0x10008274,
  std::_RefCountedPtr 0x10008a10, logic_error::what 0x100089a0, exception::what 0x100089d8, the two byte swaps 0x1000439c 0x100043e4,
  TbITable::AddRef 0x10005324, TbMBStringVTag::Format 0x1000937c.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses
(big-endian) for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit are
fakes on both sides that record the same events: operator delete (0x1000f694), delete[] (0x1000f6ac), TbResource::AddRef,
TbIFont::~TbIFont, and the virtual calls through __ptr_glue (a resource's Release, a buffer's AppendMB). The vectors' tear_down runs
as the real code on both sides.

The world of a case: the blocks of case["blocks"] (labels 1, 2 ... in the log), filled by the case. What is compared: the answer, the
call log and the watched fields (words, halfwords, raw bytes, pointers as labels). The compiler's hidden delete flag is a signed 16
bit value: the host wrappers call the explicit destructor and ::operator delete for a flag above 0, as the original does.

No fused operations. Not tested: the vtable words the destructors store (the host's differ by nature); _RefCountedPtr's answer (the
header declares it void; the original answers this); the class destructors' base-class steps (inlined on both sides).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_IsDelimiter"
MASK = 0xFFFFFFFF

FN = dict(isdelim=0x100068c4, isparen=0x10006864, sinit=0x10004068, sethj=0x1000632c, vchar=0x100060ec, clist=0x10006060, vline=0x10005fb0,
          vpoint=0x10005e7c, uni=0x10005674, addref=0x10005324, smb=0x1000514c, smu=0x10004f64, mslstr=0x10008274,
          lwhat=0x100089a0, ewhat=0x100089d8, refc=0x10008a10, sw16=0x100043e4, sw32=0x1000439c, rle=0x10004150,
          fmt=0x1000937c, mb=0x100032a8)
OWN = set(FN.values())

DELETE, DELETE_ARRAY, PTR_GLUE, RES_ADDREF, FONT_DTOR = 0x1000f694, 0x1000f6ac, 0x1000f91c, 0x10005ae4, 0x10002cb8
RELEASE_SLOT = 0x7100      # the fake resource vtable's slot +8
BUF_SLOT = 0x7000          # the fake buffer vtable's slots +8 + 4k hold BUF_SLOT + k

# the five globals __sinit sets: (Mac address, the offset from the TOC (r2) of the vtable it points to)
SINIT = [(0x10011754, 0xe60), (0x10011758, 0xf14), (0x1001175c, 0xcf4), (0x10011760, 0xf74), (0x10011764, 0xd70)]

# offsets by key: (the Mac's, the host's layout index)
OFFS = {"hj": (0x98, 0), "cnt": (4, 1), "msg": (4, 2), "str": (0xC, 3)}
LAY = {}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


def stub_delete_array(e):
    L = e.cl
    if e.r(3) & MASK:   # the host's global fake logs delete[] under the same name and ignores a null pointer
        L.rec("delete", (L.lab(e.r(3)),))


def stub_glue(e):
    """__ptr_glue: the virtual call; r12 holds the slot's value, which names the fake it is."""
    L = e.cl
    r12 = e.r(12) & MASK
    if r12 == RELEASE_SLOT:
        L.rec("Release", (L.lab(e.r(3)),))
    else:
        L.rec("virt", (L.lab(e.r(3)), r12 - BUF_SLOT, L.lab(e.r(4)), s32(e.r(5))))
    e.set_r(3, 0)


def stub_res_addref(e):
    L = e.cl
    L.rec("TbResource_AddRef", (L.lab(e.r(3)),))


def stub_font_dtor(e):
    # The host links the real TbIFont::~TbIFont (src/TbIRLE4bitFont.cpp), which records nothing, so the stub records nothing either.
    pass


STUBS.internal(DELETE, stub_delete)
STUBS.internal(DELETE_ARRAY, stub_delete_array)
STUBS.internal(PTR_GLUE, stub_glue)
STUBS.internal(RES_ADDREF, stub_res_addref)
STUBS.internal(FONT_DTOR, stub_font_dtor)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def hw(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def rd_hw(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def A(self, label):
        return self.blk[label]

    def lab(self, a):
        return 0 if a == 0 else self.rev.get(a, -1)

    def init_blocks(self, case):
        self.blk, self.rev = {}, {}
        for label, (size, kind) in sorted(case["blocks"].items()):
            a = self.alloc(size)
            self.blk[label] = a
            self.rev[a] = label
            self.make(a, kind)

    def watch(self, case):
        out = []
        for label, spec, kind in case.get("watch", []):
            if label in getattr(self, "freed", ()):
                out.append("freed")   # given back to the allocator: the host's memory is dead, the fake leaves the Mac's alone
                continue
            a = self.A(label) + self.off(spec)
            if kind == "p":
                out.append(self.lab(self.rd_ptr(a)))
            elif kind == "w":
                out.append(self.rd_word(a))
            elif kind == "h":
                out.append(self.rd_hw(a))
            else:
                out.append(self.read(a, kind))
        return out


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.vt_res = e.alloc(0x40)
        self.vt_buf = e.alloc(0x40)
        e.write(self.vt_res + 8, struct.pack(">I", RELEASE_SLOT))
        e.write(self.vt_buf + 8, b"".join(struct.pack(">I", BUF_SLOT + k) for k in range(6)))
        self.init_blocks(case)
        self.L = e.cl = CL.PpcLog({a: label for a, label in self.rev.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def make(self, a, kind):
        self.write(a, bytes(self.size_of(a)))
        if kind == "res":
            self.words(a, [self.vt_res, 1])
        elif kind == "buf":
            self.words(a, [self.vt_buf])

    def size_of(self, a):
        return self._sizes[a]

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def off(self, spec):
        return OFFS[spec][0] if isinstance(spec, str) else spec

    def alloc(self, n):
        a = self.e.alloc(n)
        self._sizes = getattr(self, "_sizes", {})
        self._sizes[a] = n
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def wptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def call(self, key, args):
        r3, _, _ = T.call_ppc(self.e, FN[key], [a & MASK for a in args])
        return r3 & MASK

    def ret_lab(self, r):
        return self.lab(r)

    def cstr(self, r):
        return None if r == 0 else self.e.cstr(r)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self._kinds = {}
        self.init_blocks(case)
        CL.host_begin(lib, {label: a for label, a in self.blk.items()}, case["queues"])

    def make(self, a, kind):
        if kind == "res":
            self.lib.tpw_u_make(self._res_kind, a)
        elif kind == "buf":
            self.lib.tpw_u_make(3, a)

    _res_kind = 1

    def done(self):
        pass

    def off(self, spec):
        return LAY[OFFS[spec][1]] if isinstance(spec, str) else spec

    def alloc(self, n):
        return self.lib.tpw_u_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, key, args):
        lib = self.lib
        a = [x & 0xFFFFFFFFFFFFFFFF if i == 0 or key in PTR_ARGS.get(key, ()) else x for i, x in enumerate(args)]
        return HOST_CALLS[key](lib, args)

    def ret_lab(self, r):
        return self.lab(r)

    def cstr(self, r):
        return None if r == 0 else C.string_at(r).decode("latin-1")


PTR_ARGS = {}


def _vec(kind):
    return lambda lib, a: lib.tpw_u_vdtor(kind, a[0], s32(a[1]))


def _cd(kind):
    return lambda lib, a: lib.tpw_u_cdtor(kind, a[0], s32(a[1]))


def _sm(kind):
    return lambda lib, a: lib.tpw_u_smart(kind, a[0], s32(a[1]))


HOST_CALLS = {
    "isdelim": lambda lib, a: lib.tpw_u_isdelim(a[0]),
    "isparen": lambda lib, a: lib.tpw_u_isparen(a[0]),
    "sethj": lambda lib, a: lib.tpw_u_sethj(a[0], s32(a[1])),
    "vchar": _vec(0), "vline": _vec(1), "vpoint": _vec(2), "clist": _vec(3),
    "uni": _cd(0), "mb": _cd(1), "rle": _cd(2),
    "smb": _sm(0), "smu": _sm(1),
    "mslstr": lambda lib, a: lib.tpw_u_mslstring(a[0], s32(a[1])),
    "refc": lambda lib, a: lib.tpw_u_refdtor(a[0], s32(a[1])),
    "lwhat": lambda lib, a: lib.tpw_u_logicwhat(a[0]),
    "ewhat": lambda lib, a: lib.tpw_u_excwhat(a[0]),
    "sw16": lambda lib, a: lib.tpw_u_swap16(a[0]),
    "sw32": lambda lib, a: lib.tpw_u_swap32(a[0]),
    "addref": lambda lib, a: lib.tpw_u_addref(a[0]),
    "fmt": lambda lib, a: lib.tpw_u_format(a[0], a[1], a[2]),
}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32

    def sig(name, res, *args):
        f = getattr(lib, "tpw_u_" + name)
        f.restype, f.argtypes = res, list(args)

    sig("addr", u64, C.c_char_p)
    sig("alloc", u64, u32)
    sig("make", None, i32, u64)
    sig("layout", i32, C.POINTER(C.c_int64))
    sig("sinit", None)
    sig("isdelim", u32, u64)
    sig("isparen", u32, u64)
    sig("sethj", i32, u64, i32)
    sig("vdtor", u64, i32, u64, i32)
    sig("cdtor", u64, i32, u64, i32)
    sig("smart", u64, i32, u64, i32)
    sig("mslstring", u64, u64, i32)
    sig("refdtor", None, u64, i32)
    sig("logicwhat", u64, u64)
    sig("excwhat", u64, u64)
    sig("swap16", None, u64)
    sig("swap32", None, u64)
    sig("addref", None, u64)
    sig("format", i32, u64, u64, u64)
    out = (C.c_int64 * 32)()
    n = lib.tpw_u_layout(out)
    LAY.update({i: out[i] for i in range(n)})
    assert LAY[1] == 8 and LAY[2] == 8 and out[4] == 0, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world and the call


def run(s, case):
    try:
        op = case["op"]
        if op == "smart_host_kind":
            pass
        build(s, case)
        answer = execute(s, case)
        s.freed = {e[1][0] for e in s.log() if e[0] == "delete" and e[1]}
        return {"answer": answer, "log": s.log(), "watch": s.watch(case), "extra": extra(s, case)}
    finally:
        s.done()


def build(s, case):
    op = case["op"]
    A = s.A
    if op in ("isdelim", "isparen"):
        s.write(A(1), case["rest"])
        s.hw(A(1), case["ch"])
    elif op == "sethj":
        s.write(A(2), b"\x55\x55\x55\x55")
        s.words(A(1) + s.off("hj") - 4, struct.unpack(">3I", case["around"]))
    elif op in ("vec",):
        s.words(A(1), [case["cap"], case["size"]])
        s.wptr(A(1) + 8, A(2) if case["data"] else 0)
    elif op in ("mslstr", "refc"):
        s.wptr(A(1), A(3) if case["text"] else 0)
        s.wptr(A(1) + s.off("cnt"), A(2) if case["counter"] else 0)
        s.words(A(2), [case["count"]])
    elif op == "lwhat":
        s.wptr(A(1) + s.off("msg"), case["msg"](s))
        s.write(A(2), b"logic\0")
    elif op == "smart":
        s.wptr(A(1), A(2) if case["held"] else 0)
    elif op in ("sw",):
        s.write(A(1), case["bytes"])
    elif op == "fmt":
        s.wptr(A(1) + s.off("str"), A(3) if case["text"] else 0)
        s.write(A(3), b"abc\0")
        s.write(A(4), case["format"])
    elif op == "sinit":
        for i, (mac, _) in enumerate(SINIT):
            s.wptr(s.g_sinit(i), case["garbage"][i]) if False else s.set_garbage(i, case["garbage"][i])


def execute(s, case):
    op, A = case["op"], s.A
    if op in ("isdelim", "isparen"):
        return s.call(op, [A(1)])
    if op == "sethj":
        if isinstance(s, Host):
            r = s32(s.call("sethj", [A(1), case["value"]]))
            s.words(A(2), [r])
            return r
        s.call("sethj", [A(2), A(1), case["value"]])
        return s32(s.rd_word(A(2)))
    if op == "vec":
        return s.ret_lab(s.call(case["fn"], [A(1) if not case["null"] else 0, case["flag"]]))
    if op == "mslstr":
        return s.ret_lab(s.call("mslstr", [A(1) if not case["null"] else 0, case["flag"]]))
    if op == "refc":
        s.call("refc", [A(1) if not case["null"] else 0, case["flag"]])
        return None
    if op == "cdtor":
        return s.ret_lab(s.call(case["fn"], [A(1) if not case["null"] else 0, case["flag"]]))
    if op == "smart":
        return s.ret_lab(s.call(case["fn"], [A(1) if not case["null"] else 0, case["flag"]]))
    if op == "lwhat":
        return s.ret_lab(s.call("lwhat", [A(1)]))
    if op == "ewhat":
        return s.cstr(s.call("ewhat", [A(1) if not case["null"] else 0]))
    if op == "sw":
        s.call(case["fn"], [A(1) + case["at"]])
        return None
    if op == "addref":
        s.call("addref", [A(1) if not case["null"] else 0])
        return None
    if op == "fmt":
        r = s.call("fmt", [A(1), A(2), A(4)])
        return s32(r)
    if op == "sinit":
        s.call_sinit()
        return None
    raise AssertionError(op)


def extra(s, case):
    if case["op"] == "sinit":
        return s.sinit_values()
    if case["op"] == "sw":
        return s.read(s.A(1), 16)
    return None


# the sinit pieces of each side
def _ppc_g(self, i):
    return SINIT[i][0]


def _ppc_garbage(self, i, v):
    self.words(SINIT[i][0], [v])


def _ppc_call_sinit(self):
    T.call_ppc(self.e, FN["sinit"], [])


def _ppc_sinit_values(self):
    r2 = self.e.r(2) & MASK
    toc = {imm: k for k, (_, imm) in enumerate(SINIT)}
    return [toc.get((self.rd_word(mac) - r2) & MASK, -1) for mac, _ in SINIT]


def _host_garbage(self, i, v):
    a = self.lib.tpw_u_addr(("vptr%d" % i).encode())
    self.write(a, struct.pack("<Q", v))


def _host_call_sinit(self):
    self.lib.tpw_u_sinit()


def _host_sinit_values(self):
    vt = {self.lib.tpw_u_addr(("vtbl%d" % k).encode()): k for k in range(5)}
    out = []
    for i in range(5):
        a = self.lib.tpw_u_addr(("vptr%d" % i).encode())
        out.append(vt.get(struct.unpack("<Q", self.read(a, 8))[0], -1))
    return out


Ppc.set_garbage, Ppc.call_sinit, Ppc.sinit_values = _ppc_garbage, _ppc_call_sinit, _ppc_sinit_values
Host.set_garbage, Host.call_sinit, Host.sinit_values = _host_garbage, _host_call_sinit, _host_sinit_values


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    s = Host(lib, case) if case.get("res_kind") is None else _host_with(lib, case)
    return run(s, case)


def _host_with(lib, case):
    Host._res_kind = case["res_kind"]
    return Host(lib, case)


# ---------------------------------------------------------------------------------------------------------------- the cases

FLAGS = [0, 1, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000, -32768, 32767, 0x7FFF0000, -0x10001]


def rflag(r):
    return r.choice(FLAGS) if r.random() < 0.6 else r.randrange(-2 ** 31, 2 ** 31)


def base(op, **kw):
    c = {"op": op, "queues": {}, "blocks": {}, "watch": []}
    c.update(kw)
    return c


def rbytes(r, n):
    return bytes(r.randrange(256) for _ in range(n))


# ---- IsDelimiter
def isdelim_case(r, ch=None):
    c = base("isdelim", blocks={1: (16, "p")}, watch=[(1, 0, "h"), (1, 2, 14)])
    c["ch"] = r.choice([0x20, 0x2C, 0x2D, 0x21, 0, 0x120, 0x2C00, 0x2000, 0xFFFF, r.randrange(65536)]) if ch is None else ch
    c["rest"] = rbytes(r, 16)
    return c


def isdelim_edges():
    r = random.Random(101)
    for ch in (0x20, 0x2C, 0x1F, 0x21, 0x2B, 0x2D, 0, 0x120, 0x20 << 8, 0x2C << 8, 0xFFFF, 0x8020, 0x802C, 0x09, 0x2E):
        yield isdelim_case(r, ch)


# ---- IsParenthesis (unit v5L21)
def isparen_case(r, ch=None):
    c = isdelim_case(r, ch)
    c["op"] = "isparen"
    if ch is None:
        c["ch"] = r.choice([0x28, 0x29, 0x27, 0x2A, 0x5B, 0x5D, 0, 0x128, 0x2800, 0x2900, 0xFFFF, r.randrange(65536)])
    return c


def isparen_edges():
    r = random.Random(102)
    for ch in (0x28, 0x29, 0x27, 0x2A, 0x5B, 0x5D, 0x7B, 0x7D, 0, 0x128, 0x129, 0x28 << 8, 0x29 << 8, 0xFFFF, 0x8028, 0x8029):
        yield isparen_case(r, ch)


# ---- sethj
def sethj_case(r, value=None):
    c = base("sethj", blocks={1: (0x200, "p"), 2: (8, "p")}, watch=[(1, "hj", "w"), (2, 0, "w")])
    c["watch"] = [(1, "hj", "w"), (2, 0, "w")]
    c["value"] = (r.choice([0, 1, 2, 3, 4, 5, -1, -2, 2 ** 31 - 1, -2 ** 31, 0x100, r.randrange(-2 ** 31, 2 ** 31)])
                  if value is None else value) & MASK
    c["around"] = rbytes(r, 12)
    return c


def sethj_edges():
    r = random.Random(102)
    for v in (0, 1, 2, 3, 4, 5, -1, -2, 7, 2 ** 31 - 1, -2 ** 31, 0x10000, 0x100000002):
        yield sethj_case(r, v)


# ---- the vectors and CCharInfoList
def vec_case(r, fn, null=None, flag=None, data=None):
    c = base("vec", fn=fn, blocks={1: (16, "p"), 2: (0x40, "p")})
    c["watch"] = [(1, 0, "w"), (1, 4, "w"), (1, 8, "p")]
    c["null"] = (r.random() < 0.15) if null is None else null
    c["flag"] = rflag(r) if flag is None else flag
    c["data"] = (r.random() < 0.7) if data is None else data
    c["cap"], c["size"] = r.choice([(0, 0), (4, 2), (16, 16), (1, 0), (0xFFFFFFFF, 0xFFFFFFFF)]), None
    c["cap"], c["size"] = c["cap"]
    return c


def vec_edges(fn):
    def gen():
        r = random.Random(103)
        for null in (False, True):
            for flag in FLAGS:
                for data in (True, False):
                    yield vec_case(r, fn, null, flag, data)
    return gen


# ---- the resource destructors
def cdtor_case(r, fn, null=None, flag=None):
    c = base("cdtor", fn=fn, blocks={1: (0x40, "p")})
    c["null"] = (r.random() < 0.15) if null is None else null
    c["flag"] = rflag(r) if flag is None else flag
    return c


def cdtor_edges(fn):
    def gen():
        r = random.Random(104)
        for null in (False, True):
            for flag in FLAGS:
                yield cdtor_case(r, fn, null, flag)
    return gen


# ---- the smart pointers
def smart_case(r, fn, null=None, flag=None, held=None):
    c = base("smart", fn=fn, blocks={1: (16, "p"), 2: (0x20, "res")}, res_kind=1 if fn == "smb" else 2)
    c["watch"] = [(1, 0, "p")]
    c["null"] = (r.random() < 0.15) if null is None else null
    c["flag"] = rflag(r) if flag is None else flag
    c["held"] = (r.random() < 0.7) if held is None else held
    return c


def smart_edges(fn):
    def gen():
        r = random.Random(105)
        for null in (False, True):
            for flag in FLAGS:
                for held in (True, False):
                    yield smart_case(r, fn, null, flag, held)
    return gen


# ---- _RefCountedPtr and _MSLstring
COUNTS = [1, 2, 0, -1, 5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 3]


def counted_case(r, op, null=None, flag=None, counter=None, count=None, text=None):
    c = base(op, blocks={1: (32, "p"), 2: (16, "p"), 3: (16, "p")})
    c["watch"] = [(1, 0, "p"), (1, "cnt", "p"), (2, 0, "w")]
    c["null"] = (r.random() < 0.1) if null is None else null
    c["flag"] = rflag(r) if flag is None else flag
    c["counter"] = (r.random() < 0.85) if counter is None else counter
    c["count"] = r.choice(COUNTS) if count is None else count
    c["text"] = (r.random() < 0.8) if text is None else text
    return c


def counted_edges(op):
    def gen():
        r = random.Random(106)
        for null in (False, True):
            for count in COUNTS:
                for flag in (0, 1, -1, 0x10000, 0x8000):
                    for counter in (True, False):
                        yield counted_case(r, op, null, flag, counter, count, True)
        yield counted_case(r, op, False, 1, True, 1, False)
    return gen


# ---- what
def lwhat_case(r, kind=None):
    c = base("lwhat", blocks={1: (32, "p"), 2: (16, "p")}, watch=[(1, "msg", "p")])
    k = r.choice(["block", "null", "self", "wild"]) if kind is None else kind
    c["msg"] = {"block": lambda s: s.A(2), "null": lambda s: 0, "self": lambda s: s.A(1),
                "wild": lambda s: 0x12345678}[k]
    return c


def lwhat_edges():
    r = random.Random(107)
    for k in ("block", "null", "self", "wild"):
        yield lwhat_case(r, k)


def ewhat_case(r, null=None):
    c = base("ewhat", blocks={1: (16, "p")})
    c["null"] = (r.random() < 0.3) if null is None else null
    return c


def ewhat_edges():
    r = random.Random(108)
    yield ewhat_case(r, False)
    yield ewhat_case(r, True)


# ---- the swaps
def sw_case(r, fn, at=None, data=None):
    c = base("sw", fn=fn, blocks={1: (32, "p")}, watch=[(1, 0, 32)])
    c["at"] = r.choice([0, 4, 8, 16]) if at is None else at
    c["bytes"] = rbytes(r, 32) if data is None else data
    return c


def sw_edges(fn):
    def gen():
        r = random.Random(109)
        for data in (bytes(32), b"\xff" * 32, bytes(range(32)), b"\x01\x02\x03\x04" * 8, b"\x80\x00\x00\x01" * 8):
            for at in (0, 4, 8):
                yield sw_case(r, fn, at, data)
    return gen


# ---- AddRef
def addref_case(r, null=None):
    c = base("addref", blocks={1: (0x40, "p")})
    c["null"] = (r.random() < 0.2) if null is None else null
    return c


def addref_edges():
    r = random.Random(110)
    yield addref_case(r, False)
    yield addref_case(r, True)


# ---- Format
def fmt_case(r, text=None):
    c = base("fmt", blocks={1: (0x20, "p"), 2: (0x20, "buf"), 3: (16, "p"), 4: (16, "p")}, watch=[(1, "str", "p")])
    c["text"] = (r.random() < 0.8) if text is None else text
    c["format"] = rbytes(r, 12)
    return c


def fmt_edges():
    r = random.Random(111)
    yield fmt_case(r, True)
    yield fmt_case(r, False)


# ---- __sinit
def sinit_case(r):
    c = base("sinit")
    c["garbage"] = [r.choice([0, 0xDEADBEEF, r.randrange(2 ** 32)]) for _ in range(5)]
    return c


def sinit_edges():
    r = random.Random(112)
    for _ in range(3):
        yield sinit_case(r)


def rnd(fn, *a):
    return lambda r: fn(r, *a)


def edges_of(gen):
    return gen


TESTS = [
    FnTest("IsDelimiter", ppc, host, edge=isdelim_edges, rnd=rnd(isdelim_case)),
    FnTest("IsParenthesis", ppc, host, edge=isparen_edges, rnd=rnd(isparen_case)),
    FnTest("__sinit_TbIResourceIndex_cpp", ppc, host, edge=sinit_edges, rnd=sinit_case),
    FnTest("TbTextFormatter::SetHorizontalJustification", ppc, host, edge=sethj_edges, rnd=rnd(sethj_case)),
    FnTest("std::vector<CCharacterInfo,std::allocator<CCharacterInfo>>::~vector", ppc, host, edge=vec_edges("vchar"), rnd=rnd(vec_case, "vchar")),
    FnTest("CCharInfoList::~CCharInfoList", ppc, host, edge=vec_edges("clist"), rnd=rnd(vec_case, "clist")),
    FnTest("std::vector<CLineInfo,std::allocator<CLineInfo>>::~vector", ppc, host, edge=vec_edges("vline"), rnd=rnd(vec_case, "vline")),
    FnTest("std::vector<TbPoint,std::allocator<TbPoint>>::~vector", ppc, host, edge=vec_edges("vpoint"), rnd=rnd(vec_case, "vpoint")),
    FnTest("TbIUnicodeToMB::~TbIUnicodeToMB", ppc, host, edge=cdtor_edges("uni"), rnd=rnd(cdtor_case, "uni")),
    FnTest("TbITable::AddRef", ppc, host, edge=addref_edges, rnd=addref_case),
    FnTest("TbRefReleaseSmartPtr<TbIMBToUnicode>::~TbRefReleaseSmartPtr", ppc, host, edge=smart_edges("smb"), rnd=rnd(smart_case, "smb")),
    FnTest("TbRefReleaseSmartPtr<TbIUnicodeToMB>::~TbRefReleaseSmartPtr", ppc, host, edge=smart_edges("smu"), rnd=rnd(smart_case, "smu")),
    FnTest("std::_MSLstring::~_MSLstring", ppc, host, edge=counted_edges("mslstr"), rnd=rnd(counted_case, "mslstr")),
    FnTest("std::logic_error::what", ppc, host, edge=lwhat_edges, rnd=lwhat_case),
    FnTest("std::exception::what", ppc, host, edge=ewhat_edges, rnd=ewhat_case),
    FnTest("std::_RefCountedPtr<char,std::_Array<char>>::~_RefCountedPtr", ppc, host, edge=counted_edges("refc"), rnd=rnd(counted_case, "refc")),
    FnTest("SamsUtilities::USwapInplace16<unsigned_short>", ppc, host, edge=sw_edges("sw16"), rnd=rnd(sw_case, "sw16")),
    FnTest("SamsUtilities::USwapInplace32<unsigned_long>", ppc, host, edge=sw_edges("sw32"), rnd=rnd(sw_case, "sw32")),
    FnTest("TbIRLE4bitFont::~TbIRLE4bitFont", ppc, host, edge=cdtor_edges("rle"), rnd=rnd(cdtor_case, "rle")),
    FnTest("TbMBStringVTag::Format", ppc, host, edge=fmt_edges, rnd=fmt_case),
    FnTest("TbIMBToUnicode::~TbIMBToUnicode", ppc, host, edge=cdtor_edges("mb"), rnd=rnd(cdtor_case, "mb")),
]
