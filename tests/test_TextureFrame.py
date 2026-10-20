"""The texture cache's frame calls (engine library, tb_texturecache_frame2.cpp):
CTextureCache::Prepare, UploadToVRAM, GenerateTexturePages, ReInitialize,
DumpTCStreamToLog_Prepare and DumpTCStreamToLog_Execute, and CTextureLink::PageUpdated.

A case is one call on a small world: the cache, two pages (a link and an OpenGL texture
each), four texture entries in one array, eight place records (four of their own and the four
the entries carry), a quad and the engine's globals. The test builds the world on both sides
from the same description (the host library reads it as a flat list, the PowerPC side is
written into emulator memory), runs the call and compares its answer, the recorded calls and
the state of everything the call could change, read in the order tests/unit_TextureFrame.cpp
writes the host's.

Everything outside the functions is a fake on both sides that records its arguments: the
texture's Download, GetTexture and UnlockSurface (virtual calls through the original's own
vtable, behind its this-adjusting thunks), the cache's Close, AutoCreateSurfaces and
PackTextures, the assertion handler and the C library's fopen, fwrite, fflush and fclose. The
bytes of every fwrite are recorded (the stream's record decoded, with the entry as its label).

The original's fused multiply-subtracts (UploadToVRAM's fnmsubs) and its fctiwz (the sort key of
Prepare and GenerateTexturePages) run under HardwareRounding, as in test_NodeGamuts.py and
test_OpenGLDraw.py: Unicorn rounds the first twice and saturates the second the wrong way.
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_TextureFrame"

FN = dict(Prepare=0x10032ddc, UploadToVRAM=0x1003820c, GenerateTexturePages=0x10037494,
          DumpPrepare=0x10034bc4, DumpExecute=0x10034d50, PageUpdated=0x100385f4, ReInitialize=0x1003447c)
PACK, CLOSE, AUTO_CREATE = 0x1003678c, 0x10034514, 0x10033ecc
DOWNLOAD, GET_TEXTURE, UNLOCK = 0x100465f8, 0x10046adc, 0x100464d0
# The members they reach that run as the original: the OpenGL texture's and CTextureBase's
# constructors and CTextureBase::PositionLock.
INNER = [0x10045d88, 0x10032b58, 0x10032cdc]

SYSTEM, SETTINGS, DISPLAY, FREQUENCY, UPDATES = 0x10165f80, 0x10165fc8, 0x10165634, 0x10052d74, 0x10173194
MASK = 0xFFFFFFFF
LINKS, ENTRIES = 2, 4
CAPACITY = 1024
PACK_DEFAULT = 8 << 8      # the "Pack" queue's default: take nothing, answer 0

# Labels (tests/unit_TextureFrame.cpp has the same numbers).
LABEL_CACHE, LABEL_FILE, LABEL_TEXTURE, LABEL_LINK, LABEL_ENTRY, LABEL_PLACE, LABEL_QUAD = 1, 5, 11, 21, 31, 41, 51

# Operations.
PREPARE, UPLOAD, GENERATE, DUMP_PREPARE, DUMP_EXECUTE, PAGE_UPDATED, REINITIALIZE = range(1, 8)

# Entry flag bits.
ON_PAGE, QUEUED, LOCKED, NO_PREPARE = 0x1000, 0x2000, 0x4000, 0x2000000
WRAP_U, WRAP_V, WRAP = 0x10000, 0x20000, 0x30000
NEXT_IS_PLANE, BLEND_KIND, FLAG_400, FIXUP, CENTRED = 0x800000, 0x80000, 0x400, 0x4000000, 0x80000000
VIEW_CHANGED, LIGHTS_CHANGED = 0x80000000, 0x40000000


def setup(runner):
    T.install_std_stubs(runner.emu)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def fb(x):
    return T.f32_bits(x)


def canon(word):
    """A single-precision value's bits with every NaN the same."""
    return 0x7FC00000 if (word & 0x7F800000) == 0x7F800000 and (word & 0x007FFFFF) else word


def s16(v):
    return T.s16(v)


# ----------------------------------------------------------------- the world
# Pointers in the description are indexes: an entry 1..4 (0: null), a page 1..2.


def blank_place(rnd=None):
    if rnd is None:
        return dict(bytes=[0] * 16, f10=0, f14=0)
    return dict(bytes=[rnd.choice([0, 1, 2, 0xfe, 0xff, rnd.randrange(256)]) for _ in range(16)],
                f10=rnd.choice([0, 1, -1, 17, rnd.randint(-1000, 1000)]),
                f14=rnd.choice([0, 1, 0x8000, 0x7fff, -1, rnd.randint(-0x80000, 0x80000)]))


def blank_entry():
    return dict(flags=0, w=32, h=32, cw=32, ch=32, page=1, texture=0, usize=0.0, vsize=0.0, uoff=0.0, voff=0.0,
                prio=1.0, pitch=32, sortkey=0, next=0, f38=0, place=0, own=blank_place())


def blank_world():
    return dict(
        sys_flags=0, renderer=3, settings=[0] * 20, display_flags=0, frame=0, freq=0, updates=0,
        head=0, cache_flags=0, size_mask=7, scale=1.0, mf1830=[0] * 6, f1860=4, f18b0=0, f18b8=0, f18bc=0,
        mipmaps=0, largest=0, ui=0, f18dc=0, f18e0=0, f1908=0, pagecount=[0] * 6,
        links=[dict(entries=0, f08=0, f0c=0, f10=0, f14=-1, f18=0, f1c=0, tflags=0, tw=256, th=256, tpitch=1024)
               for _ in range(LINKS)],
        places=[blank_place() for _ in range(4)],
        entries=[blank_entry() for _ in range(ENTRIES)],
        quad=dict(x=0, y=0, w=0, h=0, entry=0),
        queues={})


def set_settings(w, flags=0, fmt=4, half=0.5, fixups=(-1.0, -1.0, 0.0, 0.0)):
    s = w["settings"]
    s[0], s[1], s[6] = flags, fmt, fb(half)
    for k, v in enumerate(fixups):
        s[8 + k] = fb(v)


