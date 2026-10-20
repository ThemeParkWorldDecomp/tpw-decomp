"""The life of a ride script (unit w169), against libtpw_unit_RideScriptLife.dylib (tests/unit_RideScriptLife.cpp), main app
("SimTheme Park"), src/rsse_script_life.cpp:
  RSSE_InitScript 0x100d37b4, KillScript 0x100d2ecc, RSSE_KillScript 0x100d42d0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the file reads and the byte swap, the ride database, the particle and sound calls, the data node lookup, the bounding box, the
script lookup, the assert and the trace) are fakes on both sides that record their arguments and answer from the case's queues.
memset, strcpy and strlen run for real on both sides. LbMem_Alloc is the emulator's allocator on the Mac side and the arena on the host
side, LbMem_Free the emulator's no-op (it notes the block) and the arena: the sizes asked for are compared per table, and which blocks were
freed (by the arena's block header on the host).

The world of a case: the system state words, the list of script records (each with its code, variable, stack, limbo, bounce, walk, saved
word and saved block tables, its name and its objects), the thing rows, three ride sets (node with the height, a mesh), the ride record's
two effect numbers and the script file. The file's words are little-endian, as the original's were (the Mac swaps them, the host reads them).
What is compared: the answer, the system words, the list of records (by label, pointers as labels), the new record and its tables, the
sizes of the blocks the unit asked for, the blocks freed (and the bytes of the freed tables from byte 8 on: the arena writes its link into
the first eight), the live object count and the log.

Not tested: a thing row without a thing, a NULL ride database answer, a handle of mF10 that names no script, a path of 257 or more
characters (the local is 258 bytes), a program of 0x40000000 words (the swap would run forever), a ride slot without a set, a script list
that holds a record twice, the order of the frees (the arena does not show it), the first eight bytes of a freed table.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_RideScriptLife"
MASK = 0xFFFFFFFF

FN = dict(RSSE_InitScript=0x100D37B4, KillScript=0x100D2ECC, RSSE_KillScript=0x100D42D0)
SZ = dict(RSSE_InitScript=2756, KillScript=1004, RSSE_KillScript=696)
SIZES = {FN[n]: SZ[n] for n in FN}
OWN = set(SIZES)

A_ASSERT, A_CTRACE, A_OPEN, A_READ, A_CLOSE = 0x1000DDE8, 0x1006FCBC, 0x10003FD4, 0x10004260, 0x10004354
A_FLYREM, A_LIFE, A_RANDPOS, A_INIT, A_GETSCRIPT = 0x100781F4, 0x100BE2E8, 0x100BE3F4, 0x100BEC30, 0x100D668C
A_STOP, A_FADE, A_ADDOBJ, A_DELOBJ, A_DIP = 0x100DE058, 0x100DE0FC, 0x100DE8E4, 0x100DE918, 0x100DEB04
A_GETINFO, A_GRMBA, A_FINDNODE, A_DISABLE = 0x10154868, 0x101ED058, 0x101FFDC8, 0x1020039C
A_ALLOC, A_FREE, A_SWAP = 0x10238518, 0x10238548, 0x10239A78

ADDR = dict(F7BE0=0x102F7BE0, SUM=0x102F7BE8, STATE=0x102F7BEC, RIDEDB=0x1037F6C8, RIDESETS=0x102C79A4, THINGS=0x1034CBFC)
# host layout: sizeof(RSSE_SCRIPT), sizeof(RSSE_OBJECT), mCode, mObjects, mThingId, mFCC, mFE6, mFB9, mF14, mScripts, CObject mObjectType,
# CObject mF1C, mF78 of the header, sizeof(RSSE_WALK), mKind, mState, create effect word, destroy effect word
HOST_LAYOUT = [0xF4, 0x1C, 0x18, 0xB0, 0xAC, 0xCC, 0xE6, 0xB9, 0x14, 0x10, 0xE, 0x1C, 0x78, 0x20, 0x16, 0x18, 0xC0, 0xC8]

L_RIDEDB, L_FILE, L_NEW = 61, 62, 99
FLAG_HEADS = 4


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def u32at(e, a):
    return struct.unpack(">I", e.read(a, 4))[0]


def glue_toc(e):
    """The cross-TOC glue the stubs stand for saves r2 at 0x14(r1); the caller reloads it afterwards."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))


def desc(e, dest):
    """Where a read or swap lands: the offset in the newest record, 0x1000 + the offset of mCode or mSaveBlock, else -1."""
    head = u32at(e, ADDR["STATE"] + 0x10)
    if not head:
        return -1
    if head <= dest < head + 0xF4:
        return dest - head
    for off in (0x18, 0x34):
        if u32at(e, head + off) and dest == u32at(e, head + off):
            return 0x1000 + off
    return -1


def stub_ctrace(e):
    e.cl.rec("CTRACE", (s32(e.r(3)), s32(e.r(5))), (e.cstr(e.r(4))[:159],))


