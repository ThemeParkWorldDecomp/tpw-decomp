"""Unit v5934: the ride sign regeneration and the object instance table, against libtpw_unit_RegenerateRideSign.dylib
(tests/unit_RegenerateRideSign.cpp), main app ("SimTheme Park"), src/game/rides/RidePlacement.cpp:
  RegenerateRideSign 0x101eeb9c, RegenerateRideSigns 0x101eeeb4, SetRideScriptID 0x101eefd0, GetAnimStats 0x101ef044,
  AllocateObjectInstanceToTable 0x101ef1f4, RemoveObjectInstanceFromTable 0x101ef310

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, 64-bit pointers: the structs' offsets come from the library's layout
export) for the translation. The callees outside the unit are fakes on both sides: CObject::GetDBInfo, GetSignNameA and B (they write a
two character sign from the case's queue), CSYS_TPFileInfo::GetLocalPath, the string template's constructors and destructors,
UniStringToString (answers the case's queue, points the narrow string at a name buffer), GenerateRideSign (the folder, name, font path
and both signs as hashes), SetCoasterScriptID and GAME_ASSERTMSG. Each records its arguments.

The world of a case: the renderer word of the player settings, the ride sets (flags, +0xd4 word), the ride record (folder, item name),
the local path, the world's object list head and end marker, the thing array rows with their objects (+0x1c instance, +0xc link), and for
the table functions the three counters and the table. What is compared: the answer, the call log, every set's flags and +0xd4 word,
the counters and the table.

No fused operations. Not tested: UniStringToString answering 0 in RegenerateRideSign (the original then answers its caller's r31, the
translation 0). Table indexes past the 64 slots the test builds.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_RegenerateRideSign"
MASK = 0xFFFFFFFF

FN_SIGN, FN_SIGNS, FN_SCRIPT, FN_STATS, FN_ALLOC, FN_REMOVE = 0x101EEB9C, 0x101EEEB4, 0x101EEFD0, 0x101EF044, 0x101EF1F4, 0x101EF310
SIZES = {FN_SIGN: 400, FN_SIGNS: 240, FN_SCRIPT: 76, FN_STATS: 300, FN_ALLOC: 212, FN_REMOVE: 220}
OWN = set(SIZES)

ADDR = dict(rideanimsets=0x102C79A4, things=0x1034CBFC, worldptr=0x1037EBFC, settings=0x1038071C, listend=0x102B12E6)

MAC = dict(P=4, flags=4, anims=0xAC, groups=0x18, gstride=0xC, gcount=4, gent=8, estride=8, eanim=4, fd4=0xD4, setsize=0x100,
           folder=0x28C, item=0x68C, infosize=0x800, tsize=0x14, wlist=0x1DA746, mcurrent=0, mdata=8)
WORLD = {}


def fnv(vals):
    h = 0x811C9DC5
    for v in vals:
        h = ((h ^ (v & MASK)) * 0x01000193) & MASK
    return h


def pstr(e, a):
    if a == 0:
        return MASK
    out = []
    while True:
        b = e.read(a, 1)[0]
        if b == 0:
            return fnv(out)
        out.append(b)
        a += 1


def pwide(e, a):
    if a == 0:
        return MASK
    out = []
    while True:
        v = struct.unpack(">H", e.read(a, 2))[0]
        if v == 0:
            return fnv(out)
        out.append(v)
        a += 2


CUR = None


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s_dbinfo(e):
    e.cl.rec("GetDBInfo", (0,))
    e.set_r(3, CUR.info)


def s_path(e):
    e.cl.rec("GetLocalPath", (0,))
    e.set_r(3, CUR.pathobj)


def s_sign(name):
    def stub(e):
        L = e.cl
        this, buf, size = e.r(3), e.r(4), e.r(5)
        L.rec(name, (e.u32(this + 0x1C), size))
        k = L.ret(name)
        e.write(buf, struct.pack(">3H", 0x41 + (k & 15), 0x61 + (k & 15), 0))
    return stub


def s_wctor(e):
    text = e.r(4)
    e.write(e.r(3) + 8, struct.pack(">I", text))
    e.cl.rec("WCtor", (pwide(e, text),))


def s_cctor(e):
    e.cl.rec("CCtor", (0,))


def s_wdtor(e):
    e.cl.rec("WDtor", (0,))


def s_cdtor(e):
    e.cl.rec("CDtor", (0,))


def s_uni(e):
    L = e.cl
    L.rec("Uni", (pwide(e, e.u32(e.r(4) + 8)),))
    e.write(e.r(3) + 8, struct.pack(">I", CUR.namebuf))
    e.set_r(3, L.ret("Uni") & MASK)


def s_gen(e):
    L = e.cl
    L.rec("GenerateRideSign", (L.lab(e.r(3)), pstr(e, e.r(4)), pstr(e, e.r(5)), pstr(e, e.r(6)), pwide(e, e.r(7)), pwide(e, e.r(8))))
    e.set_r(3, L.ret("GenerateRideSign") & MASK)


STUBS.internal(0x10107CD4, s_dbinfo)
STUBS.internal(0x10004D4C, s_path)
STUBS.internal(0x101107C4, s_sign("SignA"))
STUBS.internal(0x101108DC, s_sign("SignB"))
STUBS.internal(0x10238800, s_wctor)
STUBS.internal(0x10238458, s_cctor)
STUBS.internal(0x10238710, s_wdtor)
STUBS.internal(0x102382F0, s_cdtor)
STUBS.internal(0x10005670, s_uni)
STUBS.internal(0x101EE290, s_gen)
STUBS.internal(0x1000DDE8, CL.recorder("Assert", "is"))
STUBS.internal(0x1004373C, CL.recorder("SetCoaster", "ui"))


def setup(runner):
    T.install_std_stubs(runner.emu)
    WORLD["ppc"] = T.persist_alloc(runner.emu, 0x1DA800)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.hfmt, v & 0xFFFF))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_ptrs(self, a, n):
        return list(struct.unpack(self.pfmt[0] + "%d%s" % (n, self.pfmt[1]), self.read(a, n * self.geo["P"])))

    def wide(self, a, text):
        self.write(a, b"".join(struct.pack(self.hfmt, ord(c)) for c in text) + b"\0\0")


class Ppc(Side):
    fmt, hfmt, pfmt = ">I", ">H", ">I"
    geo = MAC

    def __init__(self, e, case):
        global CUR
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.info = self.pathobj = self.namebuf = 0
        CUR = self

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def world(self):
        return WORLD["ppc"]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_info(self, info, pathobj, namebuf):
        self.info, self.pathobj, self.namebuf = info, pathobj, namebuf
        self.words(ADDR["worldptr"], [WORLD["ppc"]])

    def pathobj_new(self):
        return self.alloc(32)

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK

    def cstr(self, a):
        return self.e.cstr(a)


class Host(Side):
    fmt, hfmt, pfmt = "<I", "<H", "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.geo = lib.geo
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rs_addr(name.encode())

    def world(self):
        return self.g("world")

    def alloc(self, n):
        return self.lib.tpw_rs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_info(self, info, pathobj, namebuf):
        self.write(self.g("info"), struct.pack("<Q", info))
        self.write(self.g("namep"), struct.pack("<Q", namebuf))

    def pathobj_new(self):
        return self.g("pathobj")

    def cstr(self, a):
        return C.string_at(a).decode("latin1")


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rs_addr.restype, lib.tpw_rs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rs_alloc.restype, lib.tpw_rs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rs_RegenerateRideSign.restype, lib.tpw_rs_RegenerateRideSign.argtypes = C.c_int32, [C.c_uint64] * 3
    lib.tpw_rs_RegenerateRideSigns.restype, lib.tpw_rs_RegenerateRideSigns.argtypes = None, []
    lib.tpw_rs_SetRideScriptID.restype, lib.tpw_rs_SetRideScriptID.argtypes = None, [C.c_uint32, C.c_int32]
    lib.tpw_rs_GetAnimStats.restype, lib.tpw_rs_GetAnimStats.argtypes = C.c_uint64, [C.c_uint64, C.c_uint32] + [C.c_uint64] * 3
    lib.tpw_rs_Allocate.restype, lib.tpw_rs_Allocate.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32, C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_rs_Remove.restype, lib.tpw_rs_Remove.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rs_layout(out)
    v = list(out)[:n]
    lib.geo = dict(P=8, folder=v[0], item=v[1], infosize=v[2], flags=v[3], anims=v[4], groups=v[5], gstride=v[6], gcount=v[7], gent=v[8],
                   estride=v[9], eanim=v[10], fd4=v[11], setsize=v[12], tsize=v[13], wlist=v[14], wsize=v[15], mcurrent=v[16], mdata=v[17])
    assert lib.geo["mcurrent"] == 0, lib.geo
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    g = s.geo
    P = g["P"]
    # the player settings: the renderer word
    s.words(s.g("settings") + g["mcurrent"], [case["renderer"]])
    # the ride sets
    sets_base = s.g("rideanimsets")
    s.setaddr = [0] * 8
    for i in range(8):
        sd = case["sets"][i]
        if sd is not None:
            a = s.alloc(g["setsize"])
            s.words(a + g["flags"], [sd["flags"]])
            s.words(a + g["fd4"], [sd["fd4"]])
            s.setaddr[i] = a
        s.ptr(sets_base + i * P, s.setaddr[i])
    # the ride record
    info = 0
    if case["info"] is not None:
        info = s.alloc(g["infosize"])
        s.write(info + g["folder"], case["info"]["folder"].encode() + b"\0")
        s.wide(info + g["item"], case["info"]["item"])
    namebuf = s.alloc(16)
    s.write(namebuf, b"Nm\0")
    # the local path: a string object whose data pointer sits at +mdata; the string has a zero byte before it
    pobj = s.pathobj_new()
    if case["path"] is None:
        s.ptr(pobj + (8 if P == 4 else g["mdata"]), 0)
    else:
        blk = s.alloc(64)
        s.write(blk + 8, case["path"].encode() + b"\0")
        s.ptr(pobj + g["mdata"], blk + 8)
    s.set_info(info, pobj, namebuf)
    # the object list
    wbase = s.world()
    s.half(wbase + g["wlist"], case["head"])
    s.half(s.g("listend"), case["end"])
    tb = s.g("things")
    s.ptr(tb, 0x1234 if case["thing0"] else 0)
    for i in range(1, 8):
        a = 0
        if i in case["chain"]:
            inst, nxt = case["chain"][i]
            a = s.alloc(0x40)
            s.words(a + 0x1C, [inst])
            s.half(a + 0xC, nxt)
        s.ptr(tb + i * g["tsize"], a)


def sets_state(s):
    g = s.geo
    out = []
    for a in s.setaddr:
        out.append((s.rd_words(a + g["flags"], 1)[0], s.rd_words(a + g["fd4"], 1)[0]) if a else None)
    return out


def execute(s, case):
    g = s.geo
    P = g["P"]
    op = case["op"]
    res = {}
    if op == "sign":
        obj = s.alloc(0x40)
        s.words(obj + 0x1C, [case["inst"]])
        wa = wb = 0
        if case["signA"] is not None:
            wa = s.alloc(32)
            s.wide(wa, case["signA"])
        if case["signB"] is not None:
            wb = s.alloc(32)
            s.wide(wb, case["signB"])
        if isinstance(s, Ppc):
            res["answer"] = s.call(FN_SIGN, [obj, wa, wb])
        else:
            res["answer"] = s.lib.tpw_rs_RegenerateRideSign(obj, wa, wb) & MASK
    elif op == "signs":
        if isinstance(s, Ppc):
            s.call(FN_SIGNS, [])
        else:
            s.lib.tpw_rs_RegenerateRideSigns()
    elif op == "script":
        if isinstance(s, Ppc):
            s.call(FN_SCRIPT, [case["inst"], case["script"]])
        else:
            s.lib.tpw_rs_SetRideScriptID(case["inst"], case["script"] if case["script"] < 2 ** 31 else case["script"] - 2 ** 32)
    elif op == "stats":
        a = s.alloc(g["setsize"])
        anims = s.alloc(4 * P)
        for i, v in enumerate(case["anims"]):
            s.ptr(anims + i * P, v)
        s.ptr(a + g["anims"], anims)
        for gi, (count, entries) in enumerate(case["groups"]):
            ents = s.alloc(max(count, 1) * g["estride"])
            for k, v in enumerate(entries):
                s.words(ents + k * g["estride"], [k])
                s.ptr(ents + k * g["estride"] + g["eanim"], v)
            s.words(a + g["groups"] + gi * g["gstride"] + g["gcount"], [count])
            s.ptr(a + g["groups"] + gi * g["gstride"] + g["gent"], ents)
        out = s.alloc(12)
        s.words(out, [0xDEADBEEF] * 3)
        ptrs = [out + 4 * k if case["outs"][k] else 0 for k in range(3)]
        if isinstance(s, Ppc):
            r3 = s.call(FN_STATS, [a, case["index"]] + ptrs)
            res["name"] = s.cstr(r3)
        else:
            r3 = s.lib.tpw_rs_GetAnimStats(a, case["index"], *ptrs)
            res["name"] = s.cstr(r3)
        res["outs"] = s.rd_words(out, 3)
    elif op in ("alloc", "remove"):
        cnt = s.alloc(12)
        s.words(cnt, case["counters"])
        table = s.alloc(64 * P)
        for i, v in enumerate(case["table"]):
            s.ptr(table + i * P, v)
        if op == "alloc":
            if isinstance(s, Ppc):
                res["answer"] = s.call(FN_ALLOC, [cnt, cnt + 4, cnt + 8, case["limit"], table, 0x55, case["extra"]])
            else:
                res["answer"] = s.lib.tpw_rs_Allocate(cnt, case["limit"], table, 0x55, case["extra"]) & MASK
        else:
            if isinstance(s, Ppc):
                s.call(FN_REMOVE, [cnt, cnt + 4, cnt + 8, table, case["id"]])
            else:
                s.lib.tpw_rs_Remove(cnt, table, case["id"])
        res["counters"] = s.rd_words(cnt, 3)
        res["table"] = s.rd_ptrs(table, 64)
    res["sets"] = sets_state(s)
    res["log"] = s.log()
    return res


def run(s, case):
    try:
        build(s, case)
        return execute(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rtext(r, n):
    return "".join(r.choice("ABCxyz:") for _ in range(r.randrange(0, n + 1)))


def base(r, op):
    sets = [None]
    for _ in range(7):
        if r.random() < 0.15:
            sets.append(None)
        else:
            sets.append(dict(flags=r.choice([0, 1, 0x8000000, 0x8000001, 0xFFFFFFFF, 0x40000, 0x40001, 0x8040000, r.getrandbits(32)]),
                             fd4=r.getrandbits(32)))
    n = r.randrange(0, 7)
    ids = r.sample(range(1, 7), n)
    end = r.choice([0, 7, 9])
    chain = {}
    for k, i in enumerate(ids):
        chain[i] = (r.randrange(0, 8), ids[k + 1] if k + 1 < len(ids) else end)
    c = {"op": op, "sets": sets,
         "info": None if r.random() < 0.15 else dict(folder=rtext(r, 8), item=rtext(r, 4)),
         "path": r.choice([None, "", "D", "Disk:", "A:B", "X::", "Hd:Game"]),
         "renderer": r.choice([0, 1, 1, 2, MASK]),
         "head": ids[0] if ids else end, "end": end, "chain": chain, "thing0": r.random() < 0.1,
         "queues": {"GenerateRideSign": [r.choice([0, 1, 5, -1, 100]) for _ in range(4)], "Uni": [1],
                    "SignA": [r.randrange(0, 16) for _ in range(6)], "SignB": [r.randrange(0, 16) for _ in range(6)]}}
    return c


def sign_case(r, **over):
    c = base(r, "sign")
    c["inst"] = r.randrange(0, 8)
    c["signA"] = None if r.random() < 0.1 else rtext(r, 3)
    c["signB"] = None if r.random() < 0.1 else rtext(r, 3)
    c.update(over)
    return c


def sign_edges():
    r = random.Random(11)
    for over in ({"info": None}, {"inst": 0}, {"path": None}, {"path": ""}, {"path": "D:"}, {"path": "D"}, {"renderer": 0}, {"renderer": 1},
                 {"inst": 1, "info": dict(folder="F", item="I"), "renderer": 1}):
        for _ in range(4):
            yield sign_case(r, **over)
    for fl in (0, 1, 0x8000000, 0xFFFFFFFF):
        c = sign_case(r, inst=3, renderer=1, info=dict(folder="Fo", item="It"), path="Hd")
        c["sets"][3] = dict(flags=fl, fd4=5)
        yield c
    c = sign_case(r, inst=3, renderer=1, info=dict(folder="Fo", item="It"), path="Hd")
    c["sets"][3] = None
    yield c


def signs_case(r, **over):
    c = base(r, "signs")
    c["renderer"] = r.choice([0, 1, 1])
    if c["info"] is None and r.random() < 0.8:
        c["info"] = dict(folder=rtext(r, 8), item=rtext(r, 4))
    if c["path"] is None and r.random() < 0.8:
        c["path"] = "Hd"
    c.update(over)
    return c


def signs_edges():
    r = random.Random(12)
    for over in ({"head": 0, "end": 0, "chain": {}}, {"thing0": True}, {"renderer": 1}):
        for _ in range(6):
            yield signs_case(r, **over)
    for _ in range(10):
        c = signs_case(r, renderer=1, info=dict(folder="Fo", item="It"), path="Hd")
        for i in range(1, 8):
            c["sets"][i] = dict(flags=0x8000000, fd4=i)
        yield c


def script_case(r, **over):
    c = base(r, "script")
    for i in range(1, 8):
        if c["sets"][i] is None:
            c["sets"][i] = dict(flags=r.choice([0, 0x40000]), fd4=0)
    c["inst"] = r.randrange(0, 8)
    c["script"] = r.choice([0, 1, 0x7FFFFFFF, 0x80000000, MASK, r.getrandbits(32)])
    c.update(over)
    return c


def script_edges():
    r = random.Random(13)
    for inst in range(8):
        for fl in (0, 0x40000, 0xFFFBFFFF):
            c = script_case(r, inst=inst)
            if inst:
                c["sets"][inst] = dict(flags=fl, fd4=9)
            yield c


def stats_case(r, **over):
    c = base(r, "stats")
    c["anims"] = [r.randrange(0, 4) for _ in range(4)]
    c["groups"] = []
    for _ in range(12):
        count = r.choice([0, 0, 1, 2, 3])
        c["groups"].append((count, [r.randrange(0, 4) for _ in range(count)]))
    c["index"] = r.randrange(0, 4)
    c["outs"] = [r.random() < 0.7 for _ in range(3)]
    c.update(over)
    return c


def stats_edges():
    r = random.Random(14)
    for gi in range(12):
        for outs in ([True] * 3, [False] * 3, [True, False, True]):
            c = stats_case(r, outs=outs, index=1, anims=[0, 5, 0, 0])
            c["groups"] = [(0, [])] * 12
            c["groups"][gi] = (3, [7, 6, 5])
            c["groups"][gi] = (3, [7, 5, 5])
            yield c
    c = stats_case(r, index=0, anims=[9, 0, 0, 0])
    c["groups"] = [(2, [1, 2])] * 12
    yield c
    c = stats_case(r, index=0, anims=[2, 0, 0, 0])
    c["groups"] = [(2, [1, 2])] * 12
    yield c


def alloc_case(r, **over):
    c = base(r, "alloc")
    top = r.randrange(0, 12)
    holes = r.choice([0, 0, 1, 2, 5])
    low = r.choice([top + 1, top + 1, r.randrange(1, 20)])
    c["counters"] = [top, holes, low]
    c["limit"] = r.choice([0, top, top + 1, 5, 12])
    c["extra"] = MASK if r.random() < 0.75 else r.randrange(0, 40)
    c["table"] = [r.choice([0, 0x77]) for _ in range(64)]
    c.update(over)
    return c


def alloc_edges():
    r = random.Random(15)
    for top in (0, 3):
        for holes in (0, 2):
            for limit in (top, top + 1):
                for low in (top + 1, top + 3, 1):
                    c = alloc_case(r, counters=[top, holes, low], limit=limit, extra=MASK)
                    yield c
    yield alloc_case(r, extra=0)
    yield alloc_case(r, extra=39)
    c = alloc_case(r, counters=[3, 2, 2], limit=9, extra=MASK)
    c["table"] = [0x77] * 64
    yield c


def remove_case(r, **over):
    c = base(r, "remove")
    top = r.randrange(0, 25)
    holes = r.choice([0, 0, 1, 2, 5])
    c["counters"] = [top, holes, r.randrange(0, 30)]
    c["id"] = r.choice([0, top, top + holes, r.randrange(1, 40)])
    c["table"] = [r.choice([0, 0x77]) for _ in range(64)]
    c.update(over)
    return c


def remove_edges():
    r = random.Random(16)
    yield remove_case(r, id=0)
    for top in (0, 1, 5):
        for holes in (0, 1, 3):
            for id_ in (top, top + holes, top + holes + 1, 1, 2):
                yield remove_case(r, counters=[top, holes, 4], id=id_)
    c = remove_case(r, counters=[6, 3, 4], id=9)
    c["table"] = [0] * 64
    yield c
    c = remove_case(r, counters=[6, 3, 4], id=9)
    c["table"] = [0x77] * 64
    yield c
    yield remove_case(r, counters=[4, 1, 2], id=1)


TESTS = [
    FnTest("RegenerateRideSign", ppc, host, edge=sign_edges, rnd=lambda r: sign_case(r)),
    FnTest("RegenerateRideSigns", ppc, host, edge=signs_edges, rnd=lambda r: signs_case(r)),
    FnTest("SetRideScriptID", ppc, host, edge=script_edges, rnd=lambda r: script_case(r)),
    FnTest("GetAnimStats", ppc, host, edge=stats_edges, rnd=lambda r: stats_case(r)),
    FnTest("AllocateObjectInstanceToTable", ppc, host, edge=alloc_edges, rnd=lambda r: alloc_case(r)),
    FnTest("RemoveObjectInstanceFromTable", ppc, host, edge=remove_edges, rnd=lambda r: remove_case(r)),
]