def flat(w):
    """The description as the host library reads it (the order of tests/unit_TextureFrame.cpp)."""
    d = [w["sys_flags"], w["renderer"]] + list(w["settings"])
    d += [w["display_flags"], w["frame"], w["freq"], w["updates"]]
    d += [w["head"], w["cache_flags"], w["size_mask"], fb(w["scale"])] + list(w["mf1830"])
    d += [w["f1860"], w["f18b0"], w["f18b8"], w["f18bc"], w["mipmaps"], w["largest"], w["ui"], w["f18dc"],
          w["f18e0"], w["f1908"]] + list(w["pagecount"])
    for link in w["links"]:
        d += [link[k] for k in ("entries", "f08", "f0c", "f10", "f14", "f18", "f1c", "tflags", "tw", "th", "tpitch")]
    for place in w["places"]:
        d += list(place["bytes"]) + [place["f10"], place["f14"]]
    for entry in w["entries"]:
        d += [entry["flags"], entry["w"], entry["h"], entry["cw"], entry["ch"], entry["page"], entry["texture"]]
        d += [fb(entry[k]) for k in ("usize", "vsize", "uoff", "voff", "prio")]
        d += [entry["pitch"], entry["sortkey"], entry["next"], entry["f38"], entry["place"]]
        d += list(entry["own"]["bytes"]) + [entry["own"]["f10"], entry["own"]["f14"]]
    q = w["quad"]
    d += [q["x"], q["y"], q["w"], q["h"], q["entry"]]
    return [v & 0xFFFFFFFFFFFFFFFF for v in d]


def signed64(v):
    return v - (1 << 64) if v >= (1 << 63) else v


