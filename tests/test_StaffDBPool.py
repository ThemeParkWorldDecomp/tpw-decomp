"""Unit w226: 21 original StaffDB.cpp functions versus StaffDB_pool.cpp.

One shared fixture writes the database fields in each machine's byte order, guards the
record area, and drives real serialisation templates over recording file fakes. Every
case compares every database field, padding and guard bytes, both scratch buffers,
output text, mouse state and words, result and complete dependency log. Random draws,
text formatting, staff queries, world walks, allocation and UI callbacks are faked on
both sides. TimeSig inlines and WorldSavvy::GetWorld run real. No floats are involved.
Invalid Generate types (unset r26), timeout divisors of zero, NULL mouse allocation,
and a minimum pool which cannot grow are outside the defined test domain.
"""
import ctypes as C
import random
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_StaffDBPool"
MASK = 0xFFFFFFFF
NAMES = ("dtor", "info_save", "name", "genname", "generate", "salary", "valid", "db_save", "get", "hire", "cap", "count",
         "minimum", "genstaff", "numtype", "num", "fate", "inuse", "free", "opening", "sinit")
ADDRS = (0x101283D4, 0x10128440, 0x10128768, 0x10128860, 0x10128B0C, 0x10128E1C, 0x10128E7C, 0x10128FF4,
         0x101292F8, 0x10129348, 0x10129758, 0x10129830, 0x10129A14, 0x10129BB4, 0x10129C6C, 0x10129CEC,
         0x10129D50, 0x10129E20, 0x1012A48C, 0x1012A4F8, 0x1012A7B8)
A = dict(zip(NAMES, ADDRS))
WORLD_SIZE = 0x1DA748
# Global entries correspond to tpw_sdp_addr.
GLOBALS = (None, 0x1034C4E0, 0x1034C5E0, None, 0x1037FEFC, None, 0x10396440, None, None,
           0x102DC1E0, 0x102DC1E4, 0x1034C6E0, 0x1034C6E4)
FILE = 0x6E100000
S = CL.Stubs()
S.real.update(ADDRS)
S.real.update({0x1000F4B0, 0x1000EF18, 0x1000F70C, 0x1000F144, 0x1000F5DC, 0x1000F02C,
               0x100F1A04, 0x100F17C8, 0x1015EF5C, 0x1015F02C, 0x1014205C})


def wide(e, p):
    chars = []
    while True:
        ch = int.from_bytes(e.read(p + len(chars) * 2, 2), "big")
        if not ch:
            return "".join(map(chr, chars))
        chars.append(ch)


def format_name(e):
    table, size, ident = e.cl.lab(e.r(3)), e.r(5), e.r(6)
    e.cl.rec("format", (table, size, ident))
    text = ("T%d:%d" % (table - 20, ident))[:max(0, size - 1)]
    if size:
        e.write(e.r(4), (text + "\0").encode("utf-16-be"))
    e.set_r(3, 0)


def compare(e):
    a, b = wide(e, e.r(3)), wide(e, e.r(4))
    e.cl.rec("compare", (), (a, b))
    e.set_r(3, 0 if a == b else 1 if a > b else MASK)


def walk(name):
    def stub(e):
        values = [e.cl.lab(e.r(4))]
        if name == "next":
            values.append(e.cl.lab(e.r(5)))
        e.cl.rec(name, values)
        e.write(e.r(3), (e.cl.ret(name) & 0xFFFF).to_bytes(2, "big"))
    return stub


def staff_name(e):
    label = e.cl.lab(e.r(3))
    e.cl.rec("staff_name", (label,))
    e.set_r(3, e.sdp_names[label - 40])


def op_new(e):
    size = e.r(3)
    e.cl.rec("new", (1 if size == 1 else 2 if size == 8 else 3,))
    e.set_r(3, e.alloc(size, b"\0" * size) if e.cl.ret("new", 1) else 0)


def op_delete(e):
    p = e.r(3)
    if p and e.cl.lab(p) > 0:
        e.cl.rec("delete", (e.cl.lab(p),))
        e.cl.forget(p)


def file_io(writing):
    def stub(e):
        length = e.r(6)
        data = e.read(e.r(5), length) if writing else e.sdp_file[e.sdp_cursor:e.sdp_cursor + length]
        e.cl.rec("write" if writing else "read", (e.cl.lab(e.r(4)), length), (data.hex(),))
        forced, result = e.cl.ret("xfer", -1), e.cl.ret("io")
        xfer = length if forced < 0 else forced
        if not writing:
            if result == 0 and xfer == length:
                e.write(e.r(5), data)
            e.sdp_cursor += length
        e.write(e.r(7), T.u32(xfer).to_bytes(4, "big"))
        e.write(e.r(3), T.u32(result).to_bytes(4, "big"))
    return stub


