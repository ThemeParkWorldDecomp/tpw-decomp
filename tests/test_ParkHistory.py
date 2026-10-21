"""Unit w157, the analyser's history rings and value templates, against libtpw_unit_ParkHistory.dylib (tests/unit_ParkHistory.cpp), main app
("SimTheme Park"), src/game/park/analyser.cpp (43 addresses, one FnTest each):
  CHistory SetAtEnd (8 rings), GetValue (8), HowFarBack (7), GetSumOfLast (5), GetAverageOfLast (4), SetFromTemp (3), GetHighestValue (2),
  GetLowestValue (1); CParkAnalyserVal<int> Get 0x100ef3c8 and Set 0x100ef400; the three tag_IDX_TYPE default constructors 0x100e2ef0, 0x100f1d7c,
  0x10116ccc (a lone blr each).

A case is a ring and a call. The ring image (the Mac layout: mTemp, the elements, mCurrentEntry, mNumEntries, mWrappedAround, the padding at 0xA5) sits
between two 0x40-byte guards (0x5A); the answer slot (a T, or the double of GetSumOfLast) sits between two 0x10-byte guards. It is built twice by ONE
piece of Python code: in emulator memory (big-endian) for the original and in the host library's memory (little-endian) for the translation, and the
same code decodes both back (every field by its width, the padding and the guards byte by byte). The only callee outside the unit is GAME_ASSERTMSG, a
fake on both sides that records the condition and the text; CTimeSig::SetTimeSig (0x1015f02c) runs as the original on both sides over a world block
(mTurn at 0x1da70c, the global gWorld at 0x1037ec0c on the PowerPC, the library's own gWorld on the host).

Every index a case reads or writes lies inside the image and its guards (a Python model of each function decides; cases that would leave them are
dropped). The flag byte is 0 or 1 only (the host field is a bool). The ObjectId ring's value goes in the HIGH halfword of r4 (the tag passed by value),
random bits in the low half. A one-byte value comes with random bits above it in r4. NaNs are compared as one value (the processors make different
NaNs; a quiet NaN and a signalling NaN are both in the rings). No fused operations; fctiwz only sees means inside the int range.
Not tested: mNumEntries above N + 1 or negative with a read past the guards, a flag byte other than 0 or 1, GetLowestValue of the float ring (no
instance in the program).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ParkHistory"
MASK = 0xFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF

ASSERT, SETTIMESIG = 0x1000DDE8, 0x1015F02C
GWORLD = 0x1037EC0C
TURN_AT, WORLD_SIZE = 0x1DA70C, 0x1DA748
ASSERT_TEXT = ("CHistory - You asked for the sum of more elements than there is defined history for\n"
               "The request will be truncated, and you'll get what we can give\n")
GUARD, OUTG, PAD = 0x40, 0x10, 0xA5


class Ring:
    """A ring type by the Mac layout: element width, the offsets and the kind (u8, i32, u32, f32, tag)."""

    def __init__(self, n, es, kind, data, cur, num, wrap, size):
        self.n, self.es, self.kind = n, es, kind
        self.data, self.cur, self.num, self.wrap, self.size = data, cur, num, wrap, size
        self.code = {1: "B", 2: "H", 4: "I"}[es]


RINGS = [
    Ring(50, 1, "u8", 1, 0x34, 0x38, 0x3C, 0x40),        # 0 <unsigned char,50>
    Ring(50, 4, "i32", 4, 0xCC, 0xD0, 0xD4, 0xD8),       # 1 <int,50>
    Ring(144, 4, "f32", 4, 0x244, 0x248, 0x24C, 0x250),  # 2 <float,144>
    Ring(144, 4, "i32", 4, 0x244, 0x248, 0x24C, 0x250),  # 3 <int,144>
    Ring(3, 2, "tag", 2, 8, 0xC, 0x10, 0x14),            # 4 <ObjectId,3>
    Ring(5, 4, "i32", 4, 0x18, 0x1C, 0x20, 0x24),        # 5 <int,5>
    Ring(30, 4, "i32", 4, 0x7C, 0x80, 0x84, 0x88),       # 6 <int,30>
    Ring(30, 4, "u32", 4, 0x7C, 0x80, 0x84, 0x88),       # 7 <unsigned int,30>
]
HOST_LAYOUT = []
for _r in RINGS:
    HOST_LAYOUT += [_r.size, _r.data, _r.cur, _r.num, _r.wrap]

# (address, ring, operation, name): the 43 addresses of the manifest
FUNCS = []
for _a, _t in ((0x100EF8C4, 0), (0x100F0114, 1), (0x100F069C, 2), (0x100F09DC, 3), (0x100F0E38, 4), (0x100F2944, 5), (0x10111D14, 6), (0x10111E24, 7)):
    FUNCS.append((_a, _t, "set", "CHistory::SetAtEnd"))
for _a, _t in ((0x100F0F04, 4), (0x100F1DCC, 0), (0x100F1E68, 1), (0x100F1F0C, 7), (0x1001BF6C, 5), (0x1001C2A8, 3), (0x10111D80, 6), (0x101F6854, 2)):
    FUNCS.append((_a, _t, "get", "CHistory::GetValue"))
for _a, _t in ((0x100EFB1C, 0), (0x100F0370, 1), (0x100F1134, 7), (0x1001C010, 5), (0x1001C254, 3), (0x101114C0, 6), (0x101F69D8, 2)):
    FUNCS.append((_a, _t, "back", "CHistory::HowFarBack"))
for _a, _t in ((0x100EF9E8, 0), (0x100F0238, 1), (0x100F1000, 7), (0x1001C11C, 3), (0x1011132C, 6)):
    FUNCS.append((_a, _t, "sum", "CHistory::GetSumOfLast"))
for _a, _t in ((0x100EF930, 0), (0x100F0180, 1), (0x1001C064, 3), (0x10111274, 6)):
    FUNCS.append((_a, _t, "avg", "CHistory::GetAverageOfLast"))
for _a, _t in ((0x100F0A48, 3), (0x10111464, 6), (0x101117E8, 7)):
    FUNCS.append((_a, _t, "temp", "CHistory::SetFromTemp"))
FUNCS += [(0x101F68F8, 2, "high", "CHistory::GetHighestValue"), (0x101F6A2C, 3, "high", "CHistory::GetHighestValue"),
          (0x101F6B0C, 3, "low", "CHistory::GetLowestValue"),
          (0x100EF3C8, None, "valget", "CParkAnalyserVal::Get"), (0x100EF400, None, "valset", "CParkAnalyserVal::Set"),
          (0x100E2EF0, None, "tag0", "tag_IDX_TYPE::tag_IDX_TYPE"), (0x100F1D7C, None, "tag1", "tag_IDX_TYPE::tag_IDX_TYPE"),
          (0x10116CCC, None, "tag2", "tag_IDX_TYPE::tag_IDX_TYPE")]
assert len(FUNCS) == 43
OWN = {f[0] for f in FUNCS} | {SETTIMESIG}


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fake

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_assert(e):
    """GAME_ASSERTMSG(int condition, const char* format, ...): the condition and the text."""
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4)),))


STUBS.internal(ASSERT, stub_assert)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def pack(self, code, v):
        return struct.pack(self.end + code.upper(), v & ((1 << (8 * struct.calcsize(code))) - 1))

    def unpack(self, code, data):
        return struct.unpack(self.end + code, data)[0]


class Ppc(Side):
    end = ">"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def world(self, turn):
        w = self.e.alloc(WORLD_SIZE)
        self.e.write(w + TURN_AT, self.pack("I", turn))
        self.e.write(GWORLD, self.pack("I", w))

    def log(self):
        return self.L.log

    def call(self, case, addr, args):
        """args: ints (words), T.F for a float; answers r3."""
        return T.call_ppc(self.e, addr, args)[0] & MASK


class Host(Side):
    end = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ph_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def world(self, turn):
        w = self.lib.tpw_ph_addr(b"world")
        C.memmove(w + TURN_AT, self.pack("I", turn), 4)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, case, addr, args):
        """The same call through the library's wrappers."""
        op = case["op"]
        ring = case.get("ring")
        if op in ("set", "get", "back", "sum", "avg", "temp"):
            code = {"set": 0, "get": 1, "back": 2, "sum": 3, "avg": 4, "temp": 5}[op]
            return self.lib.tpw_ph_op(ring, code, case["obj"], case.get("out", 0), case.get("arg", 0), case.get("bits", 0)) & MASK
        if op in ("high", "low"):
            return self.lib.tpw_ph_best(ring, 1 if op == "high" else 0, case["obj"], case["arg"], case["bits"]) & MASK
        if op in ("valget", "valset"):
            return self.lib.tpw_ph_val(0 if op == "valget" else 1, case["obj"], case.get("arg", 0)) & MASK
        self.lib.tpw_ph_tag(int(op[3]), case["obj"])
        return 0


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_ph_addr.restype, lib.tpw_ph_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ph_alloc.restype, lib.tpw_ph_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ph_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ph_op.restype = C.c_uint32
    lib.tpw_ph_op.argtypes = [C.c_int32, C.c_int32, C.c_uint64, C.c_uint64, C.c_int32, C.c_uint32]
    lib.tpw_ph_best.restype = C.c_uint32
    lib.tpw_ph_best.argtypes = [C.c_int32, C.c_int32, C.c_uint64, C.c_int32, C.c_uint32]
    lib.tpw_ph_val.restype = C.c_uint32
    lib.tpw_ph_val.argtypes = [C.c_int32, C.c_uint64, C.c_int32]
    lib.tpw_ph_tag.argtypes = [C.c_int32, C.c_uint64]
    out = (C.c_int64 * 64)()
    n = lib.tpw_ph_layout(out)
    got = list(out)[:n]
    k = len(HOST_LAYOUT)
    assert got[:k] == HOST_LAYOUT, got[:k]
    assert got[k] == TURN_AT and got[k + 1] >= WORLD_SIZE and got[k + 2:k + 5] == [0, 4, 8], got[k:]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def ring_image(s, ring, c):
    """The ring's bytes in the side's byte order: padding 0xA5, mTemp, the elements, mCurrentEntry, mNumEntries, mWrappedAround."""
    img = bytearray([PAD]) * ring.size
    img[0:ring.es] = s.pack(ring.code, c["temp"])
    for i, v in enumerate(c["vals"]):
        img[ring.data + i * ring.es:ring.data + (i + 1) * ring.es] = s.pack(ring.code, v)
    img[ring.cur:ring.cur + 4] = s.pack("i", c["cur"])
    img[ring.num:ring.num + 4] = s.pack("i", c["num"])
    img[ring.wrap] = c["wrap"]
    return bytes(img)