# ----------------------------------------------------------------- the emulator's fused multiply-adds and fctiwz

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]
# UploadToVRAM (fnmsubs), Prepare and GenerateTexturePages (fctiwz of the sort key).
CODE_RANGES = [(FN["UploadToVRAM"], FN["UploadToVRAM"] + 824), (FN["Prepare"], FN["Prepare"] + 636),
               (FN["GenerateTexturePages"], FN["GenerateTexturePages"] + 572)]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Two things Unicorn (QEMU 5.0) does not execute as the processor does, corrected for the
    code under test and nowhere else (as in test_NodeGamuts.py and test_OpenGLDraw.py):
    - fmadds, fmsubs, fnmadds and fnmsubs are computed in double precision and rounded to
      single, which can differ by one unit in the last place from the processor's single
      rounding of the exact result; the result is replaced by the C library's fmaf;
    - fctiwz of a value above INT32_MAX (or +infinity) gives 0x80000000; the processor
      saturates to 0x7fffffff.
    A code hook looks at each instruction before it runs and rewrites its target register just
    before the next one."""

    changed = 0    # results the correction replaced (all cases so far), for the notes

    def __init__(self, e):
        self.e, self.pending, self.hooks = e, None, []

    def __enter__(self):
        for begin, end in CODE_RANGES:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            kind, target, value = self.pending
            self.pending = None
            if kind == "fctiwz":
                HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], (fpr(e, target) & 0xffffffff00000000) | 0x7fffffff)
            else:
                corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
                if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                    HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        if word >> 26 == 63 and (word >> 1) & 0x3ff == 15:          # fctiwz frD, frB
            if fpr_double(e, rb) >= 2147483648.0:
                self.pending = ("fctiwz", rd, None)
            return
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = ("fused", rd, value)


# ----------------------------------------------------------------- PowerPC side


class Ppc:
    """The world in emulator memory, the fakes, the call and the state."""

    def __init__(self, e, w):
        self.e, self.w = e, w

    def build(self):
        e, w = self.e, self.w
        w32(e, SYSTEM, w["sys_flags"])
        w32(e, SYSTEM + 0x1c, w["renderer"])
        e.write(SETTINGS, struct.pack(">20I", *[v & MASK for v in w["settings"]]))
        w32(e, DISPLAY, w["display_flags"])
        w32(e, DISPLAY + 0x80, w["frame"])
        w32(e, FREQUENCY, w["freq"])
        w32(e, UPDATES, w["updates"])

        self.cache = e.alloc(0x1910, b"\xa5" * 0x1910)
        c = self.cache
        self.links = e.alloc(LINKS * 0x20, b"\xa5" * (LINKS * 0x20))
        self.textures = [e.alloc(0x34, b"\xa5" * 0x34) for _ in range(LINKS)]
        self.places = e.alloc(4 * 0x18, b"\xa5" * (4 * 0x18))
        self.entries = e.alloc(ENTRIES * 0x5c, b"\xa5" * (ENTRIES * 0x5c))
        self.quad = e.alloc(0x14, b"\xa5" * 0x14)
        self.handle = e.alloc(8, b"\xa5" * 8)

        def entry(index):
            return self.entries + 0x5c * (index - 1) if index > 0 else 0

        def link(index):
            return self.links + 0x20 * (index - 1) if index > 0 else 0

        self.entry_at = entry
        w32(e, c + 0x18ac, entry(w["head"]))
        w32(e, c + 0x18b4, w["cache_flags"])
        w32(e, c + 0x18cc, w["size_mask"])
        w32(e, c + 0x18e8, fb(w["scale"]))
        for k, v in enumerate(w["mf1830"]):
            w32(e, c + 0x1830 + 4 * k, v)
        for off, key in ((0x1860, "f1860"), (0x18b0, "f18b0"), (0x18b8, "f18b8"), (0x18bc, "f18bc"), (0x18d0, "mipmaps"),
                         (0x18d4, "largest"), (0x18d8, "ui"), (0x18dc, "f18dc"), (0x18e0, "f18e0"), (0x1908, "f1908")):
            w32(e, c + off, w[key])
        for k, v in enumerate(w["pagecount"]):
            w32(e, c + 0x1800 + 4 * k, v)

        self.bases = []
        for i in range(LINKS):
            t = w["links"][i]
            obj = self.textures[i]
            T.call_ppc(e, 0x10045d88, [obj, 1])            # the original's constructor makes the vtables
            base = obj + 0x1c
            self.bases.append(base)
            e.write(base, struct.pack(">IIII", t["tflags"] & MASK, t["tw"] & MASK, t["th"] & MASK, t["tpitch"] & MASK))
            a = self.links + 0x20 * i
            e.write(a, struct.pack(">IIIIIIII", base, entry(t["entries"]), t["f08"] & MASK, t["f0c"] & MASK,
                                   t["f10"] & MASK, t["f14"] & MASK, t["f18"] & MASK, t["f1c"] & MASK))
        for k, p in enumerate(w["places"]):
            self.write_place(self.places + 0x18 * k, p)
        for i, en in enumerate(w["entries"]):
            a = self.entries + 0x5c * i
            place = self.places + 0x18 * (en["place"] - 1) if en["place"] > 0 else a + 0x44
            texture = self.textures[en["texture"] - 1] if en["texture"] > 0 else 0
            e.write(a, struct.pack(">IIIhhhhIIIIIII", place, entry(en["next"]), en["flags"] & MASK, s16(en["w"]), s16(en["h"]),
                                   s16(en["cw"]), s16(en["ch"]), link(en["page"]), texture, fb(en["usize"]), fb(en["vsize"]),
                                   fb(en["uoff"]), fb(en["voff"]), fb(en["prio"])))
            e.write(a + 0x30, struct.pack(">hxx", s16(en["pitch"])))
            w32(e, a + 0x34, en["sortkey"])
            w32(e, a + 0x38, 0x7C0F1234 if en["f38"] else 0)
            self.write_place(a + 0x44, en["own"])
        q = w["quad"]
        e.write(self.quad, struct.pack(">IIIII", q["x"] & MASK, q["y"] & MASK, q["w"] & MASK, q["h"] & MASK, entry(q["entry"])))

        fixed = {self.cache: LABEL_CACHE, self.handle: LABEL_FILE, self.quad: LABEL_QUAD}
        for i in range(LINKS):
            fixed[self.textures[i]] = LABEL_TEXTURE + i
            fixed[self.links + 0x20 * i] = LABEL_LINK + i
        for i in range(ENTRIES):
            fixed[self.entries + 0x5c * i] = LABEL_ENTRY + i
            fixed[self.places + 0x18 * i] = LABEL_PLACE + i
            fixed[self.entries + 0x5c * i + 0x44] = LABEL_PLACE + ENTRIES + i
        self.fixed = fixed

    def write_place(self, a, p):
        self.e.write(a, bytes(p["bytes"]) + struct.pack(">II", p["f10"] & MASK, p["f14"] & MASK))

    # -- the fakes
    def stubs(self):
        e = self.e
        st = CL.Stubs()
        st.real = set(FN.values()) | set(INNER)
        st.internal(CLOSE, CL.recorder("Close", "L", "i"))
        st.internal(AUTO_CREATE, CL.recorder("AutoCreateSurfaces", "L", "i"))
        st.internal(DOWNLOAD, CL.recorder("Download", "LLii", "i"))
        st.internal(GET_TEXTURE, CL.recorder("GetTexture", "L"))
        st.internal(UNLOCK, CL.recorder("UnlockSurface", "L", "i"))
        st.internal(PACK, self.pack)
        st.imp("__assertion_failed", lambda emu: emu.cl.rec("assert", (T.s32(emu.r(5)),), (emu.cstr(emu.r(3)), emu.cstr(emu.r(4)))))
        st.imp("fopen", self.fopen)
        st.imp("fwrite", self.fwrite)
        st.imp("fflush", lambda emu: (emu.cl.rec("fflush", (emu.cl.lab(emu.r(3)),)), emu.set_r(3, 0)))
        st.imp("fclose", lambda emu: (emu.cl.rec("fclose", (emu.cl.lab(emu.r(3)),)), emu.set_r(3, 0)))
        return st

    def chain(self, head):
        out, e = [], self.e
        while head and len(out) < 12:
            out.append(head)
            head = e.u32(head + 4)
        return out

    def pack(self, e):
        L = e.cl
        this, listp = e.r(3), e.r(4)
        chain = self.chain(e.u32(listp))
        L.rec("PackTextures", (L.lab(this), int(listp == self.cache + 0x18ac), len(chain)))
        labels = [L.lab(x) for x in chain] + [0] * 12
        L.rec("PackList", tuple(labels[:12]))
        action = L.ret("Pack", PACK_DEFAULT)
        head = e.u32(listp)
        for _ in range(action & 0xff):
            if head == 0:
                break
            head = e.u32(head + 4)
        w32(e, listp, head)
        e.set_r(3, T.u32((action >> 8) - 8))

    def fopen(self, e):
        e.cl.rec("fopen", (), (e.cstr(e.r(3)), e.cstr(e.r(4))))
        e.set_r(3, self.handle if e.cl.ret("fopen", 1) else 0)

    def fwrite(self, e):
        L = e.cl
        data, size, count, handle = e.r(3), e.r(4), e.r(5), e.r(6)
        L.rec("fwrite", (size, count, L.lab(handle)))
        nbytes = size * count
        raw = e.read(data, nbytes)
        if nbytes == 1:
            L.rec("fbyte", (raw[0],))
        elif nbytes == 0x10:
            pointer, flags, width, height, priority = struct.unpack(">IIhhI", raw)
            L.rec("frecord", (0 if pointer == 0 else L.lab(pointer), flags, width, height, canon(priority)))
        else:
            for first in range(0, nbytes, 32):
                chunk = raw[first:first + 32]
                words = list(struct.unpack(">%dI" % (len(chunk) // 4), chunk))
                L.rec("fwords", tuple([len(words)] + words + [0] * (8 - len(words))))
        e.set_r(3, count)

    # -- the call and the state
    def call(self, op, a):
        e = self.e
        cache = self.cache
        if op == PREPARE:
            T.call_ppc(e, FN["Prepare"], [cache, self.entry_at(a[0]), T.F(T.bits_f32(a[1]))])
            return 0
        if op == UPLOAD:
            link = self.links + 0x20 * (a[0] - 1) if a[0] > 0 else 0
            return T.s32(T.call_ppc(e, FN["UploadToVRAM"], [cache, link, self.quad if a[1] else 0, self.entry_at(a[2])])[0])
        if op == GENERATE:
            return T.s32(T.call_ppc(e, FN["GenerateTexturePages"], [cache])[0])
        if op == DUMP_PREPARE:
            T.call_ppc(e, FN["DumpPrepare"], [cache, self.entry_at(a[0])])
            return 0
        if op == DUMP_EXECUTE:
            T.call_ppc(e, FN["DumpExecute"], [cache])
            return 0
        if op == PAGE_UPDATED:
            return T.s32(T.call_ppc(e, FN["PageUpdated"], [self.links + 0x20 * (a[0] - 1)])[0])
        return T.s32(T.call_ppc(e, FN["ReInitialize"], [cache, a[0] & MASK])[0])

    def lab(self, address):
        return self.e.cl.lab(address)

    def state(self):
        e, u, s = self.e, self.e.u32, T.s32
        out = [s(u(UPDATES)), u(FREQUENCY), s(u(SETTINGS + 4)), u(SETTINGS), u(SYSTEM)]
        c = self.cache
        out.append(self.lab(u(c + 0x18ac)))
        out += [s(u(c + 0x1830 + 4 * k)) for k in range(6)]
        out += [s(u(c + 0x1860)), s(u(c + 0x18b0)), u(c + 0x18b4), s(u(c + 0x18b8)), s(u(c + 0x18bc)), s(u(c + 0x18cc)),
                s(u(c + 0x18d0)), s(u(c + 0x18d4)), s(u(c + 0x18d8)), s(u(c + 0x18dc)), s(u(c + 0x18e0)),
                canon(u(c + 0x18e8)), s(u(c + 0x1908))]
        out += [s(u(c + 0x1800 + 4 * k)) for k in range(6)]
        for i in range(LINKS):
            a = self.links + 0x20 * i
            out.append(self.lab(u(a + 4)))
            out += [s(u(a + off)) for off in (8, 0xc, 0x10, 0x14, 0x18, 0x1c)]
            base = self.bases[i]
            out += [u(base), s(u(base + 4)), s(u(base + 8)), s(u(base + 0xc))]
        for i in range(ENTRIES):
            a = self.entries + 0x5c * i
            out += [self.lab(u(a)), self.lab(u(a + 4)), u(a + 8)]
            out += list(struct.unpack(">hhhh", e.read(a + 0xc, 8)))
            out += [self.lab(u(a + 0x14)), self.lab(u(a + 0x18))]
            out += [canon(u(a + off)) for off in (0x1c, 0x20, 0x24, 0x28, 0x2c)]
            out += [struct.unpack(">h", e.read(a + 0x30, 2))[0], s(u(a + 0x34)), int(u(a + 0x38) != 0)]
            out += self.place(a + 0x44)
        for k in range(4):
            out += self.place(self.places + 0x18 * k)
        q = self.quad
        out += [s(u(q)), s(u(q + 4)), s(u(q + 8)), s(u(q + 0xc)), self.lab(u(q + 0x10))]
        return out

    def place(self, a):
        return list(self.e.read(a, 16)) + [T.s32(self.e.u32(a + 0x10)), T.s32(self.e.u32(a + 0x14))]


def ppc_run(e, case):
    w = case["world"]
    p = Ppc(e, w)
    st = p.stubs()
    saved = st.activate(e)
    try:
        p.build()
        e.cl = CL.PpcLog(p.fixed, w["queues"])
        with HardwareRounding(e):
            result = p.call(case["op"], case["args"])
        return dict(result=result, state=p.state(), log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host_run(lib, case):
    w = case["world"]
    CL.host_begin(lib, {}, w["queues"])
    description = flat(w)
    d = (C.c_int64 * len(description))(*[signed64(v) for v in description])
    args = (C.c_int64 * 4)(*[signed64(v & 0xFFFFFFFFFFFFFFFF) for v in (case["args"] + [0, 0, 0, 0])[:4]])
    out = (C.c_int64 * CAPACITY)()
    run = lib.tpw_tf_run
    run.restype = C.c_int64
    run.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32]
    result = run(case["op"], args, d, out, CAPACITY)
    lib.tpw_tf_count.restype = C.c_int32
    n = lib.tpw_tf_count()
    log = CL.host_end(lib)
    assert n <= CAPACITY, "the state buffer is too small"
    return dict(result=result, state=list(out[:n]), log=log)


# ----------------------------------------------------------------- cases

NAN, INF = math.nan, math.inf


def rfl(r):
    """A float for a field: mostly moderate, some edge values."""
    k = r.random()
    if k < 0.2:
        return r.choice([0.0, -0.0, 1.0, -1.0, 0.5, 1.5, 2.0, 4.0, 0.25, 1024.0, 1e-3, 100.0, -100.0, 3.0e9, -3.0e9,
                         1.0e-40, NAN, INF, -INF, 3.4028234663852886e38])
    if k < 0.6:
        return T.f32(r.uniform(-3.0, 3.0))
    return T.f32(r.uniform(-2000.0, 2000.0))


def rsize(r):
    return r.choice([8, 16, 32, 64, 128, 256, r.randint(1, 300), r.randint(1, 40), 1, 7, 9, 0, -3, 0x7fff, -0x8000])


def chain_lists(r, w):
    """Makes the queue and the entries' links acyclic: a random run of entries, the others
    pointing at nothing or into the run."""
    order = r.sample(range(1, ENTRIES + 1), r.randint(0, ENTRIES))
    w["head"] = order[0] if order else 0
    for k, index in enumerate(order):
        w["entries"][index - 1]["next"] = order[k + 1] if k + 1 < len(order) else 0
    for index in range(1, ENTRIES + 1):
        if index not in order:
            w["entries"][index - 1]["next"] = r.choice([0] + order)
    return order


def random_flags(r):
    """An entry's flags: random bits with the ones the functions test set on purpose."""
    keep = r.getrandbits(32) & r.getrandbits(32) & ~(ON_PAGE | QUEUED | LOCKED | NO_PREPARE | WRAP | NEXT_IS_PLANE |
                                                      BLEND_KIND | FLAG_400 | FIXUP | CENTRED)
    for bit, p in ((ON_PAGE, 0.4), (QUEUED, 0.4), (LOCKED, 0.4), (NO_PREPARE, 0.12), (WRAP_U, 0.2), (WRAP_V, 0.2),
                   (NEXT_IS_PLANE, 0.3), (BLEND_KIND, 0.3), (FLAG_400, 0.4), (FIXUP, 0.4), (CENTRED, 0.4)):
        if r.random() < p:
            keep |= bit
    return keep & MASK


