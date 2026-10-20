"""Unit v51039, against libtpw_unit_std_list_std_basic_string_std_allocator_std_basi_4.dylib
(tests/unit_std_list_std_basic_string_std_allocator_std_basi_4.cpp), main app ("SimTheme Park"),
src/Serialise_basic_std_list_std_basic_string_std_allocator_std_basi_4.cpp:
  std::list<std::wstring>::clear 0x1016d4e8, SwearFilter::~SwearFilter 0x101697bc, std::map<...>::~map 0x101695a8,
  InterfaceFontWrapper::GetCharWidth 0x10168e04, CFSOnlineSystemParkList::StoreReceivedList 0x10178bb0, UIMain_IndentClient 0x10180940,
  UIMain_GetStaffStateFrame 0x10180a1c, UIMain_SetStaffStateText 0x10180aa0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit is a fake on both
sides that records its arguments: the wide string destructor and operator delete (the nodes are told apart by the tag in their string's
first word), the tree's destroy, operator new, the park constructor, the font's two virtuals (through __ptr_glue, told apart by the
TVector in r12), GetMBValue, the rect functions, GetConstString, ConvertString and the window's SetText.

The world of a case: lists in the Mac layout (count, sentinel links, nodes) in the arena, a tree head, a font wrapper with its font, gText's
resource pointers 0, 1 and 10, a received list with its 0x66-byte infos, a window with its table at +0x118, the conversion buffer.
What is compared: the answer, the event log, the list heads after the call (links as sentinel / node / null), the conversion buffer.

No fused operations. Not tested: SwearFilter::~SwearFilter with this = 0xfffffff4 (its second test of this + 0xc; unmapped memory).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_list_std_basic_string_std_allocator_std_basi_4"
MASK = 0xFFFFFFFF

CLEAR, SWEAR, MAPDTOR, GETCW, STORE, INDENT, FRAME, SETTEXT = (0x1016D4E8, 0x101697BC, 0x101695A8, 0x10168E04, 0x10178BB0, 0x10180940,
                                                              0x10180A1C, 0x10180AA0)
SIZES = {CLEAR: 140, SWEAR: 120, MAPDTOR: 116, GETCW: 152, STORE: 116, INDENT: 156, FRAME: 84, SETTEXT: 160}
OWN = set(SIZES)

WSTR_DTOR, OP_DELETE, OP_NEW, TREE_DESTROY, PARK_CTOR = 0x101468F8, 0x10238218, 0x102384B8, 0x1016D784, 0x1017C0D8
RECT_CTOR, GETPOS, ADJUST, SETCLIENT, RECT_DTOR = 0x101C7448, 0x101DB884, 0x101C7A78, 0x101DB3FC, 0x101C74E0
GETCONST, CONVERT, GETMB, PTRGLUE = 0x10238B78, 0x10238B90, 0x1023B9C8, 0x1023BAA8
TV_INDEX, TV_SIZE, TV_SETTEXT = 0x70001000, 0x70002000, 0x70003000

ADDR = dict(gText=0x1037FF18, buf=0x10380C48)
# the Mac's offsets; the host's come from tpw_swf_layout
MAC = dict(root=4, infos=4, font=0x14, res=0x54C, psz=4)
HOST = {}
NAME_CLEAR = ("std::list<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>,std::allocator<std::basic_string<"
              "wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>>>::clear")
NAME_MAP = ("std::map<TbDynamicStringTemplate<char>,CFSGlobalParkData*,std::less<TbDynamicStringTemplate<char>>,std::allocator<std::pair<"
            "const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::~map")


def s16(v):
    return T.s16(v)


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_wstr_dtor(e):
    e.cl.rec("WStringDtor", (e.u32(e.r(3)), s16(e.r(4))))


def stub_delete(e):
    L = e.cl
    p = e.r(3)
    l = L.lab(p)
    L.rec("delete", (l, e.u32(p + 8) if l == -1 else 0))


def stub_tree_destroy(e):
    L = e.cl
    L.rec("TreeDestroy", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_new(e):
    size = e.r(3)
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size))


def stub_park_ctor(e):
    L = e.cl
    b = e.read(e.r(4), 4)
    L.rec("ctor", (b[0], b[1], b[2], b[3], L.lab(e.r(5))))


def stub_rect_ctor(e):
    e.cl.rec("RectCtor", (0,))


def stub_getpos(e):
    e.cl.rec("GetPosition", (e.cl.lab(e.r(3)), s16(e.r(5))))
    e.set_r(3, 0)


def stub_adjust(e):
    e.cl.rec("Adjust", (s16(e.r(4)), s16(e.r(5)), s16(e.r(6)), s16(e.r(7))))


def stub_setclient(e):
    e.cl.rec("SetClientRect", (e.cl.lab(e.r(3)),))


def stub_rect_dtor(e):
    e.cl.rec("RectDtor", (s16(e.r(4)),))


def stub_getmb(e):
    L = e.cl
    L.rec("GetMBValue", (L.lab(e.r(3)), e.r(4) & 0xFFFF))
    e.set_r(3, L.ret("GetMBValue") & 0xFFFF)


def stub_getconst(e):
    L = e.cl
    L.rec("GetConstString", (L.lab(e.r(3)), e.r(4) & MASK))
    e.set_r(3, L.cstr if L.ret("GetConstString") else 0)


def stub_convert(e):
    L = e.cl
    L.rec("ConvertString", (L.lab(e.r(4)), L.lab(e.r(5)), L.lab(e.r(6)), e.r(7) & 0xFFFF))
    v = L.ret("conv") & 0xFFFF
    e.write(e.r(5), struct.pack(">HHH", v, (v + 1) & 0xFFFF, 0))


def stub_ptr_glue(e):
    L = e.cl
    tv = e.r(12)
    if tv == TV_INDEX:
        p = e.u32(e.r(4))
        L.rec("GetCharIndex", (L.lab(e.r(3)), struct.unpack(">H", e.read(p, 2))[0]))
        e.set_r(3, L.ret("GetCharIndex") & MASK)
    elif tv == TV_SIZE:
        L.rec("GetCharSize", (L.lab(e.r(4)), e.r(5) & 0xFFFF))
        e.write(e.r(3), struct.pack(">II", L.ret("width") & MASK, 0))
    elif tv == TV_SETTEXT:
        L.rec("SetText", (L.lab(e.r(3)), L.lab(e.r(4))))
    else:
        raise AssertionError("unknown TVector %#x" % tv)


for _a, _f in ((WSTR_DTOR, stub_wstr_dtor), (OP_DELETE, stub_delete), (OP_NEW, stub_new), (TREE_DESTROY, stub_tree_destroy),
               (PARK_CTOR, stub_park_ctor), (RECT_CTOR, stub_rect_ctor), (GETPOS, stub_getpos), (ADJUST, stub_adjust),
               (SETCLIENT, stub_setclient), (RECT_DTOR, stub_rect_dtor), (GETMB, stub_getmb), (GETCONST, stub_getconst),
               (CONVERT, stub_convert), (PTRGLUE, stub_ptr_glue)):
    STUBS.internal(_a, _f)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_u16s(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def wu16s(self, a, vs):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(vs), *[v & 0xFFFF for v in vs]))

    def wptr(self, a, value):
        self.write(a, struct.pack(self.fmt[0] + ("I" if self.off["psz"] == 4 else "Q"), value))


class Ppc(Side):
    fmt = ">I"
    off = MAC

    def __init__(self, e, case):
        self.e = e
        self.blk = [e.alloc(0x140) for _ in range(5)]
        fixed = {b: i + 1 for i, b in enumerate(self.blk)}
        fixed[ADDR["buf"]] = 6
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.L.cstr = self.blk[4]
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def lnk(self, addr):
        return addr

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, addr, args):
        return T.call_ppc(self.e, addr, [x & MASK for x in args])[0] & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = HOST
        CL.host_begin(lib, {}, case["queues"])   # (the arena hands out 32-bit addresses once a case has begun)
        self.blk = [lib.tpw_swf_alloc(0x40) for _ in range(5)]   # small blocks come from the 32-bit arena (the Mac's window needs 0x140)
        fixed = {i + 1: b for i, b in enumerate(self.blk)}
        fixed[6] = self.g("buf")
        CL.host_begin(lib, fixed, case["queues"])
        lib.tpw_swf_setconst(self.blk[4])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_swf_addr(name.encode())

    def lnk(self, addr):
        return (addr - self.lib.tpw_swf_base()) & MASK

    def alloc(self, n):
        return self.lib.tpw_swf_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_swf_"
    u64, i16, u32, i32 = C.c_uint64, C.c_int16, C.c_uint32, C.c_int32
    for name, res, args in (("addr", u64, [C.c_char_p]), ("alloc", u64, [u32]), ("setconst", None, [u64]), ("clear", None, [u64]),
                            ("swear", u64, [u64, i16]), ("mapdtor", u64, [u64, i16]), ("charwidth", i32, [u64, u32]),
                            ("store", None, [u64, u64]), ("indent", None, [u64, i16, i16]), ("frame", i32, [i32]),
                            ("settext", None, [u64, i32])):
        f = getattr(lib, p + name)
        f.restype, f.argtypes = res, args
    lib.tpw_swf_base.restype, lib.tpw_swf_base.argtypes = u64, []
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert n == 5, list(out)[:n]
    HOST.update(root=out[0], infos=out[1], font=out[2], res=out[3], psz=8)
    assert out[4] == 0x66
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def kind(w, sent):
    return 0 if w == 0 else ("S" if w == sent else "N")


def make_list(s, base, tags, count):
    """A Mac std::list<std::wstring> at `base`: count, sentinel (base + 4), nodes of previous, next, the string's first word (tag).
    The links are words: addresses on the Mac, offsets from the arena base on the host (s.lnk)."""
    sent = s.lnk(base + 4)
    nodes = [s.alloc(0x20) for _ in tags]
    ln = [s.lnk(n) for n in nodes]
    first = ln[0] if nodes else sent
    last = ln[-1] if nodes else sent
    s.words(base, [count, last, first])
    for i, nd in enumerate(nodes):
        s.words(nd, [ln[i - 1] if i else sent, ln[i + 1] if i + 1 < len(nodes) else sent, tags[i]])


def obs_list(s, base):
    w = s.rd_words(base, 3)
    sent = s.lnk(base + 4)
    return [w[0], kind(w[1], sent), kind(w[2], sent)]


def set_res(s, index, value):
    s.wptr(s.g("gText") + s.off["res"] + s.off["psz"] * index, value)


def build(s, case):
    op = case["op"]
    a, b, c, d, e5 = s.blk
    if op == "clear":
        make_list(s, a, case["tags"], case["count"])
    elif op == "swear":
        make_list(s, a, case["tags"], case["count"])
        make_list(s, a + 0xC, case["tags2"], case["count2"])
    elif op == "mapdtor":
        s.words(a, [case["treecount"]])
        s.wptr(a + s.off["root"], b if case["root"] else 0)
    elif op == "charwidth":
        s.wptr(a + s.off["font"], b if case["font"] else 0)
        s.words(b, [d])
        s.words(d + 0x1C, [TV_INDEX])
        s.words(d + 0x24, [TV_SIZE])
        set_res(s, 1, c)
    elif op == "store":
        n = case["n"]
        infos = s.alloc(0x66 * max(n, 1) + 8) if case["infos"] else 0
        for i in range(max(n, 0) if infos else 0):
            s.write(infos + i * 0x66, bytes(case["tagbytes"][i]))
        s.words(b, [n & MASK])
        s.wptr(b + s.off["infos"], infos)
    elif op == "settext":
        s.wptr(a + 0x118, d) if s.off["psz"] == 4 else None
        s.words(d + 0x2C, [TV_SETTEXT])
        set_res(s, 10, c)
        set_res(s, 0, b)
        set_res(s, 1, d if case["res1"] else 0)
        s.write(e5, bytes(case["str"]))
        s.wu16s(s.g("buf"), case["bufinit"])


def z(_):
    return 0


def execute(s, case):
    op = case["op"]
    a, b = s.blk[0], s.blk[1]
    if op == "clear":
        return z(s.call(CLEAR, [a]) if isinstance(s, Ppc) else s.lib.tpw_swf_clear(a))
    if op == "swear":
        this = 0 if case["null"] else a
        r = s.call(SWEAR, [this, case["flag"]]) if isinstance(s, Ppc) else s.lib.tpw_swf_swear(this, case["flag"])
        return r == this
    if op == "mapdtor":
        this = 0 if case["null"] else a
        r = s.call(MAPDTOR, [this, case["flag"]]) if isinstance(s, Ppc) else s.lib.tpw_swf_mapdtor(this, case["flag"])
        return r == this
    if op == "charwidth":
        if isinstance(s, Ppc):
            return s32(s.call(GETCW, [a, case["ch"]]))
        return s.lib.tpw_swf_charwidth(a, case["ch"])
    if op == "store":
        if isinstance(s, Ppc):
            return z(s.call(STORE, [a, b]))
        return z(s.lib.tpw_swf_store(a, b))
    if op == "indent":
        if isinstance(s, Ppc):
            return z(s.call(INDENT, [a, case["a"], case["b"]]))
        return z(s.lib.tpw_swf_indent(a, case["a"], case["b"]))
    if op == "frame":
        if isinstance(s, Ppc):
            return s32(s.call(FRAME, [case["state"]]))
        return s.lib.tpw_swf_frame(case["state"])
    if op == "settext":
        if isinstance(s, Ppc):
            return z(s.call(SETTEXT, [a, case["state"]]))
        return z(s.lib.tpw_swf_settext(a, case["state"]))
    raise AssertionError(op)


def observe(s, case, answer):
    op = case["op"]
    a = s.blk[0]
    out = {"answer": answer, "log": s.log()}
    if op == "clear":
        out["list"] = obs_list(s, a)
    elif op == "swear":
        out["list"] = obs_list(s, a)
        out["list2"] = obs_list(s, a + 0xC)
    elif op == "settext":
        out["buf"] = s.rd_u16s(s.g("buf"), 6)
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


def base(r, op, **kw):
    q = {"GetMBValue": [r.choice([0, 0x41, 0xFFFF, r.randrange(0x10000), 0x12345678])],
         "GetCharIndex": [r.choice([0, 1, 0xFFFF, 0x12345, r.randrange(1 << 32)])],
         "width": [r.choice([0, 5, 0x7FFF, 0x8000, 0xFFFF, 0x10005, 0xFFFFFFFF, r.randrange(1 << 32)])],
         "GetConstString": [r.choice([0, 1])], "conv": [r.choice([0, 0x41, 0xFFFF, r.randrange(0x10000)])]}
    c = {"op": op, "queues": q}
    c.update(kw)
    return c


def tags(r, n):
    return [r.choice([1, 0xFFFFFFFF, r.randrange(1, 1 << 32)]) for _ in range(n)]


def list_kw(r, n=None, count=None, suffix=""):
    n = r.choice([0, 1, 2, 3, 5]) if n is None else n
    t = tags(r, n)
    k = r.random()
    cnt = len(t) if count is None else count
    if count is None and k < 0.1:
        cnt = 0
    elif count is None and k < 0.2 and not t:
        cnt = r.randrange(1, 5)
    return {"tags" + suffix: t, "count" + suffix: cnt}


def clear_case(r, n=None, count=None):
    return base(r, "clear", **list_kw(r, n, count))


def clear_edges():
    r = random.Random(11)
    for n in (0, 1, 2, 3, 6):
        yield clear_case(r, n)
    yield clear_case(r, 3, 0)
    yield clear_case(r, 0, 2)
    yield clear_case(r, 1, 0x7FFFFFFF)


def clear_random(r):
    return clear_case(r)


FLAGS = [-1, 0, 1, 2, 0x7FFF, -0x8000, 100]


def swear_case(r, null=None, flag=None, n1=None, n2=None):
    c = base(r, "swear", null=(r.random() < 0.1) if null is None else null, flag=r.choice(FLAGS) if flag is None else flag)
    c.update(list_kw(r, n1))
    c.update(list_kw(r, n2, suffix="2"))
    return c


def swear_edges():
    r = random.Random(12)
    yield swear_case(r, True, 1)
    for flag in FLAGS:
        yield swear_case(r, False, flag, 2, 3)
    yield swear_case(r, False, 1, 0, 0)
    yield swear_case(r, False, 0, 1, 0)
    yield swear_case(r, False, 2, 0, 1)


def swear_random(r):
    return swear_case(r)


def map_case(r, null=None, flag=None, root=None):
    return base(r, "mapdtor", null=(r.random() < 0.1) if null is None else null, flag=r.choice(FLAGS) if flag is None else flag,
                root=r.random() < 0.7 if root is None else root, treecount=r.choice([0, 1, 5, r.randrange(1 << 32)]))


def map_edges():
    r = random.Random(13)
    yield map_case(r, True, 1, True)
    for flag in FLAGS:
        for root in (True, False):
            yield map_case(r, False, flag, root)


def map_random(r):
    return map_case(r)


def cw_case(r, font=None, ch=None):
    return base(r, "charwidth", font=(r.random() < 0.85) if font is None else font,
                ch=r.choice([0, 0x41, 0x7FFF, 0x8000, 0xFFFF, r.randrange(0x10000)]) if ch is None else ch)


def cw_edges():
    r = random.Random(14)
    yield cw_case(r, False, 0x41)
    for ch in (0, 0x41, 0xFFFF):
        yield cw_case(r, True, ch)
    for w in (0, 5, 0x7FFF, 0x8000, 0xFFFF, 0x12345, 0xFFFFFFFF):
        c = cw_case(r, True)
        c["queues"]["width"] = [w]
        yield c


def cw_random(r):
    return cw_case(r)


def store_case(r, n=None):
    n = r.choice([-2, -1, 0, 1, 2, 3, 5]) if n is None else n
    return base(r, "store", n=n, infos=(r.random() < 0.9) or n > 0, tagbytes=[[r.randrange(256) for _ in range(4)] for _ in range(max(n, 0))])


def store_edges():
    r = random.Random(15)
    for n in (-1, 0, 1, 2, 3, 6):
        yield store_case(r, n)
    c = store_case(r, 0)
    c["infos"] = False
    yield c


def store_random(r):
    return store_case(r)


SH = [-32768, -1, 0, 1, 4, 8, 10, 32767]


def indent_case(r, a=None, b=None):
    return base(r, "indent", a=r.choice(SH + [r.randrange(-32768, 32768)]) if a is None else a,
                b=r.choice(SH + [r.randrange(-32768, 32768)]) if b is None else b)


def indent_edges():
    r = random.Random(16)
    for a, b in ((0, 10), (4, 0), (0, 8), (-32768, -32768), (32767, -1), (-1, 32767)):
        yield indent_case(r, a, b)


def indent_random(r):
    return indent_case(r)


def frame_case(r, state=None):
    return base(r, "frame", state=r.choice([-5, -1, 0, 1, 2, 3, 4, 5, 6, 100, r.randrange(-1 << 31, 1 << 31)]) if state is None else state)


def frame_edges():
    r = random.Random(17)
    for state in (-0x80000000, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 0x7FFFFFFF):
        yield frame_case(r, state)


def frame_random(r):
    return frame_case(r)


def text_case(r, state=None, res1=None):
    return base(r, "settext", state=r.choice([0, 1, 0x1C9, 0x7FFFFFFF, -1, r.randrange(-1 << 31, 1 << 31)]) if state is None else state,
                res1=(r.random() < 0.7) if res1 is None else res1, str=[r.randrange(256) for _ in range(4)],
                bufinit=[r.randrange(1, 0x10000) for _ in range(6)])


def text_edges():
    r = random.Random(18)
    for const in (0, 1):
        for res1 in (True, False):
            c = text_case(r, 5, res1)
            c["queues"]["GetConstString"] = [const]
            yield c


def text_random(r):
    return text_case(r)


TESTS = [
    FnTest(NAME_CLEAR, ppc, host, edge=clear_edges, rnd=clear_random),
    FnTest("SwearFilter::~SwearFilter", ppc, host, edge=swear_edges, rnd=swear_random),
    FnTest(NAME_MAP, ppc, host, edge=map_edges, rnd=map_random),
    FnTest("InterfaceFontWrapper::GetCharWidth", ppc, host, edge=cw_edges, rnd=cw_random),
    FnTest("CFSOnlineSystemParkList::StoreReceivedList", ppc, host, edge=store_edges, rnd=store_random),
    FnTest("UIMain_IndentClient", ppc, host, edge=indent_edges, rnd=indent_random),
    FnTest("UIMain_GetStaffStateFrame", ppc, host, edge=frame_edges, rnd=frame_random),
    FnTest("UIMain_SetStaffStateText", ppc, host, edge=text_edges, rnd=text_random),
]
