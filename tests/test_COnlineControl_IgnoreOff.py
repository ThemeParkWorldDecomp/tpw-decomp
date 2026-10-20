"""Unit v5897, COnlineControl's name commands and contact-list updates, against libtpw_unit_COnlineControl_IgnoreOff.dylib
(tests/unit_COnlineControl_IgnoreOff.cpp), main app ("SimTheme Park"),
src/COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_IgnoreOff.cpp:
  IgnoreOff 0x1021922c, IgnoreOn 0x10219034, BuddyOff 0x10218e3c, SendText 0x10217b5c, RemoveContact 0x1021ba28,
  RemoveBuddy 0x1021bdcc, UpdateUserList 0x1021bf30

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. Every callee outside the unit (the string constructors, the
set and map finds, Argument and the vector, DoCommand, the parser, the UI calls, the tree increment) is a fake on both sides that
records its arguments; text is logged as a hash of its UTF-16 units, pointers into the object as offsets from it, stack objects not
at all (the events and their order carry them). Queues answer the finds (found or not), GetDataForUser, and what the parser fake does.

The world of a case: a COnlineControl block (0xe90 bytes), the name as a wide string (a rep with the length at +0 and the data pointer at
+0xc on the Mac, a std::u16string on the host), and for UpdateUserList a chain of contact nodes (the first node in the word at 0xe30, the
end node at 0xe2c; the increment fake walks the chain). What is compared: the log of events (the whole call sequence).

No fused operations. Not tested: nothing skipped; SendText assumes the first word of the string's rep is its length (empty returns early).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_IgnoreOff"
MASK = 0xFFFFFFFF

FN = dict(IgnoreOff=0x1021922C, IgnoreOn=0x10219034, BuddyOff=0x10218E3C, SendText=0x10217B5C, RemoveContact=0x1021BA28,
          RemoveBuddy=0x1021BDCC, UpdateUserList=0x1021BF30)
SIZES = {0x1021922C: 388, 0x10219034: 388, 0x10218E3C: 388, 0x10217B5C: 492, 0x1021BA28: 88, 0x1021BDCC: 236, 0x1021BF30: 196}
OWN = set(SIZES)

ADDR = dict(ui=0x10396440)
HOST_LAYOUT = [0xE8C, 24]
PARSE_TEXTS = [[], list("hi there".encode("utf-16-be")[1::2]), list("Zed".encode("utf-16-be")[1::2])]

ST = {}       # per-case state of the stubs: obj, nodes, parser, vec, output


def H(units):
    h = 0x811C9DC5
    for u in units:
        h = ((h ^ u) * 0x01000193) & MASK
    return (h ^ ((len(units) * 0x9E3779B1) & MASK)) & MASK


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ctext(e, p):
    out = []
    while len(out) < 400:
        c = struct.unpack(">H", e.read(p, 2))[0]
        if c == 0:
            break
        out.append(c)
        p += 2
    return out


def txt(e, obj):
    return ctext(e, e.u32(e.u32(obj) + 0xC))


def mkrep(e, units):
    data = e.alloc(2 * len(units) + 2, b"".join(struct.pack(">H", u) for u in units) + b"\0\0")
    return e.alloc(0x10, struct.pack(">IIII", len(units), 0, 0, data))


def off(p):
    return p - ST["obj"]


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def st_string_ctor(e):
    e.write(e.r(3), b"\0\0\0\0")
    rec(e, "StringCtor")


def st_string_dtor(e):
    rec(e, "StringDtor", s16(e.r(4)))


def st_wstring_ctor(e):
    e.write(e.r(3), struct.pack(">I", mkrep(e, [])))
    rec(e, "WStringCtor")


def st_wstring_dtor(e):
    rec(e, "WStringDtor", s16(e.r(4)), H(txt(e, e.r(3))))


def copy_into(e, name):
    pos, count = e.r(5) & MASK, e.r(6) & MASK
    src = txt(e, e.r(4))
    rec(e, name, pos, count, H(src))
    part = src[pos:] if count == MASK else src[pos:pos + count]
    e.write(e.r(3), struct.pack(">I", mkrep(e, part)))


def st_wstring_copy(e):
    copy_into(e, "WStringCopy")


def st_nocase_copy(e):
    copy_into(e, "NocaseCopy")


def st_nocase_dtor(e):
    rec(e, "NocaseDtor", s16(e.r(4)), H(txt(e, e.r(3))))


def st_set_find(e):
    rec(e, "NocaseSetFind", off(e.r(4)), H(txt(e, e.r(5))))
    e.write(e.r(3), struct.pack(">I", (e.r(4) + (0x40 if e.cl.ret("find") else 4)) & MASK))


def st_buddy_find(e):
    rec(e, "BuddyFind", off(e.r(4)), H(txt(e, e.r(5))))
    e.write(e.r(3), struct.pack(">I", (e.r(4) + (0x40 if e.cl.ret("bfind") else 4)) & MASK))


def st_buddy_erase(e):
    rec(e, "BuddyErase", off(e.r(3)), H(txt(e, e.r(4))))


def st_argument_ctor(e):
    rec(e, "ArgumentCtor", H(txt(e, e.r(4))))
    e.write(e.r(3), struct.pack(">III", 0, e.u32(e.r(4)), 0))


def st_push_back(e):
    a = e.r(4)
    rec(e, "PushBack", e.u32(a), e.u32(a + 8), H(txt(e, a + 4)))


def st_tear_down(e):
    rec(e, "TearDown", e.u32(e.r(3) + 4))


def st_ui_add(e):
    rec(e, "UIAddContact", e.cl.lab(e.r(3)), H(ctext(e, e.r(4))), T.s32(e.r(5)))


def st_ui_update(e):
    rec(e, "UIUpdateContact", e.cl.lab(e.r(3)), H(ctext(e, e.r(4))), T.s32(e.r(5)))


def st_ui_remove_contact(e):
    rec(e, "UIRemoveContact", e.cl.lab(e.r(3)), H(ctext(e, e.r(4))))


def st_ui_remove_buddy(e):
    rec(e, "UIRemoveBuddy", e.cl.lab(e.r(3)), H(ctext(e, e.r(4))))


def st_do_command(e):
    rec(e, "DoCommand", off(e.r(3)), e.u32(e.r(4) + 4))


def st_get_data(e):
    rec(e, "GetDataForUser", off(e.r(3)), H(txt(e, e.r(4))))
    e.set_r(3, e.cl.ret("gdfu") & MASK)


def st_parser_ctor(e):
    ST["parser"], ST["vec"], ST["output"] = e.r(3), e.r(4), e.r(5)
    rec(e, "ParserCtor", H(txt(e, e.r(5))))


def st_do_parse(e):
    rec(e, "DoParse", H(txt(e, e.r(4))))
    if e.cl.ret("parsepush"):
        e.write(ST["vec"] + 4, struct.pack(">I", 1))
    e.write(ST["output"], struct.pack(">I", mkrep(e, PARSE_TEXTS[e.cl.ret("parsetext") % 3])))


def st_list_clear(e):
    rec(e, "ListClear", e.r(3) - ST["parser"])


def st_parser_dtor(e):
    rec(e, "ParserDtor", 1 if e.r(3) == ST["parser"] else 0, s16(e.r(4)), e.u32(e.r(3)))


def st_contact_erase(e):
    rec(e, "ContactErase", off(e.r(3)), H(txt(e, e.r(4))))
    e.set_r(3, 0)


def st_increment(e):
    rec(e, "Increment")
    cur = e.u32(e.r(3))
    nodes = ST["nodes"]
    nxt = nodes[nodes.index(cur) + 1] if cur in nodes and nodes.index(cur) + 1 < len(nodes) else ST["obj"] + 0xE2C
    e.write(e.r(3), struct.pack(">I", nxt))


for a, f in ((0x1007363C, st_string_ctor), (0x1000221C, st_string_dtor), (0x101E0B84, st_wstring_ctor), (0x101468F8, st_wstring_dtor),
             (0x101F7424, st_wstring_copy), (0x1022213C, st_nocase_copy), (0x10222020, st_nocase_dtor), (0x1021FD4C, st_set_find),
             (0x1021F8D8, st_buddy_find), (0x1021E8E8, st_buddy_erase), (0x102011A8, st_argument_ctor), (0x10201478, st_push_back),
             (0x10201310, st_tear_down), (0x101F09DC, st_ui_add), (0x101F0AA0, st_ui_update), (0x101F0A40, st_ui_remove_contact),
             (0x101F0B68, st_ui_remove_buddy), (0x10219BEC, st_do_command), (0x1021B854, st_get_data), (0x1020E400, st_parser_ctor),
             (0x1020E608, st_do_parse), (0x102109FC, st_list_clear), (0x102111D4, st_parser_dtor), (0x1021E664, st_contact_erase),
             (0x10152DB0, st_increment)):
    STUBS.internal(a, f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["ui"]: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def ptr(self, a):
        return a

    def mkstr(self, units):
        return self.e.alloc(4,struct.pack(">I", mkrep(self.e, units)))

    def mknode(self, units):
        return self.e.alloc(0x10, struct.pack(">IIII", 0, 0, 0, mkrep(self.e, units)))

    def set_nodes(self, nodes):
        ST["nodes"] = list(nodes)

    def call(self, op, obj, name):
        ST["obj"] = obj
        args = [obj] if op == "UpdateUserList" else [obj, name]
        T.call_ppc(self.e, FN[op], args)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {2: lib.tpw_cocig_addr(b"ui")}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_cocig_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr(self, a):
        return self.lib.tpw_cocig_offset(a)

    def buf(self, units):
        return (C.c_uint16 * (len(units) + 1))(*units)

    def mkstr(self, units):
        return self.lib.tpw_cocig_u16(self.buf(units), len(units))

    def mknode(self, units):
        node = self.alloc(0x30)
        self.lib.tpw_cocig_node(node, self.buf(units), len(units))
        return node

    def set_nodes(self, nodes):
        self.lib.tpw_cocig_clear_nodes()
        for n in nodes:
            self.lib.tpw_cocig_push_node(n)

    def call(self, op, obj, name):
        fn = getattr(self.lib, "tpw_cocig_" + op)
        if op == "UpdateUserList":
            fn(obj)
        else:
            fn(obj, name)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cocig_addr.restype, lib.tpw_cocig_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cocig_alloc.restype, lib.tpw_cocig_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cocig_u16.restype, lib.tpw_cocig_u16.argtypes = C.c_uint64, [C.c_void_p, C.c_uint32]
    lib.tpw_cocig_node.restype, lib.tpw_cocig_node.argtypes = None, [C.c_uint64, C.c_void_p, C.c_uint32]
    lib.tpw_cocig_offset.restype, lib.tpw_cocig_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_cocig_clear_nodes.restype, lib.tpw_cocig_clear_nodes.argtypes = None, []
    lib.tpw_cocig_push_node.restype, lib.tpw_cocig_push_node.argtypes = None, [C.c_uint64]
    lib.tpw_cocig_layout.argtypes = [C.POINTER(C.c_int64)]
    for op in FN:
        fn = getattr(lib, "tpw_cocig_" + op)
        fn.restype = None
        fn.argtypes = [C.c_uint64] if op == "UpdateUserList" else [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cocig_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def run(s, case):
    try:
        obj = s.alloc(0xE90)
        name = s.mkstr(case["name"])
        nodes = [s.mknode(t) for t in case["nodes"]]
        s.set_nodes(nodes)
        s.words(obj + 0xE30, [s.ptr(nodes[0] if nodes else obj + 0xE2C)])   # a link word: the Mac address, the host's arena offset
        s.call(case["op"], obj, name)
        return {"log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NAMES = [[], [0x61], list(b"Bob"), list(b"Zed Q"), [0x63, 0x61, 0x66, 0xE9], [0x4E2D, 0x6587], [0x78] * 40, [0xFFFF, 0x100, 0x20]]


def runits(r):
    n = r.choice([0, 1, 2, 3, 5, 11, 12, 13, 20])
    return [r.choice([r.randrange(0x20, 0x7F), 0xE9, 0x4E2D, 0x100, 0xFF, 0xFFFF]) for _ in range(n)]


def base(r, op, name=None, nodes=None):
    nodes = [] if nodes is None else nodes
    return {"op": op, "name": runits(r) if name is None else name, "nodes": nodes,
            "queues": {"find": [r.choice([0, 1])], "bfind": [r.choice([0, 1])], "parsepush": [r.choice([0, 1])],
                       "parsetext": [r.randrange(3)], "gdfu": [r.randrange(16) for _ in range(max(1, len(nodes) + 1))]}}


def make_tests(op):
    def edges():
        r = random.Random(sum(map(ord, op)))
        for name in NAMES:
            for flag in (0, 1):
                c = base(r, op, name, [name, [0x61]] if op == "UpdateUserList" and flag else [])
                c["queues"]["find"], c["queues"]["bfind"] = [flag], [flag]
                c["queues"]["parsepush"], c["queues"]["parsetext"] = [flag], [len(name) % 3]
                yield c
        if op == "UpdateUserList":
            for k in (0, 1, 2, 3, 6):
                yield base(r, op, [], [runits(r) for _ in range(k)])

    def rnd(r):
        return base(r, op, None, [runits(r) for _ in range(r.randrange(0, 6))] if op == "UpdateUserList" else [])

    return FnTest("COnlineControl::" + op, ppc, host, edge=edges, rnd=rnd)


TESTS = [make_tests(op) for op in FN]