def random_world(r):
    w = blank_world()
    w["sys_flags"] = r.choice([0, VIEW_CHANGED, LIGHTS_CHANGED, VIEW_CHANGED | LIGHTS_CHANGED, r.getrandbits(32)])
    w["renderer"] = r.choice([3, 3, 3, 3, 3, 0, 1, 2, 4, -1, r.randint(-5, 9)])
    w["settings"] = [r.getrandbits(32) for _ in range(20)]
    set_settings(w, flags=r.getrandbits(32) & ~0x80 | (0x80 if r.random() < 0.4 else 0), fmt=r.randrange(6),
                 half=r.choice([0.5, 0.5, rfl(r)]), fixups=[rfl(r) for _ in range(4)])
    w["display_flags"] = r.getrandbits(32)
    w["frame"] = r.choice([0, 1, 100, -1, r.randint(-1000, 1000), r.getrandbits(31)])
    w["freq"] = r.randrange(16)
    w["updates"] = r.choice([0, 1, 41, 0x7fffffff, -1, r.randint(-1000, 1000)])
    w["cache_flags"] = r.getrandbits(32) & ~0x60000000 | (0x20000000 if r.random() < 0.5 else 0) | (0x40000000 if r.random() < 0.4 else 0)
    w["size_mask"] = r.choice([0, 7, 7, 7, 3, 15, 1, 31, r.randint(0, 255), -1])
    w["scale"] = r.choice([1.0, 1.0, 1.0, 0.5, 2.0, rfl(r)])
    w["mf1830"] = [r.choice([0, r.randint(-100, 100), r.getrandbits(31)]) for _ in range(6)]
    for key in ("f1860", "f18b0", "f18b8", "f18bc", "mipmaps", "largest", "ui", "f18dc", "f18e0", "f1908"):
        w[key] = r.choice([0, 1, 4, r.randint(-1000, 1000)])
    w["pagecount"] = [r.choice([0, 1, 5, 256]) for _ in range(6)]
    for link in w["links"]:
        link.update(entries=r.randint(0, ENTRIES), f08=r.choice([0, r.randint(-99, 99)]), f0c=r.choice([0, r.randint(-99, 99)]),
                    f10=r.randint(-99, 99), f14=r.choice([-1, r.randint(-99, 99)]), f18=r.randint(-99, 99),
                    f1c=r.randint(-99, 99), tflags=r.choice([0, 1, 2, r.getrandbits(32)]),
                    tw=r.choice([0, 8, 32, 64, 128, 256, 256, r.randint(-5, 600)]),
                    th=r.choice([0, 8, 32, 64, 128, 256, 256, r.randint(-5, 600)]), tpitch=r.randint(0, 4096))
    w["places"] = [blank_place(r) for _ in range(4)]
    for i, en in enumerate(w["entries"]):
        flags = random_flags(r)
        width, height = rsize(r), rsize(r)
        if (flags & WRAP) != 0 and r.random() < 0.6:
            height = width
        en.update(flags=flags, w=width, h=height,
                  cw=r.choice([width, height, (width + 7) & ~7, rsize(r)]), ch=r.choice([height, width, (height + 7) & ~7, rsize(r)]),
                  page=r.choice([1, 2]) if r.random() < 0.9 else 0, texture=r.choice([0, 1, 2]),
                  usize=rfl(r), vsize=rfl(r), uoff=rfl(r), voff=rfl(r), prio=rfl(r),
                  pitch=r.choice([0, 32, r.randint(-0x8000, 0x7fff)]), sortkey=r.choice([0, r.getrandbits(32)]),
                  f38=r.randrange(2), place=r.choice([0, 0, 0, 1, 2, 3, 4]), own=blank_place(r))
        if (flags & (ON_PAGE | LOCKED)) != 0 and en["page"] == 0:
            en["page"] = 1
    w["entries"][ENTRIES - 1]["flags"] &= ~NEXT_IS_PLANE
    chain_lists(r, w)
    w["quad"] = dict(x=r.choice([0, r.randint(-5, 300)]), y=r.choice([0, r.randint(-5, 300)]),
                     w=r.randint(0, 300), h=r.randint(0, 300), entry=r.randint(1, ENTRIES))
    w["queues"] = {"Close": [r.choice([0, 1, -1, 5])], "AutoCreateSurfaces": [r.choice([0, 1, 1, -1, 7])],
                   "UnlockSurface": [r.choice([0, 1, 1, -1, 5])], "Download": [r.choice([0, 1, 1])],
                   "fopen": [r.choice([1, 1, 1, 0])]}
    return w


