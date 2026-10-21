"""CsTexture, CTextureLoadManager::RemoveTexture and SetLoadFlags, DelayedLoad_LoadTextures
(engine library, 0x1001658c..0x1001722c; src/engine/CsTexture.cpp, src/engine/TextureLoadManager.cpp).

A case is a script of operations on two CsTexture objects, two texture records with their
infos, the load manager's table and the delayed-load list. After every operation the test
compares the result, the fakes' events (the allocator, the manager's LoadFile and
LoadTexture with folder, name and load flags, the texture entries' construction,
destruction, Close and Create, errors, assertions, deletions) and the state: both
objects, the entries in their lists with their flags, the manager's table, the delayed
list, the texture settings' flags.

An operation the original cannot survive in the state it would meet (Close on an object
whose list was never made, LoadTextureFromDisk without a record, an entry index outside
the list) is left out on both sides: the PowerPC side decides from the state and the host
side skips the same ones.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_LoadManager"

FN = dict(ctor=0x100167b0, dtor=0x1001680c, Reset=0x10016890, LoadEntry=0x100168d4, LoadName=0x100169b4,
          LoadRecord=0x10016b30, LoadTextureFromDisk=0x10016da0, ReLoad=0x10016f60, Close=0x10016fc0,
          SetTextureEntries=0x100170ec, GetFrameCount=0x1001712c, GetsTexture=0x10017168, SetTextureEntry=0x1001719c,
          GetTextureEntry=0x100171ec, GetTextureEntryAddr=0x1001722c, RemoveTexture=0x1001658c,
          SetLoadFlags=0x100166c8, DelayedLoad=0x1001670c)
LOAD_FILE, LOAD_TEXTURE, ENTRY_CTOR, ENTRY_DTOR, ENTRY_CLOSE, ENTRY_CREATE, ERROR_PUSH = (
    0x10015f9c, 0x10016324, 0x10040d30, 0x10040dfc, 0x10040f7c, 0x10040e84, 0x10008904)
P_MALLOC, P_FREE, P_MANAGER, DELAYED_COUNT, DELAYED, SETTINGS = (
    0x10051f70, 0x10051f74, 0x100af1c8, 0x100af1d0, 0x100af1d4, 0x10165fc8)
MANAGER_BYTES = 0x1000c
SKIPPED = -8


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if not hasattr(e, "lm_fns"):
        e.lm_fns = {name: T.fake_function(e, (lambda n: lambda emu: getattr(emu.lm_side, n)())(name))
                    for name in ("cb_malloc", "cb_free")}
        e.lm_manager = T.persist_alloc(e, MANAGER_BYTES)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        e.lm_side = self
        self.L = CL.PpcLog({}, case.get("q"))
        e.cl = self.L
        self.objects = e.alloc(0x20, b"\xa5" * 0x20)
        self.textures = e.alloc(16)
        self.infos = e.alloc(32)
        self.names = e.alloc(2 * 4 * 0x14)
        self.folder = e.alloc(32, b"data:textures\0")
        self.lists = {}       # list block -> how many pointers it holds
        self.arrays = {}      # first entry of an array -> how many entries
        self.manager = e.lm_manager

    # ---- fakes
    def new_array(self, count):
        """An array of blank entries as operator new[] and the constructor fakes leave it."""
        e, L = self.e, self.L
        base = e.alloc(8 + 0x5c * count, bytes(8 + 0x5c * count))
        L.newlab(base)
        for i in range(count):
            L.newlab(base + 8 + 0x5c * i)
        self.arrays[base + 8] = count
        return base + 8

    def cb_malloc(self):
        e, L = self.e, self.L
        count = e.r(3) // 4
        L.rec("Malloc", (count,))
        if not L.ret("Malloc", 1):
            e.set_r(3, 0)
            return
        block = e.alloc(max(count, 1) * 4, bytes(max(count, 1) * 4))
        L.newlab(block)
        self.lists[block] = count
        e.set_r(3, block)

    def cb_free(self):
        e, L = self.e, self.L
        block = e.u32(e.r(3))
        L.rec("Free", (L.lab(block),))
        L.forget(block)
        w32(e, e.r(3), 0)

    def load_file(self, e):
        self.L.rec("LoadFile", (), (e.cstr(e.r(4)),))
        count = self.L.ret("LoadFile", 1)
        e.set_r(3, self.new_array(count) if count else 0)

    def load_texture(self, e):
        self.L.rec("LoadTexture", (e.u32(e.r(3) + 0x10008),), (e.cstr(e.r(4)) if e.r(4) else "(null)", e.cstr(e.r(5))))
        count = self.L.ret("LoadTexture", 1)
        if not count:
            e.set_r(3, 0)
            return
        entries = self.new_array(count)
        e.write(entries + 0xc, struct.pack(">hh", T.s16(self.L.ret("Width", 16)), T.s16(self.L.ret("Height", 16))))
        e.set_r(3, entries)

    def entry_dtor(self, e):
        # Called directly only where the original deletes a loaded entry as a single object
        # (see CsTexture::Load(char*) in the source): the entry is destroyed, nothing more.
        self.L.rec("Entry.dtor", (self.L.lab(e.r(3)),))
        self.L.forget(e.r(3))

    def new_array_import(self, e):
        # operator new[](100): the block for one entry and its count word.
        if not self.L.ret("NewArray", 1):
            e.set_r(3, 0)
            return
        base = e.alloc(e.r(3), bytes(e.r(3)))
        self.L.newlab(base)
        e.set_r(3, base)

    def construct_array(self, e):
        base, count = e.r(3), e.r(7)
        if not base:
            e.set_r(3, 0)
            return
        for i in range(count):
            self.L.newlab(base + 8 + 0x5c * i)
        self.arrays[base + 8] = count
        e.set_r(3, base + 8)

    def destroy_array(self, e):
        L, first = self.L, e.r(3)
        if not first:
            return
        for i in reversed(range(self.arrays.get(first, 1))):
            L.rec("Entry.dtor", (L.lab(first + 0x5c * i),))
            L.forget(first + 0x5c * i)
        L.rec("delete", (L.lab(first - 8),))
        L.forget(first - 8)

    def stubs(self):
        L = self.L
        st = CL.Stubs()
        st.real = set(FN.values())
        st.internal(LOAD_FILE, self.load_file)
        st.internal(LOAD_TEXTURE, self.load_texture)
        st.internal(ENTRY_DTOR, self.entry_dtor)
        st.internal(ENTRY_CLOSE, CL.recorder("Entry.Close", "L", ret=lambda e, L: 1))
        st.internal(ENTRY_CREATE, CL.recorder("Entry.Create", "Lii", ret=lambda e, L: L.ret("Create", 1)))
        st.internal(ERROR_PUSH, CL.recorder("Error", "xii"))
        st.imp("__nwa__FUl", self.new_array_import)
        st.imp("__construct_new_array", self.construct_array)
        st.imp("__destroy_new_array", self.destroy_array)
        st.imp("__assertion_failed", lambda e: L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
        return st

    def reset(self):
        e = self.e
        w32(e, P_MALLOC, e.lm_fns["cb_malloc"])
        w32(e, P_FREE, e.lm_fns["cb_free"])
        w32(e, P_MANAGER, self.manager)
        e.write(self.manager, bytes(0x100))
        e.write(self.manager + 0x10000, struct.pack(">iiI", 0, -1, 0))
        w32(e, DELAYED_COUNT, 0)
        e.write(DELAYED, bytes(16))
        w32(e, SETTINGS, 0)

    # ---- the object's fields
    def fields(self, k):
        e, o = self.e, self.objects + 0x10 * (k & 1)
        count, references = struct.unpack(">hh", e.read(o + 0xc, 4))
        return o, e.u32(o), e.u32(o + 4), e.u32(o + 8), count, references

    def capacity(self, k):
        return self.lists.get(self.fields(k)[3], 0) if self.L.lab(self.fields(k)[3]) > 0 else 0

    def entries(self, k):
        e, entries = self.e, self.fields(k)[3]
        return [e.u32(entries + 4 * i) for i in range(self.capacity(k))]

    def call(self, name, *args):
        return T.call_ppc(self.e, FN[name], list(args))[0]

    # ---- one operation; None when it is left out
    def op(self, op, a):
        e, L = self.e, self.L
        k = a[0] & 1 if a else 0
        o, texture, info, entries, count, references = self.fields(k)
        built = references != T.s16(0xa5a5)
        if op == 1:
            self.call("ctor", o)
            return 0
        if op in (2, 3, 4, 5, 6, 7, 8, 9, 10, 11) and not built:
            return None
        if op == 2:
            return T.s32(self.call("LoadEntry", o, self.new_array(a[1])))
        if op == 3:
            return T.s32(self.call("LoadName", o, e.alloc(16, b"data:tex.tga\0")))
        if op in (4, 6):
            if op == 6:
                source = self.fields(a[1])
                if not source[1] or not source[2]:
                    return None
                record, record_info = source[1], source[2]
            else:
                record, record_info = self.textures + 8 * (a[1] & 1), self.infos + 16 * (a[1] & 1)
            flags = struct.unpack(">H", e.read(record, 2))[0]
            if entries:
                # A loaded object only marks its entries: every one of them must be there.
                if flags & 0x30 and count > 0 and (count > self.capacity(k) or not all(self.entries(k)[:count])):
                    return None
            elif not flags & 0x80 and struct.unpack(">H", e.read(record_info + 0xa, 2))[0] > 4:
                return None
            if op == 6:
                return T.s32(self.call("ReLoad", o, self.objects + 0x10 * (a[1] & 1)))
            return T.s32(self.call("LoadRecord", o, record, record_info))
        if op == 5:
            if not texture or not info or count < 0 or count > self.capacity(k):
                return None
            return T.s32(self.call("LoadTextureFromDisk", o))
        if op in (7, 8):
            if references == 1:
                # The last reference frees: the list must exist and hold what the count says.
                if not entries or (count >= 0 and count > self.capacity(k)) or (count < 0 and self.capacity(k) < 1):
                    return None
            if op == 7:
                return T.s32(self.call("Close", o))
            self.call("dtor", o, 0)
            return 0
        if op == 9:
            if a[2] == 0:
                return self.call("GetFrameCount", o) & 0xffff if info else None
            if a[2] == 1:
                value = self.call("GetsTexture", o)
                return 0 if value == 0 else 1 + (value - self.textures) // 8
            if a[1] >= self.capacity(k):
                return None
            if a[2] == 2:
                return L.lab(self.call("GetTextureEntry", o, a[1]))
            return (self.call("GetTextureEntryAddr", o, a[1]) - entries) // 4
        if op == 10:
            if not (0 <= a[1] <= self.capacity(k)) or not texture or not info:
                return None
            self.call("SetTextureEntries", o, a[1])
            return 0
        if op == 11:
            if a[1] >= self.capacity(k):
                return None
            self.call("SetTextureEntry", o, self.new_array(1), a[1])
            return 0
        if op == 12:
            t = a[0] & 1
            e.write(self.textures + 8 * t, struct.pack(">H", a[1] & 0xffff))
            names = self.names + t * 4 * 0x14
            e.write(self.infos + 16 * t + 4, struct.pack(">I", self.folder if a[3] else 0))
            e.write(self.infos + 16 * t + 0xa, struct.pack(">HI", a[2] & 0xffff, names))
            for i in range(4):
                e.write(names + i * 0x14, (bytes([ord("a") + t, ord("0") + i]) + b".tga").ljust(0x14, b"\0"))
            e.write(names + 6, struct.pack(">H", a[4] & 0xffff))
            return 0
        if op == 13:
            # Every object on the list must still be what it was when it was listed.
            for i in range(T.s32(e.u32(DELAYED_COUNT))):
                listed = (e.u32(DELAYED + 4 * i) - self.objects) // 0x10
                _, texture, info, _, count, _ = self.fields(listed)
                if not texture or not info or count < 0 or count > self.capacity(listed):
                    return None
            self.call("DelayedLoad")
            return 0
        if op == 14:
            w32(e, SETTINGS, a[1])
            return 0
        if op == 16:
            w32(e, self.manager + 0x10000, a[1])
            for i in range(a[1]):
                e.write(self.manager + 16 * i, struct.pack(">IIIi", 0, 0, self.new_array(1), a[2 + i]))
            return 0
        if op == 17:
            if a[1] >= T.s32(e.u32(self.manager + 0x10000)):
                return None
            texture = e.u32(self.manager + 16 * a[1] + 8) if a[1] >= 0 else self.new_array(1)
            return T.s32(self.call("RemoveTexture", self.manager, texture))
        if op == 18:
            self.call("SetLoadFlags", self.manager, a[1])
            return 0
        return -99

    def state(self):
        e, L, u = self.e, self.L, self.e.u32
        out = []
        record = lambda p, base, stride: 0 if not p else (1 + (p - base) // stride if 0 <= p - base < 2 * stride else -1)
        for k in range(2):
            o, texture, info, entries, count, references = self.fields(k)
            out += [record(texture, self.textures, 8), record(info, self.infos, 16), L.lab(entries), count, references]
            held = self.entries(k)
            out += [L.lab(held[i]) if i < len(held) else -7 for i in range(4)]
            out += [u(held[i] + 8) if i < len(held) and held[i] and L.lab(held[i]) > 0 else 0 for i in range(4)]
        count = T.s32(u(self.manager + 0x10000))
        out += [count, u(self.manager + 0x10008)]
        for i in range(6):
            used = i < count
            out += [L.lab(u(self.manager + 16 * i + 8)) if used else -7, T.s32(u(self.manager + 16 * i + 12)) if used else -7]
        delayed = T.s32(u(DELAYED_COUNT))
        out.append(delayed)
        out += [record(u(DELAYED + 4 * i), self.objects, 0x10) if i < delayed else -7 for i in range(4)]
        out.append(u(SETTINGS))
        return out


def ppc(e, case):
    side = PpcSide(e, case)
    words = (P_MALLOC, P_FREE, P_MANAGER, DELAYED_COUNT, SETTINGS)
    saved_words = [(a, e.u32(a)) for a in words]
    saved_list = e.read(DELAYED, 16)
    saved = side.stubs().activate(e)
    try:
        side.reset()
        results, states, marks, skipped = [], [], [], []
        for op, args in case["script"]:
            result = side.op(op, args)
            skipped.append(result is None)
            results.append(SKIPPED if result is None else result)
            states.append(side.state())
            marks.append(len(side.L.log))
    finally:
        CL.Stubs.deactivate(e, saved)
        for a, v in saved_words:
            w32(e, a, v)
        e.write(DELAYED, saved_list)
    case["_skipped"] = skipped
    return dict(results=results, log=side.L.log, marks=marks, states=states)


def host(lib, case):
    lib["tpw_lm_reset"]()
    op_fn = lib["tpw_lm_op"]
    op_fn.restype, op_fn.argtypes = C.c_int64, [C.c_int32, C.POINTER(C.c_int32)]
    state_fn = lib["tpw_lm_state"]
    state_fn.restype, state_fn.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    CL.host_begin(lib, {}, case.get("q"))
    results, states, marks = [], [], []
    buffer = (C.c_int64 * 128)()
    try:
        for (op, args), skip in zip(case["script"], case["_skipped"]):
            if skip:
                results.append(SKIPPED)
            else:
                padded = (C.c_int32 * 12)(*[T.s32(v) for v in (list(args) + [0] * 12)[:12]])
                results.append(op_fn(op, padded))
            states.append(list(buffer[:state_fn(buffer)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(results=results, log=log, marks=marks, states=states)


# ----------------------------------------------------------------- scripts

RECORD_FLAGS = [0, 0, 0x10, 0x20, 0x30, 0x80, 0x1000, 0x2000, 0x4000, 0x7030, 0x90, 0xffff]


def record_op(r):
    return (12, [r.randrange(2), r.choice(RECORD_FLAGS), r.choice([0, 1, 1, 2, 3, 4]), r.randrange(2),
                 r.choice([0, 8, 16, 64, 0x1234])])


def texture_op(r):
    kind = r.choice([1, 2, 3, 4, 4, 4, 5, 6, 7, 7, 8, 9, 9, 10, 11, 12, 13, 14, 18])
    k = r.randrange(2)
    if kind == 2:
        return (2, [k, r.randrange(1, 3)])
    if kind == 4:
        return (4, [k, r.randrange(2)])
    if kind == 6:
        return (6, [k, r.randrange(2)])
    if kind == 9:
        return (9, [k, r.randrange(5), r.randrange(4)])
    if kind == 10:
        return (10, [k, r.randrange(5)])
    if kind == 11:
        return (11, [k, r.randrange(4)])
    if kind == 12:
        return record_op(r)
    if kind == 14:
        return (14, [0, r.choice([0, 0x40000000, 0x40000100, 0x17a])])
    if kind == 18:
        return (18, [0, r.choice([0, 2, 4, 8, r.getrandbits(32)])])
    return (kind, [k])


def manager_op(r):
    if r.random() < 0.3:
        count = r.randrange(0, 6)
        return (16, [0, count] + [r.choice([0, 1, 1, 2, 3]) for _ in range(count)])
    return (17, [0, r.choice([-1, 0, 0, 1, 2, 3, 4, 5])])


def queues(r, fail):
    q = {"LoadFile": [r.choice([1, 1, 2, 0]) for _ in range(4)], "LoadTexture": [r.choice([1, 1, 1, 2, 0]) for _ in range(8)],
         "Width": [r.choice([16, 16, 32, 1, 24, 0, -8]) for _ in range(8)], "Height": [r.choice([16, 16, 32, 1, 8]) for _ in range(8)],
         "Create": [r.choice([1, 1, 1, 0]) for _ in range(4)]}
    if fail:
        q["Malloc"] = [1 if r.random() < 0.75 else 0 for _ in range(r.randrange(1, 8))] + [1]
        q["NewArray"] = [1 if r.random() < 0.75 else 0 for _ in range(r.randrange(1, 4))] + [1]
    return q


def case_of(r, makers, length, fail=False):
    script = [(1, [0]), (1, [1]), record_op(r), record_op(r)]
    script += [r.choice(makers)(r) for _ in range(length)]
    return dict(q=queues(r, fail), script=script)


def edge():
    r = random.Random(111)
    base = [(1, [0]), (1, [1])]
    # One entry handed over, shared, closed twice.
    yield dict(script=base + [(2, [0, 1]), (9, [0, 0, 2]), (9, [0, 0, 3]), (7, [0]), (7, [0])])
    yield dict(q={"Malloc": [0]}, script=base + [(2, [0, 1]), (7, [0])])
    # By name: the file there or not, either list failing.
    for q in ({}, {"LoadFile": [0]}, {"LoadFile": [2]}, {"Malloc": [0]}, {"Malloc": [1, 0]}):
        yield dict(q=q, script=base + [(3, [0]), (9, [0, 0, 2]), (7, [0])])
    # From a record: each flag of the texture word, 0 to 4 frames, with and without a folder.
    for flags in RECORD_FLAGS:
        for frames in (0, 1, 3):
            for folder in (0, 1):
                yield dict(script=base + [(12, [0, flags, frames, folder, 16]), (4, [0, 0]), (9, [0, 0, 0]),
                                          (9, [0, 0, 1]), (4, [0, 0]), (7, [0]), (7, [0]), (7, [0])])
    # Sizes of the loaded textures against the "square power of two" assertion.
    for width, height in ((16, 16), (16, 8), (24, 24), (1, 1), (0, 0), (-8, -8), (32, 32)):
        for flags in (0x10, 0x20, 0x30, 0):
            yield dict(q={"Width": [width], "Height": [height]},
                       script=base + [(12, [0, flags, 1, 1, 0]), (4, [0, 0]), (7, [0])])
    for q in ({"LoadTexture": [0]}, {"LoadTexture": [1, 0, 1]}, {"Malloc": [0]}):
        yield dict(q=q, script=base + [(12, [0, 0x30, 3, 1, 0]), (4, [0, 0]), (7, [0])])
    # The blank image of the object's own: its size word, Create failing.
    for size in (0, 8, 64, 0x1234):
        for q in ({}, {"Create": [0]}, {"NewArray": [0]}):
            yield dict(q=q, script=base + [(12, [1, 0x80, 1, 0, size]), (4, [0, 1]), (9, [0, 0, 2]), (7, [0])])
    # Loading put off: the list of delayed objects, loaded later, then again with nothing.
    for frames in (1, 2):
        yield dict(script=base + [(14, [0, 0x40000000]), (12, [0, 0x1030, frames, 1, 0]), (12, [1, 0, 1, 0, 0]),
                                  (4, [0, 0]), (4, [1, 1]), (13, []), (13, []), (7, [0]), (7, [1])])
    yield dict(script=base + [(12, [0, 0, 2, 1, 0]), (4, [0, 0]), (6, [1, 0]), (6, [1, 0]), (5, [1]), (8, [1]), (8, [0])])
    # The manager's table: the last reference, an earlier one, a texture it does not hold.
    for references in ([1], [2], [1, 1, 1], [3, 1, 2], [1, 2, 1, 1, 1, 1]):
        for which in range(-1, len(references)):
            yield dict(script=[(16, [0, len(references)] + references), (17, [0, which]), (17, [0, which]), (18, [0, 6])])
    for _ in range(30):
        yield case_of(r, [texture_op], 16)
    for _ in range(10):
        yield case_of(r, [texture_op, manager_op], 16, fail=True)


TESTS = [
    FnTest("CsTexture, the delayed loads and RemoveTexture", ppc, host, edge=edge,
           rnd=lambda r: case_of(r, [texture_op, texture_op, texture_op, manager_op], 20, fail=r.random() < 0.3)),
]
