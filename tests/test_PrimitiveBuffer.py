"""The draw primitive's buffer (engine tb_drawprimitive.cpp, 0x10006a58..0x10007ea0): the
functions that make a primitive in the render memory's table, clip a triangle list into it
and execute it: GetNextPrimitive, DuplicateCurrentPrimitive, ExternExecute, both
ExecuteCurrentPrimitive, ExecutePrimitiveIfInstant, AddClipPolyList, ApplyFog,
CheckFogOnCurrentPrimitive, ProjectCurrentPrimitive and RoundOffPrimitive.

Every callee outside the unit is a fake on both sides that records its arguments: the
render memory (LoadInfo, LoadInfo_CopyVDB, SaveInfo and the two end positions), the clipper
(CPP_TestData, ClipPoly), the light manager, the math unit's Project, the face sorter, the
assertion and the renderer's Execute. Pointers into the test's memory are recorded as a
place (region number << 32 plus the byte offset) so both sides log the same numbers.

A case is one scenario: the object's words, a table of render memory records, a vertex
area, an index area, a list of input indices, the system and display flags, queues of
answers for the fakes, a stream of numbers for the clipper's fakes, and a script of
operations. After every operation the whole state is dumped and compared: the object, the
eight records, the work array the sort writes behind a record, the index area and the
vertex area.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_PrimitiveBuffer"

# The functions under test.
GET_NEXT = 0x10006a58
DUPLICATE = 0x10006bec
EXTERN_EXECUTE = 0x10006eb4
EXECUTE_LONG = 0x10006f40
EXECUTE_SHORT = 0x10007360
EXECUTE_INSTANT = 0x10007444
ADD_CLIP_POLY_LIST = 0x100074fc
APPLY_FOG = 0x10007c4c
CHECK_FOG = 0x10007d08
PROJECT = 0x10007df4
ROUND_OFF = 0x10007ea0
UNDER_TEST = (GET_NEXT, DUPLICATE, EXTERN_EXECUTE, EXECUTE_LONG, EXECUTE_SHORT, EXECUTE_INSTANT,
              ADD_CLIP_POLY_LIST, APPLY_FOG, CHECK_FOG, PROJECT, ROUND_OFF)

# The callees that are fakes.
LOAD_INFO = 0x1002720c
LOAD_INFO_COPY = 0x10027354
SAVE_INFO = 0x10027460
VERTEX_END = 0x1002750c
INDEX_END = 0x1002756c
QSORT_FACES = 0x10006d40
TEST_DATA = 0x1003f6dc
CLIP_POLY = 0x1004087c
FOG_INTENSITIES = 0x10014d38

# The globals the functions read.
RENDER_MEMORY = 0x10165630
DISPLAY = 0x10165634
SYSTEM = 0x10165f80
LIGHT_MANAGER = 0x100af1b8
MATH_UNIT = 0x100afe94
CLIPPING = 0x102b3638

MASK = 0xffffffff
NO_OFFSET = 0x80000000      # a pointer field that is NULL
JUNK = 0xa5a5a5a5
RECORDS = 40                # records in the table
DUMPED = 8                  # the records the state dump covers
SCRATCH = 9                 # record number LoadInfo(6) is made to return: its work array is dumped
RECORD_SIZE = 0x38
VERTEX_BYTES = 0x2800
INDEX_ENTRIES = 768
INPUT_ENTRIES = 128
VERTEX_REGION, INDEX_REGION = 1, 2

# Operation numbers (tests/unit_PrimitiveBuffer.cpp, tpw_pb_op).
OP_GET_NEXT, OP_DUPLICATE, OP_EXTERN, OP_EXECUTE, OP_EXECUTE_DEPTH, OP_INSTANT = 1, 2, 3, 4, 5, 6
OP_ADD_CLIP, OP_APPLY_FOG, OP_CHECK_FOG, OP_PROJECT, OP_ROUND_OFF, OP_GLOBALS, OP_SET_COUNTS = 7, 8, 9, 10, 11, 12, 13

CODE_ADDRESS = {
    OP_GET_NEXT: GET_NEXT, OP_DUPLICATE: DUPLICATE, OP_EXTERN: EXTERN_EXECUTE, OP_EXECUTE: EXECUTE_LONG,
    OP_EXECUTE_DEPTH: EXECUTE_SHORT, OP_INSTANT: EXECUTE_INSTANT, OP_ADD_CLIP: ADD_CLIP_POLY_LIST,
    OP_APPLY_FOG: APPLY_FOG, OP_CHECK_FOG: CHECK_FOG, OP_PROJECT: PROJECT, OP_ROUND_OFF: ROUND_OFF,
}


def ws(e, address, values):
    """Writes big-endian words."""
    e.write(address, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def rs(e, address, count):
    """Reads big-endian words."""
    return list(struct.unpack(">" + "I" * count, e.read(address, count * 4)))


def r16(e, address, count):
    """Reads big-endian 16-bit entries."""
    return list(struct.unpack(">" + "H" * count, e.read(address, count * 2)))


def fb(x):
    """The bits of a single-precision float."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bf(word):
    """The single-precision float of some bits."""
    return struct.unpack(">f", struct.pack(">I", word & MASK))[0]


def canon(word):
    """A NaN as one value: a multiply or an add may leave different payloads on the two sides."""
    return 0x7fc00000 if (word & 0x7f800000) == 0x7f800000 and (word & 0x007fffff) else word


def s32(value):
    return T.s32(value & MASK)


# ----------------------------------------------------------------- the PowerPC side


def setup(runner):
    """Standard stubs, plus the fake renderer object and math unit that live across cases."""
    e = runner.emu
    T.install_std_stubs(e)
    if hasattr(e, "pb_vtable"):
        return
    execute = T.fake_function(e, lambda em: em.pb.execute(em))
    project = T.fake_function(e, lambda em: em.pb.project(em))
    # CDrawPrimitive's vtable: the slot at +0x18 is Execute.
    e.pb_vtable = T.persist_alloc(e, 0x40)
    e.write(e.pb_vtable + 0x18, struct.pack(">I", execute))
    # The math unit: +4 is the vtable pointer, whose slot at +0x14 is Project.
    math_vtable = T.persist_alloc(e, 0x40)
    e.write(math_vtable + 0x14, struct.pack(">I", project))
    e.pb_math = T.persist_alloc(e, 0x10)
    e.write(e.pb_math, struct.pack(">II", 0, math_vtable))