def case(op, args, w):
    return dict(op=op, args=list(args), world=w)


def plan_pack(r, length):
    """A queue of PackTextures answers that ends the packing loop for a queue of `length` entries
    without dropping an empty list's head (the original would read through a null pointer)."""
    queue, left, result = [], length, 1
    while left > 0 and result != 0:
        if len(queue) >= 12:
            take, result = 255, r.choice([0, 1])
        else:
            result = r.choice([1, 1, -1, -1, 0, 2, 5, -2])
            take = r.randint(0, 3)
        if result == -1:
            take = min(take, left - 1)
            left -= take + 1
            queue.append(take + ((result + 8) << 8))
            result = 1
            continue
        left = max(left - take, 0)
        queue.append(take + ((result + 8) << 8))
    return queue or [PACK_DEFAULT]


def rnd_prepare(r):
    w = random_world(r)
    entry = r.randint(1, ENTRIES) if r.random() > 0.04 else 0
    return case(PREPARE, [entry, fb(rfl(r))], w)


def rnd_upload(r):
    w = random_world(r)
    entry = r.choice([0, 0, 1, 2, 3, 4])
    return case(UPLOAD, [r.randint(1, LINKS), 1, entry], w)


def rnd_generate(r):
    w = random_world(r)
    order = chain_lists(r, w)
    w["queues"]["Pack"] = plan_pack(r, len(order))
    return case(GENERATE, [], w)