def ring_fields(ring):
    """The byte ranges of the decoded fields; everything else of the image is padding and compared raw."""
    spans = [(0, ring.es), (ring.cur, ring.cur + 4), (ring.num, ring.num + 4), (ring.wrap, ring.wrap + 1)]
    spans += [(ring.data + i * ring.es, ring.data + (i + 1) * ring.es) for i in range(ring.n)]
    return spans


def decode_ring(s, ring, raw):
    """raw is guard + image + guard: the fields by width (floats with one NaN), the rest as bytes."""
    img = raw[GUARD:GUARD + ring.size]
    covered = set()
    for a, b in ring_fields(ring):
        covered.update(range(a, b))

    def element(a):
        v = s.unpack(ring.code, img[a:a + ring.es])
        return canon_f(v) if ring.kind == "f32" else v

    return {"temp": element(0), "ring": [element(ring.data + i * ring.es) for i in range(ring.n)],
            "cur": s.unpack("i", img[ring.cur:ring.cur + 4]), "num": s.unpack("i", img[ring.num:ring.num + 4]), "wrap": img[ring.wrap],
            "pad": bytes(img[i] for i in range(ring.size) if i not in covered), "guards": (raw[:GUARD], raw[GUARD + ring.size:])}


def build(s, case):
    """Writes the case's ring (or Val, or tag block) and answer slot through the side; the world for Set."""
    op = case["op"]
    if op in ("valget", "valset"):
        base = s.alloc(0xC + 2 * GUARD)
        img = bytearray([PAD]) * 0xC
        img[0:4] = s.pack("I", case["stamp"])
        img[4:8] = s.pack("i", case["val"])
        img[8] = case["worked"]
        s.write(base, bytes([0x5A]) * GUARD + bytes(img) + bytes([0x5A]) * GUARD)
        case["base"], case["obj"] = base, base + GUARD
        s.world(case["turn"])
        return
    if op.startswith("tag"):
        base = s.alloc(0x20 + 2 * GUARD)
        s.write(base, bytes([0x5A]) * GUARD + bytes(case["bytes"]) + bytes([0x5A]) * GUARD)
        case["base"], case["obj"] = base, base + GUARD
        return
    ring = RINGS[case["ring"]]
    base = s.alloc(ring.size + 2 * GUARD)
    s.write(base, bytes([0x5A]) * GUARD + ring_image(s, ring, case) + bytes([0x5A]) * GUARD)
    case["base"], case["obj"] = base, base + GUARD
    if op in ("get", "sum", "avg"):
        slot = 8 if op == "sum" else ring.es
        ob = s.alloc(8 + 2 * OUTG)
        s.write(ob, bytes([0x5A]) * OUTG + bytes([0xC3]) * 8 + bytes([0x5A]) * OUTG)
        case["obase"], case["out"] = ob, ob + OUTG
        case["oslot"] = slot


