"""Unit v5234, TbMapStreamer::CCatFixupTable::Load, against libtpw_unit_TbMapStreamer_CCatFixupTable_Load.dylib
(tests/unit_TbMapStreamer_CCatFixupTable_Load.cpp), sound library, src/sound/TbMapStreamer.cpp:
  TbMapStreamer::CCatFixupTable::Load 0x1002388c

A case is a file image (size word, count word, count slot words, then records: a length byte and a name) read through a fake
file. The callees outside the unit are fakes on both sides that record the same events: Unload, the file's Read (the original calls it
through the pointer glue 0x1002f154), the allocator 0x1002ebfc (modelled as operator new[]), TbMapStreamer::Instance and
FindCatagory (answers from the case's queue), TbError::TbError.

Compared: the answer, the event log, the count, and the whole allocated block (slots rewritten, backslashes turned to colons).
No fused operations. Not tested: a block shorter than its records (the original reads past it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_TbMapStreamer_CCatFixupTable_Load"
MASK = 0xFFFFFFFF

LOAD_LABEL = ".Load__Q213TbMapStreamer14CCatFixupTableFRQ33bfl10FileSystem5CFile"
CALLEE_READ = 0x1002F154
CALLEE_NEW = 0x1002EBFC
CALLEE_UNLOAD = 0x10023A78
CALLEE_INSTANCE = 0x1001F6EC
CALLEE_FIND = 0x100237A4
CALLEE_TBERROR = 0x100015B8
HOST_LAYOUT = [0, 8]

PP = {}
CUR = {"case": None, "pos": 0}


def s32(v):
    return T.s32(v)


def stub_read(e):
    L = e.cl
    n = e.r(5) & MASK
    L.rec("Read", (L.lab(e.r(3)), n))
    data = CUR["case"]["file"]
    pos = CUR["pos"]
    chunk = bytes(data[pos + i] if pos + i < len(data) else 0 for i in range(n))
    CUR["pos"] = pos + n
    if n:
        e.write(e.r(4), chunk)
    e.set_r(3, n)


def stub_new(e):
    n = e.r(3) & MASK
    e.cl.rec("new", (n,))
    e.set_r(3, e.alloc(max(n, 4)))


def stub_tberror(e):
    e.write(e.r(3), struct.pack(">I", e.r(4) & MASK))


STUBS = CL.Stubs()
STUBS.real.add(0x1002388C)
STUBS.internal(CALLEE_READ, stub_read)
STUBS.internal(CALLEE_NEW, stub_new)
STUBS.internal(CALLEE_TBERROR, stub_tberror)
STUBS.internal(CALLEE_UNLOAD, CL.recorder("Unload", "L"))
STUBS.internal(CALLEE_INSTANCE, CL.recorder("Instance", "", ret=lambda e, log: PP["inst"]))
STUBS.internal(CALLEE_FIND, CL.recorder("FindCatagory", "Ls", ret=lambda e, log: log.ret("FindCatagory") & MASK))


def setup(runner):
    T.install_std_stubs(runner.emu)
    PP["inst"] = T.persist_alloc(runner.emu, 16)
    PP["table"] = T.persist_alloc(runner.emu, 8)
    PP["vt"] = T.persist_alloc(runner.emu, 0x40)
    PP["file"] = T.persist_alloc(runner.emu, 8, struct.pack(">II", PP["vt"], 0))
    PP["hidden"] = T.persist_alloc(runner.emu, 4)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cft_addr.restype, lib.tpw_cft_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cft_alloc.restype, lib.tpw_cft_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cft_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cft_setfile.restype, lib.tpw_cft_setfile.argtypes = None, [C.c_char_p, C.c_uint32]
    lib.tpw_cft_table.restype, lib.tpw_cft_table.argtypes = None, [C.POINTER(C.c_uint64)]
    lib.tpw_cft_load.restype, lib.tpw_cft_load.argtypes = C.c_int32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_cft_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def norm(block, count, fmt):
    """The slot words read in the side's byte order, then the tail bytes as they are."""
    n = min(count, len(block) // 4)
    return {"slots": list(struct.unpack(fmt + "%dI" % n, block[:4 * n])), "tail": block[4 * n:]}


def ppc(e, case):
    CUR["case"], CUR["pos"] = case, 0
    L = e.cl = CL.PpcLog({PP["inst"]: 1, PP["table"]: 2, PP["file"]: 3}, case["queues"])
    saved = STUBS.activate(e)
    try:
        e.write(PP["table"], struct.pack(">II", 0xDEADBEEF, 0))
        e.write(PP["hidden"], struct.pack(">I", 0x12345678))
        T.call_ppc(e, 0x1002388C, [PP["hidden"], PP["table"], PP["file"]])
        answer = s32(e.u32(PP["hidden"]))
        count, data = e.u32(PP["table"]), e.u32(PP["table"] + 4)
        block = e.read(data, case["size"]) if data else b""
        return {"answer": answer, "log": L.log, "count": count, "block": norm(block, count, ">")}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    bind(lib)
    CL.host_begin(lib, {1: lib.tpw_cft_addr(b"inst"), 2: lib.tpw_cft_addr(b"table"), 3: lib.tpw_cft_addr(b"file")},
                  case["queues"])
    lib.tpw_cft_setfile(bytes(case["file"]), len(case["file"]))
    answer = lib.tpw_cft_load()
    out = (C.c_uint64 * 2)()
    lib.tpw_cft_table(out)
    block = C.string_at(out[1], case["size"]) if out[1] else b""
    return {"answer": answer, "log": CL.host_end(lib), "count": out[0], "block": norm(block, out[0], "<")}


def make_case(r, names):
    """names: a list of bytes (a name without its NUL) or None (a record whose length byte is 0); a tuple (name, extra) pads."""
    count = len(names)
    slots = b"".join(struct.pack("<I", r.getrandbits(32)) for _ in range(count))
    body = b""
    for n in names:
        if n is None:
            body += b"\x00"
        else:
            extra = 0
            if isinstance(n, tuple):
                n, extra = n
            body += bytes([len(n) + 1 + extra]) + n + b"\x00" * (1 + extra)
    block = slots + body
    return {"file": struct.pack("<II", len(block), count) + block, "size": len(block),
            "queues": {"FindCatagory": [r.randrange(0, 40) for _ in range(max(count, 1))]}}


ALPHA = [ord("a"), ord("Z"), ord("\\"), ord("\\"), ord(":"), ord("/"), 0x80, 0xFF, 0x5B, 0x5D, 1]


def rname(r):
    return bytes(r.choice(ALPHA) for _ in range(r.randrange(0, 9)))


def rcase(r):
    names = []
    for _ in range(r.randrange(0, 7)):
        k = r.random()
        if k < 0.2:
            names.append(None)
        elif k < 0.3:
            names.append((rname(r), r.randrange(1, 4)))
        else:
            names.append(rname(r))
    return make_case(r, names)


def edges():
    r = random.Random(5234)
    yield make_case(r, [])
    yield make_case(r, [None])
    yield make_case(r, [b""])
    yield make_case(r, [b"\\"])
    yield make_case(r, [b"a\\b\\\\c"])
    yield make_case(r, [None, b"x", None, b"\xff\\\x80"])
    yield make_case(r, [(b"ab", 3), b"q\\r", None])
    yield make_case(r, [b"abc", b"def"])


TESTS = [
    FnTest("TbMapStreamer::CCatFixupTable::Load", ppc, host, edge=edges, rnd=rcase),
]