def rnd_dump_prepare(r):
    w = random_world(r)
    return case(DUMP_PREPARE, [r.randint(1, ENTRIES)], w)


def rnd_dump_execute(r):
    return case(DUMP_EXECUTE, [], random_world(r))


def rnd_page_updated(r):
    return case(PAGE_UPDATED, [r.randint(1, LINKS)], random_world(r))


def rnd_reinitialize(r):
    return case(REINITIALIZE, [r.choice([-1, -1, 0, 3, 4, 5, r.randint(-5, 9), r.getrandbits(31)])], random_world(r))


def with_entry(world, index, **kw):
    world["entries"][index - 1].update(kw)
    return world


def edge_prepare():
    # A plain request: a 32 x 32 entry on no page, queued behind another.
    for dist in (0.0, 0.25, 1.5, 2.0, -3.0, NAN, INF, -INF, 1.0e9, -1.0e9):
        w = blank_world()
        w["head"] = 2
        set_settings(w)
        w["entries"][1]["flags"] = QUEUED
        yield case(PREPARE, [1, fb(dist)], w)
    # No entry.
    yield case(PREPARE, [0, fb(1.0)], blank_world())
    # The asserts: a width of 0, a negative width, no cache size, a wrapping entry that is not square.
    for width, cw, height, flags in ((0, 8, 8, 0), (-4, 8, 8, 0), (8, 0, 8, 0), (8, 8, 16, WRAP_U), (8, 8, 16, WRAP_V),
                                     (8, 8, 8, WRAP), (16, 16, 8, WRAP), (0, 0, 8, WRAP), (-1, 0, -2, 0)):
        w = blank_world()
        with_entry(w, 1, w=width, cw=cw, h=height, flags=flags)
        yield case(PREPARE, [1, fb(0.5)], w)
    # An entry flagged to be ignored.
    for flags in (NO_PREPARE, NO_PREPARE | QUEUED, NO_PREPARE | ON_PAGE):
        w = blank_world()
        with_entry(w, 1, flags=flags)
        yield case(PREPARE, [1, fb(0.5)], w)
    # The use count: every byte of the frame's place, 0, 0xfe, 0xff, and the score bits.
    for freq in (0, 1, 7, 15):
        for count in (0, 1, 0xfe, 0xff):
            w = blank_world()
            w["freq"] = freq
            w["entries"][0]["own"] = dict(bytes=[count] * 16, f10=100, f14=0x1234)
            yield case(PREPARE, [1, fb(1.0)], w)
    # A separate place record.
    w = blank_world()
    w["places"][2] = dict(bytes=list(range(16)), f10=5, f14=0)
    with_entry(w, 1, place=3)
    yield case(PREPARE, [1, fb(1.0)], w)
    # Already queued or on a page: the nearer distance wins; the lock needs a changed view.
    for flags in (QUEUED, ON_PAGE, ON_PAGE | LOCKED, LOCKED, QUEUED | LOCKED, ON_PAGE | QUEUED | LOCKED):
        for sys_flags in (0, VIEW_CHANGED, LIGHTS_CHANGED, VIEW_CHANGED | LIGHTS_CHANGED):
            for prio, dist in ((1.0, 0.5), (0.5, 1.0), (1.0, 1.0), (NAN, 0.5), (0.5, NAN)):
                w = blank_world()
                w["sys_flags"] = sys_flags
                with_entry(w, 1, flags=flags, prio=prio, page=2)
                yield case(PREPARE, [1, fb(dist)], w)
    # The sort key: locked and the 0x80000 kind, wide and tall sizes, the size mask.
    for flags in (LOCKED, BLEND_KIND, LOCKED | BLEND_KIND, 0x30, 0xc0, 0xf0, 0x20, 0x10, 0x40, 0x80):
        for mask in (0, 7, 3, 15, 255, -1):
            w = blank_world()
            w["size_mask"] = mask
            with_entry(w, 1, flags=flags, w=13, h=200, cw=1, ch=1)
            yield case(PREPARE, [1, fb(0.75)], w)
    for width, height in ((0x7fff, 0x7fff), (-0x8000, 5), (255, 256), (1, 1)):
        w = blank_world()
        w["size_mask"] = 7
        with_entry(w, 1, w=width, h=height, cw=1, ch=1)
        yield case(PREPARE, [1, fb(1.25)], w)
    # An image of several planes: the next entries come along.
    for flags in ((NEXT_IS_PLANE, NEXT_IS_PLANE, 0), (NEXT_IS_PLANE, 0, 0), (NEXT_IS_PLANE, NEXT_IS_PLANE | ON_PAGE, 0),
                  (NEXT_IS_PLANE, NO_PREPARE | NEXT_IS_PLANE, 0), (NEXT_IS_PLANE, QUEUED | NEXT_IS_PLANE, NEXT_IS_PLANE)):
        w = blank_world()
        for i, f in enumerate(flags):
            with_entry(w, i + 1, flags=f)
        yield case(PREPARE, [1, fb(0.5)], w)
    # The debug stream: the first call starts the file, later ones append; the open may fail.
    for cache_flags in (0, 0x40000000):
        for opened in (1, 0):
            for chain in (0, NEXT_IS_PLANE):
                w = blank_world()
                set_settings(w, flags=0x80)
                w["cache_flags"] = cache_flags
                w["queues"] = {"fopen": [opened]}
                with_entry(w, 1, flags=chain, prio=0.5)
                yield case(PREPARE, [1, fb(0.25)], w)
    w = blank_world()
    set_settings(w, flags=0x80)
    with_entry(w, 1, flags=NO_PREPARE)
    yield case(PREPARE, [1, fb(0.25)], w)