def stafftype(save):
    def stub(e):
        value = e.u32(e.r(4))
        e.cl.rec("stafftype_save" if save else "stafftype_load",
                 (e.cl.lab(e.r(3)), e.r(4) - e.sdp_world, T.s32(e.r(6)), value), (e.cstr(e.r(5)),))
        if not save:
            e.write(e.r(4), T.u32(e.cl.ret("stafftype")).to_bytes(4, "big"))
        e.set_r(3, T.u32(e.cl.ret("stafftype_result")))
    return stub


S.internal(0x1013B1E4, CL.recorder("GetRand", "L", ret=lambda e, l: l.ret("rand")))
S.internal(0x1007C900, CL.recorder("sprites", "i", ret="i"))
S.internal(0x101EFE04, CL.recorder("StaffDBChange", "Li"))
S.internal(0x1013ADB0, walk("head"))
S.internal(0x1013AE08, walk("next"))
S.internal(0x10126F98, staff_name)
S.internal(0x101270B0, CL.recorder("salary", "ib", ret="i"))
S.internal(0x1006FCFC, CL.recorder("TRACE", "s"))
S.internal(0x1006FCBC, CL.recorder("CTRACE", "us"))
S.internal(0x1000DDE8, CL.recorder("assert", "is"))
S.internal(0x1006FC90, CL.recorder("debug", "su"))
S.internal(0x1015B3DC, CL.recorder("serialise", "Lisi"))
S.internal(0x1007EA78, CL.recorder("default", "N"))
S.internal(0x1007EB94, CL.recorder("place", "Nibb i".replace(" ", "")))
S.internal(0x1007E2E0, CL.recorder("assign", "LL"))
S.internal(0x100F9518, stafftype(False))
S.internal(0x100F9788, stafftype(True))
S.imp("FormatString__8TbITableCFPwUiUle", format_name)
S.imp("wcscmp", compare)
S.imp("__nw__FUl", op_new)
S.imp("__dl__FPv", op_delete)
S.imp("LbFile_Read__FPvPvUlPUl", file_io(False))
S.imp("LbFile_Write__FPvPCvUlPUl", file_io(True))
S.imp("LbFile_GetPosition__FPv", CL.recorder("position", "L", ret="i"))
S.imp("memset", lambda e: e.write(e.r(3), bytes([e.r(4) & 255]) * e.r(5)) if e.r(5) else None)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "sdp_bound", False):
        return
    signatures = {
        "addr": (C.c_uint64, [C.c_int32]), "table": (C.c_uint64, [C.c_int32]), "thing": (C.c_uint64, [C.c_int32]),
        "thing_name": (C.c_uint64, [C.c_int32]), "row": (None, [C.c_int32, C.c_uint64]),
        "balance": (None, [C.c_int32, C.c_int32]), "reset": (None, [C.c_int32]), "mouse": (C.c_int32, []),
        "layout": (C.c_int32, []), "dtor": (None, [C.c_int32, C.c_int32]),
        "info_save": (C.c_int32, [C.c_int32, C.c_int32, C.c_int32, C.c_uint32]), "name": (None, [C.c_int32, C.c_int32]),
        "genname": (None, [C.c_int32]), "generate": (None, [C.c_int32, C.c_int32]), "salary": (C.c_uint32, [C.c_int32]),
        "valid": (None, [C.c_int32, C.c_int32, C.c_int32]), "db_save": (C.c_int32, [C.c_int32, C.c_int32, C.c_uint32]),
        "get": (C.c_int32, [C.c_uint32]), "hire": (C.c_int32, [C.c_uint32]), "cap": (None, []), "count": (None, []),
        "minimum": (None, []), "genstaff": (C.c_int32, [C.c_int32]), "numtype": (C.c_int32, [C.c_int32]),
        "num": (C.c_int32, []), "fate": (None, [C.c_int32]), "inuse": (C.c_int32, []), "free": (C.c_uint32, []),
        "opening": (None, []), "sinit": (None, [C.POINTER(C.c_int32)])}
    for name, (restype, args) in signatures.items():
        fn = getattr(lib, "tpw_sdp_" + name)
        fn.restype, fn.argtypes = restype, args
    assert lib.tpw_sdp_layout() == 1
    lib.sdp_bound = True