def execute(s, case, addr):
    """The call: the PowerPC side with its argument placement (floats in f1, a tag in the high half of r4), the host through the wrappers."""
    op, ring = case["op"], case.get("ring")
    obj = case["obj"]
    if isinstance(s, Host):
        return s.call(case, addr, None)
    if op == "set":
        kind = RINGS[ring].kind
        if kind == "f32":
            args = [obj, T.F(bitsf(case["bits"]))]
        elif kind == "tag":
            args = [obj, ((case["bits"] & 0xFFFF) << 16) | case["junk"] & 0xFFFF]
        elif kind == "u8":
            args = [obj, ((case["junk"] << 8) | (case["bits"] & 0xFF)) & MASK]
        else:
            args = [obj, case["bits"] & MASK]
    elif op in ("get", "sum", "avg"):
        args = [obj, case["out"], case["arg"] & MASK]
    elif op in ("back", "temp"):
        args = [obj]
    elif op in ("high", "low"):
        args = [obj, T.F(bitsf(case["bits"])) if RINGS[ring].kind == "f32" else case["bits"] & MASK, case["arg"] & MASK]
    elif op == "valget":
        args = [obj]
    elif op == "valset":
        args = [obj, case["arg"] & MASK]
    else:
        args = [obj]
    return s.call(case, addr, args)