def stub_open(e):
    L = e.cl
    L.rec("TPFile_Open", (), (e.cstr(e.r(3))[:159], e.cstr(e.r(4))[:159]))
    L.position = 0
    e.set_r(3, L.dyn["FILE"] if L.ret("TPFile_Open", 1) != 0 else 0)


def stub_read(e):
    L, dest, size, count = e.cl, e.r(3), e.r(4), e.r(5)
    L.rec("TPFile_Read", (desc(e, dest), size, count, L.lab(e.r(6))))
    length = (size * count) & MASK
    chunk = L.file[L.position:L.position + length]
    L.position += len(chunk)
    if length:
        e.write(dest, chunk + bytes(length - len(chunk)))
    e.set_r(3, len(chunk) // size if size else 0)


def stub_swap(e):
    glue_toc(e)
    a, n = e.r(3), s32(e.r(4))
    e.cl.rec("SwapBlock32", (n, desc(e, a)))
    for i in range(max(n, 0)):
        e.write(a + 4 * i, e.read(a + 4 * i, 4)[::-1])


def stub_alloc(e):
    glue_toc(e)
    size = e.r(3)
    a = e.alloc(max(size, 1), b"\xa5" * max(size, 1))
    e.cl.sizes[a] = size
    e.set_r(3, a)


def stub_free(e):
    glue_toc(e)
    e.cl.freed.append(e.r(3))


def stub_getinfo(e):
    L = e.cl
    L.rec("GetRideInfo", (L.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, L.dyn["INFO"])


def stub_grmba(e):
    L = e.cl
    L.rec("GetRideMasterBoundArea", (e.r(3),))
    ans = L.ret("grmba", 0) & MASK
    if ans:
        box = e.r(4)
        mins = [L.ret("bx%d" % i) for i in range(3)]
        maxs = [L.ret("by%d" % i) for i in range(3)]
        e.write(box, b"".join(struct.pack(">I", v & MASK) for v in mins + maxs))
    e.set_r(3, ans)


def stub_getscript(e):
    L = e.cl
    handle = s32(e.r(3))
    L.rec("RSSE_GetScriptPointer", (handle,))
    e.set_r(3, L.known.get(handle, 0))


STUBS.internal(A_ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(A_CTRACE, stub_ctrace)
STUBS.internal(A_OPEN, stub_open)
STUBS.internal(A_READ, stub_read)
STUBS.internal(A_CLOSE, CL.recorder("TPFile_Close", "L"))
STUBS.internal(A_FLYREM, CL.recorder("FLY_RemoveBase", "i"))
STUBS.internal(A_LIFE, CL.recorder("PAR_SetEmitterLife", "ii"))
STUBS.internal(A_RANDPOS, CL.recorder("PAR_SetEmitterRandomPos", "iiii"))
STUBS.internal(A_INIT, CL.recorder("PAR_InitEmitter", "iiii", "i"))
STUBS.internal(A_GETSCRIPT, stub_getscript)
STUBS.internal(A_STOP, CL.recorder("SOUND_StopSoundEffect", "i"))
STUBS.internal(A_FADE, CL.recorder("SOUND_FadeSoundEffect", "i"))
STUBS.internal(A_ADDOBJ, CL.recorder("SOUND_AddObject", "fffff", "i"))
STUBS.internal(A_DELOBJ, CL.recorder("SOUND_DelObject", "u"))
STUBS.internal(A_DIP, CL.recorder("SND_DipMusic", "i"))
STUBS.internal(A_GETINFO, stub_getinfo)
STUBS.internal(A_GRMBA, stub_grmba)
STUBS.internal(A_FINDNODE, CL.recorder("FindDataNodeIndexByID", "Luu", "i"))
STUBS.internal(A_DISABLE, CL.recorder("DisableRideHead", "Lu"))
STUBS.internal(A_ALLOC, stub_alloc)
STUBS.internal(A_FREE, stub_free)
STUBS.internal(A_SWAP, stub_swap)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

# The script record by field: (offset, kind, count); W a word, H a halfword, B a byte (the kinds that differ in byte order are read
# and written by value, so one list of numbers builds and reads both sides).
FIELDS = ([(o, "W", 1) for o in range(0, 0x6C, 4)] + [(0x6C, "H", 2)] + [(o, "W", 1) for o in range(0x70, 0x88, 4)] + [(0x88, "H", 2)] +
          [(o, "W", 1) for o in range(0x8C, 0xAC, 4)] + [(0xAC, "H", 1), (0xAE, "B", 2), (0xB0, "W", 1), (0xB4, "B", 12), (0xC0, "H", 1),
                                                       (0xC2, "B", 6)] +
          [(o, "W", 1) for o in range(0xC8, 0xE4, 4)] + [(0xE4, "H", 1), (0xE6, "B", 2)] + [(o, "W", 1) for o in range(0xE8, 0xF4, 4)])
PTR_OFFS = (0x00, 0x04, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0xB0)
TABLE_OFFS = (0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38)
NFIELDS = sum(n if k != "W" else 1 for o, k, n in FIELDS)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_halfs(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def put_fields(self, rec, vals):
        i = 0
        for o, k, n in FIELDS:
            if k == "W":
                self.write(rec + o, struct.pack(self.fmt, vals[i] & MASK))
                i += 1
            elif k == "H":
                for j in range(n):
                    self.write(rec + o + 2 * j, struct.pack(self.fmt[0] + "H", vals[i] & 0xFFFF))
                    i += 1
            else:
                for j in range(n):
                    self.write(rec + o + j, bytes([vals[i] & 0xFF]))
                    i += 1

    def get_fields(self, rec):
        raw = self.read(rec, 0xF4)
        out = []
        for o, k, n in FIELDS:
            if k == "W":
                out.append(struct.unpack(self.fmt, raw[o:o + 4])[0])
            elif k == "H":
                out += [struct.unpack(self.fmt[0] + "H", raw[o + 2 * j:o + 2 * j + 2])[0] for j in range(n)]
            else:
                out += list(raw[o:o + n])
        return out

    def field_index(self, off):
        i = 0
        for o, k, n in FIELDS:
            if o == off:
                return i
            i += 1 if k == "W" else n
        raise KeyError(off)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["RIDEDB"]: L_RIDEDB}, case["queues"])
        self.dyn = self.L.dyn = {"INFO": e.alloc(0x100), "FILE": e.alloc(16)}
        self.L.labels[self.dyn["FILE"]] = L_FILE
        self.L.sizes, self.L.freed, self.L.known = {}, [], {}
        self.L.file, self.L.position = case["file"], 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return self.dyn.get(name) or ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n, bytes(n))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def label(self, a, n):
        self.L.labels[a] = n

    def up(self, w):
        return w

    def p(self, a):
        return a

    def reset(self):
        pass

    def set_thing(self, row, a):
        self.words(ADDR["THINGS"] + 0x14 * row, [a])

    def set_info(self, create, destroy):
        self.words(self.dyn["INFO"] + 0x60, [create, destroy])

    def set_file(self, data):
        pass

    def set_known(self, i, handle, a):
        self.L.known[handle] = a

    def set_state(self, words):
        self.words(ADDR["STATE"], words)

    def set_head(self, a):
        self.words(ADDR["STATE"] + 0x10, [a])

    def get_head(self):
        return self.rd_words(ADDR["STATE"] + 0x10, 1)[0]

    def make_ride(self, slot, node):
        n = self.alloc(0x50)
        self.words(n, node)
        header = self.alloc(0x80)
        self.words(header + 0x78, [n])
        mesh = self.alloc(0x40)
        self.words(mesh + 4, [header])
        st = self.alloc(0x10)
        self.words(st + 8, [mesh])
        self.words(ADDR["RIDESETS"] + 4 * slot, [st])
        return st, mesh

    def block_info(self, a):
        return self.L.sizes.get(a, -1), a not in self.L.freed

    def freed_blocks(self):
        return set(self.L.freed)

    def call(self, op, args):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN[op], [a & MASK for a in args])
        return r3 & MASK

    def cstring(self, text):
        return self.e.alloc(len(text) + 1, text.encode() + b"\0")


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_rl_base()
        CL.host_begin(lib, {L_RIDEDB: self.g("RIDEDB"), L_FILE: self.g("FILE")}, case["queues"])
        lib.tpw_rl_clear()
        self.keep = []
        self.dead = set()
        data = (C.c_uint8 * max(len(case["file"]), 1))(*case["file"])
        self.keep.append(data)
        lib.tpw_rl_file(data, len(case["file"]))

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rl_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_rl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def label(self, a, n):
        self.lib.tpw_cl_label(n, a)

    def up(self, w):
        return self.base + w if w else 0

    def p(self, a):
        return a - self.base if a else 0

    def set_thing(self, row, a):
        self.lib.tpw_rl_thing(row, a)

    def set_info(self, create, destroy):
        self.lib.tpw_rl_info(create, destroy)

    def set_known(self, i, handle, a):
        self.lib.tpw_rl_known(i, handle, a)

    def set_state(self, words):
        self.write(self.g("STATE"), b"".join(struct.pack("<I", w & MASK) for w in words))

    def set_head(self, a):
        self.lib.tpw_rl_head(a)

    def get_head(self):
        return self.lib.tpw_rl_gethead()

    def make_ride(self, slot, node):
        arr = (C.c_int64 * 20)(*node)
        out = (C.c_uint64 * 2)()
        self.lib.tpw_rl_ride(slot, arr, out)
        return out[0], out[1]

    def block_info(self, a):
        size = C.c_uint64()
        live = self.lib.tpw_rl_live(a, C.byref(size))
        return size.value, bool(live)

    def call(self, op, args):
        f = getattr(self.lib, "tpw_rl_" + op)
        v = f(*(list(args) + [0] * (5 - len(args))))
        return v

    def cstring(self, text):
        buf = C.create_string_buffer(text.encode())
        self.keep.append(buf)
        return C.addressof(buf)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_rl_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "base").restype = C.c_uint64
    getattr(lib, p + "gethead").restype = C.c_uint64
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rl_thing.argtypes = [C.c_uint32, C.c_uint64]
    lib.tpw_rl_info.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_rl_file.argtypes = [C.c_void_p, C.c_int32]
    lib.tpw_rl_known.argtypes = [C.c_int32, C.c_int32, C.c_uint64]
    lib.tpw_rl_head.argtypes = [C.c_uint64]
    lib.tpw_rl_ride.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_uint64)]
    lib.tpw_rl_live.argtypes = [C.c_uint64, C.POINTER(C.c_uint64)]
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    for n in FN:
        f = getattr(lib, p + n)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64] * 5
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def rec_label(i, k=0):
    return 100 + 16 * i + k


