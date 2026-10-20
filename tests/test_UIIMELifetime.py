"""w92: the lifetime of the IME classes, main app (TPEmbed.cpp, ui_IME.cpp, ui_iterators.cpp).

IME::CIME::CIME 0x101fe934, IME::CIME::~CIME 0x101fe9ec, std::vector<IME::list_cand_data_t>::tear_down 0x101feb58,
IME::list_cand_data_t::~list_cand_data_t 0x101f6bec, std::vector<std::basic_string<wchar_t>>::tear_down 0x101f6ecc and
CUIIME::~CUIIME 0x101f7d18.

The Mac objects are built by hand in emulator memory (Mac layout) and the host's in native memory from the same description.
Boundaries on both sides: the string library's constructor 0x101e0b84 (hands out the tokens 0x7001, 0x7002.. in call order)
and destructor 0x101468f8 (records the object's label, the word it holds and the flag), and operator delete (every labelled
block records "delete" and loses its label). The destructors run with the flag 0 (the first part) and 1 (the deleting call,
which on the host is `delete p`). After the operation the surviving object is dumped (pointers as labels), and the number of
blocks operator delete handled is compared.
"""
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIIMELifetime"
CTOR, DTOR, DEL, CAND_DTOR, CAND_DEL, UI_DTOR, UI_DEL, VEC_TEAR, STR_TEAR = 0x101FE934, 0x101FE9EC, 0, 0x101F6BEC, 0, 0x101F7D18, 0, 0x101FEB58, 0x101F6ECC
STRING_CTOR, STRING_DTOR = 0x101E0B84, 0x101468F8
OPS = {0: CTOR, 1: DTOR, 2: DTOR, 3: CAND_DTOR, 4: CAND_DTOR, 5: UI_DTOR, 6: UI_DTOR, 7: VEC_TEAR, 8: STR_TEAR}
FLAGS = {2: 1, 4: 1, 6: 1}
VTABLE = 0x102B15B0
MASK = 0xFFFFFFFF
# The labels of the fixture (see unit_UIIMELifetime.cpp).
OBJECT, STRINGS, VECTOR_BLOCK, MEMBER_BLOCK, MEMBER_STRINGS, ELEMENT_BLOCK, ELEMENT_STRINGS = 1, 4, 7, 8, 90, 20, 40

# A string vector: (capacity, size, has_block, tokens).  A CIME: (tokens3, capacity, size, has_block, [string vectors]).


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def build_strings(e, base, spec, block_label, string_label, fixed):
    """Writes the string vector (capacity, size, data) at `base` and its block of tokens."""
    capacity, size, has_block, tokens = spec
    data = 0
    if has_block:
        count = max(len(tokens), 1)
        data = e.alloc(4 * count)
        for j, token in enumerate(tokens):
            w32(e, data + 4 * j, token)
        fixed[data] = block_label
        for j in range(1, count):
            fixed[data + 4 * j] = string_label + j
    w32(e, base, capacity)
    w32(e, base + 4, size)
    w32(e, base + 8, data)


def build_cime(e, base, spec, fixed):
    tokens, capacity, size, has_block, elements = spec
    for i, token in enumerate(tokens):
        w32(e, base + 8 + 4 * i, token)
        fixed[base + 8 + 4 * i] = STRINGS + i
    data = 0
    if has_block:
        data = e.alloc(0x20 * max(len(elements), 1))
        e.write(data, e.read(base + 0x38, 1) * (0x20 * max(len(elements), 1)))   # the fill byte
        for i, element in enumerate(elements):
            build_strings(e, data + 0x20 * i + 0x10, element, ELEMENT_BLOCK + i, ELEMENT_STRINGS + 6 * i, fixed)
        fixed[data] = VECTOR_BLOCK
    w32(e, base + 0x14, capacity)
    w32(e, base + 0x18, size)
    w32(e, base + 0x1C, data)


def build_cand(e, base, spec, fixed):
    build_strings(e, base + 0x10, spec, MEMBER_BLOCK, MEMBER_STRINGS, fixed)


class Ppc:
    """The emulator side of one case."""

    def __init__(self, e, case):
        self.e, self.case = e, case
        self.fixed = {}
        self.log = CL.PpcLog(self.fixed)
        self.ordinal = 0

    def lab(self, a):
        return self.log.lab(a)

    def image_cime(self, b):
        e = self.e
        words = [e.u32(b + 4), e.u32(b + 8), e.u32(b + 12), e.u32(b + 16), e.u32(b + 0x14), e.u32(b + 0x18), self.lab(e.u32(b + 0x1C))]
        words += list(e.read(b + 0x20, 4))
        words += [e.u32(b + 0x24), e.u32(b + 0x28), T.s32(e.u32(b + 0x2C)), e.u32(b + 0x30), e.u32(b + 0x34)]
        words += [e.u32(b + 0x38 + 4 * i) for i in range(64)]
        return words

    def image_cand(self, b):
        e = self.e
        return [e.u32(b + 4 * i) for i in range(4)] + [e.u32(b + 0x10), e.u32(b + 0x14), self.lab(e.u32(b + 0x18))]

    def image_cuiime(self, b):
        e = self.e
        return (list(e.read(b + 8, 0x14)) + self.image_cime(b + 0x1C) + self.image_cand(b + 0x154) + list(e.read(b + 0x174, 8))
                + [e.u32(b + 0x17C) >> 16, e.u32(b + 0x17C) & 0xFFFF])


