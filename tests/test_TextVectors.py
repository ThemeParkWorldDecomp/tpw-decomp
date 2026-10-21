"""Differential tests for the text formatter's vector members and its line processor, against libtpw_unit_TextVectors.dylib
(tests/unit_TextVectors.cpp), unit w44, src/ltms/TbTextFormatter.cpp (ltms library):

  std::vector<CCharacterInfo>::clear 0x10007d74 and push_back 0x10007e0c
  std::vector<CLineInfo>::clear 0x10008454 and push_back 0x100084e4
  std::vector<TbPoint>::clear 0x1000884c, resize 0x100088d4, erase 0x10008be0 and insert 0x10008cf4
  ProcessLine 0x10006cfc and FixParenthesis 0x1000691c

Nothing is faked but operator new and operator delete (the library's are the harness's, which count; the PowerPC side's are stubs that count and
hand out fresh blocks filled with 0xa5, as the host's test blocks are). The vectors are compared cell by cell: the capacity, the size, whether the
data pointer moved, the heap traffic of each operation and the cells (all up to the size; and, when the block stayed, up to the old size, where erase
leaves the stale ones). The elements of a pushed or inserted value are copied field by field, so the padding bytes are not compared for the
vectors; ProcessLine and FixParenthesis swap whole 16-byte records, so for them every byte (padding and a guard record on each side) is compared.

The length_error path (a vector of 0xfffffff elements) is not tested: it builds an exception object through the C++ runtime on the Mac.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_TextVectors"

MASK = 0xFFFFFFFF
CLEAR = {0: 0x10007D74, 1: 0x10008454, 2: 0x1000884C}
PUSH = {0: 0x10007E0C, 1: 0x100084E4}
RESIZE, ERASE, INSERT = 0x100088D4, 0x10008BE0, 0x10008CF4
PROCESS_LINE, FIX_PARENTHESIS = 0x10006CFC, 0x1000691C
OPERATOR_NEW, OPERATOR_DELETE = 0x1000F76C, 0x1000F694
ELEMENT_BYTES = {0: 16, 1: 12, 2: 8}
ELEMENT_WORDS = {0: 5, 1: 3, 2: 2}


def s32(v):
    return T.s32(v & MASK)


def pack_element(kind, w):
    """An element's words as the Mac's bytes: padding is left 0xa5."""
    if kind == 0:
        b = bytearray(b"\xa5" * 16)
        b[0:2] = struct.pack(">H", w[0] & 0xFFFF)
        b[2:4] = struct.pack(">H", w[1] & 0xFFFF)
        b[4] = w[2] & 0xFF
        b[8:12] = struct.pack(">I", w[3] & MASK)
        b[12:14] = struct.pack(">H", w[4] & 0xFFFF)
        return bytes(b)
    if kind == 1:
        return struct.pack(">3I", *[v & MASK for v in w])
    return struct.pack(">2I", *[v & MASK for v in w])


def unpack_element(kind, d):
    if kind == 0:
        return [struct.unpack(">H", d[0:2])[0], struct.unpack(">H", d[2:4])[0], d[4], struct.unpack(">I", d[8:12])[0], struct.unpack(">H", d[12:14])[0]]
    return list(struct.unpack(">%dI" % ELEMENT_WORDS[kind], d))


# ======================================================================= the vectors


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.case = case
        e.heap_in_scratch = True
        self.heap = [0, 0, 0, 0]
        kind = case["kind"]
        self.kind = kind
        self.vec = e.alloc(12, bytes(12))
        data = 0
        if case["cap"]:
            block = bytearray(b"\xa5" * (case["cap"] * ELEMENT_BYTES[kind]))
            for i, w in enumerate(case["elems"]):
                block[i * ELEMENT_BYTES[kind]:(i + 1) * ELEMENT_BYTES[kind]] = pack_element(kind, w)
            data = e.alloc(len(block), bytes(block), alignment=16)
        e.write(self.vec, struct.pack(">3I", case["cap"], case["size"], data))
        self.stubs = self.make_stubs()
        self.old = self.stubs.activate(e)

    def make_stubs(self):
        st = CL.Stubs()
        st.real = {CLEAR[0], CLEAR[1], CLEAR[2], PUSH[0], PUSH[1], RESIZE, ERASE, INSERT}

        def new(e):
            size = e.r(3)
            self.heap[0] += 1
            e.set_r(3, e.alloc(max(size, 1), b"\xa5" * max(size, 1), alignment=16))

        def delete(e):
            if e.r(3):
                self.heap[2] += 1

        st.internal(OPERATOR_NEW, new)
        st.internal(OPERATOR_DELETE, delete)
        return st

    def finish(self):
        CL.Stubs.deactivate(self.e, self.old)

    def header(self):
        return struct.unpack(">3I", self.e.read(self.vec, 12))

    def step(self, op):
        e = self.e
        kind = self.kind
        size = ELEMENT_BYTES[kind]
        cap0, size0, data0 = self.header()
        self.heap[:] = [0, 0, 0, 0]
        answer = 0
        name = op[0]
        if name == "clear":
            T.call_ppc(e, CLEAR[kind], [self.vec])
        elif name == "push":
            element = e.alloc(16, pack_element(kind, op[1]), alignment=16)
            T.call_ppc(e, PUSH[kind], [self.vec, element])
        elif name == "resize":
            T.call_ppc(e, RESIZE, [self.vec, op[1] & MASK])
        elif name == "erase":
            r = T.call_ppc(e, ERASE, [self.vec, data0 + op[1] * 8, data0 + op[2] * 8])[0]
            answer = s32((r - data0) // 8)
        elif name == "insert":
            value = e.alloc(8, struct.pack(">2I", op[3] & MASK, op[4] & MASK), alignment=8)
            T.call_ppc(e, INSERT, [self.vec, data0 + op[1] * 8, op[2] & MASK, value])
        cap, count, data = self.header()
        moved = 1 if data != data0 else 0
        upto = count
        if not moved and size0 > upto:
            upto = size0
        if data == 0:
            upto = 0
        cells = []
        for i in range(upto):
            cells += unpack_element(kind, bytes(e.read(data + i * size, size)))
        return dict(answer=answer, heap=list(self.heap), cap=cap, size=count, moved=moved, cells=cells)


def run_ppc(e, case):
    ppc = Ppc(e, case)
    try:
        return dict(steps=[ppc.step(op) for op in case["ops"]])
    finally:
        ppc.finish()


_prototyped = set()


def prototypes(lib):
    if id(lib) in _prototyped:
        return
    _prototyped.add(id(lib))
    u32p = C.POINTER(C.c_uint32)
    lib.tpw_tv_reset.argtypes = []
    lib.tpw_tv_make.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, u32p]
    lib.tpw_tv_op.argtypes = [C.c_int32, C.c_int32, u32p, u32p]
    lib.tpw_tv_op.restype = C.c_int64
    lib.tpw_tv_heap.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tv_records.argtypes = [C.c_int32, u32p]
    lib.tpw_tv_line.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_tv_dump_records.argtypes = [u32p]
    lib.tpw_tv_dump_records.restype = C.c_int32


def u32array(words):
    return (C.c_uint32 * max(len(words), 1))(*[w & MASK for w in words])


def run_host(lib, case):
    prototypes(lib)
    lib.tpw_tv_reset()
    kind = case["kind"]
    lib.tpw_tv_make(kind, case["cap"], case["size"], u32array([v for w in case["elems"] for v in w]))
    opcodes = dict(clear=0, push=1, resize=2, erase=3, insert=4)
    steps = []
    heap = (C.c_int64 * 4)()
    out = (C.c_uint32 * 8192)()
    for op in case["ops"]:
        if op[0] == "push":
            args = op[1]
        elif op[0] == "insert":
            args = [op[1], op[2], op[3], op[4]]
        else:
            args = list(op[1:]) + [0, 0]
        answer = lib.tpw_tv_op(kind, opcodes[op[0]], u32array(args), out)
        lib.tpw_tv_heap(heap)
        n = out[3]
        steps.append(dict(answer=answer, heap=list(heap), cap=out[0], size=out[1], moved=out[2],
                          cells=list(out[4:4 + n * ELEMENT_WORDS[kind]])))
    lib.tpw_tv_reset()
    return dict(steps=steps)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ======================================================================= cases


def elem(kind, r=None, n=0):
    if r is None:
        if kind == 0:
            return [0x41 + n, n, 1 + (n & 0x7F), 10 + n, n * 3]
        if kind == 1:
            return [0x1000 + 16 * n, 0x1000 + 16 * n + 16, 100 + n]
        return [n, -n]
    if kind == 0:
        return [r.getrandbits(16), r.getrandbits(16), r.getrandbits(8), r.getrandbits(32), r.getrandbits(16)]
    if kind == 1:
        return [r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)]
    return [r.getrandbits(32), r.getrandbits(32)]


def case(kind, cap, size, ops, r=None):
    return dict(kind=kind, cap=cap, size=size, elems=[elem(kind, r, i + 1) for i in range(size)], ops=ops)


def edge_clear(kind):
    def gen():
        for cap, size in ((0, 0), (1, 0), (1, 1), (4, 0), (4, 3), (4, 4), (8, 5)):
            yield case(kind, cap, size, [("clear",)])
            yield case(kind, cap, size, [("clear",), ("clear",)])
    return gen


def edge_push(kind):
    def gen():
        for cap, size in ((0, 0), (1, 0), (1, 1), (2, 2), (3, 3), (4, 0), (4, 3), (4, 4), (5, 5), (8, 5), (8, 8), (9, 9)):
            yield case(kind, cap, size, [("push", elem(kind, None, 99))])
        yield case(kind, 0, 0, [("push", elem(kind, None, i)) for i in range(1, 20)])
        yield case(kind, 0, 0, [("push", elem(kind, None, 1)), ("clear",), ("push", elem(kind, None, 2)), ("push", elem(kind, None, 3)),
                                ("clear",), ("push", elem(kind, None, 4))])
        yield case(kind, 4, 4, [("push", [0xFFFF, 0xFFFF, 0xFF, MASK, 0xFFFF] if kind == 0 else [MASK] * ELEMENT_WORDS[kind])])
    return gen


def rnd_vector(kind, ops_names, n_ops=6):
    def rnd(r):
        size = r.choice([0, 0, 1, 2, 3, 4, 5, 7, 8, r.randint(0, 20)])
        cap = size + r.choice([0, 0, 0, 1, 2, 5, 16]) if r.random() < 0.9 else r.choice([0, 1, 2, 4, 8])
        cap = max(cap, size)
        ops = []
        current = size
        for _ in range(r.randint(1, n_ops)):
            name = r.choice(ops_names)
            if name == "clear":
                ops.append(("clear",))
                current = 0
            elif name == "push":
                ops.append(("push", elem(kind, r)))
                current += 1
            elif name == "resize":
                n = r.choice([0, current, current + 1, current + 2, r.randint(0, 24), max(current - 1, 0)])
                ops.append(("resize", n))
                current = n
            elif name == "erase":
                a = r.randint(0, current)
                b = r.randint(a, current)
                ops.append(("erase", a, b))
                current -= b - a
            elif name == "insert":
                pos = r.randint(0, current)
                n = r.choice([0, 1, 1, 2, 3, r.randint(0, 12)])
                ops.append(("insert", pos, n, r.getrandbits(32), r.getrandbits(32)))
                current += n
        return case(kind, cap, size, ops, r)
    return rnd


def edge_resize():
    for size in range(0, 7):
        for cap in sorted({size, size + 1, size + 4, 8, 16, 0 if size == 0 else size}):
            if cap < size:
                continue
            for target in (0, 1, 2, 3, size, size + 1, size + 2, size + 5, 9, 16, 17, 33):
                yield case(2, cap, size, [("resize", target)])
    yield case(2, 0, 0, [("resize", 5), ("resize", 2), ("resize", 2), ("resize", 0), ("resize", 40)])


def edge_erase():
    for size in range(0, 8):
        for a in range(size + 1):
            for b in range(a, size + 1):
                yield case(2, size + 2, size, [("erase", a, b)])
    yield case(2, 4, 4, [("erase", 0, 4), ("erase", 0, 0)])
    # first above last (undefined in the C++ library, but the original's loop is plain): the tail moves UP and the size grows by the gap
    yield case(2, 6, 5, [("erase", 3, 2)])


def edge_insert():
    for size in range(0, 6):
        for cap in sorted({size, size + 1, size + 3, 8, 12}):
            if cap < size:
                continue
            for pos in range(size + 1):
                for n in (0, 1, 2, 3, 5, 8, 9, 17):
                    yield case(2, cap, size, [("insert", pos, n, 0x11111111, -7 & MASK)])
    yield case(2, 3, 3, [("insert", 3, 1, 5, 6), ("insert", 0, 2, 7, 8), ("insert", 2, 4, 9, 10), ("insert", 8, 30, 1, 2)])


# ---- ProcessLine and FixParenthesis

PALETTE = [0x41, 0x42, 0x61, 0x30, 0x20, 0x2C, 0x28, 0x29, 0x5B0, 0x5B1, 0x5D0, 0x5EA, 0x5EB, 0x5AF, 0x2E, 0x28, 0x29, 0x5D1, 0x20, 0xFFFF, 0]


def record(char, tag=0, pad=0xA5):
    """A record of the array: 8 words (character, index, type, pad5, pad6, width, position, padE); the tag makes records tell apart."""
    return [char & 0xFFFF, tag & 0xFFFF, (tag * 7) & 0xFF, pad, 0xBEEF if pad else 0, 1000 + tag, tag & 0xFFFF, 0xC0DE]


def line_case(chars, first=None, last=None, op=0, r=None):
    records = [record(c, i + 1) for i, c in enumerate(chars)]
    if r is not None:
        records = [[w if k in (0, 1) else r.getrandbits(8) if k in (2, 3) else r.getrandbits(16) if k in (4, 6, 7) else r.getrandbits(32)
                    for k, w in enumerate(rec)] for rec in records]
        for rec, c in zip(records, chars):
            rec[0] = c
    return dict(records=records, first=0 if first is None else first, last=len(chars) if last is None else last, op=op)


def ppc_line(e, c):
    st = CL.Stubs()
    st.real = {PROCESS_LINE, FIX_PARENTHESIS}
    old = st.activate(e)
    try:
        n = len(c["records"])
        block = bytearray(b"\xa5" * (16 * (n + 2)))
        for i, w in enumerate(c["records"]):
            b = bytearray(16)
            b[0:2] = struct.pack(">H", w[0] & 0xFFFF)
            b[2:4] = struct.pack(">H", w[1] & 0xFFFF)
            b[4], b[5] = w[2] & 0xFF, w[3] & 0xFF
            b[6:8] = struct.pack(">H", w[4] & 0xFFFF)
            b[8:12] = struct.pack(">I", w[5] & MASK)
            b[12:14] = struct.pack(">H", w[6] & 0xFFFF)
            b[14:16] = struct.pack(">H", w[7] & 0xFFFF)
            block[16 * (i + 1):16 * (i + 2)] = b
        base = e.alloc(len(block), bytes(block), alignment=16) + 16
        T.call_ppc(e, PROCESS_LINE if c["op"] == 0 else FIX_PARENTHESIS, [base + 16 * c["first"], base + 16 * c["last"]])
        d = bytes(e.read(base - 16, len(block)))
        out = []
        for i in range(n + 2):
            x = d[16 * i:16 * i + 16]
            out += [struct.unpack(">H", x[0:2])[0], struct.unpack(">H", x[2:4])[0], x[4], x[5], struct.unpack(">H", x[6:8])[0],
                    struct.unpack(">I", x[8:12])[0], struct.unpack(">H", x[12:14])[0], struct.unpack(">H", x[14:16])[0]]
        return dict(records=out)
    finally:
        CL.Stubs.deactivate(e, old)


def host_line(lib, c):
    prototypes(lib)
    lib.tpw_tv_reset()
    lib.tpw_tv_records(len(c["records"]), u32array([v for w in c["records"] for v in w]))
    lib.tpw_tv_line(c["op"], c["first"], c["last"])
    out = (C.c_uint32 * (8 * (len(c["records"]) + 2)))()
    lib.tpw_tv_dump_records(out)
    lib.tpw_tv_reset()
    return dict(records=list(out))


def line_edges(op):
    def gen():
        H = 0x5D0
        words = [
            [], [H], [0x41], [0x28], [0x29], [H, 0x28], [0x28, H], [H, 0x29], [0x41, H], [H, 0x41], [H, H, H], [0x41, 0x42, 0x43],
            [0x28, H, 0x29], [0x29, H, 0x28], [0x28, 0x41, 0x29, H], [H, 0x20, 0x41, 0x42, 0x20, H], [0x41, 0x20, H, 0x20, 0x42],
            [H, 0x2C, 0x41, 0x2C, 0x28, H, 0x29, 0x20], [0x20, 0x20, H], [H, 0x20, 0x20], [0x20], [0x2C], [0x20, H], [H, 0x20],
            [0x28, 0x28, H, 0x29, 0x29], [0x28, 0x29, 0x29, H], [0x29, 0x28, 0x28, H], [0x28, 0x41, 0x28, 0x42, 0x29, 0x43, 0x29, H],
            [0x5AF], [0x5B0], [0x5B1], [0x5EA], [0x5EB], [0x5AF, 0x5B0], [0x5EB, 0x5EC], [0x5B1, 0x41, 0x20, 0x5EA],
            [0x41, 0x20, 0x42, 0x20, 0x43, 0x20, H], [H, 0x20, 0x41, 0x20, 0x42, 0x20, 0x43],
            [0x28, 0x41, 0x20, 0x29, H], [0x28, H, 0x20, 0x41, 0x29],
        ]
        for chars in words:
            n = len(chars)
            yield line_case(chars, op=op)
            for first in range(n + 1):
                for last in range(first, n + 1):
                    if (first, last) != (0, n) and (n <= 5 or (first + last) % 2 == 0):
                        yield line_case(chars, first, last, op)
            if n >= 2:
                yield line_case(chars, n, 0, op)
    return gen


def line_rnd(op):
    def rnd(r):
        n = r.choice([0, 1, 2, 3, 4, 5, 6, 8, r.randint(0, 14)])
        palette = PALETTE if r.random() < 0.7 else [0x28, 0x29, 0x5D0, 0x20, 0x41, 0x2C]
        chars = [r.choice(palette) for _ in range(n)]
        first = r.randint(0, n) if r.random() < 0.3 else 0
        last = r.randint(first, n) if r.random() < 0.3 else n
        return line_case(chars, first, last, op, r)
    return rnd


TESTS = [
    FnTest("std::vector<CCharacterInfo>::clear 0x10007d74", run_ppc, run_host, edge=edge_clear(0), rnd=rnd_vector(0, ["clear", "push"], 3)),
    FnTest("std::vector<CCharacterInfo>::push_back 0x10007e0c", run_ppc, run_host, edge=edge_push(0), rnd=rnd_vector(0, ["push", "push", "clear"])),
    FnTest("std::vector<CLineInfo>::clear 0x10008454", run_ppc, run_host, edge=edge_clear(1), rnd=rnd_vector(1, ["clear", "push"], 3)),
    FnTest("std::vector<CLineInfo>::push_back 0x100084e4", run_ppc, run_host, edge=edge_push(1), rnd=rnd_vector(1, ["push", "push", "clear"])),
    FnTest("std::vector<TbPoint>::clear 0x1000884c", run_ppc, run_host, edge=edge_clear(2), rnd=rnd_vector(2, ["clear", "resize"], 3)),
    FnTest("std::vector<TbPoint>::resize 0x100088d4", run_ppc, run_host, edge=edge_resize, rnd=rnd_vector(2, ["resize", "resize", "clear"])),
    FnTest("std::vector<TbPoint>::erase 0x10008be0", run_ppc, run_host, edge=edge_erase, rnd=rnd_vector(2, ["erase", "erase", "insert", "resize"])),
    FnTest("std::vector<TbPoint>::insert 0x10008cf4", run_ppc, run_host, edge=edge_insert, rnd=rnd_vector(2, ["insert", "insert", "erase", "resize"])),
    FnTest("FixParenthesis 0x1000691c", ppc_line, host_line, edge=line_edges(1), rnd=line_rnd(1)),
    FnTest("ProcessLine 0x10006cfc", ppc_line, host_line, edge=line_edges(0), rnd=line_rnd(0)),
]