class Side:
    def put(self, p, width, value):
        self.write(p, (value & ((1 << (width * 8)) - 1)).to_bytes(width, self.byte))

    def get(self, p, width):
        return int.from_bytes(self.read(p, width), self.byte)

    def text(self, p, value):
        self.write(p, (value + "\0").encode("utf-16-be" if self.byte == "big" else "utf-16-le"))


class Ppc(Side):
    byte = "big"

    def __init__(self, e, c):
        self.e = e
        self.world = e.alloc(WORLD_SIZE)
        e.sdp_world = self.world
        self.addrs = list(GLOBALS)
        self.addrs[0] = self.world
        self.addrs[3] = e.alloc(256)
        self.addrs[5] = FILE
        self.addrs[7] = e.alloc(8)
        self.addrs[8] = e.alloc(4096)
        self.tables = [e.alloc(8) for _ in range(5)]
        self.things = [e.alloc(128) for _ in c["things"]]
        self.names = [e.alloc(256) for _ in c["things"]]
        fixed = {self.world: 1}
        fixed.update({self.addrs[i]: i + 1 for i in range(1, 13)})
        fixed.update({p: 20 + i for i, p in enumerate(self.tables)})
        fixed.update({p: 40 + i for i, p in enumerate(self.things)})
        if c["op"] == "dtor" and c["slot"] != 0:
            fixed[self.world + c["slot"] * 20] = 80
        self.logobj = e.cl = CL.PpcLog(fixed, c["queues"])
        e.sdp_names = self.names
        e.sdp_file, e.sdp_cursor = c["file"], 0
        self.saved = S.activate(e)
        self.put(0x1037EC0C, 4, self.world)
        self.put(0x102DC1F4, 4, self.addrs[7] if c["mouse"] else 0)
        for i, table in enumerate(self.tables):
            self.put(0x1037FF18 + 0x578 + i * 4, 4, table)
        self.put(0x1034CBFC, 4, self.things[0] if c["badzero"] and self.things else 0)
        for i, p in enumerate(self.things):
            self.put(0x1034CBFC + (i + 1) * 20, 4, p)

    def read(self, p, n):
        return self.e.read(p, n)

    def write(self, p, data):
        self.e.write(p, data)

    def balance(self, slot, value):
        self.put(0x102B4568 + 8 + slot * 4, 4, value)

    def execute(self, c):
        op, slot = c["op"], c["slot"]
        obj = self.world + 20 * slot
        args = {
            "dtor": [obj, c["flag"]], "info_save": [obj, FILE, c["save"], c["version"], c["unused"]],
            "name": [obj, self.addrs[3], c["size"]], "genname": [obj], "generate": [obj, c["type"], slot],
            "salary": [obj], "valid": [obj, c["valid"], c["id"]], "db_save": [self.world, FILE, c["save"], c["version"], c["unused"]],
            "get": [self.world, c["id"]], "hire": [self.world, c["id"]], "cap": [self.world], "count": [self.world],
            "minimum": [self.world], "genstaff": [self.world, c["type"]], "numtype": [self.world, c["type"]],
            "num": [self.world], "fate": [self.world, c["fate"]], "inuse": [self.world, self.addrs[3]],
            "free": [self.world], "opening": [self.world], "sinit": []}[op]
        if op == "sinit":
            self.put(self.addrs[11], 4, 0)
            self.put(self.addrs[12], 4, 0)
        result = T.call_ppc(self.e, A[op], [x & MASK for x in args], max_insns=50_000_000, timeout_us=300_000_000)[0] & MASK
        if op == "get":
            return -1 if result == 0 else (result - self.world) // 20
        if op in RETURNS:
            return result
        return None

    def mouse(self):
        p = self.get(0x102DC1F4, 4)
        return 0 if p == 0 else 1 if p == self.addrs[7] else 2

    def finish(self):
        log = self.logobj.log
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.cl = None
        return log


