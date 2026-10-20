"""Sky shadow pass, shadow grid line and procedural texture loader versus the Mac originals.

The draw harness reuses HFieldExecute's primitive memory, clip fake and case helpers. The
real shadow line runs on both sides. Recording Transform and CPP_TestData fakes dump its
inputs and supply scripted transformed rows. Direct helper cases poison and compare the
entire arrays, including padding and untouched w words. Loader fakes record allocation,
logging, construction, destruction, initialization, loading and error calls.
Fused instructions use the same single-rounding correction as HFieldExecute.
"""
import contextlib
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
import test_HFieldExecute as X
import test_HFieldHelpers as H

PROGRAM = 'engine symbols shared'
SHIM_LIBRARY = 'unit_HFieldShadow'
DRAW, LINE, LOAD = 0x10011180, 0x10010e7c, 0x1000d96c
SKY_ENTRY, SKY_POSITION, BLEND = 0x1002b554, 0x1002b584, 0x100410a4
SKY, SETTINGS, LOGGER, ERROR = 0x101661ec, 0x10165fc8, 0x102b36cc, 0x10052b44
UINIT, VINIT, USCALE, VSCALE = 0x10052dc4, 0x10052dbc, 0x10052dc8, 0x10052dc0
LMATH, LSKY, LPROC, LOLD, LLOGGER, LERROR = 100, 101, 102, 103, 104, 105
ws, rs, fb, canon, array = X.ws, X.rs, X.fb, X.canon, X.array
MASK = X.MASK
GUARDS = [(X.DISPLAY, 0x93c), (X.SYSTEM, 0x20), (SETTINGS, 0x20), (LOGGER, 4), (SKY, 0x2f14),
          (H.MATH_VAR, 4), (H.FINAL_VAR, 4), (H.CLIP_VAR, 4), (X.DRAW_VAR, 4),
          (X.FACE[-1], 0x24), (VINIT, 16)]


@contextlib.contextmanager
def guarded(e):
    data = [(a, e.read(a, n)) for a, n in GUARDS + [(e.data_import('gLogger__13SamsUtilities'), 4)]]
    try:
        yield
    finally:
        for a, b in data:
            e.write(a, b)


class Rounding(X.HardwareRounding):
    def __init__(self, e):
        old_fn, old_size = X.EXECUTE_FN, X.EXECUTE_SIZE
        X.EXECUTE_FN, X.EXECUTE_SIZE = LINE, 712
        super().__init__(e)
        first = self.sites.copy()
        X.EXECUTE_FN, X.EXECUTE_SIZE = DRAW, 5488
        super().__init__(e)
        self.sites.update(first)
        X.EXECUTE_FN, X.EXECUTE_SIZE = old_fn, old_size


def setup(runner):
    T.install_std_stubs(runner.emu)
    e = runner.emu
    vtable = T.persist_alloc(e, 0x40)
    e.hs_math = T.persist_alloc(e, 0x10)
    ws(e, e.hs_math + 4, [vtable])
    ws(e, vtable + 0x10, [T.fake_function(e, lambda em: em.hs_side.transform(em))])


class MathFakes:
    """Common math fakes; direct cases mix input bits and drawing cases supply scripted rows."""
    def transform(self, e):
        L = self.L
        out, source = e.r(5), e.r(6)
        os, ins, count, kind = [T.s32(e.r(i)) for i in range(7, 11)]
        base = self.vbase if self.direct else out
        self.last_transform = out
        L.rec('Transform', (L.lab(e.r(3)), L.lab(e.r(4)), out - base, source - base, os, ins, count, kind))
        for i in range(count):
            words = [canon(x) for x in rs(e, source + i * ins, 4)]
            self.dump.extend(words[:4 if self.direct else 3])
            value = H.mix_transform(words, i) if self.direct else self.case['lines'][self.lines][1][4 * i:4 * i + 4]
            ws(e, out + i * os, value)

    def test_data(self, e):
        L = self.L
        codes, points = e.r(5), e.r(6)
        count, stride, mask = T.s32(e.r(7)), T.s32(e.r(8)), e.r(9)
        base = self.vbase if self.direct else self.last_transform
        L.rec('CPP_TestData', (L.lab(e.r(4)), codes - self.cbase if self.direct else 0,
                              points - base, count, stride, mask))
        for i in range(count):
            words = [canon(x) for x in rs(e, points + i * stride, 4)]
            self.dump.extend(words)
            code = H.mix_code(words, mask) if self.direct else self.case['lines'][self.lines][0][i]
            e.write(codes + i, bytes([code]))
        e.write(e.r(3), b'\x12\x34\x56\x78')
        if not self.direct:
            self.lines += 1