def edge_upload():
    def world(**kw):
        w = blank_world()
        w["quad"] = dict(x=8, y=16, w=32, h=32, entry=2)
        set_settings(w, fixups=(0.25, 0.125, -0.5, 1.5))
        w["links"][0].update(tw=256, th=128)
        w.update(kw)
        return w

    # The entry from the quad or the call; a renderer that is not OpenGL stops early.
    for entry in (0, 1, 2):
        for renderer in (3, 0, 4, 2):
            w = world(renderer=renderer)
            with_entry(w, 1, flags=QUEUED | 0x10, w=20, h=20, cw=32, ch=32, texture=0)
            with_entry(w, 2, flags=QUEUED, w=30, h=31, cw=32, ch=32)
            yield case(UPLOAD, [1, 1, entry], w)
    # The flags: wrapping along either or both directions with and without the 0x400 flag and the
    # second block; the fixup flag; the centring flag with a smaller image in either direction.
    for flags in (0, WRAP_U, WRAP_V, WRAP, FIXUP, WRAP | FIXUP, CENTRED, CENTRED | FIXUP, CENTRED | WRAP,
                  WRAP | FLAG_400, WRAP_U | FLAG_400, FLAG_400, WRAP | FLAG_400 | FIXUP, ON_PAGE | QUEUED | 0x1000):
        for f38 in (0, 1):
            for link in (1, 2):
                w = world()
                with_entry(w, 1, flags=flags, w=17, h=19, cw=32, ch=32, f38=f38, texture=0, page=0)
                yield case(UPLOAD, [link, 1, 1], w)
    for (width, height, cw, ch) in ((32, 32, 32, 32), (20, 32, 32, 32), (32, 20, 32, 32), (20, 21, 32, 32), (40, 40, 32, 32),
                                    (33, 31, 32, 32), (1, 1, 8, 8), (0, 0, 8, 8), (-4, 5, 8, 8), (7, 8, 16, 33)):
        for flags in (CENTRED, 0, CENTRED | FIXUP):
            w = world()
            with_entry(w, 1, flags=flags, w=width, h=height, cw=cw, ch=ch)
            yield case(UPLOAD, [1, 1, 1], w)
    # Page sizes of 0, negative and huge; scales; settings that are not numbers.
    for tw, th in ((0, 0), (0, 64), (64, 0), (-64, 64), (0x7fffffff, 1), (1, 0x7fffffff), (-0x80000000, 3), (3, 7)):
        for flags in (0, WRAP, FIXUP):
            w = world()
            w["links"][0].update(tw=tw, th=th)
            with_entry(w, 1, flags=flags, w=12, h=13, cw=16, ch=16)
            yield case(UPLOAD, [1, 1, 1], w)
    for scale in (0.0, 1.0, 0.5, 3.0, NAN, INF, -2.0, 1.0e30, 1.0e-30):
        for flags in (0, WRAP_U):
            w = world(scale=scale)
            with_entry(w, 1, flags=flags, w=12, h=13, cw=16, ch=16)
            yield case(UPLOAD, [1, 1, 1], w)
    for half in (0.5, 0.0, -0.0, 1.0, NAN, INF, 0.3, 1.0e30):
        for fixups in ((-1.0, -1.0, 0.0, 0.0), (0.1, 0.2, 0.3, 0.4), (NAN, 1.0, INF, -INF), (1.0e30, -1.0e30, 5.0, 1.0e-30),
                       (0.0, -0.0, 0.0, -0.0), (3.0, 3.0, 1.0e-45, 2.0)):
            for flags in (FIXUP, 0, WRAP):
                w = world()
                set_settings(w, half=half, fixups=fixups)
                with_entry(w, 1, flags=flags, w=12, h=13, cw=16, ch=16)
                yield case(UPLOAD, [1, 1, 1], w)
    # Sums and quotients that round: random sizes, corners and fixups.
    r = random.Random(11)
    for _ in range(300):
        w = world(scale=r.choice([1.0, 1.0, 0.5, 3.7, T.f32(r.uniform(0.1, 4.0))]))
        set_settings(w, half=T.f32(r.uniform(0.0, 1.0)), fixups=[T.f32(r.uniform(-2.0, 2.0)) for _ in range(4)])
        w["links"][0].update(tw=r.randint(1, 1024), th=r.randint(1, 1024))
        w["quad"].update(x=r.choice([r.randint(0, 9), r.randint(0, 1023), r.randint(0, 1 << 20)]),
                         y=r.choice([r.randint(0, 9), r.randint(0, 1023), r.randint(0, 1 << 20)]), entry=1)
        flags = r.choice([0, FIXUP, FIXUP, CENTRED | FIXUP, WRAP, WRAP_U, WRAP_V | FLAG_400])
        with_entry(w, 1, flags=flags, w=r.randint(1, 256), h=r.randint(1, 256), cw=r.randint(1, 256), ch=r.randint(1, 256))
        yield case(UPLOAD, [1, 1, 0], w)
    # Large offsets: the sums that round.
    for x, y in ((0x7fffffff, -0x80000000), (16777217, 16777219), (-16777217, 3), (100000, 99999)):
        w = world()
        w["quad"].update(x=x, y=y, entry=1)
        with_entry(w, 1, flags=FIXUP, w=16777217, h=-16777219 & 0x7fff, cw=16, ch=16)
        yield case(UPLOAD, [1, 1, 0], w)