class Host(Side):
    byte = "little"

    def __init__(self, lib, c):
        bind(lib)
        self.lib = lib
        lib.tpw_sdp_reset(c["mouse"])
        self.addrs = [lib.tpw_sdp_addr(i) for i in range(13)]
        self.world = self.addrs[0]
        self.tables = [lib.tpw_sdp_table(i) for i in range(5)]
        self.things = [lib.tpw_sdp_thing(i) for i in range(len(c["things"]))]
        self.names = [lib.tpw_sdp_thing_name(i) for i in range(len(c["things"]))]
        fixed = {i + 1: p for i, p in enumerate(self.addrs)}
        fixed.update({20 + i: p for i, p in enumerate(self.tables)})
        fixed.update({40 + i: p for i, p in enumerate(self.things)})
        if c["op"] == "dtor" and c["slot"] != 0:
            fixed[80] = self.world + c["slot"] * 20
        CL.host_begin(lib, fixed, c["queues"])
        lib.tpw_sdp_row(0, self.things[0] if c["badzero"] and self.things else 0)
        for i, p in enumerate(self.things):
            lib.tpw_sdp_row(i + 1, p)

    def read(self, p, n):
        return C.string_at(p, n)

    def write(self, p, data):
        C.memmove(p, data, len(data))

    def balance(self, slot, value):
        self.lib.tpw_sdp_balance(slot, T.s32(value))

    def execute(self, c):
        op, slot = c["op"], c["slot"]
        args = {
            "dtor": [slot, int(T.s16(c["flag"]) > 0)], "info_save": [slot, c["save"], c["version"], c["unused"]],
            "name": [slot, c["size"]], "genname": [slot], "generate": [slot, c["type"]], "salary": [slot],
            "valid": [slot, c["valid"], c["id"]], "db_save": [c["save"], c["version"], c["unused"]],
            "get": [c["id"]], "hire": [c["id"]], "cap": [], "count": [], "minimum": [], "genstaff": [c["type"]],
            "numtype": [c["type"]], "num": [], "fate": [c["fate"]], "inuse": [], "free": [], "opening": []}
        if op == "sinit":
            values = (C.c_int32 * 2)()
            self.lib.tpw_sdp_sinit(values)
            return None
        result = getattr(self.lib, "tpw_sdp_" + op)(*args[op])
        return (result & MASK) if op in RETURNS else result if op == "get" else None

    def mouse(self):
        return self.lib.tpw_sdp_mouse()

    def finish(self):
        return CL.host_end(self.lib)


RETURNS = {"info_save", "salary", "db_save", "hire", "genstaff", "numtype", "num", "inuse", "free"}
# Typed fields plus untouched bytes: logical words compare despite machine byte order.
FIELDS = [(i * 20 + off, width) for i in range(32) for off, width in ((0, 4), (4, 4), (8, 1), (9, 1), (10, 1), (11, 1), (12, 4), (16, 4))]
FIELDS += [(0x280 + i * 4, 4) for i in range(5)] + [(0x294, 4)] + [(i, 1) for i in range(0x298, 0x2B0)]


def build(side, c):
    side.write(side.world, b"\xa5" * 0x2B0)
    for slot, words in enumerate(c["records"]):
        for (offset, width), value in zip(((0, 4), (4, 4), (8, 1), (9, 1), (10, 1), (11, 1), (12, 4), (16, 4)), words):
            side.put(side.world + slot * 20 + offset, width, value)
    for i, count in enumerate(c["counts"]):
        side.put(side.world + 0x280 + 4 * i, 4, count)
    side.put(side.world + 0x294, 4, c["stamp"])
    side.put(side.world + 0x298, 1, c["opened"])
    for i, stop in enumerate(c["stops"]):
        side.put(side.world + 0x299 + i, 1, stop)
    side.put(side.world + 0x1DA70C, 4, c["turn"])
    for i, value in enumerate(c["balance"]):
        side.balance(148 + i, value)
    for i in (1, 2, 3):
        side.write(side.addrs[i], b"\x00" * 256)
        side.text(side.addrs[i], c["texts"][i - 1])
    for i, (kind, text) in enumerate(c["things"]):
        side.write(side.things[i], b"\xa5" * 128)
        side.put(side.things[i], 2, i + 1)
        side.put(side.things[i] + 2, 1, kind)
        side.text(side.names[i], text)
    side.write(side.addrs[8], c["file"])
    side.put(side.addrs[9], 4, 0x11223344)
    side.put(side.addrs[10], 4, 0x55667788)
    if side.byte == "big":
        side.put(side.addrs[11], 4, 0x10000)
        side.put(side.addrs[12], 4, 0)