class DrawSide(X.PpcSide, MathFakes):
    def __init__(self, e, c):
        super().__init__(e, c)
        self.direct = False
        grid(e, self.field, e.u32(self.field), c)
        self.L.labels.update({SKY: LSKY, e.hs_math: LMATH})
        ws(e, H.MATH_VAR, [e.hs_math])
        ws(e, H.FINAL_VAR, [X.LIGHT_MATRIX])
        ws(e, SKY + 0x1fa0, c['sky'])
        ws(e, SKY + 0x1fd0, [self.entries[c['entry']]])
        ws(e, self.field + 0x2958, [0 if c['retained'] < 0 else self.records[c['retained']][0]])
        ws(e, self.field + 0x295c, c['previous'])
        ws(e, X.DISPLAY + 0x8fc, [c['colour']])
        e.write(UINIT, bytes([c['scale'][0]]))
        e.write(VINIT, bytes([c['scale'][1]]))
        ws(e, USCALE, [c['scale'][2]])
        ws(e, VSCALE, [c['scale'][3]])
        ws(e, self.draw + 0x24, [self.vertices + c['vlimit'] * 32, self.records[0][1] + c['ilimit'] * 2])
        self.before = e.read(self.field, X.FIELD_SIZE), e.read(e.u32(self.field), 0x30)
        e.hs_side = self

    def stubs(self):
        st = CL.Stubs()
        st.real = {DRAW, LINE}
        st.internal(H.CPP_TEST_DATA, self.test_data)
        st.internal(SKY_ENTRY, CL.recorder('GetSkyTEntry', 'L', ret=lambda e, L: self.entries[self.case['entry']]))
        st.internal(SKY_POSITION, CL.recorder('GetPosition', 'L', ret=lambda e, L: SKY + 0x1fa0))
        st.internal(BLEND, CL.recorder('GetBlendingMode', 'L', ret=lambda e, L: L.ret('Blend', 0)))
        st.internal(X.GET_NEXT, lambda e: self.hand_out(e, 'GetNext', 'GetNextPrimitive', 0))
        st.internal(X.EXECUTE_PRIMITIVE, self.execute)
        st.internal(X.CLIP_POLY, self.clip_poly)
        return st

    def state(self):
        e = self.e
        after = e.read(self.field, X.FIELD_SIZE)
        unchanged = int(self.before[0][:0x2958] == after[:0x2958] and
                        self.before[0][0x2968:] == after[0x2968:] and
                        self.before[1] == e.read(e.u32(self.field), 0x30))
        return [self.L.lab(e.u32(self.field + 0x2958)) & MASK] + rs(e, self.field + 0x295c, 3) + [
            e.read(UINIT, 1)[0], e.read(VINIT, 1)[0], e.u32(USCALE), e.u32(VSCALE)] + [e.u32(a) for a in X.FACE] + [unchanged]


def grid(e, field, record, c):
    heights = e.alloc(4 * len(c['heights']))
    adjusted = e.alloc(4 * len(c['adjusted']))
    ws(e, heights, c['heights'])
    ws(e, adjusted, c['adjusted'])
    ws(e, record + 8, c['record'])
    ws(e, record + 0x28, [heights + 4 * c['before']])
    ws(e, field + 8, [c['w']])
    ws(e, field + 0x12970, [adjusted + 4 * c['before'] if c['use_adjusted'] else 0])
    ws(e, X.DISPLAY + 0x938, [c['lift']])


def result(ret, log, dump, vertices, state):
    return {'ret': ret, 'log': log, 'dump': dump, 'vertices': X.vertex_bytes(vertices), 'state': state}


def ppc_draw(e, c):
    with guarded(e):
        side = DrawSide(e, c)
        st = side.stubs()
        saved = st.activate(e)
        try:
            with Rounding(e):
                ret = T.call_ppc(e, DRAW, [side.field])[0]
            for i in range(len(c['records'])):
                side.dump_record(i)
            return result(T.s32(ret), side.L.log, side.dump, rs(e, side.vertices, len(c['vertices'])), side.state())
        finally:
            st.deactivate(e, saved)
            e.hs_side = None