def make_stubs():
    st = CL.Stubs()
    st.real = {CTOR, DTOR, VEC_TEAR, CAND_DTOR, UI_DTOR, STR_TEAR}

    def string_ctor(e):
        L = e.cl
        L.ordinal += 1
        L.rec("wstr.ctor", (L.lab(e.r(3)),))
        e.write(e.r(3), struct.pack(">I", 0x7000 + L.ordinal))

    def string_dtor(e):
        L = e.cl
        L.rec("wstr.dtor", (L.lab(e.r(3)), e.u32(e.r(3)), T.s16(e.r(4))))

    def operator_delete(e):
        L = e.cl
        p = e.r(3)
        if p:
            e.deletes.append(p)
        if L.lab(p) > 0:
            L.rec("delete", (L.lab(p),))
            L.forget(p)

    st.internal(STRING_CTOR, string_ctor)
    st.internal(STRING_DTOR, string_dtor)
    st.imp("__dl__FPv", operator_delete)
    return st


STUBS = make_stubs()


def run_ppc(e, case):
    op, fill, cime, cand = case["op"], case["fill"], case["cime"], case["cand"]
    w = Ppc(e, case)
    e.cl = w.log
    w.log.ordinal = 0
    L = w.log
    if not hasattr(e, "deletes"):
        e.deletes = []
    del e.deletes[:]
    if op == 0:
        obj = e.alloc(0x138, bytes([fill]) * 0x138)
        for i in range(3):
            w.fixed[obj + 8 + 4 * i] = STRINGS + i
        w.fixed[obj] = OBJECT
    elif op in (1, 2, 7):
        obj = e.alloc(0x138, bytes([fill]) * 0x138)
        build_cime(e, obj, cime, w.fixed)
        w.fixed[obj] = OBJECT
    elif op in (3, 4, 8):
        obj = e.alloc(0x20, bytes([fill]) * 0x20)
        build_cand(e, obj, cand, w.fixed)
        w.fixed[obj] = OBJECT
    else:
        obj = e.alloc(0x180, bytes([fill]) * 0x180)
        build_cime(e, obj + 0x1C, cime, w.fixed)
        build_cand(e, obj + 0x154, cand, w.fixed)
        w.fixed[obj] = OBJECT
    L.labels = {a: l for a, l in w.fixed.items() if a}
    saved = STUBS.activate(e)
    try:
        # The vector operations take the vector's own address.
        target = {7: obj + 0x14, 8: obj + 0x10}.get(op, obj)
        T.call_ppc(e, OPS[op], [target, FLAGS.get(op, 0)] if op in (1, 2, 3, 4, 5, 6) else [target])
    finally:
        CL.Stubs.deactivate(e, saved)
    state = []
    if op == 0:
        state.append(int(e.u32(obj) == VTABLE))
        state += w.image_cime(obj)
    elif op in (1, 7):
        state += w.image_cime(obj)
    elif op in (3, 8):
        state += w.image_cand(obj)
    elif op == 5:
        state += w.image_cuiime(obj)
    state.append(len(e.deletes))
    return dict(state=state, log=L.log)


def run_host(lib, case):
    import ctypes as C
    lib.tpw_ime_run.restype = C.c_int32
    lib.tpw_ime_run.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    flat = [case["op"], case["fill"]] + flat_cime(case["cime"]) + flat_strings(case["cand"])
    values = (C.c_int64 * len(flat))(*flat)
    out = (C.c_int64 * 2048)()
    CL.host_begin(lib, {})
    n = lib.tpw_ime_run(values, out)
    log = CL.host_end(lib)
    return dict(state=[int(out[i]) for i in range(n)], log=log)


def flat_strings(spec):
    capacity, size, has_block, tokens = spec
    return [capacity, size, int(has_block), len(tokens)] + list(tokens)


def flat_cime(spec):
    tokens, capacity, size, has_block, elements = spec
    out = list(tokens) + [capacity, size, int(has_block), len(elements)]
    for element in elements:
        out += flat_strings(element)
    return out


# ----------------------------------------------------------------- the cases

NO_STRINGS = (0, 0, False, [])
NO_CIME = ((0, 0, 0), 0, 0, False, [])


def case(op, cime=NO_CIME, cand=NO_STRINGS, fill=0xA5):
    return dict(op=op, fill=fill, cime=cime, cand=cand)