def run(side, c):
    build(side, c)
    result = side.execute(c)
    state = [side.get(side.world + offset, width) for offset, width in FIELDS]
    buffers = [[side.get(side.addrs[i] + 2 * k, 2) for k in range(128)] for i in (1, 2, 3)]
    words = [side.get(side.addrs[i], 4) for i in (9, 10, 11, 12)]
    # The sinit words are constant globals for other operations too.
    return {"answer": result, "state": state, "buffers": buffers, "words": words,
            "mouse": side.mouse(), "log": side.finish()}


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


def base(op):
    return {"op": op, "slot": 0, "id": 0, "type": 0, "valid": 1, "fate": 0, "flag": 0, "size": 128,
            "save": 1, "version": 1, "unused": 0xFEDCBA98, "mouse": 1, "badzero": False,
            "records": [[0, 0, 2, 3, 0, 0, 0x11223344, 0xAABBCCDD] for _ in range(32)],
            "counts": [0] * 5, "stops": [0] * 7, "opened": 0, "stamp": 0xDEADBEEF, "turn": 0xAABBCCDD,
            "balance": [2] * 5 + [0] * 5 + [10] * 5 + [0] * 5 + [100] * 5 + [0, 0, 0, 0, 20],
            "things": [], "texts": ["stale", "generated", "query"], "file": bytes((i * 17 + 2) & 255 for i in range(4096)),
            "queues": {"rand": [34], "sprites": [7], "salary": [0xF1234567], "head": [0], "next": [0], "new": [1]}}


def edges(op):
    yield base(op)
    if op in ("info_save", "db_save"):
        for save in (0, 1, -1):
            for version in (-1, 0, 1, 2):
                c = base(op)
                c.update(save=save, version=version)
                yield c
        fields = 7 if op == "info_save" else 236
        for save in (0, 1):
            for pos in range(fields):
                for key, fail in (("io", -1), ("xfer", 0)):
                    c = base(op)
                    c["save"] = save
                    c["queues"][key] = ([0] if key == "io" else [-1]) * pos + [fail]
                    yield c
            for pos in range(1 if op == "info_save" else 32):
                c = base(op)
                c["save"] = save
                c["queues"]["stafftype_result"] = [0] * pos + [1]
                yield c
        for flag in (2, 255):
            c = base(op)
            c.update(save=0, file=bytes([flag]) * 4096)
            yield c
    elif op == "dtor":
        for flag in (-1, 1, 32767, 32768, 65535, 65536, 65537):
            c = base(op)
            c["flag"] = flag
            yield c
    elif op in ("name", "genname", "salary", "inuse"):
        for typ in (0, 1, 2, 3, 4, 5, 6, -1, -2147483648):
            c = base(op)
            c["records"][0][0] = typ
            c["records"][0][4] = 255
            c["texts"][2] = "T%d:0" % typ if typ < 5 and typ >= 0 else "stale"
            c["queues"]["rand"] = [0]
            yield c
        if op == "name":
            for size in (0, 1, 2, 4, 128):
                c = base(op)
                c["size"] = size
                yield c
        if op == "genname":
            c = base(op)
            c["slot"] = 31
            c["records"][0][4] = 1
            c["queues"]["rand"] = [0, 1]
            yield c
            c = base(op)
            c["things"] = [(5, "T0:0")]
            c["queues"].update(head=[1], rand=[0, 1])
            yield c
        if op == "inuse":
            for kind in (0, 1, 3, 4, 5, 6, 7, 8, 9, 255):
                c = base(op)
                c["things"] = [(kind, "query")]
                c["queues"]["head"] = [1]
                c["badzero"] = True
                yield c
    elif op in ("generate", "genstaff", "minimum", "opening"):
        for typ in range(5):
            for grade in (0, 1, 4, 5, 127, 128, 255, 256, -1):
                c = base(op)
                c["type"] = typ
                c["balance"][typ] = grade
                c["queues"]["rand"] = [0, 0, 0, 0xFFFFFFFF]
                if op == "minimum":
                    c["balance"][15 + typ] = 2
                elif op == "opening":
                    c["balance"][5 + typ] = 2
                yield c
        if op in ("opening", "genstaff"):
            c = base(op)
            for record in c["records"]:
                record[4] = 255
            c["balance"][5:10] = [3] * 5
            yield c
            c = base(op)
            c["stops"] = [255] * 7
            c["balance"][5:10] = [2] * 5
            yield c
        if op == "opening":
            for opened in (1, 2, 255):
                c = base(op)
                c["opened"] = opened
                yield c
    else:
        if op == "numtype":
            for typ in (-2147483648, -1, 5, 6, 2147483647):
                c = base(op)
                c["type"] = typ
                c["records"][31][0] = typ
                c["records"][31][4] = 255
                yield c
        for ident in (0, 31, 32, 0xFFFFFFFF):
            for valid in (0, 1, 2, 255):
                c = base(op)
                c.update(id=ident, slot=ident % 32, valid=valid & 1)
                c["records"][ident % 32][4] = valid
                yield c
        for typ in range(7):
            c = base(op)
            c["records"][0][0] = typ
            c["records"][0][4] = 255
            c["stops"] = [0] * 5 + [2, 255]
            c["things"] = [(5, "a"), (4, "b"), (6, "c"), (7, "d"), (8, "e"), (0, "f")]
            c["balance"][20:25] = [1, 0, -1, 2, 1]
            c["queues"].update(head=[1], next=[2, 3, 4, 5, 6, 0])
            yield c
        if op == "hire":
            for typ in range(5):
                for mouse, new in ((0, [1, 1, 1]), (0, [0, 1, 1]), (0, [1, 1, 0]), (1, [0])):
                    c = base(op)
                    c.update(mouse=mouse)
                    c["records"][0][0] = typ
                    c["records"][0][4] = 1
                    c["queues"]["new"] = new
                    yield c
        for fate in (0, 1, -1, 2):
            c = base(op)
            c["fate"] = fate
            for i, record in enumerate(c["records"]):
                record[4] = 255
                record[5] = [0, 1, 2, 255][i % 4]
            yield c