class PpcSide:
    """One case on the PowerPC side: memory laid out as the Mac does, the fakes installed."""

    def __init__(self, e, case):
        self.e = e
        e.pb = self
        self.recs = e.alloc(RECORDS * RECORD_SIZE, b"\xa5" * (RECORDS * RECORD_SIZE))
        self.vtx = e.alloc(VERTEX_BYTES)
        self.idx = e.alloc(INDEX_ENTRIES * 2)
        self.inp = e.alloc(INPUT_ENTRIES * 2)
        self.this = e.alloc(0x30, b"\xa5" * 0x30)
        self.count = e.alloc(4)
        self.render = e.alloc(16)
        self.clipping = e.alloc(8)
        self.light = e.alloc(16)
        ws(e, self.vtx, case["vtx"])
        e.write(self.idx, struct.pack(">" + "H" * INDEX_ENTRIES, *case["idx"]))
        e.write(self.inp, struct.pack(">" + "H" * INPUT_ENTRIES, *case["inp"]))
        for k, words in enumerate(case["recs"]):
            self.plant_record(k, words)
        w = list(case["this"])
        w[7] = self.record(w[7])
        w[9] = 0 if w[9] == NO_OFFSET else self.vtx + s32(w[9])
        w[10] = 0 if w[10] == NO_OFFSET else self.idx + s32(w[10])
        ws(e, self.this, w)
        ws(e, self.this + 0x2c, [e.pb_vtable])
        self.stream = list(case["stream"])
        self.stream_next = 0
        self.saved = {a: e.read(a, 4) for a in (RENDER_MEMORY, DISPLAY, SYSTEM, SYSTEM + 0x1c, LIGHT_MANAGER, MATH_UNIT, CLIPPING)}
        ws(e, RENDER_MEMORY, [self.render])
        ws(e, LIGHT_MANAGER, [self.light])
        ws(e, MATH_UNIT, [e.pb_math])
        ws(e, CLIPPING, [self.clipping])
        ws(e, SYSTEM, [case["sys"]])
        ws(e, SYSTEM + 0x1c, [case["renderer"]])
        ws(e, DISPLAY, [case["disp"]])
        fixed = {self.this: 1, self.render: 2, self.clipping: 3, self.light: 4, e.pb_math: 5}
        e.cl = self.log = CL.PpcLog(fixed, case["queues"])
        stubs = CL.Stubs()
        stubs.real.update(UNDER_TEST)
        stubs.internal(LOAD_INFO, self.load_info)
        stubs.internal(LOAD_INFO_COPY, self.load_info_copy)
        stubs.internal(SAVE_INFO, self.save_info)
        stubs.internal(VERTEX_END, self.vertex_end)
        stubs.internal(INDEX_END, self.index_end)
        stubs.internal(QSORT_FACES, self.sort_faces)
        stubs.internal(TEST_DATA, self.test_data)
        stubs.internal(CLIP_POLY, self.clip_poly)
        stubs.internal(FOG_INTENSITIES, self.fog)
        stubs.imp("__assertion_failed", self.assertion)
        self.stubs = stubs
        self.old = stubs.activate(e)

    def close(self):
        """Puts the shared emulator back as it was."""
        CL.Stubs.deactivate(self.e, self.old)
        for address, data in self.saved.items():
            self.e.write(address, bytes(data))

    def plant_record(self, k, w):
        """A record from its 13 words (the Mac's third filler word is junk)."""
        vdata = 0 if w[9] == NO_OFFSET else self.vtx + s32(w[9])
        idata = 0 if w[10] == NO_OFFSET else self.idx + s32(w[10])
        ws(self.e, self.recs + k * RECORD_SIZE, [w[0], w[1], w[2], w[3], w[4], JUNK, w[5], w[6], w[7], w[8], vdata, idata, w[11], w[12]])

    def record(self, number):
        """The address of record `number` (1-based), or 0."""
        return self.recs + (number - 1) * RECORD_SIZE if 1 <= number <= RECORDS else 0

    def record_id(self, address):
        """1 + the record's index, 0 for NULL, -1 for anything else."""
        if address == 0:
            return 0
        offset = address - self.recs
        if offset < 0 or offset >= RECORDS * RECORD_SIZE or offset % RECORD_SIZE:
            return -1
        return offset // RECORD_SIZE + 1

    def place(self, region, address):
        """Region number and offset of a pointer, as the host's Place."""
        if address == 0:
            return 0
        base = self.vtx if region == VERTEX_REGION else self.idx
        return (region << 32) + s32(address - base)

    def pop(self):
        """The next number of the clipper's stream (0 when it has run out)."""
        if self.stream_next < len(self.stream):
            self.stream_next += 1
            return self.stream[self.stream_next - 1]
        return 0

    # Fakes of the render memory.

    def load_info(self, e):
        self.log.rec("LoadInfo", [self.log.lab(e.r(3)), s32(e.r(4))])
        e.set_r(3, self.record(self.log.ret("LoadInfo")))

    def load_info_copy(self, e):
        self.log.rec("LoadInfo_CopyVDB", [self.log.lab(e.r(3)), s32(e.r(4))])
        e.set_r(3, self.record(self.log.ret("CopyVDB")))

    def save_info(self, e):
        self.log.rec("SaveInfo", [self.log.lab(e.r(3)), self.place(VERTEX_REGION, e.r(4)), self.place(INDEX_REGION, e.r(5))])

    def vertex_end(self, e):
        self.log.rec("GetVertexEndPosition", [self.log.lab(e.r(3))])
        e.set_r(3, self.vtx + self.log.ret("VertexEnd"))

    def index_end(self, e):
        self.log.rec("GetIndexEndPosition", [self.log.lab(e.r(3))])
        e.set_r(3, self.idx + self.log.ret("IndexEnd"))

    # Fake of the face sorter: records the keys and the index triples it was given.

    def sort_faces(self, e):
        keys, indices, low, high = e.r(3), e.r(4), s32(e.r(5)), s32(e.r(6))
        n = high + 1 if 0 < high + 1 <= 100 else 0
        hash_keys = 0xcbf29ce484222325
        hash_indices = 0xcbf29ce484222325
        first = [0, 0, 0, 0, 0]
        words = rs(e, keys, n) if n else []
        entries = r16(e, indices, 3 * n) if n else []
        for i, word in enumerate(words):
            word = canon(word)
            hash_keys = ((hash_keys ^ word) * 0x100000001b3) & 0xffffffffffffffff
            if i < 5:
                first[i] = word
            for k in range(3):
                hash_indices = ((hash_indices ^ entries[i * 3 + k]) * 0x100000001b3) & 0xffffffffffffffff
        self.log.rec("QSortFaces_float", [self.record_id(keys - 0x28), self.place(INDEX_REGION, indices), low, high, n,
                                          hash_keys & 0x7fffffffffffffff, hash_indices & 0x7fffffffffffffff] + first)

    # Fakes of the clipper.

    def test_data(self, e):
        result, clipping, codes, vertices, count, stride, mask = [e.r(i) for i in range(3, 10)]
        self.log.rec("CPP_TestData", [self.log.lab(clipping), int(result != 0), int(codes != 0), self.place(VERTEX_REGION, vertices),
                                      s32(count), s32(stride), mask])
        every = 0xff
        some = 0
        data = bytearray()
        for _ in range(s32(count)):
            code = (self.pop() & MASK) & mask
            data.append(code & 0xff)
            every &= code
            some |= code
        if data:
            e.write(codes, bytes(data))
        e.write(result, bytes([every & 0xff, some & 0xff]))

    def clip_poly(self, e):
        this, planes, vertices, lst, count_address, stride = [e.r(i) for i in range(3, 9)]
        list_count, list_pointer = rs(e, lst, 2)
        triangle = r16(e, list_pointer, 3)
        self.log.rec("ClipPoly", [self.log.lab(this), s32(planes), self.place(VERTEX_REGION, vertices), s32(list_count),
                                  self.place(INDEX_REGION, list_pointer)] + triangle + [s32(e.u32(count_address)), s32(stride)])
        n = s32(self.pop())
        added = s32(self.pop())
        entries = [self.pop() & 0xffff for _ in range(max(n, 0))]
        if entries:
            e.write(list_pointer, struct.pack(">" + "H" * len(entries), *entries))
        ws(e, lst, [n])
        ws(e, count_address, [e.u32(count_address) + added])

    # Fake of the light manager.

    def fog(self, e):
        self.log.rec("CalcVertexFogIntensities", [self.log.lab(e.r(3)), self.place(VERTEX_REGION, e.r(4)), s32(e.r(5)), s32(e.r(6))])
        e.set_r(3, self.log.ret("Fog"))

    # Fakes called through the vtables: the renderer's Execute and the math unit's Project.

    def execute(self, e):
        p = e.r(4)
        w = rs(e, p, 14)
        self.log.rec("Execute", [self.record_id(p), w[0], w[1], w[2], s32(w[6]), w[7], s32(w[8]), s32(w[9]),
                                 self.place(VERTEX_REGION, w[10]), self.place(INDEX_REGION, w[11]), canon(w[12]), s32(w[13])])
        move = self.log.ret("ExecuteMove")
        if move:
            ws(e, self.this + 0x1c, [self.record(move)])
        e.set_r(3, self.log.ret("Execute", 1))

    def project(self, e):
        self.log.rec("Project", [self.log.lab(e.r(3)), self.place(VERTEX_REGION, e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))])
        poke = self.log.ret("ProjectPoke")
        if poke:
            ws(e, e.u32(self.this + 0x1c) + 0x30, [poke])
        e.set_f(1, bf(self.log.ret("Project")))

    def assertion(self, e):
        self.log.rec("assert", [s32(e.r(5))], [e.cstr(e.r(3)), e.cstr(e.r(4))])

    # Operations and the state dump.

    def run(self, op, a):
        """One operation; returns what the host's tpw_pb_op returns."""
        e = self.e
        this = self.this
        if op == OP_GLOBALS:
            ws(e, SYSTEM, [a[0]])
            ws(e, SYSTEM + 0x1c, [a[1]])
            ws(e, DISPLAY, [a[2]])
            return 0
        if op == OP_SET_COUNTS:
            current = e.u32(this + 0x1c)
            ws(e, current + 0x20, [a[0], a[1]])
            return 0
        address = CODE_ADDRESS[op]
        if op in (OP_GET_NEXT, OP_DUPLICATE):
            result = T.call_ppc(e, address, [this, a[0], a[1], a[2], a[3], a[4]])[0]
            return self.record_id(result)
        if op in (OP_EXTERN, OP_INSTANT):
            return s32(T.call_ppc(e, address, [this, self.record(a[0])])[0])
        if op == OP_EXECUTE:
            return s32(T.call_ppc(e, address, [this])[0])
        if op == OP_EXECUTE_DEPTH:
            return s32(T.call_ppc(e, address, [this, T.F(bf(a[0]))])[0])
        if op == OP_ADD_CLIP:
            ws(e, self.count, [a[0]])
            mapped = False
            if e.u32(this + 0x1c) == 0:
                # The original reads low memory before it notices there is no primitive.
                e.uc.mem_map(0, 0x1000)
                mapped = True
            try:
                result = T.call_ppc(e, address, [this, self.count, self.inp + 2 * a[1], a[2], a[3]])[0]
            finally:
                if mapped:
                    e.uc.mem_unmap(0, 0x1000)
            return s32(result)
        if op == OP_APPLY_FOG:
            T.call_ppc(e, address, [this, self.vtx + a[0], a[1]])
            return 0
        if op == OP_PROJECT:
            T.call_ppc(e, address, [this, a[0]])
            return 0
        T.call_ppc(e, address, [this])
        return 0

    def state(self):
        """The same words as the host's tpw_pb_state."""
        e = self.e
        out = rs(e, self.this, 11)
        out[7] = self.record_id(out[7]) & MASK
        out[9] = NO_OFFSET if out[9] == 0 else (out[9] - self.vtx) & MASK
        out[10] = NO_OFFSET if out[10] == 0 else (out[10] - self.idx) & MASK
        out.append(e.u32(self.count))
        for k in range(DUMPED):
            w = rs(e, self.recs + k * RECORD_SIZE, 14)
            out += [w[0], w[1], w[2], w[3], w[4], w[6], w[7], w[8], w[9]]
            out += [NO_OFFSET if w[10] == 0 else (w[10] - self.vtx) & MASK]
            out += [NO_OFFSET if w[11] == 0 else (w[11] - self.idx) & MASK]
            out += [canon(w[12]), w[13]]
        out += [canon(w) for w in rs(e, self.recs + (SCRATCH - 1) * RECORD_SIZE + 0x28, 24)]
        out += r16(e, self.idx, INDEX_ENTRIES)
        out += rs(e, self.vtx, VERTEX_BYTES // 4)
        return out


def ppc(e, case):
    side = PpcSide(e, case)
    try:
        states = [side.state()]
        results = []
        for op, args in case["ops"]:
            results.append(side.run(op, args))
            states.append(side.state())
        return dict(results=results, states=states, log=side.log.log)
    finally:
        side.close()


# ----------------------------------------------------------------- the host side


def host(lib, case):
    CL.host_begin(lib, {}, case["queues"])
    lib.tpw_pb_begin.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_pb_record.argtypes = [C.c_int32, C.POINTER(C.c_uint32)]
    lib.tpw_pb_data.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint16), C.POINTER(C.c_uint16), C.POINTER(C.c_int64), C.c_int32]
    lib.tpw_pb_op.argtypes = [C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_pb_op.restype = C.c_int64
    lib.tpw_pb_state.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_pb_state.restype = C.c_int32
    begin = list(case["this"]) + [case["sys"], case["renderer"], case["disp"]]
    lib.tpw_pb_begin((C.c_uint32 * len(begin))(*[v & MASK for v in begin]))
    for k, words in enumerate(case["recs"]):
        lib.tpw_pb_record(k, (C.c_uint32 * 13)(*[v & MASK for v in words]))
    stream = case["stream"]
    lib.tpw_pb_data((C.c_uint32 * len(case["vtx"]))(*case["vtx"]), (C.c_uint16 * INDEX_ENTRIES)(*case["idx"]),
                    (C.c_uint16 * INPUT_ENTRIES)(*case["inp"]), (C.c_int64 * max(len(stream), 1))(*stream), len(stream))
    out = (C.c_uint32 * 4096)()

    def state():
        return list(out[:lib.tpw_pb_state(out)])

    states = [state()]
    results = []
    for op, args in case["ops"]:
        a = (C.c_int32 * 8)(*[s32(v) for v in args])
        results.append(lib.tpw_pb_op(op, a))
        states.append(state())
    return dict(results=results, states=states, log=CL.host_end(lib))


# ----------------------------------------------------------------- case building


def default_this():
    """The object: instant mask 0x3000, 32-byte vertices, record 1 current, room to spare."""
    return [0, 0, 0x3000, JUNK, MASK, 0, 0x20, 1, 0, 0x2000, 0x600]


def default_record(k):
    return [0, 0, 0x01020300 + k, JUNK, JUNK, 4, 0x1c4, 4, 6, 0, 0, fb(1.0), MASK]


def default_area(seed, count, make):
    state = seed
    out = []
    for _ in range(count):
        state = (state * 1103515245 + 12345) & 0x7fffffff
        out.append(make(state))
    return out


# Vertices, indices and inputs a case gets unless it brings its own.
DEFAULT_VERTICES = default_area(7, VERTEX_BYTES // 4, lambda s: fb((s % 2001 - 1000) / 8.0))
# (Every default index is a valid vertex number: a primitive that is sorted reads the vertices
# its indices name, and a number outside the area would read memory the test does not own.)
DEFAULT_INDICES = default_area(11, INDEX_ENTRIES, lambda s: s % 24)
DEFAULT_INPUT = default_area(9, INPUT_ENTRIES, lambda s: s % 3)


def mk(ops, this=None, records=None, sys=0, renderer=1, disp=0, vtx=None, idx=None, inp=None, stream=(), queues=None):
    """A case; `this` and `records` override the defaults word by word: a dict of word index:
    value for `this`, and for `records` a dict of record number (1 to 8): list of 13 words or
    dict of word index: value."""
    this_words = default_this()
    for k, v in (this or {}).items():
        this_words[k] = v & MASK
    recs = [default_record(k) for k in range(DUMPED)]
    for number, change in (records or {}).items():
        if isinstance(change, dict):
            for index, value in change.items():
                recs[number - 1][index] = value & MASK
        else:
            recs[number - 1] = [v & MASK for v in change]
    inp = list(inp or []) + DEFAULT_INPUT[len(inp or []):]
    return dict(ops=list(ops), this=this_words, recs=recs, sys=sys & MASK, renderer=renderer & MASK, disp=disp & MASK,
                vtx=vtx or DEFAULT_VERTICES, idx=idx or DEFAULT_INDICES, inp=inp, stream=list(stream),
                queues=dict(queues or {}))


def random_float_word(r):
    """A float of moderate size and random precision, now and then a special value."""
    if r.random() < 0.04:
        return r.choice([0, 0x80000000, 0x7f800000, 0xff800000, 0x7fc00000, 0xffc00001, 0x00000001, 0x007fffff, 0x7f7fffff])
    return fb(r.uniform(-1.0, 1.0) * 2.0 ** r.randrange(-20, 21))


def random_vertices(r):
    return [random_float_word(r) for _ in range(VERTEX_BYTES // 4)]


def random_sys(r):
    return r.getrandbits(32)


# ----------------------------------------------------------------- GetNextPrimitive and its sibling


FORMATS = [0xc4, 0x1c4, 0x2c4, 0x3c4]
WRONG_FORMATS = [0, 1, 0xc3, 0xc5, 0x1c3, 0x1c5, 0x2c3, 0x2c5, 0x3c3, 0x3c5, 0x1c4 | 0x10000, MASK, 0x80000000]


def make_header_case(r, op):
    """GetNextPrimitive / DuplicateCurrentPrimitive with random arguments and state."""
    fmt = r.choice(FORMATS + FORMATS + [0, 0xc5, 0x4c4, r.getrandbits(32)])
    flags = r.choice([1, 2, 3, 5, 6, 7, 0, r.getrandbits(32)])
    mode = r.choice([0, 0x1000, 0x2000, 0x3000, 0x800, 0x4000000, r.getrandbits(32)])
    args = [flags, r.getrandbits(32), fmt, mode, r.getrandbits(32)]
    mask = r.choice([0x3000, 0, 0x1000, 0x2000, r.getrandbits(32)])
    this = {i: r.getrandbits(32) for i in range(11) if i not in (7, 9, 10)}
    this[2] = mask
    this[6] = r.choice([0, 0x18, 0x20, 0x30, r.getrandbits(32)])
    this[7] = r.choice([0, 1, 2, 5])
    this[9] = r.choice([0, 0x100, 0x2000, NO_OFFSET])
    this[10] = r.choice([0, 0x80, 0x600, NO_OFFSET])
    queues = {
        "LoadInfo": [r.choice([0, 1, 2, 3, 5, 8])],
        "CopyVDB": [r.choice([0, 1, 2, 3, 5, 8])],
        "VertexEnd": [r.choice([0, 0x40, 0x1000, 0x2400])],
        "IndexEnd": [r.choice([0, 0x20, 0x300, 0x5f0])],
    }
    return mk([(op, args)], this=this, sys=random_sys(r), queues=queues)


def header_edges(op):
    def edges():
        for fmt in FORMATS + WRONG_FORMATS:
            for flags in (1, 2, 3, 5, 6, 0):
                for mode in (0, 0x1000, 0x3000):
                    for sys in (0, 0x4000000):
                        yield mk([(op, [flags, 4, fmt, mode, 0x11223344])], sys=sys,
                                 queues={"LoadInfo": [2], "CopyVDB": [3], "VertexEnd": [0x2000], "IndexEnd": [0x500]})
        for number in (0, 1, 8):
            yield mk([(op, [2, 4, 0xc4, 0x1000, 0x55])], queues={"LoadInfo": [number], "CopyVDB": [number]})
        for mask in (0, 0x1000, 0x2000, 0x3000, 0x800, MASK):
            for mode in (0x1000, 0x2000, 0x800, 0x3000, 0):
                for sys in (0, 0x4000000, MASK):
                    yield mk([(op, [2, 1, 0x1c4, mode, 0])], this={2: mask}, sys=sys,
                             queues={"LoadInfo": [4], "CopyVDB": [4]})
        # a second call changes the record, the vertex size and the ends again
        yield mk([(op, [3, 4, 0xc4, 0, 1]), (op, [3, 5, 0x3c4, 0x1000, 2])],
                 queues={"LoadInfo": [2, 0], "CopyVDB": [2, 0], "VertexEnd": [0x100, 0x200], "IndexEnd": [0x20, 0x40]})
    return edges


# ----------------------------------------------------------------- ExternExecute, ExecutePrimitiveIfInstant


COUNTS = [0, 1, 2, 3, 100, -1, -2, 0x7fffffff, 0x80000000, MASK]


def extern_case(r):
    records = {1: {7: r.choice(COUNTS), 8: r.choice(COUNTS), 0: r.getrandbits(32)}}
    queues = {"Execute": [r.choice([0, 1, 2, MASK, 0x100])]}
    if r.random() < 0.3:
        queues["ExecuteMove"] = [r.randrange(1, 5)]
    return mk([(OP_EXTERN, [r.choice([1, 2, 3])])], records=records, queues=queues)


def extern_edges():
    for vertices in COUNTS:
        for indices in (0, 1, -1, 5, 0x80000000):
            for result in (0, 1, 7):
                yield mk([(OP_EXTERN, [2])], records={1: {7: vertices, 8: indices}}, queues={"Execute": [result]})


def instant_case(r):
    mode = r.choice([0, 0x1000, 0x2000, 0x3000, 0x800, 0x4000, r.getrandbits(32)])
    mask = r.choice([0x3000, 0x1000, 0, 0x2000, r.getrandbits(32)])
    records = {2: {0: r.getrandbits(32), 1: mode}}
    queues = {"Execute": [r.choice([0, 1, 3, MASK])]}
    if r.random() < 0.3:
        queues["ExecuteMove"] = [r.randrange(1, 5)]
    return mk([(OP_INSTANT, [r.choice([1, 2, 3])])], this={2: mask}, records=records, queues=queues)


def instant_edges():
    for mode in (0, 0x1000, 0x2000, 0x3000, 0x800, MASK):
        for mask in (0, 0x1000, 0x3000, MASK):
            for result in (0, 1, 2):
                for flags in (0, 0x10, MASK):
                    yield mk([(OP_INSTANT, [3])], this={2: mask}, records={2: {0: flags, 1: mode}}, queues={"Execute": [result]})


# ----------------------------------------------------------------- fog and projection


def fog_state(r):
    """The record and flags the fog functions read."""
    return dict(
        disp=r.choice([0, 0x2000, 0x4000, 0x6000, r.getrandbits(32)]),
        renderer=r.choice([1, 3, 2, 0, 4, 5, -1, -2, 0x7fffffff]),
        records={1: {1: r.choice([0, 0x80000, 0x100000, 0x180000, r.getrandbits(32)]), 7: r.choice([0, 1, 4, 30, -2]),
                     9: r.choice([0, 0x40, 0x100])}},
        this={6: r.choice([0x18, 0x20, 0x28, 0x30, r.getrandbits(32)])},
        queues={"Fog": [r.choice([0, 1, 2, 0x100, 0x180, 0xff, MASK])]},
    )


def apply_fog_case(r):
    s = fog_state(r)
    args = [r.choice([0, 0x20, 0x100]), r.choice([0, 1, 5, 40, -1])]
    return mk([(OP_APPLY_FOG, args)], this=s["this"], records=s["records"], disp=s["disp"], renderer=s["renderer"], queues=s["queues"])


def apply_fog_edges():
    for disp in (0, 0x2000, 0x4000, 0x6000, MASK, 0x20000):
        for result in (0, 1, 0x100, 0xff):
            for mode in (0, 0x80000, 0x100000, 0x180000, MASK):
                yield mk([(OP_APPLY_FOG, [0x40, 7])], disp=disp, records={1: {1: mode}}, queues={"Fog": [result]})


def check_fog_case(r):
    s = fog_state(r)
    return mk([(OP_CHECK_FOG, [])], this=s["this"], records=s["records"], disp=s["disp"], renderer=s["renderer"], queues=s["queues"])


def check_fog_edges():
    for renderer in (-1, 0, 1, 2, 3, 4, 5, 0x7fffffff, 0x80000000):
        for disp in (0, 0x4000, 0x2000, MASK):
            for result in (0, 1, 0x100):
                for mode in (0, 0x80000, 0x100000, MASK):
                    yield mk([(OP_CHECK_FOG, [])], disp=disp, renderer=renderer, records={1: {1: mode, 7: 9, 9: 0x30}},
                             queues={"Fog": [result]})


def project_case(r):
    records = {1: {7: r.choice([0, 1, 3, 40, -1, 0x7fffffff]), 11: random_float_word(r), 9: r.choice([0, 0x80])}}
    queues = {"Project": [random_float_word(r)]}
    if r.random() < 0.25:
        queues["ProjectPoke"] = [random_float_word(r)]
    return mk([(OP_PROJECT, [r.choice([0, 1, 2, 3, 15, -1])])], this={6: r.choice([0x18, 0x20, 0x28, 0x30])}, records=records,
              queues=queues)


def project_edges():
    for count in (0, 1, -1, 1000):
        for depth in (0.0, 1.5, -2.25, 1e30):
            for projected in (0.0, 0.5, -0.5, 3.0e30, float("inf"), float("-inf")):
                yield mk([(OP_PROJECT, [2])], records={1: {7: count, 11: fb(depth)}}, queues={"Project": [fb(projected)]})
    yield mk([(OP_PROJECT, [1])], records={1: {7: 4, 11: fb(1.0)}}, queues={"Project": [fb(2.0)], "ProjectPoke": [fb(10.0)]})
    for projected in (0x7fc00000, 0xffc00001, 0x7f800000, 0xff800000):
        yield mk([(OP_PROJECT, [3])], records={1: {7: 4, 11: fb(1.0)}}, queues={"Project": [projected]})
        yield mk([(OP_PROJECT, [3])], records={1: {7: 4, 11: 0x7f800000}}, queues={"Project": [projected]})


def round_off_case(r):
    records = {k: [r.getrandbits(32) for _ in range(13)] for k in range(3)}
    this = {i: r.getrandbits(32) for i in range(11) if i not in (7, 9, 10)}
    return mk([(OP_ROUND_OFF, [])], this=this, records=records, sys=random_sys(r))


def round_off_edges():
    yield mk([(OP_ROUND_OFF, [])])
    yield mk([(OP_ROUND_OFF, []), (OP_ROUND_OFF, [])], this={7: 0})


# ----------------------------------------------------------------- ExecuteCurrentPrimitive


def execute_state(r, sorted_path):
    """A random current primitive and the memory it points at."""
    mask = r.choice([0x3000, 0, 0x1000, 0x2000, 0x800, 0x3800])
    flags = r.choice([0, 0x40, 0x20, 0x400, 0x200, 0x10, 0x60, 0x460, 0x4f, 0x2f, 0x25]) | r.choice([0, 0, 1, 2, 4, 8])
    if r.random() < 0.15:
        flags = r.getrandbits(32)
    mode = r.choice([0, 0x800, 0x1000, 0x1800, 0x2000, 0x3000, 0x100, 0x80000, 0x180000, r.getrandbits(32)])
    if sorted_path:
        # no instant bit in the mode, no "execute now" flag: the faces are sorted
        mask = r.choice([0x2000, 0, 0x2000, 0x8000])
        mode = r.choice([0, 0x100, 0x1000, 0x800, 0x80000, 0x400000, 0x4000, 0x800 | 0x4000]) & ~mask
        flags &= ~0x200
    stride = r.choice([0x18, 0x20, 0x28, 0x30])
    faces = r.choice([0, 1, 2, 3, 7, 12, 20, 20, 5])
    icount = faces * 3 + r.choice([0, 0, 0, 1, 2])
    if r.random() < 0.05:
        icount = r.choice([-1, -9, -2, 1, 2])
    vcount = r.choice([1, 2, 4, 9, 40, 0, -3, 0x7fffffff, 3])
    vdata = r.choice([0, 0x10, 0x100])
    idata = r.choice([0, 2, 0x40])
    records = {1: [flags & MASK, mode & MASK, r.getrandbits(32), JUNK, JUNK, r.getrandbits(8), r.choice(FORMATS), vcount & MASK,
                   icount & MASK, vdata, idata, random_float_word(r), MASK]}
    renderer = r.choice([1, 3, 1, 3, 2, 0, 4, 5, -1])
    disp = r.choice([0, 0x4000, 0x2000, 0x6000, r.getrandbits(32)])
    queues = {
        "Fog": [r.choice([0, 1, 0x100])],
        "Project": [random_float_word(r)],
        "Execute": [r.choice([1, 1, 1, 0, 2])],
        "LoadInfo": [SCRATCH],
    }
    if r.random() < 0.15:
        queues["ProjectPoke"] = [random_float_word(r)]
    if r.random() < 0.15:
        queues["ExecuteMove"] = [r.randrange(2, 5)]
    vtx = random_vertices(r)
    idx = [r.randrange(0, 40) for _ in range(INDEX_ENTRIES)]
    this = {2: mask, 6: stride}
    return dict(this=this, records=records, renderer=renderer, disp=disp, sys=random_sys(r), queues=queues, vtx=vtx, idx=idx)


def execute_long_case(r):
    s = execute_state(r, r.random() < 0.6)
    return mk([(OP_EXECUTE, [])], **s)


def execute_long_edges():
    for flags in (0, 0x40, 0x20, 0x400, 0x200, 0x10, 0x25, 0x60, 0x460):
        for mode in (0, 0x800, 0x1000, 0x3000, 0x2000, 0x180000, 0x1800):
            for renderer in (1, 2, 3, 4, 0, 5, -1):
                for result in (0, 1):
                    yield mk([(OP_EXECUTE, [])],
                             records={1: [flags, mode, 0x7000, JUNK, JUNK, 4, 0x1c4, 5, 9, 0, 0, fb(2.0), MASK]},
                             renderer=renderer, disp=0x4000, queues={"Execute": [result], "LoadInfo": [SCRATCH], "Fog": [1], "Project": [fb(0.25)]},
                             idx=[(i * 7) % 5 for i in range(INDEX_ENTRIES)])
    # a record with a single face, depths that cancel, equal keys and negative zero
    for faces in (1, 2, 3, 4, 5, 6, 8, 9, 21):
        yield mk([(OP_EXECUTE, [])], records={1: [0, 0, 0, JUNK, JUNK, 4, 0x1c4, 9, faces * 3, 0, 0, fb(1.0), MASK]},
                 queues={"LoadInfo": [SCRATCH]}, idx=[(i * 5) % 9 for i in range(INDEX_ENTRIES)])
    for count in (0, -1, 1, 0x7fffffff, 0x80000000):
        for depth in (1.0, 0.0, -3.5, float("inf"), 1e38):
            yield mk([(OP_EXECUTE, [])], records={1: {7: count, 11: fb(depth), 1: 0x1000}}, queues={"LoadInfo": [SCRATCH]})
    for stride in (0x18, 0x20, 0x28, 0x30):
        for vdata in (0, 8, 0x100):
            yield mk([(OP_EXECUTE, [])], this={6: stride}, records={1: {9: vdata, 8: 18, 7: 12}},
                     queues={"LoadInfo": [SCRATCH]}, idx=[(i * 11) % 12 for i in range(INDEX_ENTRIES)])
    for planted in (0x7fc00000, 0x7f800000, 0xff800000, 0x80000000, 0x00000001):
        words = list(DEFAULT_VERTICES)
        for v in range(0, 40):
            words[(0x100 + v * 0x20 + 8) // 4] = planted if v % 3 == 0 else fb(1.0 + v)
        yield mk([(OP_EXECUTE, [])], records={1: {9: 0x100, 8: 12, 7: 12}}, queues={"LoadInfo": [SCRATCH]}, vtx=words,
                 idx=[(i * 3) % 12 for i in range(INDEX_ENTRIES)])
    yield mk([(OP_EXECUTE, [])], queues={"Execute": [1], "ExecuteMove": [3]}, records={1: {0: 0x200}, 3: {0: 0x40}})
    yield mk([(OP_EXECUTE, [])], records={1: {0: 0x20}},
             queues={"Project": [fb(1.0)], "ProjectPoke": [fb(8.0)], "LoadInfo": [SCRATCH]})


def execute_short_case(r):
    s = execute_state(r, r.random() < 0.6)
    depth = r.choice([fb(-1.0), fb(-1.0), 0, fb(2.5), random_float_word(r), 0x7fc00000, 0xbf800001])
    s["this"][7] = r.choice([1, 1, 1, 1, 0])
    if r.random() < 0.2:
        s["records"][1][7] = 0
    if r.random() < 0.2:
        s["records"][1][8] = 0
    return mk([(OP_EXECUTE_DEPTH, [depth])], **s)


def execute_short_edges():
    base = [0, 0, 0x7000, JUNK, JUNK, 4, 0x1c4, 5, 9, 0x20, 0x10, fb(2.0), MASK]
    for depth in (fb(-1.0), fb(1.0), 0, 0x80000000, 0x7fc00000, 0xbf800001, 0xbf7fffff, fb(-1.0000001)):
        for stride in (0x18, 0x20, 0x30):
            yield mk([(OP_EXECUTE_DEPTH, [depth])], this={6: stride}, records={1: base}, queues={"LoadInfo": [SCRATCH]})
    for vcount, icount in ((0, 9), (5, 0), (0, 0), (-1, 9), (5, -1), (0x40000000, 2), (3, 0x40000001), (0x7fffffff, 3)):
        # mode 0x1000 is an instant one: the faces of a huge index count are not sorted
        yield mk([(OP_EXECUTE_DEPTH, [fb(3.0)])], records={1: {7: vcount, 8: icount, 1: 0x1000}}, queues={"LoadInfo": [SCRATCH]})
    yield mk([(OP_EXECUTE_DEPTH, [fb(3.0)])], this={7: 0})
    yield mk([(OP_EXECUTE_DEPTH, [fb(3.0)])], this={7: 0}, records={1: base})
    yield mk([(OP_EXECUTE_DEPTH, [fb(3.0)]), (OP_EXECUTE_DEPTH, [fb(4.0)])], records={1: base}, queues={"LoadInfo": [SCRATCH]})


# ----------------------------------------------------------------- AddClipPolyList


def script(*calls):
    """The clipper's stream for ClipPoly calls: for each (n, added, entries) the polygon's vertex
    count, the vertices the clip adds, and the n entries of the polygon."""
    out = []
    for n, added, entries in calls:
        out += [n, added] + list(entries)
    return out


def random_script(r, calls):
    out = []
    for _ in range(calls):
        n = r.choice([0, 1, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 19, 20])
        out += [n, r.choice([0, 0, 1, 2, 3])] + [r.randrange(0, 40) for _ in range(n)]
    return out


def add_clip_case(r):
    """A list of triangles over `vertices` vertices with random outside codes and clip scripts."""
    vertices = r.choice([0, 1, 3, 4, 6, 10, 12, 20, 30, 30])
    triangles = r.choice([0, 1, 2, 3, 5, 8, 12, 20, 20])
    count = triangles * 3 + r.choice([0, 0, 0, 1, 2])
    stride = r.choice([0x18, 0x20, 0x28, 0x30])
    mask = r.choice([0, 0, 0xff, 0x0f, 0x03, 0x01, 0x10, 0xf0, 0x3f, r.getrandbits(32)])
    code_set = r.choice([[0], [0, 0, 0, 1, 2, 4], [0, 1, 3, 5, 7, 0x3f], list(range(64)), [0, 0x10, 0x20, 0x30], [0, 0xff], [0, 3, 12]])
    codes = [r.choice(code_set) for _ in range(vertices)]
    stream = codes + random_script(r, 24)
    inp = [r.randrange(0, max(vertices, 1)) for _ in range(INPUT_ENTRIES)]
    offset = r.choice([0, 0, 1, 5])
    prim_vertices = r.choice([0, 0, 0, 1, 7])
    prim_indices = r.choice([0, 0, 0, 2, 9])
    vdata = r.choice([0, 0, 0x20, 0x100])
    idata = r.choice([0, 0, 2, 0x30])
    needed_v = vdata + stride * (prim_vertices + vertices)
    needed_i = idata + 2 * (prim_indices + count)
    if r.random() < 0.7:
        vend = needed_v + 0x140 + r.choice([0, 0x20, 0x100, 0x400, 0x1000]) + 0x20 * count
        iend = needed_i + 0x18 + r.choice([0, 6, 0x40, 0x100, 0x300, 0x400])
    else:
        vend = r.choice([needed_v + d for d in (-stride, -1, 0, 1, 0x100, 0x13f, 0x140, 0x141, 0x180, 0x400)] + [0x2400, 0, 0x0fffffff])
        iend = r.choice([needed_i + d for d in (-2, -1, 0, 1, 0x10, 0x17, 0x18, 0x19, 0x30, 0x100)] + [0x580, 0, 0x300])
    iend = min(iend, 0x5a0)
    current = 0 if r.random() < 0.05 else 1
    return mk([(OP_ADD_CLIP, [vertices, offset, count, mask])],
              this={6: stride, 7: current, 9: vend & MASK, 10: iend & MASK},
              records={1: {7: prim_vertices, 8: prim_indices, 9: vdata, 10: idata}}, inp=inp, stream=stream)


def add_clip_edges():
    # nothing to clip, nothing in the list, a primitive that is not there
    yield mk([(OP_ADD_CLIP, [0, 0, 0, 0])])
    yield mk([(OP_ADD_CLIP, [3, 0, 0, 0])], stream=[0, 0, 0])
    yield mk([(OP_ADD_CLIP, [3, 0, 3, 0])], this={7: 0}, stream=[0, 0, 0])
    yield mk([(OP_ADD_CLIP, [3, 0, 3, 0])], this={7: 0, 9: 0, 10: 0}, stream=[0, 0, 0])
    yield mk([(OP_ADD_CLIP, [3, 0, 3, 0])], this={7: 0, 9: 0x2000, 10: 0x600}, stream=[0, 0, 0])
    inp = [0, 1, 2] * 40
    # all inside: the triangles are copied; AND of the codes zero; OR zero
    for count in (3, 6, 9, 30, 4, 5, 1, 2):
        yield mk([(OP_ADD_CLIP, [3, 0, count, 0])], stream=[0, 0, 0], inp=inp)
    # every vertex outside one plane: nothing is added (the AND of the codes)
    yield mk([(OP_ADD_CLIP, [3, 0, 3, 0])], stream=[1, 1, 1], inp=inp)
    yield mk([(OP_ADD_CLIP, [3, 0, 3, 0])], stream=[0xff, 0xff, 0xff], inp=inp)
    # a triangle whose three vertices share a plane is dropped, the others are kept (mask hides it)
    inp_two = [0, 1, 2, 3, 4, 5] * 20
    for mask in (0, 1, 2, 0xff, 0xfe):
        yield mk([(OP_ADD_CLIP, [6, 0, 6, mask])], stream=[1, 1, 1, 0, 2, 4], inp=inp_two)
        yield mk([(OP_ADD_CLIP, [6, 0, 6, mask])], stream=[0, 2, 4, 3, 3, 3], inp=inp_two)
    # the AND is zero but the OR is not, so the triangle is clipped into n vertices
    for n in range(0, 13):
        yield mk([(OP_ADD_CLIP, [3, 0, 3, 0])], stream=[1, 2, 0] + script((n, 1, range(11, 11 + n))), inp=inp)
        yield mk([(OP_ADD_CLIP, [3, 0, 9, 0])], stream=[1, 2, 0] + script(*[(n, 1, range(11 + k, 11 + k + n)) for k in range(3)]), inp=inp)
    for n in (16, 19, 20):
        yield mk([(OP_ADD_CLIP, [3, 0, 3, 0])], stream=[1, 2, 0] + script((n, 2, range(30, 30 + n))), inp=inp)
        yield mk([(OP_ADD_CLIP, [3, 0, 6, 0])], stream=[1, 2, 0] + script((n, 2, range(30, 30 + n)), (3, 0, [4, 5, 6])), inp=inp)
    # the mask hides a plane: the same codes, clipped or dropped or untouched
    for mask in (0, 1, 2, 3, 0xff, 0xfe):
        yield mk([(OP_ADD_CLIP, [3, 0, 6, mask])], stream=[1, 3, 2] + script((4, 1, [4, 5, 6, 7]), (5, 0, [4, 5, 6, 7, 8])), inp=inp)
    # room: the index area, the vertex area, exactly enough, one short
    for iend in (0x0c, 0x17, 0x18, 0x19, 0x1e, 0x20, 0x30, 0x40, 0x5a0):
        yield mk([(OP_ADD_CLIP, [3, 0, 12, 0])], this={10: iend}, stream=[0, 0, 0], inp=inp)
        yield mk([(OP_ADD_CLIP, [3, 0, 12, 0])], this={10: iend}, records={1: {10: 4}}, stream=[0, 0, 0], inp=inp)
    for vend in (0x5f, 0x60, 0x61, 0x13f, 0x140, 0x1a0, 0x1a1, 0x1c0, 0x1c1, 0x2000):
        yield mk([(OP_ADD_CLIP, [3, 0, 12, 0])], this={9: vend}, stream=[0, 0, 0], inp=inp)
        yield mk([(OP_ADD_CLIP, [3, 0, 12, 0])], this={9: vend}, records={1: {7: 2, 9: 0x40}}, stream=[0, 0, 0], inp=inp)
    # vertices grow while clipping and use up the room for the next triangle
    yield mk([(OP_ADD_CLIP, [3, 0, 12, 0])], this={9: 0x1c0}, stream=[1, 2, 0] + script(*[(4, 2, [11, 12, 13, 14])] * 8), inp=inp)
    # a clip that adds no vertices, and one that adds several
    for added in (0, 1, 5, -1):
        yield mk([(OP_ADD_CLIP, [3, 0, 6, 0])], stream=[1, 2, 0] + script((4, added, [1, 2, 0, 1]), (3, added, [2, 1, 0])), inp=inp)
    # a polygon of fewer than three vertices takes the triangle out again
    yield mk([(OP_ADD_CLIP, [3, 0, 9, 0])], stream=[1, 2, 0] + script((0, 0, []), (1, 0, [5]), (2, 0, [5, 6])), inp=inp)
    # a negative count (the room test wraps the way the Mac's does while it stays in range)
    yield mk([(OP_ADD_CLIP, [-1, 0, 3, 0])], stream=[], inp=inp)
    yield mk([(OP_ADD_CLIP, [3, 0, -3, 0])], stream=[0, 0, 0], inp=inp)
    yield mk([(OP_ADD_CLIP, [3, 0, 100000, 0])], stream=[0, 0, 0], inp=inp)
    yield mk([(OP_ADD_CLIP, [100000, 0, 3, 0])], stream=[0, 0, 0], inp=inp)
    # an index count that is not a multiple of three reads a whole triple anyway
    for count in (1, 2, 4, 5, 7):
        yield mk([(OP_ADD_CLIP, [3, 0, count, 0])], stream=[0, 0, 0], inp=inp)
    # two calls in a row
    yield mk([(OP_ADD_CLIP, [3, 0, 6, 0]), (OP_ADD_CLIP, [3, 0, 6, 0])], stream=[0, 0, 0, 0, 0, 0], inp=inp)


# ----------------------------------------------------------------- sequences


def sequence_case(r):
    """One or two primitives: made, clipped into, counted, finished and executed, over one table."""
    ops = []
    # LoadInfo answers record 2 once and then the scratch record (ExecuteCurrentPrimitive's
    # work array), whichever comes first; the second primitive is a copy of the first.
    queues = {"LoadInfo": [2, SCRATCH], "CopyVDB": [3, 4, 5], "VertexEnd": [0x2000], "IndexEnd": [0x500],
              "Execute": [r.choice([1, 1, 0])], "Fog": [r.choice([0, 1])], "Project": [random_float_word(r)]}
    vertices = r.choice([3, 6, 9])
    for number in range(r.choice([1, 2])):
        make = OP_GET_NEXT if number == 0 else OP_DUPLICATE
        ops.append((make, [r.choice([2, 3, 7]), 4, r.choice(FORMATS), r.choice([0, 0x1000, 0x3000]), 0x1234]))
        if make == OP_GET_NEXT:
            triangles = r.choice([1, 2, 3])
            ops.append((OP_ADD_CLIP, [vertices, 0, triangles * 3, r.choice([0, 0, 1])]))
            ops.append((OP_SET_COUNTS, [vertices, triangles * 3]))
        for step in (OP_CHECK_FOG, OP_PROJECT, OP_INSTANT):
            if r.random() < 0.4:
                ops.append((step, [1] if step != OP_INSTANT else [2]))
        if r.random() < 0.3:
            ops.append((OP_GLOBALS, [r.getrandbits(32), r.choice([1, 3, 4]), r.choice([0x6000, 0])]))
        ops.append(r.choice([(OP_EXECUTE_DEPTH, [r.choice([fb(-1.0), fb(5.0)])]), (OP_EXECUTE, [])]))
    stream = [r.choice([0, 0, 1, 2]) for _ in range(vertices)] + [r.choice([3, 4, 5]), r.choice([0, 1, 2]), 1, 2, 3, 4, 5, 6] * 6
    inp = [r.randrange(0, vertices) for _ in range(INPUT_ENTRIES)]
    return mk(ops, this={7: 0}, renderer=r.choice([1, 3]), disp=r.choice([0x6000, 0]), sys=random_sys(r), vtx=random_vertices(r),
              idx=[r.randrange(0, 12) for _ in range(INDEX_ENTRIES)], inp=inp, stream=stream, queues=queues)


def sequence_edges():
    for fmt in FORMATS:
        yield mk([(OP_GET_NEXT, [2, 4, fmt, 0x3000, 0x1234]), (OP_ADD_CLIP, [3, 0, 3, 0]), (OP_SET_COUNTS, [3, 3]),
                  (OP_EXECUTE_DEPTH, [fb(-1.0)])],
                 this={7: 0}, stream=[0, 0, 0], inp=[0, 1, 2] * 40,
                 queues={"LoadInfo": [2, SCRATCH], "VertexEnd": [0x2000], "IndexEnd": [0x500]})
        yield mk([(OP_GET_NEXT, [2, 4, fmt, 0, 0x1234]), (OP_ADD_CLIP, [3, 0, 3, 0]), (OP_SET_COUNTS, [3, 3]),
                  (OP_EXECUTE_DEPTH, [fb(7.0)])],
                 this={7: 0}, stream=[0, 0, 0], inp=[0, 1, 2] * 40, renderer=3,
                 queues={"LoadInfo": [2, SCRATCH], "VertexEnd": [0x2000], "IndexEnd": [0x500]})


TESTS = [
    FnTest("CDrawPrimitive::GetNextPrimitive", ppc, host, edge=header_edges(OP_GET_NEXT), rnd=lambda r: make_header_case(r, OP_GET_NEXT)),
    FnTest("CDrawPrimitive::DuplicateCurrentPrimitive", ppc, host, edge=header_edges(OP_DUPLICATE),
           rnd=lambda r: make_header_case(r, OP_DUPLICATE)),
    FnTest("CDrawPrimitive::ExternExecute", ppc, host, edge=extern_edges, rnd=extern_case),
    FnTest("CDrawPrimitive::ExecuteCurrentPrimitive()", ppc, host, edge=execute_long_edges, rnd=execute_long_case),
    FnTest("CDrawPrimitive::ExecuteCurrentPrimitive(float)", ppc, host, edge=execute_short_edges, rnd=execute_short_case),
    FnTest("CDrawPrimitive::ExecutePrimitiveIfInstant", ppc, host, edge=instant_edges, rnd=instant_case),
    FnTest("CDrawPrimitive::AddClipPolyList", ppc, host, edge=add_clip_edges, rnd=add_clip_case),
    FnTest("CDrawPrimitive::ApplyFog", ppc, host, edge=apply_fog_edges, rnd=apply_fog_case),
    FnTest("CDrawPrimitive::CheckFogOnCurrentPrimitive", ppc, host, edge=check_fog_edges, rnd=check_fog_case),
    FnTest("CDrawPrimitive::ProjectCurrentPrimitive", ppc, host, edge=project_edges, rnd=project_case),
    FnTest("CDrawPrimitive::RoundOffPrimitive", ppc, host, edge=round_off_edges, rnd=round_off_case),
    FnTest("Primitive buffer sequences", ppc, host, edge=sequence_edges, rnd=sequence_case),
]