def strings(size, tokens=None, capacity=None, has_block=True, count=None):
    tokens = list(tokens) if tokens is not None else [0x1100 + 0x11 * j for j in range(count if count is not None else size)]
    return (size if capacity is None else capacity, size, has_block, tokens)


def string_shapes():
    """The string vectors the edge cases use: no block, an empty vector with a block, 1..5 strings, a size short of the block."""
    yield NO_STRINGS
    yield (7, 3, False, [])                          # a size without a block: nothing happens
    yield (4, 0, True, [])                           # a block of no strings: only the block is freed
    yield (4, 0, True, [0x55, 0x66])                 # a size of 0 on a block with strings: none is destroyed
    yield strings(1)
    yield strings(2, [0, 0xFFFFFFFF])
    yield strings(3, capacity=3)
    yield strings(5, capacity=0xFFFFFFFF)
    yield strings(2, count=5)                        # a size short of the strings the block holds
    yield strings(4, capacity=0)                     # the capacity is not read


def cime_with(elements, size=None, capacity=None, has_block=True, tokens=(0x2001, 0, 0xFFFFFFFF)):
    return (tokens, len(elements) if capacity is None else capacity, len(elements) if size is None else size, has_block, list(elements))


def cime_shapes():
    yield NO_CIME
    yield cime_with([], size=3, has_block=False)              # a size without a block
    yield cime_with([], size=0)                               # a block of no lists
    yield cime_with([NO_STRINGS])
    yield cime_with([strings(1)])
    yield cime_with([strings(2), NO_STRINGS])
    yield cime_with([strings(2), strings(3), strings(1)])
    yield cime_with([strings(2), strings(1), strings(3), strings(2)], size=2)    # a size short of the block's lists
    yield cime_with([strings(1), strings(2)], size=1, capacity=0)
    yield cime_with([strings(1)], size=0, capacity=0xFFFFFFFF)
    yield cime_with([NO_STRINGS, (3, 2, False, [])], capacity=9)
    yield cime_with([strings(3), strings(0, [], 2, True), strings(5)], tokens=(0x1111, 0x2222, 0x3333))


def edge_for(ops):
    def edges():
        for op in ops:
            if op == 0:
                for fill in (0x00, 0xA5, 0xFF, 0x5A, 0x01):
                    yield case(0, fill=fill)
            elif op in (1, 2, 7):
                for shape in cime_shapes():
                    yield case(op, cime=shape)
                yield case(op, cime=cime_with([strings(2)]), fill=0x00)
                yield case(op, cime=cime_with([strings(2)]), fill=0xFF)
            elif op in (3, 4, 8):
                for shape in string_shapes():
                    yield case(op, cand=shape)
                yield case(op, cand=strings(2), fill=0x00)
                yield case(op, cand=strings(2), fill=0xFF)
            else:
                for cime in list(cime_shapes())[::2]:
                    for cand in list(string_shapes())[::2]:
                        yield case(op, cime=cime, cand=cand)
                yield case(op, cime=cime_with([strings(2), strings(1)]), cand=strings(3), fill=0x00)
                yield case(op, cime=cime_with([strings(2), strings(1)]), cand=strings(3), fill=0xFF)
    return edges


def rand_token(r):
    return r.choice([0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32)])


def rand_strings(r):
    if r.random() < 0.15:
        return (r.getrandbits(32) if r.random() < 0.3 else r.randint(0, 4), r.randint(0, 4), False, [])
    count = r.randint(0, 5)
    size = r.randint(0, count) if r.random() < 0.8 else count
    capacity = r.choice([size, size, 0, size + r.randint(0, 3), r.getrandbits(32)])
    return (capacity, size, True, [rand_token(r) for _ in range(count)])


def rand_cime(r):
    tokens = (rand_token(r), rand_token(r), rand_token(r))
    if r.random() < 0.12:
        return (tokens, r.randint(0, 3), r.randint(0, 3), False, [])
    count = r.randint(0, 4)
    size = r.randint(0, count) if r.random() < 0.8 else count
    capacity = r.choice([size, size, 0, size + r.randint(0, 3), r.getrandbits(32)])
    return (tokens, capacity, size, True, [rand_strings(r) for _ in range(count)])


def random_for(ops):
    def rnd(r):
        op = r.choice(ops)
        return case(op, cime=rand_cime(r), cand=rand_strings(r), fill=r.choice([0xA5, 0x00, 0xFF, r.getrandbits(8)]))
    return rnd


def test(name, ops):
    return FnTest(name, run_ppc, run_host, edge=edge_for(ops), rnd=random_for(ops))


TESTS = [
    test("IME::CIME::CIME", [0]),
    test("IME::CIME::~CIME", [1, 2]),
    test("std::vector<IME::list_cand_data_t>::tear_down", [7]),
    test("IME::list_cand_data_t::~list_cand_data_t", [3, 4]),
    test("std::vector<std::basic_string<wchar_t>>::tear_down", [8]),
    test("CUIIME::~CUIIME", [5, 6]),
]