def random_case(op, r):
    c = base(op)
    c.update(slot=r.randrange(32), type=r.randrange(5), id=r.choice([0, 31, 32, MASK, r.randrange(32)]),
             mouse=r.randrange(2), turn=r.getrandbits(32), stamp=r.getrandbits(32), fate=r.randrange(-1, 3),
             valid=r.randrange(2), flag=r.choice([-1, 0, 1, 65535, 65537]), size=r.randrange(129),
             opened=r.choice([0, 0, 1, 2, 255]), save=r.choice([0, 1, -1]), version=r.choice([-1, 0, 1, 1, 2]), unused=r.getrandbits(32))
    for record in c["records"]:
        record[:] = [r.randrange(7), r.getrandbits(32), r.randrange(256), r.randrange(256), r.choice([0, 0, 1, 2, 255]),
                     r.choice([0, 0, 1, 255]), r.getrandbits(32), r.getrandbits(32)]
    c["balance"][:5] = [r.choice([-1, 0, 1, 2, 3, 4, 5, 127, 128, 255, 256]) for _ in range(5)]
    c["balance"][5:10] = [r.randrange(-1, 4) for _ in range(5)]
    c["balance"][20:25] = [r.randrange(-1, 5) for _ in range(5)]
    c["balance"][29] = r.choice([2, 3, 4, 20, 0x7FFFFFFF])
    c["stops"] = [r.choice([0, 1, 2, 255]) for _ in range(7)]
    c["counts"] = [r.randrange(0, 4) for _ in range(5)]
    c["queues"]["rand"] = [r.getrandbits(32) for _ in range(32)]
    c["queues"]["sprites"] = [r.getrandbits(32)]
    c["queues"]["salary"] = [r.getrandbits(32)]
    c["queues"]["stafftype"] = [r.getrandbits(32) for _ in range(32)]
    c["things"] = [(r.choice([0, 1, 3, 4, 5, 6, 7, 8, 9, 255]), "thing%d" % i) for i in range(r.randrange(9))]
    c["queues"]["head"] = [1 if c["things"] else 0]
    c["queues"]["next"] = list(range(2, len(c["things"]) + 1)) + [0]
    c["badzero"] = bool(r.randrange(2))
    c["file"] = bytes(r.randrange(256) for _ in range(4096))
    if op == "minimum":
        # This loop has no exhaustion exit: ensure space and no stopped short category.
        for record in c["records"]:
            record[4] = 0
        c["stops"] = [0] * 7
        c["counts"] = [0] * 5
        c["balance"][15:20] = [r.randrange(3) for _ in range(5)]
    if op == "inuse" and c["things"] and r.randrange(2):
        c["texts"][2] = r.choice(c["things"])[1]
    return c


TESTS = [FnTest("CStaffDBPool::" + op, ppc, host, edge=lambda op=op: edges(op), rnd=lambda r, op=op: random_case(op, r)) for op in NAMES]