def observe(s, case, answer):
    """Everything the call may change, decoded the same way on both sides."""
    op = case["op"]
    if op in ("set", "avg", "temp", "valset") or op.startswith("tag"):
        answer = None   # void: r3 is whatever the code left
    elif op == "get":
        answer &= 0xFF
    out = {"answer": answer, "log": s.log()}
    if op in ("valget", "valset"):
        raw = s.read(case["base"], 0xC + 2 * GUARD)
        out["val"] = [s.unpack("I", raw[GUARD:GUARD + 4]), s.unpack("i", raw[GUARD + 4:GUARD + 8]), raw[GUARD + 8], bytes(raw[GUARD + 9:GUARD + 12]),
                      bytes(raw[:GUARD]), bytes(raw[GUARD + 0xC:])]
        return out
    if op.startswith("tag"):
        out["block"] = bytes(s.read(case["base"], 0x20 + 2 * GUARD))
        return out
    ring = RINGS[case["ring"]]
    out["ring"] = decode_ring(s, ring, s.read(case["base"], ring.size + 2 * GUARD))
    if "out" in case:
        raw = s.read(case["obase"], 8 + 2 * OUTG)
        slot = raw[OUTG:OUTG + 8]
        if op == "sum":
            value = [s.unpack("Q", slot)]
        else:
            v = s.unpack(ring.code, slot[:ring.es])
            value = [canon_f(v) if ring.kind == "f32" else v, bytes(slot[ring.es:])]
        out["out"] = [value, bytes(raw[:OUTG]), bytes(raw[OUTG + 8:])]
    return out


def run_case(s, case, addr):
    try:
        case = dict(case)
        build(s, case)
        return observe(s, case, execute(s, case, addr))
    finally:
        s.done()


# ---------------------------------------------------------------------------------------------------------------- the case generators


def model_get(c, back):
    """GetValue's index under the case's fields, or None (the model of the function, used to keep every access inside the block)."""
    cur, num = c["cur"], c["num"]
    if back >= num:
        return None
    if back <= cur:
        return s32(cur - back)
    if c["wrap"]:
        return s32(num + cur - back)
    return None