def edge_generate():
    def world(count, cache_flags=0):
        w = blank_world()
        w["mf1830"] = [5, 6, 7, 8, 9, 10]
        w["cache_flags"] = cache_flags
        w["head"] = 1 if count else 0
        for i in range(count):
            en = w["entries"][i]
            en.update(next=i + 2 if i + 1 < count else 0, flags=ON_PAGE | QUEUED | LOCKED | BLEND_KIND | 0xc0 | 0x30 | (0x100 * i),
                      w=13 + i, h=100 + 3 * i, cw=1, ch=1, prio=0.25 * i - 0.5, sortkey=7)
        return w

    # An empty queue: PackTextures is not called.
    w = world(0)
    w["queues"]["Pack"] = [PACK_DEFAULT]
    yield case(GENERATE, [], w)
    # Packing that takes everything, that fails at once, that fits nothing (-1) and so drops
    # entries one by one, and mixtures.
    plans = ([(255, 1)], [(255, 5)], [(0, 0)], [(2, 0)], [(1, 1), (255, 1)], [(0, -1)], [(0, -1), (0, -1), (255, 1)],
             [(1, -1), (0, 0)], [(0, -1), (1, 1), (0, 0)], [(1, 2), (1, -2), (0, 0)], [(1, 1), (0, -1), (0, 0)],
             [(0, -1), (1, 1), (255, 1)], [(3, 1)], [(0, -1), (0, -1), (0, -1), (0, -1)])
    for count in range(5):
        for plan in plans:
            w = world(count, cache_flags=0x20000000)
            queue = []
            left = count
            for take, result in plan:
                queue.append(take + ((result + 8) << 8))
            if count == 0:
                continue
            # the plan must not drop an empty list's head: skip the ones that would
            sim, ok, result = count, True, 1
            for take, res in plan:
                if sim <= 0 or result == 0:
                    break
                if res == -1:
                    if take >= sim:
                        ok = False
                        break
                    sim -= take + 1
                    result = 1
                else:
                    sim = max(sim - take, 0)
                    result = res
            if not ok or (sim > 0 and result != 0 and len(plan) < 10):
                # keep the plan only when it ends the loop (the last answer repeats otherwise)
                if not ok:
                    continue
                last_take, last_res = plan[-1]
                if not (last_res == 0 or last_take == 255):
                    continue
            w["queues"]["Pack"] = queue
            yield case(GENERATE, [], w)
    # The sort key of a dropped entry: priorities, the 0x80000 kind, sizes.
    for prio in (0.0, 1.5, 2.0, -5.0, NAN, INF, 1.0e9):
        for flags in (0, BLEND_KIND, LOCKED, 0xffffffff):
            w = world(2)
            with_entry(w, 1, prio=prio, flags=flags, w=0x7ff9, h=-5)
            with_entry(w, 2, prio=prio, flags=flags, w=1, h=0)
            w["queues"]["Pack"] = [0 + ((-1 + 8) << 8), 255 + (8 << 8)]
            yield case(GENERATE, [], w)


def edge_dump_prepare():
    for cache_flags in (0, 0x40000000, 0x20000000, 0x60000000):
        for opened in (1, 0):
            for entry in (1, 4):
                w = blank_world()
                w["cache_flags"] = cache_flags
                w["display_flags"] = 0x12345678
                w["sys_flags"] = 0x9abcdef0
                w["settings"] = [0x01020304 * (i + 1) & MASK for i in range(20)]
                w["queues"] = {"fopen": [opened]}
                with_entry(w, entry, flags=0xffffffff, w=-2, h=0x7fff, prio=-0.0)
                yield case(DUMP_PREPARE, [entry], w)
    for prio in (0.0, 1.5, NAN, INF, -INF, 1.0e-45):
        w = blank_world()
        with_entry(w, 2, prio=prio, flags=0x80000000, w=0x7fff, h=-0x8000)
        yield case(DUMP_PREPARE, [2], w)


def edge_dump_execute():
    for opened in (1, 0):
        w = blank_world()
        w["queues"] = {"fopen": [opened]}
        w["cache_flags"] = 0x40000000
        yield case(DUMP_EXECUTE, [], w)


def edge_page_updated():
    for link in (1, 2):
        for updates in (0, 1, 0x7fffffff, -1):
            for frame in (0, 1, -1, 0x7fffffff, -0x80000000, 12345):
                for answer in (0, 1, -1, 0x7fffffff):
                    w = blank_world()
                    w["updates"], w["frame"] = updates, frame
                    w["queues"] = {"UnlockSurface": [answer]}
                    w["links"][link - 1]["f14"] = 99
                    yield case(PAGE_UPDATED, [link], w)


def edge_reinitialize():
    for cache_flags in (0, 0x20000000, 0x40000000, 0x60000000, 0xffffffff, 0xdfffffff):
        for fmt in (-1, 0, 4, 5, 3, 0x7fffffff, -2):
            for create in (0, 1, -1, 12):
                w = blank_world()
                w["cache_flags"] = cache_flags
                set_settings(w, fmt=2)
                w["queues"] = {"Close": [1], "AutoCreateSurfaces": [create]}
                yield case(REINITIALIZE, [fmt], w)


def test(name, edges, rnd):
    return FnTest(name, ppc_run, host_run, edge=edges, rnd=rnd)


TESTS = [
    test("CTextureCache::Prepare", edge_prepare, rnd_prepare),
    test("CTextureCache::UploadToVRAM", edge_upload, rnd_upload),
    test("CTextureCache::GenerateTexturePages", edge_generate, rnd_generate),
    test("CTextureCache::DumpTCStreamToLog_Prepare", edge_dump_prepare, rnd_dump_prepare),
    test("CTextureCache::DumpTCStreamToLog_Execute", edge_dump_execute, rnd_dump_execute),
    test("CTextureLink::PageUpdated", edge_page_updated, rnd_page_updated),
    test("CTextureCache::ReInitialize", edge_reinitialize, rnd_reinitialize),
]