def build(s, c):
    env = {"names": {}, "blocks": {}, "recs": [], "dead": []}
    g = c["g"]
    s.words(s.g("F7BE0"), [g["f7be0"]])
    s.words(s.g("SUM"), [g["sum"]])
    s.set_info(*g["info"])
    for row in range(7):
        t = g["things"][row]
        if t is None:
            s.set_thing(row, 0)
        else:
            b = s.alloc(0x20)
            s.write(b + 0xE, struct.pack(s.fmt[0] + "H", t[0]))
            s.words(b + 0x1C, [t[1]])
            s.set_thing(row, b)
    for slot in range(1, 4):
        st, mesh = s.make_ride(slot, c["nodes"][slot - 1])
        env["names"][st] = 10 + slot
        env["names"][mesh] = 20 + slot
        s.label(st, 10 + slot)
        s.label(mesh, 20 + slot)
    recs = c["recs"]
    addrs = [s.alloc(0xF4) for _ in recs]
    env["recs"] = addrs
    for i, r in enumerate(recs):
        a = addrs[i]
        s.label(a, rec_label(i))
        env["names"][a] = rec_label(i)
        vals = list(r["fill"])
        put = lambda off, v: vals.__setitem__(s.field_index(off), v)
        nxt = addrs[i + 1] if i + 1 < len(recs) else 0
        prv = addrs[i - 1] if i > 0 else 0
        put(0x00, s.p(nxt))
        put(0x04, s.p(prv))
        for off, v in ((0x08, r["handle"]), (0x0C, r["linked"]), (0x10, r["f10"]), (0x14, r["f14"]), (0x4C, r["nsave"]),
                       (0x7C, r["nwalk"]), (0x9C, r["fly"]), (0xAC, r["thing"]), (0xC8, r["ride"]), (0xCC, r["fcc"][0]),
                       (0xD0, r["fcc"][1]), (0xD4, r["fcc"][2])):
            put(off, v)
        vals[s.field_index(0xB4) + 5] = r["fb9"]
        tables = {0x18: 1, 0x1C: 2, 0x20: 3, 0x24: 4, 0x28: 5, 0x2C: 6, 0x30: 7, 0x34: 8, 0x38: 9}
        for off, k in tables.items():
            if not r["has"][k - 1]:
                put(off, 0)
                continue
            if off == 0x2C:
                nbytes = max(r["nwalk_alloc"], 1) * 32
            elif off == 0x30:
                nbytes = max(r["nsave_alloc"], 4) * 4
            else:
                nbytes = 16
            b = s.alloc(nbytes)
            if off == 0x30:
                s.words(b, r["savewords"][:nbytes // 4] + [0] * (nbytes // 4 - len(r["savewords"])))
            elif off == 0x2C:
                for j, (kind, state, to) in enumerate(r["walks"][:nbytes // 32]):
                    s.write(b + 32 * j, bytes(32))
                    w = b + 32 * j
                    s.write(w + 2, struct.pack(s.fmt[0] + "h", to))
                    s.write(w + 0x16, struct.pack(s.fmt[0] + "h", kind))
                    s.write(w + 0x18, struct.pack(s.fmt[0] + "h", state))
                    s.write(w + 0x1C, bytes([0x11, 0x11, 0x20 + j, 0x20 + j]))
            else:
                s.words(b, [0x1000 * k + j for j in range(nbytes // 4)])
            s.label(b, rec_label(i, k))
            env["names"][b] = rec_label(i, k)
            env["blocks"][rec_label(i, k)] = (b, nbytes, off)
            put(off, s.p(b))
        objs = []
        for j, o in enumerate(r["objs"]):
            ob = s.alloc(0x1C)
            s.label(ob, rec_label(i, 10 + j))
            env["names"][ob] = rec_label(i, 10 + j)
            env["blocks"][rec_label(i, 10 + j)] = (ob, 0x1C, None)
            objs.append(ob)
        for j, o in enumerate(r["objs"]):
            nx = objs[j + 1] if j + 1 < len(objs) else 0
            pv = objs[j - 1] if j > 0 else 0
            s.words(objs[j], [s.p(nx), s.p(pv), o[0], o[1], 0x77, 0x88, 0x99])
        put(0xB0, s.p(objs[0]) if objs else 0)
        s.put_fields(a, vals)
        env["blocks"][rec_label(i)] = (a, 0xF4, None)
        s.set_known(i, r["handle"], a)
    s.set_state([g["init"], g["f04"], g["f08"], g["count"]])
    s.set_head(addrs[0] if addrs else 0)
    env["path"] = s.cstring(c["path"]) if "path" in c else 0
    return env


def blocks_state(s, env):
    """{label: (live, size)} of every block the world built."""
    out = {}
    for label, (a, size, off) in env["blocks"].items():
        sz, live = s.block_info(a)
        out[label] = live
    return out


def snap_record(s, env, a, fresh):
    fields = s.get_fields(a)
    i = 0
    out = []
    for o, k, n in FIELDS:
        cnt = 1 if k == "W" else n
        for j in range(cnt):
            v = fields[i]
            if k == "W" and o in PTR_OFFS:
                nat = s.up(v)
                if o in (0x00, 0x04):
                    v = "new" if nat == env.get("new") else env["names"].get(nat, 1 if nat else 0)
                elif fresh:
                    v = 1 if nat else 0
                else:
                    v = env["names"].get(nat, ("?", 1 if nat else 0))
            out.append(v)
            i += 1
    return out


def observe(s, c, env, answer):
    out = {}
    head = s.get_head()
    names = env["names"]
    new = None
    if c["op"] == "RSSE_InitScript" and head and head not in names:
        new = head
        env["new"] = head
    nm = lambda a: 0 if not a else ("new" if a == new else names.get(a, ("?", a)))
    if c["op"] == "KillScript":
        out["answer"] = nm(answer)
    elif c["op"] == "RSSE_InitScript":
        out["answer"] = answer & MASK
    out["state"] = s.rd_words(s.g("STATE"), 4) + s.rd_words(s.g("F7BE0"), 1)
    out["head"] = nm(head)
    chain, a, seen = [], head, 0
    freed_recs = set()
    state = blocks_state(s, env)
    out["live"] = sorted(label for label, live in state.items() if not live)
    while a and seen < 12:
        label = nm(a)
        if label != "new" and isinstance(label, int) and not state.get(label, True):
            chain.append((label, "freed"))
            break
        chain.append((label, snap_record(s, env, a, label == "new")))
        a = s.up(s.rd_words(a, 1)[0])
        seen += 1
    out["chain"] = chain
    # the records not on the chain and still live
    if c["op"] != "RSSE_InitScript":
        others = []
        for i, a in enumerate(env["recs"]):
            if state[rec_label(i)]:
                others.append((rec_label(i), snap_record(s, env, a, False)))
        out["others"] = others
    # the freed tables, from byte 8 on (the arena writes its link into the first eight)
    freed_bytes = {}
    for label, (a, size, off) in env["blocks"].items():
        if not state[label] and off in (0x2C, 0x30):
            freed_bytes[label] = s.rd_halfs(a + 8, (size - 8) // 2) if off == 0x2C else s.rd_words(a + 8, (size - 8) // 4)
    out["freed_bytes"] = freed_bytes
    if new is not None:
        fresh = {}
        sizes = {}
        words = s.get_fields(new)
        for off in TABLE_OFFS:
            ptr = s.up(s.rd_words(new + off, 1)[0])
            if not ptr:
                continue
            size, _ = s.block_info(ptr)
            sizes[off] = size
            if off == 0x20:
                continue
            fresh[off] = s.read(ptr, size) if off != 0x18 else s.rd_words(ptr, size // 4)
        out["new_sizes"] = sizes
        out["new_tables"] = fresh
        out["new_block"] = s.block_info(new)[0]
    out["log"] = s.log()
    return out


def execute(s, c, env):
    if c["op"] == "RSSE_InitScript":
        return s.call(c["op"], [env["path"], c["thing"], c["recording"]])
    if c["op"] == "KillScript":
        target = env["recs"][c["target"]] if c["target"] is not None else 0
        return s.call(c["op"], [target, c["flag"]])
    return s.call(c["op"], [c["handle"], c["flag"]])


def run(s, c):
    try:
        env = build(s, c)
        ans = execute(s, c, env)
        return observe(s, c, env, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

TAG = 0x45535352
SPECIAL = [0, 0x80000000, 0x3F800000, 0xBF800000, 0x00000001, 0x49742400]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-50, 50))
    if k < 0.9:
        return fb(r.uniform(-3000, 3000))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-6, 6))


def ri(r):
    return r.choice([0, 1, 2, 7, 100, r.randrange(0, 1000), r.getrandbits(32)])


def mk_queues(r, heads=None):
    q = {"TPFile_Open": [r.choice([1, 1, 1, 1, 0])]}
    h = r.choice([0, 0, 1, 2, 3, 5]) if heads is None else heads
    q["FindDataNodeIndexByID"] = [r.randrange(0, 8) for _ in range(h)] + [-1]
    q["grmba"] = [r.choice([1, 1, 1, 0]) for _ in range(r.randrange(1, 3))]
    for k in ("bx0", "bx1", "bx2", "by0", "by1", "by2"):
        q[k] = [rf(r), rf(r)]
    q["PAR_InitEmitter"] = [r.choice([0, 1, r.randrange(1, 100), ri(r)])]
    q["SOUND_AddObject"] = [r.choice([0, r.randrange(1, 100), ri(r)])]
    return q


def mk_g(r):
    things = [None] + [(r.getrandbits(16), r.choice([0, 1, 2, 3, 1, 2])) for _ in range(6)]
    if r.random() < 0.08:
        things[0] = (r.getrandbits(16), 1)
    return {"f7be0": r.choice([0, 1, 5, 20, -1]), "sum": r.getrandbits(32), "info": (r.choice([0, 0, 1, r.randrange(1, 90), ri(r)]),
            r.choice([0, 0, 1, r.randrange(1, 90), ri(r)])), "things": things, "init": r.choice([1, 1, 1, 1, 0]),
            "f04": r.randrange(0, 40), "f08": r.choice([0, 1, 5, 100, r.getrandbits(32)]), "count": r.randrange(0, 10)}


def mk_nodes(r):
    nodes = []
    for _ in range(3):
        n = [r.getrandbits(32) for _ in range(20)]
        n[17] = rf(r)
        nodes.append(n)
    return nodes


def mk_rec(r, handle):
    nsave = r.choice([0, 0, 1, 2, 3, 5])
    nwalk = r.choice([0, 0, 1, 2, 3, 5])
    has = [r.random() < 0.8 for _ in range(9)]
    if nwalk:
        has[5] = True
    if nsave:
        has[6] = True
    return {"fill": [r.getrandbits(32) for _ in range(NFIELDS)], "handle": handle, "linked": 0, "f10": 0, "f14": 0,
            "nsave": nsave, "nwalk": nwalk, "nsave_alloc": nsave, "nwalk_alloc": nwalk, "has": has,
            "savewords": [r.choice([0, 1, 2, 5, r.getrandbits(32)]) for _ in range(nsave)],
            "walks": [(r.choice([0, 1, 2, 3, 4, 4, 4, 5, 6]), r.choice([0, 1, 2, 2, 3]), r.randrange(-3, 8)) for _ in range(nwalk)],
            "fly": r.choice([0, 1, 7, ri(r)]), "thing": r.choice([0, 0, 1, 2, 3, 4, 5, 6]), "ride": r.choice([0, 1, 2, 3]),
            "fcc": [r.choice([0, 0, r.randrange(1, 60)]), r.choice([0, 0, r.randrange(1, 60)]), r.choice([0, 0, r.randrange(1, 60)])],
            "fb9": r.choice([0, 0, 1, 255]),
            "objs": [(r.choice([-1, 0, 1, 2, 3, 4, 5, 8, 10, 11, 12]), r.choice([0, 1, 2, 9, r.randrange(1, 100)]))
                     for _ in range(r.choice([0, 0, 1, 2, 3, 4]))]}


def base(r, op, nrecs=None):
    n = r.randrange(0, 4) if nrecs is None else nrecs
    hs = r.sample(range(1, 40), n)
    c = {"op": op, "queues": mk_queues(r), "g": mk_g(r), "nodes": mk_nodes(r), "recs": [mk_rec(r, h) for h in hs], "file": b""}
    return c


def fix_rides(c):
    """The ride sets exist for 1..3 only: a record that walks heads (flag bit 4) has a ride."""
    if c["op"] == "RSSE_InitScript":
        return c
    for rec in c["recs"]:
        if (c["flag"] & 4) and (rec["nsave"] or rec["nwalk"]) and rec["ride"] == 0:
            rec["ride"] = 1
        if not (c["flag"] & 4) and False:
            pass
        rec["thing"] = rec["thing"]
    return c


def stream(r, tag=TAG, ver=0, counts=None, drops=None, prog=None, nbytes=None, body=None, cut=None):
    counts = counts if counts is not None else [r.choice([0, 1, 3, 6]) for _ in range(6)]
    prog = prog if prog is not None else [r.getrandbits(32) for _ in range(r.choice([1, 2, 5, 12]))]
    nbytes = nbytes if nbytes is not None else r.choice([0, 1, 3, 16, 40])
    words = [tag, ver] + counts + (drops or [r.getrandbits(32) for _ in range(4)]) + [len(prog)] + prog + [nbytes]
    data = b"".join(struct.pack("<I", w & MASK) for w in words)
    data += bytes(r.getrandbits(8) for _ in range(nbytes))
    return data if cut is None else data[:cut]


def mk_path(r):
    k = r.random()
    if k < 0.1:
        return ""
    parts = ["".join(r.choice("abcXYZ_09.") for _ in range(r.randrange(0, 12))) for _ in range(r.choice([1, 2, 3, 4]))]
    s = r.choice([":", "", "a:"]).join(parts) if False else ":".join(parts[:r.choice([1, 2, 4])])
    if k > 0.95:
        s = "p" * r.randrange(100, 250)
    return s


def init_case(r, **kw):
    c = base(r, "RSSE_InitScript")
    g = c["g"]
    ver_ok = kw.pop("ver_ok", r.random() < 0.9)
    tag = kw.pop("tag", TAG if r.random() < 0.95 else r.getrandbits(32))
    c["file"] = kw.pop("file", None) or stream(r, tag=tag, ver=g["sum"] if ver_ok else g["sum"] ^ 1, **kw.pop("st", {}))
    c["path"] = kw.pop("path", None) if "path" in kw else mk_path(r)
    if c["path"] is None:
        c["path"] = mk_path(r)
    tid = kw.pop("tid", r.choice([0, 1, 2, 3, 4, 5, 6]))
    c["thing"] = (tid << 16) | r.getrandbits(16)
    c["recording"] = kw.pop("recording", r.choice([0, 0, 1, 5]))
    for k, v in kw.items():
        if k.startswith("g_"):
            g[k[2:]] = v
        elif k.startswith("q_"):
            c["queues"][k[2:]] = v
    return c


def init_edges():
    r = random.Random(1690)
    yield init_case(r, g_init=0)
    yield init_case(r, q_TPFile_Open=[0])
    yield init_case(r, tag=1)
    yield init_case(r, ver_ok=False)
    for z in (0x40000000, 0x40000000, 0x20000000, 0x10000000, 0x08000000):
        for i in range(1):
            cnt = [2, 2, 2, 2, 2, 2]
            idx = (0x40000000, 0x20000000, 0x10000000, 0x08000000).index(z) if z != 0x40000000 else 0
            cnt[{0x40000000: 0, 0x20000000: 3, 0x10000000: 4, 0x08000000: 5}[z]] = z
            yield init_case(r, st={"counts": cnt}, tid=0)
    yield init_case(r, st={"counts": [2, 0x40000000, 2, 2, 2, 2]}, tid=0)
    for cnt in ([0] * 6, [1] * 6, [6] * 6, [3, 0, 5, 0, 2, 0]):
        yield init_case(r, st={"counts": cnt}, tid=0)
    for prog in ([], [1], list(range(12))):
        for nb in (0, 1, 40):
            yield init_case(r, st={"prog": prog, "nbytes": nb})
    for cut in (0, 3, 4, 7, 8, 12, 20, 30, 40, 60):
        yield init_case(r, st={"cut": cut})
    for path in ("", "a", "abc", "a:b", "a:b:c", ":x", "x:", ":", "::", "dir:sub:file.rsse", "p" * 250):
        yield init_case(r, path=path or "", tid=0)
    for tid in range(0, 7):
        for recording in (0, 1):
            for effect in (0, 1):
                for grmba in ([1], [0], [1, 0], [1, 1]):
                    c = init_case(r, tid=tid, recording=recording, g_info=(effect * 7, 3), q_grmba=grmba,
                                  q_FindDataNodeIndexByID=[0, 1, -1] if tid & 1 else [-1])
                    c["g"]["things"][0] = None
                    yield c
    for heads in range(0, 6):
        yield init_case(r, tid=r.randrange(1, 7), recording=1, q_FindDataNodeIndexByID=list(range(heads)) + [-1])
    c = init_case(r, tid=3, recording=0, g_info=(5, 0))
    c["g"]["things"][0] = (1, 1)
    yield c


def init_random(r):
    c = init_case(r)
    c["file"] = stream(r, ver=c["g"]["sum"], counts=[r.choice([0, 1, 2, 3, 6, 9]) for _ in range(6)]) if r.random() < 0.8 else c["file"]
    if r.random() < 0.15:
        c["file"] = c["file"][:r.randrange(0, len(c["file"]) + 1)]
    return c


def kill_case(r, **kw):
    c = base(r, "KillScript", nrecs=kw.pop("nrecs", r.randrange(1, 5)))
    c["flag"] = kw.pop("flag", r.choice([0, 4, 4, r.getrandbits(8), 0xFF]))
    c["target"] = kw.pop("target", r.randrange(0, len(c["recs"])))
    c["queues"]["FindDataNodeIndexByID"] = [r.choice([-1, 0, 1, 3]) for _ in range(r.randrange(1, 5))]
    for k, v in kw.items():
        if k.startswith("rec_"):
            c["recs"][c["target"]][k[4:]] = v
    return fix_rides(c)


def kill_edges():
    r = random.Random(1691)
    c = kill_case(r)
    c["target"] = None
    yield c
    for pos in ("head", "tail", "mid", "alone"):
        n = 1 if pos == "alone" else 3
        t = {"head": 0, "tail": 2, "mid": 1, "alone": 0}[pos]
        yield kill_case(r, nrecs=n, target=t)
    for guard in ("fb9", "fly", "fcc0", "fcc1"):
        for v in (0, 5):
            c = kill_case(r, nrecs=2, target=0)
            rec = c["recs"][0]
            rec.update({"fb9": 0, "fly": 0, "ride": 0, "nsave": 0, "nwalk": 0})
            if guard == "fb9":
                rec["fb9"] = v
            elif guard == "fly":
                rec["fly"] = 7
                rec["ride"] = v
            elif guard == "fcc0":
                rec["fcc"][0] = v
            else:
                rec["fcc"][1] = v
            c["flag"] = 0
            yield fix_rides(c)
    for flag in (0, 4, 0xFF):
        for n in (1, 3, 5):
            for kind in (4, 2):
                c = kill_case(r, nrecs=1, target=0, flag=flag)
                rec = c["recs"][0]
                rec["ride"] = 2
                rec["nsave"] = rec["nwalk"] = n
                rec["has"] = [True] * 9
                rec["nsave_alloc"] = rec["nwalk_alloc"] = n
                rec["savewords"] = [r.choice([0, 1, 4]) for _ in range(n)]
                rec["walks"] = [(r.choice([kind, 4, 1]), r.choice([0, 1, 2, 3]), r.randrange(-2, 6)) for _ in range(n)]
                c["queues"]["FindDataNodeIndexByID"] = [r.choice([-1, 0, 2]) for _ in range(4)]
                yield c
    for mask in range(512):
        if mask % 7 and mask not in (0, 511):
            continue
        c = kill_case(r, nrecs=2, target=1)
        c["recs"][1]["has"] = [bool(mask >> i & 1) for i in range(9)]
        c["recs"][1]["nsave"] = c["recs"][1]["nwalk"] = 0
        c["recs"][1]["savewords"] = c["recs"][1]["walks"] = []
        yield fix_rides(c)
    for n in range(0, 5):
        for types in ([1], [2], [3], [4, 10], [0], [-1], [11], [12], [1, 3, 11, 5]):
            c = kill_case(r, nrecs=2, target=0)
            c["recs"][0]["objs"] = [(types[i % len(types)], i + 1) for i in range(n)]
            yield c


def kill_random(r):
    c = kill_case(r)
    if r.random() < 0.05:
        c["target"] = None
    return c


def rk_case(r, **kw):
    c = base(r, "RSSE_KillScript", nrecs=kw.pop("nrecs", r.randrange(1, 5)))
    c["flag"] = kw.pop("flag", r.choice([0, 2, 2, 4, 6, r.getrandbits(8)]))
    t = kw.pop("target", r.randrange(0, len(c["recs"])))
    rec = c["recs"][t]
    c["handle"] = rec["handle"]
    others = [x for i, x in enumerate(c["recs"]) if i != t]
    r.shuffle(others)
    for key in ("linked", "f14"):
        if others and r.random() < 0.4:
            rec[key] = others.pop()["handle"]
        elif r.random() < 0.1:
            rec[key] = 99 + r.randrange(0, 5)
    if r.random() < 0.4:
        rec["f10"] = rec["handle"] if r.random() < 0.5 and not others else (others.pop()["handle"] if others else rec["handle"])
    for x in c["recs"]:
        x["thing"] = r.choice([0, 1, 2, 3, 4, 5, 6])
    for k, v in kw.items():
        if k.startswith("rec_"):
            rec[k[4:]] = v
        elif k.startswith("g_"):
            c["g"][k[2:]] = v
        elif k.startswith("q_"):
            c["queues"][k[2:]] = v
    c["g"]["things"][0] = None
    return fix_rides(c)


def rk_edges():
    r = random.Random(1692)
    c = rk_case(r, g_init=0)
    yield c
    c = rk_case(r)
    c["handle"] = 77
    yield c
    c = rk_case(r)
    c["handle"] = 0
    yield c
    for thing in (0, 3):
        for eff in (0, 9):
            for flag in (0, 2, 4, 6):
                for ride in (0, 2):
                    for grmba in ([1], [0], [1, 0]):
                        for fcc2 in (0, 12):
                            yield rk_case(r, nrecs=1, flag=flag, target=0, rec_thing=thing, rec_ride=ride, g_info=(1, eff),
                                          q_grmba=grmba, rec_fcc=[0, 0, fcc2], rec_nsave=0, rec_nwalk=0)
    for k in range(40):
        c = rk_case(r, nrecs=4, target=0)
        yield c


def rk_random(r):
    return rk_case(r)


TESTS = [
    FnTest("RSSE_InitScript", ppc, host, edge=init_edges, rnd=init_random),
    FnTest("KillScript", ppc, host, edge=kill_edges, rnd=kill_random),
    FnTest("RSSE_KillScript", ppc, host, edge=rk_edges, rnd=rk_random),
]