def hfb(c):
    return c["num"] if c["wrap"] else s32(c["cur"] + 1)


def touched(c):
    """The element indices the call reads or writes (None when it would loop for too long)."""
    op, cur, num = c["op"], c["cur"], c["num"]
    if op == "set":
        c1 = s32(cur + 1)
        return [0 if c1 >= num else c1]
    if op == "get":
        i = model_get(c, c["arg"])
        return [] if i is None else [i]
    if op in ("sum", "avg"):
        n = c["arg"]
        if hfb(c) < n:
            n = hfb(c)
        if n > 5000:
            return None
        out = []
        for i in range(max(n, 0)):
            j = model_get(c, i)
            if j is not None:
                out.append(j)
        return out
    if op == "temp":
        c1 = s32(cur + 1)
        return [0 if c1 >= num else c1]
    if op in ("high", "low"):
        out, i = [], 0
        while i < num and i <= c["arg"]:
            j = model_get(c, i)
            if j is None:
                break
            out.append(j)
            i += 1
            if i > 5000:
                return None
        return out
    return []


def safe(c):
    if "ring" not in c or c["ring"] is None or c["op"] in ("back",):
        return True
    ring = RINGS[c["ring"]]
    idx = touched(c)
    if idx is None:
        return False
    return all(element_ok(ring, i) for i in idx)


def byte_tag(ring, b):
    """What the image byte b (relative to the ring, -GUARD .. size + GUARD) belongs to: a field or element (its start and width), padding or a guard."""
    if b < 0 or b >= ring.size:
        return ("g",)
    if b < ring.es:
        return ("f", 0, ring.es)
    for i in range(ring.n):
        a = ring.data + i * ring.es
        if a <= b < a + ring.es:
            return ("f", a, ring.es)
    for a, w in ((ring.cur, 4), (ring.num, 4), (ring.wrap, 1)):
        if a <= b < a + w:
            return ("f", a, w)
    return ("p",)


def element_ok(ring, i):
    """Whether reading or writing element i touches only memory that both byte orders see as the same value: guards, padding, or one whole field
    of the element's width (an element that is a half of mCurrentEntry, or spans padding and a field, reads differently big and little endian)."""
    a = ring.data + i * ring.es
    if a < -GUARD or a + ring.es > ring.size + GUARD:
        return False
    tags = {byte_tag(ring, b) for b in range(a, a + ring.es)}
    if len(tags) != 1:
        return False
    tag = tags.pop()
    return tag[0] in ("g", "p") or (tag[1] == a and tag[2] == ring.es)


def elem_pool(kind):
    if kind == "i32":
        return [0, 1, MASK, 0x80000000, 0x7FFFFFFF, 2, 0xFFFFFFFE]
    if kind == "u32":
        return [0, MASK, 0x80000000, 1, 0x7FFFFFFF]
    if kind == "u8":
        return [0, 0x7F, 0x80, 0xFF, 1]
    if kind == "tag":
        return [0, 1, 0x7FFF, 0x8000, 0xFFFF]
    return [0, 0x80000000, fb(1.5), fb(-1e30), fb(1e30), 1, 0x7F800000, 0xFF800000, 0x7FC00000, 0x7F800001, fb(1.0), fb(-1.0)]


def rv(r, kind):
    """One element value (raw bits): mostly random, now and then a special value."""
    if r.random() < 0.3:
        return r.choice(elem_pool(kind))
    if kind == "f32":
        return fb(r.uniform(-1000, 1000)) if r.random() < 0.7 else r.getrandbits(32)
    if kind == "u8":
        return r.getrandbits(8)
    if kind == "tag":
        return r.getrandbits(16)
    return r.getrandbits(32)


def make_case(r, fn, cur, wrap, num, arg, bits=None):
    """One ring case of a function; None when the model says it leaves the block."""
    addr, t, op, name = fn
    ring = RINGS[t]
    c = {"op": op, "ring": t, "temp": rv(r, ring.kind), "junk": r.getrandbits(24),
         "cur": cur, "wrap": wrap, "num": num, "arg": arg}
    c["vals"] = [rv(r, ring.kind) for _ in range(ring.n)]
    c["bits"] = (rv(r, ring.kind) if bits is None else bits) & MASK
    if op in ("high", "low"):
        pool = c["vals"] + [c["temp"]]
        if bits is None and r.random() < 0.5:
            c["bits"] = r.choice(pool)
        if ring.kind == "i32" and bits is None and r.random() < 0.3:
            c["bits"] = r.choice([0x80000000, 0x7FFFFFFF, 0, MASK])
    if ring.kind == "f32" and (c["bits"] & 0x7F800000) == 0x7F800000 and (c["bits"] & 0x7FFFFF):
        c["bits"] = 0x7FC00000   # a quiet NaN without payload (it reaches f1 and the host unchanged, and can land in a field)
    return c if safe_case(c) else None