def bind(lib):
    lib.tpw_hx_pointer.restype = C.c_void_p
    lib.tpw_hx_pointer.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_hx_field.argtypes = [C.POINTER(C.c_int32), C.POINTER(C.c_int32), C.POINTER(C.c_uint32), C.c_int32,
                                C.c_int32, C.c_uint32, C.POINTER(C.c_uint16), C.c_int32]
    lib.tpw_hx_globals.argtypes = [C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32)]
    lib.tpw_hx_entry.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_hx_vertices.argtypes = [C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_hx_record.argtypes = [C.c_int32, C.POINTER(C.c_uint32), C.c_int32, C.POINTER(C.c_uint16), C.c_int32]
    lib.tpw_hx_lines.argtypes = [C.POINTER(C.c_uint8), C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_hx_clips.argtypes = [C.POINTER(C.c_int32), C.c_int32, C.c_uint32]
    lib.tpw_hs_grid.argtypes = [C.POINTER(C.c_uint32)] * 3 + [C.c_int32] * 4 + [C.c_uint32]
    lib.tpw_hs_shadow.argtypes = [C.c_int32] * 2 + [C.POINTER(C.c_uint32)] * 2 + [C.c_uint32,
                                C.POINTER(C.c_uint32), C.c_int32, C.c_int32]
    lib.tpw_hs_state.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_hs_line.argtypes = [C.POINTER(C.c_uint8), C.POINTER(C.c_uint32)] + [C.c_int32] * 5
    lib.tpw_hx_dump_n.restype = C.c_uint32
    lib.tpw_hx_dump_copy.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_hx_vertices_store.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_hs_load_setup.argtypes = [C.c_int32] * 4
    lib.tpw_hs_load_pointer.argtypes = [C.c_int32]
    lib.tpw_hs_load_pointer.restype = C.c_void_p
    lib.tpw_hs_load.argtypes = [C.c_char_p, C.c_int32, C.POINTER(C.c_int32)]


def labels(lib):
    p = lib.tpw_hx_pointer
    return {X.LABEL_FIELD: p(0, 0), X.LABEL_DRAW: p(1, 0), X.LABEL_MATRIX: p(3, 0), X.LABEL_CLIP: p(4, 0),
            LMATH: p(13, 0), LSKY: p(12, 0), LERROR: p(14, 0)} | {
            X.LABEL_PAGE + i: p(9, i) for i in range(1, X.N_PAGES + 1)} | {
            X.LABEL_ENTRY + i: p(10, i) for i in range(1, X.N_ENTRIES + 1)} | {
            X.LABEL_RECORD + i: p(11, i) for i in range(24)}


def dump(lib):
    n = lib.tpw_hx_dump_n()
    a = (C.c_uint32 * max(1, n))()
    lib.tpw_hx_dump_copy(a)
    return list(a)[:n]


def host_grid(lib, c):
    lib.tpw_hs_grid(array(C.c_uint32, c['record']), array(C.c_uint32, c['heights']),
                    array(C.c_uint32, c['adjusted']), len(c['heights']), c['w'], c['before'],
                    c['use_adjusted'], c['lift'])


def host_draw(lib, c):
    bind(lib)
    quads = [x for q in c['quads'] for x in q]
    lib.tpw_hx_field(array(C.c_int32, c['points']), array(C.c_int32, c['cells']),
                     array(C.c_uint32, []), 0, 0, c['row_length'], array(C.c_uint16, quads), len(c['quads']))
    display, system, _, _, face = c['g']
    lib.tpw_hx_globals(display, system, array(C.c_uint32, face))
    for label, (flags, page, place, f40) in c['entries'].items():
        lib.tpw_hx_entry(label, flags, page, array(C.c_uint32, place), T.s32(f40))
    lib.tpw_hx_vertices(array(C.c_uint32, c['vertices']), len(c['vertices']))
    for i, (header, offset, indices) in enumerate(c['records']):
        lib.tpw_hx_record(i, array(C.c_uint32, [x & MASK for x in header]), offset,
                          array(C.c_uint16, indices), len(indices))
    lib.tpw_hx_lines(array(C.c_uint8, [b for line in c['lines'] for b in line[0]]),
                     array(C.c_uint32, [w for line in c['lines'] for w in line[1]]), len(c['lines']))
    lib.tpw_hx_clips(array(C.c_int32, c['clips']), len(c['clips']), c['salt'])
    host_grid(lib, c)
    lib.tpw_hs_shadow(c['retained'], c['entry'], array(C.c_uint32, c['sky']), array(C.c_uint32, c['previous']),
                      c['colour'], array(C.c_uint32, c['scale']), c['vlimit'], c['ilimit'])
    CL.host_begin(lib, labels(lib), c['q'])
    ret = lib.tpw_hs_run()
    log = CL.host_end(lib)
    lib.tpw_hx_dump_records(len(c['records']))
    vertices = (C.c_uint32 * len(c['vertices']))()
    lib.tpw_hx_vertices_store(vertices)
    state = (C.c_uint32 * 18)()
    lib.tpw_hs_state(state)
    return result(ret, log, dump(lib), list(vertices), list(state))


def make_draw(seed, first=2, rows=2, width=5, code=None, flags=None, **kw):
    c, r = X.base(seed, first=first, rows=rows, width=width, n_records=1)
    # Fully populate each row to keep the real helper's unused stack words out of the comparison.
    c['points'][2:130] = [0] * 128
    c['points'][130:258] = [127] * 128
    if code is not None:
        c['lines'] = [X.flat_line(r, code) for _ in range(rows + 1)]
    if flags is not None:
        X.set_all_quads(c, flags)
    X.finish(c, r)
    # Shadow draws even flag-1 cells, so size for the complete visible cell rectangle.
    cells = len(X.visited(c))
    capacity = max(40, cells * max([4] + [2 * x for x in c['clips']]) + 20)
    c['vertices'] = [r.getrandbits(32) for _ in range(capacity * 8)]
    header, offset, indices = c['records'][0]
    c['records'] = [(header, 0, [r.getrandbits(16) for _ in range(capacity * 6 + 30)])]
    c.update({'w': 128, 'record': [H.rbits(r, -30, 30) for _ in range(4)], 'before': 8,
              'heights': [H.rbits(r, -10, 10) for _ in range((first + rows + 1) * 128 + 32)],
              'use_adjusted': 1, 'lift': H.rbits(r, -4, 4), 'retained': -1,
              'entry': 1, 'sky': [H.rbits(r, -2, 2) for _ in range(3)],
              'previous': [H.rbits(r, -2, 2) for _ in range(3)], 'colour': r.getrandbits(32),
              'scale': [r.choice([0, 1, 2, 255]), r.choice([0, 1, 2, 255]),
                        H.rbits(r, -3, 3), H.rbits(r, -3, 3)],
              'vlimit': capacity, 'ilimit': len(c['records'][0][2])})
    c['adjusted'] = [H.rbits(r, -10, 10) for _ in c['heights']]
    c['q']['Blend'] = [r.getrandbits(32)]
    c.update(kw)
    return c


def front_rows(count, clipped=False):
    """Every cell's two triangles face the viewer; alternating planes force clipping."""
    out = []
    for row in range(count):
        codes = [1 << (row & 1) if clipped else 0] * 128
        points = [value for column in range(128) for value in (fb(column), fb(-row), fb(0.5), fb(1))]
        out.append((codes, points, [0] * 128))
    return out


def draw_edges():
    out = [make_draw(1, use_adjusted=0), make_draw(2, entry=0), make_draw(3)]
    out[-1]['q']['GetNext'] = [-1]
    for display in (0, 0x2000, 0xffffffff):
        for flags in (0, 1, 2, 3, 0x8000, 0xffff):
            for code in (0, 1, 3):
                c = make_draw(4, first=0, code=code, flags=flags)
                X.set_g(c, display=display, system=0xc0000000)
                out.append(c)
    for rows in (0, -1):
        c = make_draw(5)
        c['cells'][1] = c['cells'][0] + rows
        out.append(c)
    for n in (0, 1, 2, 7, 8, 9, 15, 16, 17):
        c = make_draw(6, retained=0)
        X.set_g(c, system=0)
        header, offset, indices = c['records'][0]
        header[3] = n
        out.append(c)
    for vlimit in (15, 16, 17, 19, 20, 24, 40):
        out.append(make_draw(7, code=0, vlimit=vlimit))
    for ilimit in (23, 24, 25, 29, 30, 36):
        out.append(make_draw(8, code=0, ilimit=ilimit))
    for clips in ([0], [1], [2], [3], [9], [10], [11], [12], [17], [0, 5], [5, 0]):
        c = make_draw(9, flags=2)
        c['lines'] = front_rows(3, clipped=True)
        c['clips'] = clips
        c['vertices'] = [X.POISON] * (1024 * 8)
        c['vlimit'] = 1024
        out.append(c)
    # Nonzero vertex count with no triangles clears the retained primitive.
    for clips in ([1], [2]):
        c = make_draw(12, flags=2)
        c['lines'] = front_rows(3, clipped=True)
        c['clips'] = clips
        out.append(c)
    # Signed clip results: the original keeps fans only when the combined count is positive.
    for clips in ([3, -3], [4, -5], [5, -4], [-2, 3]):
        c = make_draw(13, flags=2)
        c['lines'] = front_rows(3, clipped=True)
        c['clips'] = clips
        header, _, indices = c['records'][0]
        c['records'] = [(header, 16, indices)]
        out.append(c)
    c = make_draw(10, code=0)
    c['q']['Execute'] = [0]
    c['lines'] = front_rows(3)
    out.append(c)
    for offset in (1, 3):
        c = make_draw(14, flags=2)
        header, _, indices = c['records'][0]
        c['records'] = [(header, offset, indices)]
        c['lines'] = front_rows(3)
        out.append(c)
    # No retained primitive and no system flag set: the pass builds a new one (0x100111f0 falls through)
    for system in (0, 0x3fffffff):
        c = make_draw(19, flags=2)
        c['lines'] = front_rows(3)
        X.set_g(c, system=system)
        out.append(c)
    for system in (0, 0x3fffffff, 0x40000000, 0x80000000, 0xc0000000):
        c = make_draw(15, retained=0, flags=2)
        c['records'][0][0][3] = 8
        c['lines'] = front_rows(3)
        X.set_g(c, system=system)
        out.append(c)
    # Enough visible cells to wrap the signed depth product (1,131 vertices and up).
    c = make_draw(16, first=0, rows=6, width=100, flags=2)
    c['lines'] = front_rows(7)
    out.append(c)
    # Row -1 with byte-identical adjacent words: covers the lower corners' alpha clearing.
    c = make_draw(17, first=0, rows=1, flags=2)
    c['points'][0:2] = [0, 0]
    c['points'][129] = 1
    c['cells'][0:2] = [-1, 0]
    c['cells'][129] = 1
    c['row_length'] = 0
    c['lines'] = front_rows(2)
    c['before'] = 256
    c['heights'] *= 4
    c['adjusted'] *= 4
    out.append(c)
    # Row -1 on both diagonals, unclipped and clipped: the lower corners' alpha clearing in the clipped copies of the cell
    # (0x1001176c, 0x100117bc, 0x10011860, 0x100118b0) and in the unclipped one of the other diagonal (0x10011e18, 0x10011e6c)
    for flags in (2, 0):
        for clipped in (False, True):
            c = make_draw(18, first=0, rows=1, flags=flags)
            c['points'][0:2] = [0, 0]
            c['points'][129] = 1
            c['cells'][0:2] = [-1, 0]
            c['cells'][129] = 1
            c['row_length'] = 0
            c['lines'] = front_rows(2, clipped=clipped)
            c['before'] = 256
            c['heights'] *= 4
            c['adjusted'] *= 4
            if clipped:
                c['clips'] = [3, 4]
                c['vertices'] = [X.POISON] * (1024 * 8)
                c['vlimit'] = 1024
            out.append(c)
    # A clipped fan count above 32767: the fans are written with the count narrowed to a signed halfword, the second one
    # starting after the narrowed first count, while the vertex total keeps the whole counts
    c = make_draw(20, first=0, rows=1, flags=2)
    c['points'][0:2] = [0, 0]
    c['points'][129] = 1
    c['cells'][0:2] = [-1, 0]
    c['cells'][129] = 1
    c['row_length'] = 0
    c['lines'] = front_rows(2, clipped=True)
    c['before'] = 256
    c['heights'] *= 4
    c['adjusted'] *= 4
    c['clips'] = [0x10003, 3]
    c['vertices'] = [X.POISON] * (70000 * 8)
    c['vlimit'] = 70000
    header, _, indices = c['records'][0]
    c['records'] = [(header, 0, [0] * 210000)]
    c['ilimit'] = 210000
    out.append(c)
    # Winding cases from the sibling's near-flat fused rounding corpus.
    for old in X.edges_rounding():
        c = make_draw(11, first=20, rows=1, width=60)
        c['cells'] = old['cells']
        c['lines'] = old['lines']
        c['quads'] = [(f & ~1, t) for f, t in old['quads']]
        c['row_length'] = old['row_length']
        out.append(c)
    return out


def draw_random(r):
    c = make_draw(r.getrandbits(32), first=r.choice([0, 1, 2, 30, 120]), rows=r.randint(1, 4), width=r.randint(1, 16))
    if r.random() < 0.2:
        c['retained'] = 0
        c['records'][0][0][3] = r.randint(0, 30)
        X.set_g(c, system=0)
    if r.random() < 0.1:
        c['use_adjusted'] = 0
    if r.random() < 0.1:
        c['entry'] = 0
    return c


class LineSide(MathFakes):
    def __init__(self, e, field, c):
        self.direct = True
        self.dump = []
        self.L = CL.PpcLog({field: X.LABEL_FIELD, e.hs_math: LMATH, X.LIGHT_MATRIX: X.LABEL_MATRIX,
                            H.CLIP_VAR: X.LABEL_CLIP})
        self.e = e


def ppc_line(e, c):
    with guarded(e):
        field = e.alloc(X.FIELD_SIZE, b'\xa5' * X.FIELD_SIZE)
        record = e.alloc(0x30, b'\xa5' * 0x30)
        ws(e, field, [record])
        grid(e, field, record, c)
        points = e.alloc(4 * len(c['vertices']))
        codes = e.alloc(len(c['codes']), bytes(c['codes']))
        ws(e, points, c['vertices'])
        ws(e, H.MATH_VAR, [0 if c['no_math'] else e.hs_math])
        ws(e, H.FINAL_VAR, [X.LIGHT_MATRIX])
        ws(e, H.CLIP_VAR, [H.CLIP_VAR])
        side = LineSide(e, field, c)
        side.vbase = points + 16 * c['pad']
        side.cbase = codes + c['pad']
        e.hs_side = side
        st = CL.Stubs()
        st.real = {LINE}
        st.internal(H.CPP_TEST_DATA, side.test_data)
        # Keep the assertion event compatible with the sibling helper's fake.
        st.imp('__assertion_failed', lambda em: side.L.rec('assert', (T.s32(em.r(5)),),
                                                        (em.cstr(em.r(3)), em.cstr(em.r(4)))))
        saved = st.activate(e)
        before = e.read(field, X.FIELD_SIZE), e.read(record, 0x30)
        abandoned = 0
        try:
            with Rounding(e):
                T.call_ppc(e, LINE, [field, codes + c['pad'], points + 16 * c['pad'], c['low'], c['high'], c['row']])
        except RuntimeError:
            assert c['no_math'] and e.last_fault[1] < 0x1000
            abandoned = 1
        finally:
            st.deactivate(e, saved)
        return {'abandoned': abandoned, 'vertices': [canon(x) for x in rs(e, points, len(c['vertices']))],
                'codes': list(e.read(codes, len(c['codes']))), 'log': side.L.log, 'dump': side.dump,
                'unchanged': int(before == (e.read(field, X.FIELD_SIZE), e.read(record, 0x30)))}


def host_line(lib, c):
    bind(lib)
    # Field and file storage are initialized identically before the direct helper call.
    lib.tpw_hx_field(array(C.c_int32, [0] * 258), array(C.c_int32, [0] * 258),
                     array(C.c_uint32, []), 0, 0, 0, array(C.c_uint16, []), 0)
    host_grid(lib, c)
    codes, points = array(C.c_uint8, c['codes']), array(C.c_uint32, c['vertices'])
    CL.host_begin(lib, labels(lib))
    state = lib.tpw_hs_line(codes, points, c['pad'], c['low'], c['high'], c['row'], c['no_math'])
    return {'abandoned': state & 1, 'vertices': [canon(x) for x in points], 'codes': list(codes),
            'log': CL.host_end(lib), 'dump': dump(lib), 'unchanged': state >> 1}


def line_case(seed, low=0, high=5, row=3, width=128, **kw):
    r = random.Random(seed)
    c = H.line_case(r, width, row, low, high, pad=16)
    c['adjusted'] = [H.rbits(r, -50, 50) for _ in c['heights']]
    c['lift'] = H.rbits(r, -10, 10)
    c['use_adjusted'] = r.randrange(2)
    c.update(kw)
    return c


def line_edges():
    out = []
    for low, high in ((127, 130), (128, 130), (130, 130), (200, 100), (5, 4), (5, 2), (0, -1),
                      (-8, 3), (-3, -1), (126, 127), (0, 127), (0, -2), (3, -9), (-9, -2)):
        for adjusted in (0, 1):
            c = line_case(1, low, high, use_adjusted=adjusted)
            # Cases above 127 get a larger guarded vertex buffer: only equality returns early.
            c['vertices'] = [X.POISON] * (4 * (max(128, low, high) + 2 * c['pad'] + 1))
            c['codes'] = [0xa5] * (max(128, low, high) + 2 * c['pad'] + 1)
            out.append(c)
    for n in range(1, 18):
        out.append(line_case(2, 2, 2 + n - 1))
    for low, high in ((0, 5), (5, 3), (127, 128), (128, 130), (5, -1)):
        out.append(line_case(3, low, high, no_math=True))
    for value in H.SPECIAL_BITS:
        c = line_case(4)
        c['lift'] = value
        c['record'] = [value] * 4
        out.append(c)
    return out


def line_random(r):
    low = r.randint(-8, 140)
    high = r.randint(max(-1, low - 3), max(127, low))
    c = line_case(r.getrandbits(32), low, high, row=r.choice([0, 1, 3, 127]), width=r.choice([1, 7, 128]))
    c['vertices'] = [r.getrandbits(32) for _ in range(4 * (max(128, low, high) + 2 * c['pad'] + 1))]
    c['codes'] = [r.randrange(256) for _ in range(len(c['vertices']) // 4)]
    return c


def load_case(renderer=1, name='clouds', size=64, old=0, logger=1, count=2500, q=None):
    return {'renderer': renderer, 'name': name, 'size': size, 'old': old, 'logger': logger,
            'count': count, 'q': q or {}}


def ppc_load(e, c):
    with guarded(e):
        logger_var = e.data_import('gLogger__13SamsUtilities')
        field = e.alloc(X.FIELD_SIZE, b'\xa5' * X.FIELD_SIZE)
        old = e.alloc(0x10014, b'\xa5' * 0x10014) if c['old'] else 0
        logger = e.alloc(0x1c) if c['logger'] else 0
        ws(e, field + 0x230, [old])
        ws(e, logger_var, [logger])
        ws(e, X.SYSTEM + 0x1c, [c['renderer']])
        ws(e, SETTINGS + 0x1c, [c['count']])
        L = CL.PpcLog({field: X.LABEL_FIELD, old: LOLD, logger: LLOGGER, ERROR: LERROR}, c['q'])
        e.cl = L
        st = CL.Stubs()
        st.real = {LOAD}
        def alloc(em):
            size = em.r(3)
            ok = L.ret('NewProc' if size == 0x10014 else 'NewLogger', 1)
            L.rec('new', (size, ok))
            p = em.alloc(size, b'\xa5' * size) if ok else 0
            em.set_r(3, p)
        def context(em):
            L.rec('SetContext', (L.lab(em.r(3)), T.s32(em.r(6)), em.r(7)),
                  (em.cstr(em.r(4)), em.cstr(em.r(5))))
            if L.ret('ClearLogger', 0):
                ws(em, logger_var, [0])
        def dtor(em):
            L.rec('ProcDtor', (L.lab(em.r(3)),))
            assert T.s16(em.r(4)) == 1
            L.rec('delete', (L.lab(em.r(3)),))
        st.imp('__nw__FUl', alloc)
        st.imp('__ct__Q213SamsUtilities11ULogManagerFv', CL.recorder('LoggerCtor', 'N'))
        st.imp('SetContext__Q213SamsUtilities11ULogManagerFPCcPCciUl', context)
        st.imp('Log__Q213SamsUtilities11ULogManagerFPCce', lambda em: L.rec('Log', (L.lab(em.r(3)),), (em.cstr(em.r(4)),)))
        st.internal(0x1001275c, CL.recorder('ProcCtor', 'N'))
        st.internal(0x100127f0, dtor)
        st.internal(0x1001287c, CL.recorder('ProcInit', 'Lii', ret=lambda e, L: L.ret('Init', 1)))
        st.internal(0x10012c94, CL.recorder('ProcLoad', 'Ls', ret=lambda e, L: L.ret('Load', 1)))
        st.internal(0x10008904, CL.recorder('Push', 'Lii'))
        saved = st.activate(e)
        name = 0 if c['name'] is None else e.alloc(len(c['name'].encode()) + 1, c['name'].encode() + b'\0')
        before = e.read(field, X.FIELD_SIZE)
        try:
            ret = T.call_ppc(e, LOAD, [field, name, c['size']])[0]
            after = e.read(field, X.FIELD_SIZE)
            assert before[:0x230] == after[:0x230] and before[0x234:] == after[0x234:]
            return {'ret': T.s32(ret), 'log': L.log,
                    'state': [L.lab(e.u32(field + 0x230)), L.lab(e.u32(logger_var)), 1]}
        finally:
            st.deactivate(e, saved)


def host_load(lib, c):
    bind(lib)
    lib.tpw_hs_load_setup(c['renderer'], c['old'], c['logger'], c['count'])
    fixed = labels(lib)
    if c['old']:
        fixed[LOLD] = lib.tpw_hs_load_pointer(0)
    if c['logger']:
        fixed[LLOGGER] = lib.tpw_hs_load_pointer(1)
    CL.host_begin(lib, fixed, c['q'])
    state = (C.c_int32 * 3)()
    ret = lib.tpw_hs_load(None if c['name'] is None else c['name'].encode(), c['size'], state)
    log = CL.host_end(lib)
    # Allocated objects need stable labels before their constructors log them, via calllog's dynamic labels.
    return {'ret': ret, 'log': log, 'state': list(state)}


def load_edges():
    out = []
    for renderer in (-1, 0, 1, 2, 3, 4, 5, 0x7fffffff, -0x80000000):
        for name in (None, '', 'clouds', 'x' * 256):
            for old in (0, 1):
                out.append(load_case(renderer, name, old=old, logger=0))
    for init in (0, 1, -1):
        for load in (0, 1):
            for old in (0, 1):
                out.append(load_case(old=old, q={'Init': [init], 'Load': [load]}))
    for old in (0, 1):
        out.append(load_case(old=old, q={'NewProc': [0]}))
    out.append(load_case(logger=0, q={'ClearLogger': [1]}))
    # The logger's allocation fails (0x1000d9d4): the null logger gets SetContext, then the second test allocates again
    for logger_answers in ([0], [0, 1], [0, 0], [1, 0]):
        out.append(load_case(logger=0, q={'NewLogger': logger_answers}))
        out.append(load_case(logger=0, q={'NewLogger': logger_answers, 'ClearLogger': [1]}))
    out.append(load_case(logger=1, q={'NewLogger': [0], 'ClearLogger': [1]}))
    out.append(load_case(logger=1, name=None, q={'NewLogger': [0], 'ClearLogger': [1]}))
    return out


def load_random(r):
    return load_case(renderer=r.choice([1, 1, 3, 3, 0, 2, 4]), name=r.choice([None, '', 'clouds', 'sky\\shadow.tga']),
                     size=r.choice([-1, 0, 1, 32, 64, 128, 256]), old=r.randrange(2), logger=r.randrange(2),
                     count=r.choice([-1, 0, 1, 2500, 0x7fffffff]),
                     q={'Init': [r.choice([0, 1, -1])], 'Load': [r.randrange(2)], 'NewProc': [r.choice([0, 1, 1])]})


TESTS = [FnTest('CHField::DrawSkyShadow', ppc_draw, host_draw, edge=draw_edges, rnd=draw_random),
         FnTest('CHField::CalcShadowLine', ppc_line, host_line, edge=line_edges, rnd=line_random),
         FnTest('CHField::LoadProcTextures', ppc_load, host_load, edge=load_edges, rnd=load_random)]
