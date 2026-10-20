"""Map list membership, node ordering/exhaustion and render-window setup versus PPC."""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest
PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_MapSetup"
N = 32
FN = {1: 0x10017b48, 2: 0x10017c58, 3: 0x10017d38, 4: 0x10017dd4,
      5: 0x10017f10, 6: 0x100180b4, 7: 0x100181e8, 11: 0x10019640, 12: 0x10017984}
SYSTEM, MALLOC, VIEW = 0x10165f80, 0x10051f70, 0x100b0078
MASK = 0xffffffff
def ws(e, a, values): e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))
def rs(e, a, n): return list(struct.unpack(">" + "I" * n, e.read(a, n * 4)))
def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if not hasattr(e, "ms_type"):
        e.ms_type = T.fake_function(e, lambda em: em.ms.type(em))
        e.ms_data = T.fake_function(e, lambda em: em.ms.data(em))
        e.ms_malloc = T.fake_function(e, lambda em: em.ms.malloc(em))

class Side:
    def __init__(self, e, case):
        self.e = e; e.ms = self
        self.obj = e.alloc(0x7c, b"\xa5" * 0x7c)
        self.nodes = e.alloc(N * 8, b"\xa5" * (N * 8))
        self.cells = e.alloc(24)
        self.links = [e.alloc(8) for _ in range(4)]
        self.headers = [e.alloc(0xb8) for _ in range(4)]
        self.types = [i % 3 for i in range(4)]; self.has_data = [True] * 4
        vt = e.alloc(24); ws(e, vt + 16, [e.ms_type, e.ms_data])
        for link in self.links: ws(e, link, [vt])
        for i in range(N): ws(e, self.nodes + i * 8 + 4, [self.nodes + (i + 1) * 8 if i + 1 < N else 0])
        ws(e, self.obj, [self.nodes, self.nodes, N, 0, self.cells] + [0] * 7)
        ws(e, self.obj + 0x38, [3, 2])
        ws(e, self.obj + 0x70, [case["windows"]])
        self.dummies = [e.alloc(24) for _ in range(4)]
        ws(e, self.obj + 0x60, [self.obj + 0x48 if i == case["own"] else self.dummies[i] for i in range(4)])
        self.saved = {a: e.read(a, 4) for a in (SYSTEM, MALLOC, VIEW)}
        self.view = e.alloc(64)
        ws(e, SYSTEM, [case["flags"]]); ws(e, VIEW, [self.view]); ws(e, MALLOC, [e.ms_malloc])
        self.fail = False
        e.cl = self.log = CL.PpcLog({})
        self.st = CL.Stubs(); self.st.real.update([*FN.values(), 0x10017808])
        self.st.internal(0x10017a74, lambda em: (em.cl.rec("Close"), ws(em, em.r(3) + 0x10, [0]), em.set_r(3, 1)))
        self.st.internal(0x10008904, CL.recorder("Error", "xii"))
        self.old = self.st.activate(e)
    def type(self, e):
        i = self.links.index(e.r(3)); self.log.rec("Type", [i + 1]); e.set_r(3, self.types[i])
    def data(self, e):
        i = self.links.index(e.r(3)); self.log.rec("Data", [i + 1]); e.set_r(3, self.headers[i] if self.has_data[i] else 0)
    def malloc(self, e):
        self.log.rec("Malloc", [e.r(3) // 4]); e.set_r(3, 0 if self.fail else e.alloc(e.r(3)))
    def op(self, op, a):
        e, o = self.e, self.obj
        if op in (1, 2): args = [o, self.links[a[0]], a[1]]
        elif op == 4: args = [o, *a[1:5], self.links[a[0]], 0]
        elif op == 5: args = [o, *a[1:5], self.links[a[0]]]
        elif op == 8: ws(e, SYSTEM, [a[0]]); return 0
        elif op == 9:
            self.types[a[0]] = a[1]; self.has_data[a[0]] = a[2]
            e.write(self.headers[a[0]] + 0xa8, bytes(v & 255 for v in a[3:7])); return 0
        elif op == 10: ws(e, o + 4, [0 if a[0] < 0 else self.nodes + a[0] * 8]); return 0
        elif op == 13: ws(e, o + 16, [0]); return 0
        elif op == 12: self.fail = a[2]; args = [o, *a[:2]]
        else: args = [o, *a]
        result = T.call_ppc(e, FN[op], [x & MASK for x in args])[0]
        return T.s32(result) if op != 11 else 0
    def node(self, p):
        return -1 if p == 0 else (p - self.nodes) // 8 if self.nodes <= p < self.nodes + N * 8 else -2
    def state(self):
        e, o = self.e, self.obj
        width, height = rs(e, o + 0x38, 2); cells = e.u32(o + 16)
        result = [e.u32(SYSTEM), e.u32(o + 12), self.node(e.u32(o + 4))]
        result += [self.node(w) for w in rs(e, o + 0x14, 7)] + [width, height, int(cells != 0)]
        if cells: result += [self.node(w) for w in rs(e, cells, width * height)]
        for i in range(N):
            link, nxt = rs(e, self.nodes + i * 8, 2)
            result += [self.links.index(link) + 1 if link in self.links else 0 if link == 0 else -1, self.node(nxt)]
        result += rs(e, o + 0x70, 1) + rs(e, o + 0x48, 5) + [1 if e.u32(o + 0x5c) == self.view else -1]
        result += [1 if w == o + 0x48 else 10 + i for i, w in enumerate(rs(e, o + 0x60, 4))]
        return [T.s32(v & MASK) for v in result]
    def close(self):
        CL.Stubs.deactivate(self.e, self.old)
        for a, b in self.saved.items(): self.e.write(a, bytes(b))
def ppc(e, case):
    side = Side(e, case)
    try:
        states = [side.state()]; results = []
        for op, a in case["ops"]: results.append(side.op(op, a)); states.append(side.state())
        return dict(states=states, results=results, log=side.log.log)
    finally: side.close()
def host(lib, case):
    CL.host_begin(lib, {})
    begin, opfn, statefn = lib.tpw_ms_begin, lib.tpw_ms_op, lib.tpw_ms_state
    begin.argtypes = [C.c_uint32, C.c_int32, C.c_int32]; begin.restype = None
    opfn.argtypes = [C.c_int32, C.POINTER(C.c_int32)]; opfn.restype = C.c_int32
    statefn.argtypes = [C.POINTER(C.c_int32)]; statefn.restype = C.c_int32
    out = (C.c_int32 * 256)()
    begin(case["flags"], case["windows"], case["own"])
    states = [list(out[:statefn(out)])]; results = []
    for op, a in case["ops"]:
        args = (C.c_int32 * max(7, len(a)))(*[T.s32(v & MASK) for v in a])
        results.append(opfn(op, args)); states.append(list(out[:statefn(out)]))
    return dict(states=states, results=results, log=CL.host_end(lib))
def case(ops, windows=0, own=-1, flags=0): return dict(ops=ops, windows=windows, own=own, flags=flags)
def edges():
    for n in range(5):
        for own in (-1, 0, 2): yield case([(11, []), (11, [])], windows=n, own=own)
    for fail in (0, 1): yield case([(12, [4, 3, fail])])
    yield case([(13, []), (12, [1, 2, 0]), (12, [2, 1, 0])])
    yield case([(1, [i, 0x70]) for i in (0, 1, 2, 3, 1)] + [(2, [i, 0x70]) for i in (0, 2, 3, 1, 1)])
    yield case([(1, [1, 0x10])] * 34 + [(7, [])])
    yield case([(10, [-1]), (1, [1, 0x10]), (4, [1, 0, 0, 2, 1])])
    for lst in (0, 1, 2, 0x10, 0x20, 0x70, 0x71):
        yield case([(1, [i, lst]) for i in range(4)] + [(3, [lst]), (7, [])])
    for rect in ((-1, -1, 5, 5), (0, 0, 0, 0), (1, 1, 0, 0), (3, 0, 4, 1), (0, 0, -1, 1), (1, -3, 2, 0)):
        yield case([(4, [1, *rect]), (4, [2, *rect]), (5, [1, *rect]), (6, list(rect)), (7, [])])
    for has in (0, 1):
        yield case([(9, [2, 2, has, 1, 0, 2, -1]), (1, [2, 1]), (7, [])])
    yield case([(13, []), (5, [0, -1, -1, 8, 8])])
def rnd(r):
    ops = []
    for _ in range(r.randrange(1, 16)):
        op = r.choice([1, 2, 3, 4, 5, 6, 7, 8])
        a = []
        if op in (1, 2): a = [r.randrange(4), r.choice([0, 2, 0x10, 0x20, 0x70])]
        if op == 3: a = [r.choice([0, 0x10, 0x20, 0x70])]
        if op in (4, 5): a = [r.randrange(4)] + [r.randrange(-2, 5) for _ in range(4)]
        if op == 6: a = [r.randrange(-2, 5) for _ in range(4)]
        if op == 8: a = [r.getrandbits(32)]
        ops.append((op, a))
    return case(ops + [(7, [])], flags=r.getrandbits(32))

def canon(w): return 0x7fc00000 if w & 0x7f800000 == 0x7f800000 and w & 0x007fffff else w
def fb(v): return struct.unpack(">I", struct.pack(">f", v))[0]
def bf(w): return struct.unpack(">f", struct.pack(">I", w))[0]
def gamut_ppc(e, case):
    obj = e.alloc(0x2d8, b"\xa5" * 0x2d8); saved = e.read(0x10165638, 4)
    st = CL.Stubs(); st.real.add(0x10009abc); old = st.activate(e)
    try:
        ws(e, 0x10165638, [case[1]])
        T.call_ppc(e, 0x10009abc, [obj, T.F(bf(case[0]))])
        return [canon(v) if i < 12 else v for i, v in enumerate(rs(e, obj, 0x2d8 // 4))]
    finally: e.write(0x10165638, bytes(saved)); CL.Stubs.deactivate(e, old)
def gamut_host(lib, case):
    fn = lib.tpw_ms_gamut; fn.argtypes = [C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32)]; fn.restype = None
    out = (C.c_uint32 * (0x2d8 // 4))(); fn(*case, out)
    return [canon(v) if i < 12 else v for i, v in enumerate(out)]
def gamut_edges():
    for fov in (0., -0., 45., 90., 115., 180., -90., 360., 1e-7):
        for aspect in (0., -0., 0.75, 1., 9 / 16, -2.): yield fb(fov), fb(aspect)
TESTS = [FnTest("CMapWho lists and render window", ppc, host, edge=edges, rnd=rnd),
         FnTest("CGamut field-of-view corners", gamut_ppc, gamut_host, edge=gamut_edges,
                rnd=lambda r: (fb(r.uniform(-300, 300)), fb(r.uniform(0.1, 2))))]