def safe_case(c):
    return safe(c)


def curs(ring):
    return sorted({-1, 0, ring.n // 2, ring.n - 1})


def nums(ring, cur):
    return sorted({ring.n, 0, 1, 2, ring.n - 1, ring.n + 1, cur + 1, max(cur, 0)})


def args_for(ring, cur, num, op):
    base = [-3, -1, 0, 1, cur, cur + 1, num - 1, num, num + 1, ring.n + 3, INT_MAX]
    if op in ("sum", "avg", "high", "low"):
        base += [INT_MIN, 2, 3, ring.n, ring.n - 1]
    return sorted(set(base))


def ring_edges(fn, seed):
    def gen():
        r = random.Random(seed)
        ring = RINGS[fn[1]]
        for cur in curs(ring):
            for wrap in (0, 1):
                for num in nums(ring, cur):
                    if fn[2] in ("set", "back", "temp"):
                        c = make_case(r, fn, cur, wrap, num, 0)
                        if c:
                            yield c
                        continue
                    for arg in args_for(ring, cur, num, fn[2]):
                        c = make_case(r, fn, cur, wrap, num, arg)
                        if c:
                            yield c
    return gen


def ring_random(fn, count_limit=None):
    def gen(r):
        ring = RINGS[fn[1]]
        for _ in range(100):
            cur = r.choice([-1, 0, ring.n - 1, r.randrange(-1, ring.n)])
            num = r.choice([ring.n] * 4 + [r.randrange(0, ring.n + 2), cur + 1, ring.n + 1])
            wrap = r.randrange(2)
            arg = r.choice([r.randrange(-3, ring.n + 4), r.randrange(-3, ring.n + 4), cur, cur + 1, num, INT_MAX, 0, 1])
            c = make_case(r, fn, cur, wrap, num, arg)
            if c:
                return c
        return make_case(r, fn, -1, 0, ring.n, 0)
    return gen


def val_cases(fn, seed, edge):
    def make(r, turn=None, value=None):
        return {"op": fn[2], "ring": None, "stamp": r.getrandbits(32), "val": s32(r.getrandbits(32)), "worked": r.randrange(2),
                "turn": r.choice([0, 1, MASK, r.getrandbits(32)]) if turn is None else turn,
                "arg": r.choice([0, 1, -1, INT_MIN, INT_MAX, s32(r.getrandbits(32))]) if value is None else value}

    def gen():
        r = random.Random(seed)
        for turn in (0, 1, MASK):
            for value in (0, 1, -1, INT_MIN, INT_MAX):
                yield make(r, turn, value)
    return gen if edge else make


def tag_cases(fn, seed, edge):
    def make(r):
        return {"op": fn[2], "ring": None, "bytes": bytes(r.getrandbits(8) for _ in range(0x20))}

    def gen():
        r = random.Random(seed)
        for _ in range(4):
            yield make(r)
    return gen if edge else make


# ---------------------------------------------------------------------------------------------------------------- the tests


def make_test(fn, seed):
    addr, t, op, name = fn

    def ppc(e, case):
        return run_case(Ppc(e, case), case, addr)

    def host(lib, case):
        return run_case(Host(lib, case), case, addr)

    if t is None and op.startswith("val"):
        edge, rnd = val_cases(fn, seed, True), val_cases(fn, seed, False)
    elif t is None:
        edge, rnd = tag_cases(fn, seed, True), tag_cases(fn, seed, False)
    else:
        edge, rnd = ring_edges(fn, seed), ring_random(fn)
    return FnTest("%s 0x%08X ring %s" % (name, addr, "-" if t is None else t), ppc, host, edge=edge, rnd=rnd)


TESTS = [make_test(fn, 1000 + i) for i, fn in enumerate(FUNCS)]
