"""Unit w176 (main app), against libtpw_unit_ArrivalControls.dylib (tests/unit_ArrivalControls.cpp):

  src/game/people/Arrival.cpp   CArrival::Serialise (0x100f2040), CreatePeople (0x100f2350), HowManyPeople (0x100f2638), CreatePerson (0x100f2840),
                           __sinit_Arrival_cpp (0x100f29b0)
  src/game/ControlManager.cpp   CControlManager::Serialise (0x100f9ed0), GetObjectControl (0x100fa0b8, and the const twin 0x100fa1b8),
                           CheckAllControlsInitialised (0x100fa32c), LOADVARSTRUCT / SAVEVARSTRUCT of ObjectControl (0x100fa4a0, 0x100fa6e4),
                           USwapInplace32 of unsigned long and long (0x100fa5fc, 0x100fa644)

They run as the original on the PowerPC side and as src/ on the host (with src/game/ui/ui_visitorStatus.cpp's GetWorld and TimeSig.h's inline members). Every
callee outside the unit is a fake that records the same event on both sides (see unit_ArrivalControls.cpp): the arrival vehicle, the analyser and
weather, the map's bus stop, CPerson's constructor and the allocation, CTRACE, GAME_ASSERTMSG, the serialise log, the file, the basic LOADVAR and
SAVEVAR instances, the ride database walk.

Memory is compared as a logical image: the host's little-endian bytes; the PowerPC side's blocks are built from the image with every multi-byte
field exchanged (and read back the same way), the control manager's cached control (a Mac pointer, the host's index + 1) is the one word converted by
meaning. Whole blocks are compared, guards included. Not tested: the game type's first-use construction (the guard byte is always set), the NULL
answer of CreatePerson's operator new, the five pad bytes of a saved object control (2, 3, 0x11 to 0x13: the Mac writes stack leftovers; the written
hex has them cleared on both sides).
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ArrivalControls"

MASK = 0xFFFFFFFF
A = dict(ser=0x100F2040, create=0x100F2350, how=0x100F2638, person=0x100F2840, sinit=0x100F29B0, cmser=0x100F9ED0, get=0x100FA0B8,
         getc=0x100FA1B8, check=0x100FA32C, lstruct=0x100FA4A0, sstruct=0x100FA6E4, uswapu=0x100FA5FC, uswapl=0x100FA644)
SECS, GETWORLD, SETTIMESIG, TSSER, OBJBYTESWAP = 0x1015F124, 0x1014205C, 0x1015F02C, 0x1015EF5C, 0x100FA688
REAL = set(A.values()) | {SECS, GETWORLD, SETTIMESIG, TSSER, OBJBYTESWAP}
WORLD, WORLDMAP, MAIN, GAME_TYPE, GAME_GUARD, SAVELOAD = 0x1037EC0C, 0x1037EC08, 0x102B4568, 0x102B3AA0, 0x102B3AA4, 0x1037FEFC
FILE_HANDLE = 0x6E100000
TAIL0 = 0x1DA6F0
TURN_WORD = (0x1DA70C - TAIL0) // 4
F738_WORD = (0x1DA738 - TAIL0) // 4
G = 0x40
ARR_TOTAL, CTL_TOTAL, TAIL, AN_SIZE, WX_SIZE, SPARE = G + 0x14 + G, G + 0x12CC + G, 0x80, 0x20D00, 0x40, 0x40
L_WORLD, L_AN, L_WX, L_MAP, L_FILE, L_SL, L_ARR, L_CTL = 1, 2, 3, 4, 5, 6, 7, 8
KINDS = {0x1000EE04: (1, 0), 0x1000F384: (1, 1), 0x1000EF18: (2, 0), 0x1000F4B0: (2, 1), 0x1000F144: (3, 0), 0x1000F70C: (3, 1),
         0x1000F02C: (4, 0), 0x1000F5DC: (4, 1), 0x100F1300: (5, 0), 0x100F1188: (5, 1)}
WIDTH = {1: 4, 2: 4, 3: 1, 4: 4, 5: 2}
ARR_FIELDS = [(G + o, 4) for o in (0, 4, 8, 0xC)]
CTL_FIELDS = [(G + 0x20 * i + o, w) for i in range(150) for o, w in ((0, 2), (4, 4), (8, 4), (0xC, 4), (0x14, 4), (0x18, 4), (0x1C, 4))] + \
             [(G + 0x12C0, 4), (G + 0x12C4, 2)]
CACHE = G + 0x12C8
TAIL_FIELDS = [(4 * i, 4) for i in range(32)]
AN_FIELDS = [(0x20CC0, 4)]
WX_FIELDS = [(0x30, 4)]
SPARE_FIELDS = [(0x40, 2)] + [(0x40 + o, 4) for o in (4, 8, 0xC, 0x14, 0x18, 0x1C)]
POOL = [0, 1, 2, 7, 0x10, 0x55, 0xFF, 0x100, 0x1234, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF]
INTS = [0, 1, 2, 5, 0x23, 0x24, 0x3C, 0x3D, 0x1F4, 0x5DC, 0x7FFFFFFF, 0x80000000, MASK, MASK - 1, 0x1000, 100]
STATUS = [-2, -1, 0, 1, 2, 3, 4, 5, 6, 7]


def s32(v):
    return T.s32(v)


def fill(seed, size):
    return bytearray(random.Random(seed).randbytes(size))


def swapf(img, fields):
    b = bytearray(img)
    for o, w in fields:
        b[o:o + w] = b[o:o + w][::-1]
    return bytes(b)


def put(img, off, width, value):
    img[off:off + width] = (value & ((1 << (8 * width)) - 1)).to_bytes(width, "little")


def get(img, off, width):
    return int.from_bytes(img[off:off + width], "little")


def be32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


class Env:
    pass


# ------------------------------------------------------------------ the case

def arr_image(c):
    img = fill(c["seed"], ARR_TOTAL)
    for off, w, key in ((0, 4, "rate"), (4, 4, "stamp"), (8, 4, "cap"), (0xC, 4, "onbus"), (0x10, 1, "off"), (0x11, 1, "gates"), (0x12, 2, "pad")):
        if key in c:
            put(img, G + off, w, c[key])
    return bytes(img)


def ctl_image(c):
    img = fill(c["seed"] + 5, CTL_TOTAL)
    ids = c["ids"]
    for i in range(150):
        if i < len(ids):
            put(img, G + 0x20 * i, 2, ids[i])
    put(img, G + 0x12C0, 4, c["count"])
    put(img, G + 0x12C4, 2, c["key"])
    put(img, CACHE, 4, c["cache"])
    return bytes(img)


def tail_image(c):
    words = [random.Random(c["seed"] + 9 + i).getrandbits(32) for i in range(32)]
    words[TURN_WORD] = c["turn"]
    words[F738_WORD] = c["f738"]
    return struct.pack("<32I", *words)


def an_image(c):
    chunk = bytes(fill(c["seed"] + 3, 64))
    img = bytearray((chunk * (AN_SIZE // 64 + 1))[:AN_SIZE])
    put(img, 0x20CC0, 4, c["anw"])
    return bytes(img)


def wx_image(c):
    img = fill(c["seed"] + 4, WX_SIZE)
    put(img, 0x30, 4, c["wx"])
    return bytes(img)


def spare_image(c):
    return bytes(fill(c["seed"] + 6, SPARE + 0x20 + SPARE))   # a guard of 0x40 bytes each side of the 0x20 control (at +0x40, see spare_off)


def base_case(r, **kw):
    c = dict(seed=r.getrandbits(32), rate=r.choice(INTS), stamp=r.choice(INTS) if r.random() < 0.5 else r.getrandbits(32), cap=r.choice(INTS),
             onbus=r.choice(INTS), off=r.choice([0, 1, 2, 0xFF]), gates=r.choice([0, 1, 2, 0xFF]), pad=r.getrandbits(16),
             turn=r.getrandbits(32), f738=r.choice([0, 1, 2, 3, 4, 4, 5, MASK]) if r.random() < 0.7 else r.getrandbits(32),
             bal={178: r.choice([0, 1, 5, 20, 100, MASK, 0x80000000]), 179: r.choice([0, 1, 2, 10, 100, 4000, MASK, 0x80000000]),
                  181: r.choice([0, 1, 100, 5000, MASK, 0x80000000]), 182: r.choice([0, 1, 2, 3, 7, 50, MASK, 0x80000000])},
             type=r.choice([0, 1, 2, 3, MASK]), tguard=r.choice([1, 0x7F, 0x80, 0xFF]), anw=r.choice(INTS), wx=r.choice([0, 1, 2, MASK, 0x80000000, 0x7FFFFFFF, r.getrandbits(32)]),
             ids=[], count=0, key=0, cache=0, file=b"", rides=[], lbase=0,
             q={"status": [r.choice(STATUS) for _ in range(3)], "wait": [r.choice([0, 1, 2, -1])], "sea": [r.choice([0, 1, -1, 0x100])],
                "cell": [r.choice([0, 1, 255, 256, 0x3FFF, 0xFFFF, r.getrandbits(16)])], "visitors": [r.choice(INTS) if r.random() < 0.6 else r.getrandbits(32)],
                "points": [r.choice(INTS) if r.random() < 0.6 else r.getrandbits(32)], "ans": [0], "lv": [r.getrandbits(32) for _ in range(8)]})
    c.update(kw)
    for k in ("visitors", "points"):
        c["q"][k] = [s32(v) for v in c["q"][k]]
    return c


# ------------------------------------------------------------------ the PowerPC side

def build_ppc(e, c):
    v = Env()
    v.L = L = CL.PpcLog({}, {k: list(x) for k, x in c["q"].items()})
    e.cl = L
    v.arr = e.alloc(ARR_TOTAL, swapf(arr_image(c), ARR_FIELDS))
    v.ctl = e.alloc(CTL_TOTAL, swapf(ctl_image(c), CTL_FIELDS))
    obj = v.ctl + G
    be32(e, v.ctl + CACHE, 0 if c["cache"] == 0 else obj + 0x20 * (c["cache"] - 1))
    v.tail = e.alloc(TAIL, swapf(tail_image(c), TAIL_FIELDS))
    v.an = e.alloc(AN_SIZE, swapf(an_image(c), AN_FIELDS))
    v.wx = e.alloc(WX_SIZE, swapf(wx_image(c), WX_FIELDS))
    v.map = e.alloc(0x40, bytes(fill(c["seed"] + 8, 0x40)))
    v.spare = e.alloc(SPARE + 0x20 + SPARE, swapf(spare_image(c), SPARE_FIELDS))
    v.world = v.tail - TAIL0
    v.base = v.arr + G if c["lbase"] == 0 else obj
    v.file = bytearray(c["file"])
    v.rides, v.at = [], 0
    for rd in c["rides"]:
        img = bytearray(fill(rd["seed"], 0x200))
        img[0:2] = (rd["id"] & 0xFFFF).to_bytes(2, "big")
        for o, key in ((0xE8, "fatigue"), (0x16C, "ripoff"), (0x180, "research"), (0x1B8, "cost")):
            img[o:o + 4] = (rd[key] & MASK).to_bytes(4, "big")
        v.rides.append(e.alloc(0x200, bytes(img)))
    L.labels.update({v.world: L_WORLD, v.an: L_AN, v.wx: L_WX, v.map: L_MAP, FILE_HANDLE: L_FILE, SAVELOAD: L_SL, v.arr + G: L_ARR, obj: L_CTL})
    be32(e, WORLD, v.world)
    be32(e, WORLDMAP, v.map)
    for slot, value in c["bal"].items():
        be32(e, MAIN + 8 + 4 * slot, value)
    be32(e, GAME_TYPE, c["type"])
    e.write(GAME_GUARD, bytes([c["tguard"]]))
    v.S = stubs(e, v, c)
    return v


def stubs(e, v, c):
    L = v.L
    S = CL.Stubs()
    S.real = REAL
    R = CL.recorder

    def trace(emu):
        fmt = emu.cstr(emu.r(4))
        L.rec("CTRACE", (emu.r(3), s32(emu.r(5)) if "%d" in fmt else 0), (fmt[:159],))

    def mkvar(kind, save):
        def fn(emu):
            ans = s32(L.ret("ans", 0))
            ptr, w = emu.r(4), WIDTH[kind]
            if save:
                shown = int.from_bytes(emu.read(ptr, w), "big")
            else:
                loaded = L.ret("lv", 0) & MASK
                shown = 0
                if ans == 0:
                    shown = loaded & ((1 << (8 * w)) - 1)
                    emu.write(ptr, shown.to_bytes(w, "big"))
            L.rec("VAR", (kind, int(save), L.lab(emu.r(3)), ptr - v.base, s32(emu.r(6)), shown, ans), (emu.cstr(emu.r(5))[:159],))
            emu.set_r(3, ans & MASK)
        return fn

    def lb_read(emu):
        buf, length = emu.r(5), emu.r(6)
        L.rec("LbFile_Read", (L.lab(emu.r(4)), length))
        count = min(len(v.file), length)
        forced = L.ret("LbFile_Read:xfer", -1)
        if forced >= 0:
            count = forced & MASK
        emu.write(emu.r(7), (count & MASK).to_bytes(4, "big"))
        result = s32(L.ret("LbFile_Read"))
        if count == length and result >= 0:
            emu.write(buf, (bytes(v.file) + bytes(length))[:length])
        used = min(len(v.file), count)
        del v.file[:used]
        emu.write(emu.r(3), (result & MASK).to_bytes(4, "big"))

    def lb_write(emu):
        length = emu.r(6)
        data = bytearray(emu.read(emu.r(5), length)) if length else bytearray()
        if length == 0x20:
            for i in (2, 3, 0x11, 0x12, 0x13):
                data[i] = 0
        L.rec("LbFile_Write", (L.lab(emu.r(4)), length), (bytes(data).hex()[:150],))
        forced = L.ret("LbFile_Write:xfer", -1)
        emu.write(emu.r(7), ((forced if forced >= 0 else length) & MASK).to_bytes(4, "big"))
        emu.write(emu.r(3), (s32(L.ret("LbFile_Write")) & MASK).to_bytes(4, "big"))

    def answer(name, key, labels=1):
        def fn(emu):
            L.rec(name, (L.lab(emu.r(3)),))
            emu.set_r(3, L.ret(key, 0) & MASK)
        return fn

    def donext(emu):
        L.rec("DoNextLeg", (L.lab(emu.r(4)), s32(emu.r(5))))
        emu.write(emu.r(3), b"\0\0")

    def visitors(emu):
        L.rec("GetVisitorsInPark", (L.lab(emu.r(3)), emu.r(4)))
        emu.set_r(3, L.ret("visitors", 0) & MASK)

    def bus(emu):
        L.rec("GetBusStopPos", (L.lab(emu.r(4)), s32(emu.r(5))))
        emu.write(emu.r(3), (L.ret("cell", 0) & 0xFFFF).to_bytes(2, "big"))

    def first(emu):
        L.rec("GetFirstRide", ())
        v.at = 0
        emu.set_r(3, v.rides[0] if v.rides else 0)

    def nxt(emu):
        L.rec("GetNextRide", ())
        if v.at < len(v.rides) - 1:
            v.at += 1
            emu.set_r(3, v.rides[v.at])
        else:
            emu.set_r(3, 0)

    def op_new(emu):
        size = emu.r(3)
        block = emu.alloc(max(size, 16), b"\xa5" * max(size, 16))
        if size == 0x22C:
            L.rec("new", (size, L.newlab(block)))
        emu.set_r(3, block)

    for addr, fn in ((0x1006FCBC, trace),
                     (0x1000DDE8, lambda emu: L.rec("GAME_ASSERTMSG", (s32(emu.r(3)),), (emu.cstr(emu.r(4))[:159],))),
                     (0x1015B3DC, R("CSaveLoad::WriteSerialiseLog", "Lisi")), (0x1016F5D8, lambda emu: emu.write(emu.r(3), bytes(4))),
                     (0x1013EBC8, lambda emu: (L.rec("GetParkAnalyser", (L.lab(emu.r(3)),)), emu.set_r(3, v.an))),
                     (0x1013ED74, lambda emu: (L.rec("GetWeather", (L.lab(emu.r(3)),)), emu.set_r(3, v.wx))),
                     (0x100E5414, visitors), (0x100E5B1C, answer("GetTotalParkAttractionPoints", "points")),
                     (0x100F2944, lambda emu: L.rec("SetAtEnd", (emu.r(3) - v.an, s32(emu.r(4))))), (0x10101A50, bus),
                     (0x10116634, lambda emu: L.rec("CPerson", (L.lab(emu.r(3)), emu.r(4) >> 16))), (0x10140234, donext),
                     (0x101406CC, answer("GetStatus", "status")), (0x10140B6C, answer("PeopleWaitingToLeave", "wait")),
                     (0x10140DE0, answer("IsSeagoing", "sea")), (0x101547B4, first), (0x10154800, nxt)):
        S.internal(addr, fn)
    for addr, (kind, save) in KINDS.items():
        S.internal(addr, mkvar(kind, save))
    S.imp("LbFile_Read__FPvPvUlPUl", lb_read)
    S.imp("LbFile_Write__FPvPCvUlPUl", lb_write)
    S.imp("__nw__FUl", op_new)
    return S


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc_report(e, v, c, **extra):
    obj = v.ctl + G
    raw = bytearray(swapf(e.read(v.ctl, CTL_TOTAL), CTL_FIELDS))
    word = int.from_bytes(raw[CACHE:CACHE + 4], "big")   # the word is not a swapped field: it is the pointer as stored
    word = int.from_bytes(e.read(v.ctl + CACHE, 4), "big")
    put(raw, CACHE, 4, 0 if word == 0 else (word - obj) // 0x20 + 1 if (word - obj) % 0x20 == 0 else 0x40000000 + word)
    out = dict(arr=swapf(e.read(v.arr, ARR_TOTAL), ARR_FIELDS), ctl=bytes(raw), tail=swapf(e.read(v.tail, TAIL), TAIL_FIELDS),
               an=swapf(e.read(v.an, AN_SIZE), AN_FIELDS), wx=swapf(e.read(v.wx, WX_SIZE), WX_FIELDS), map=e.read(v.map, 0x40),
               spare=swapf(e.read(v.spare, SPARE + 0x20 + SPARE), SPARE_FIELDS),
               file=bytes(v.file), world=v.L.lab(e.u32(WORLD)), log=v.L.log)
    out.update(extra)
    return out


def run_ppc(e, c, entry, args_of, kind="int", obj_result=False):
    v = build_ppc(e, c)
    saved = v.S.activate(e)
    try:
        r3 = T.call_ppc(e, entry, args_of(v, e))[0]
        extra = {}
        if kind == "int":
            extra["r"] = s32(r3)
        elif kind == "ctl":   # the control found, as its offset in the manager
            extra["r"] = -1 if r3 == 0 else s32(r3 - (v.ctl + G))
        elif kind == "words":
            pass
        return ppc_report(e, v, c, **extra)
    finally:
        CL.Stubs.deactivate(e, saved)
        e.cl = None


# ------------------------------------------------------------------ the host side

BOUND = set()


def bind(lib):
    if id(lib) in BOUND:
        return
    BOUND.add(id(lib))
    VP, I32, U32, I64 = C.c_void_p, C.c_int32, C.c_uint32, C.c_int64
    sig = dict(tpw_avc_addr=(VP, [I32]), tpw_avc_world=(VP, []), tpw_avc_config=(None, [I32, I32]), tpw_avc_set_file=(None, [C.c_char_p, I32]),
               tpw_avc_balance=(None, [I32, I32]), tpw_avc_mode=(None, [I32]), tpw_avc_ride=(None, [I32] * 6), tpw_avc_ride_count=(None, [I32]),
               tpw_avc_arrival_serialise=(I32, [VP, I32, I32, U32]), tpw_avc_create_people=(None, []), tpw_avc_how_many=(I32, []),
               tpw_avc_create_person=(I32, []), tpw_avc_sinit=(None, [C.POINTER(U32)]), tpw_avc_cm_serialise=(I32, [VP, I32, I32, U32]),
               tpw_avc_get=(I64, [I32, U32, I32]), tpw_avc_check=(None, []), tpw_avc_struct=(I32, [I32, VP, C.c_char_p, I32]),
               tpw_avc_uswap=(None, [I32, VP]))
    for name, (res, args) in sig.items():
        f = getattr(lib, name)
        f.restype, f.argtypes = res, args


def addr(lib, which):
    return lib.tpw_avc_addr(which)


def run_host(lib, c, call):
    bind(lib)
    lib.tpw_avc_config(c["lbase"], 1)
    C.memmove(addr(lib, 0), arr_image(c), ARR_TOTAL)
    C.memmove(addr(lib, 4), ctl_image(c), CTL_TOTAL)
    C.memmove(addr(lib, 1), tail_image(c), TAIL)
    C.memmove(addr(lib, 2), an_image(c), AN_SIZE)
    C.memmove(addr(lib, 3), wx_image(c), WX_SIZE)
    C.memmove(addr(lib, 5), bytes(fill(c["seed"] + 8, 0x40)), 0x40)
    C.memmove(addr(lib, 8), spare_image(c), SPARE + 0x20 + SPARE)
    for slot, value in c["bal"].items():
        lib.tpw_avc_balance(slot, s32(value))
    lib.tpw_avc_mode(s32(c["type"]))
    for i, rd in enumerate(c["rides"]):
        lib.tpw_avc_ride(i, rd["id"], s32(rd["fatigue"]), s32(rd["ripoff"]), s32(rd["research"]), s32(rd["cost"]))
    lib.tpw_avc_ride_count(len(c["rides"]))
    lib.tpw_avc_set_file(bytes(c["file"]), len(c["file"]))
    fixed = {L_WORLD: lib.tpw_avc_world(), L_AN: addr(lib, 2), L_WX: addr(lib, 3), L_MAP: addr(lib, 5), L_FILE: addr(lib, 6), L_SL: addr(lib, 7),
             L_ARR: addr(lib, 0) + G, L_CTL: addr(lib, 4) + G}
    CL.host_begin(lib, fixed, {k: list(x) for k, x in c["q"].items()})
    try:
        r = call(lib)
    finally:
        log = CL.host_end(lib)
    out = dict(arr=C.string_at(addr(lib, 0), ARR_TOTAL), ctl=C.string_at(addr(lib, 4), CTL_TOTAL), tail=C.string_at(addr(lib, 1), TAIL),
               an=C.string_at(addr(lib, 2), AN_SIZE), wx=C.string_at(addr(lib, 3), WX_SIZE), map=C.string_at(addr(lib, 5), 0x40),
               spare=C.string_at(addr(lib, 8), SPARE + 0x20 + SPARE), file=b"", world=L_WORLD, log=log)
    out["file"] = None
    if r is not None:
        out["r"] = r
    return out


def finish(p, h):
    """The remaining file bytes are the PowerPC side's (the host's are in the library); drop them from the comparison."""
    p.pop("file", None)
    h.pop("file", None)
    return p, h


class Fn(FnTest):
    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        try:
            p = self.ppc(runner.emu, case)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(runner.lib, case)
        finish(p, h)
        difference = CL.first_difference(p.pop("log"), h.pop("log"))
        errors = T.same_values(p, h, "", False)
        if difference:
            errors.append(difference)
        return errors


# ------------------------------------------------------------------ the functions

def fn_test(name, entry, args_of, call, kind, edge, rnd):
    def ppc(e, c):
        return run_ppc(e, c, entry, args_of, kind)

    def host(lib, c):
        return run_host(lib, c, call)
    return Fn(name, ppc, host, edge=edge, rnd=rnd)


def this_arr(v, e):
    return [v.arr + G]


# ---- CArrival::Serialise
def mk_ser(r, save, version, **kw):
    c = base_case(r, **kw)
    c.update(save=save, version=version, unused=r.getrandbits(32))
    return c


def ser_args(v, e):
    return [v.arr + G, FILE_HANDLE]


def ser_ppc(e, c):
    return run_ppc(e, c, A["ser"], lambda v, e2: [v.arr + G, FILE_HANDLE, c["save"] & MASK, c["version"] & MASK, c["unused"]], "int")


def ser_host(lib, c):
    return run_host(lib, c, lambda lb: lb.tpw_avc_arrival_serialise(addr(lb, 6), c["save"], s32(c["version"]), c["unused"]))


def ser_edges():
    r = random.Random(11)
    cases = []
    for save in (0, 1, 2, -1):
        for version in (-1, 0, 1, 2, 3, MASK, 0x80000000, 0x7FFFFFFF):
            cases.append(mk_ser(r, save, version))
    for save in (0, 1):
        for k in range(7):
            cases.append(mk_ser(r, save, r.choice([0, 1]), q=dict(base_case(r)["q"], ans=[0] * k + [r.choice([1, -1, 5, 0x100])])))
    cases.append(mk_ser(r, 0, 1, q=dict(base_case(r)["q"], lv=[0xFFFFFFFF] * 8)))
    cases.append(mk_ser(r, 0, 0, q=dict(base_case(r)["q"], lv=[0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 7, 8])))
    return cases


def ser_rnd(r):
    save = r.choice([0, 1, 0, 1, 2, -1])
    version = r.choice([0, 1, 0, 1, -1, 2, 3, r.getrandbits(32)])
    c = mk_ser(r, save, version)
    if r.random() < 0.3:
        c["q"]["ans"] = [0] * r.randrange(7) + [r.choice([1, -1, 9])]
    return c


# ---- CreatePeople
def cp_case(r, **kw):
    c = base_case(r, **kw)
    return c


def cp_edges():
    r = random.Random(21)
    cases = []
    # the seconds against the balance's TimeBetweenArrivals (an unsigned compare), offloading off
    for secs, bal in ((5, 4), (5, 5), (5, 6), (0, 0), (0, 1), (1, 0), (0x3FFFFFFF, 0x7FFFFFFF), (0x3FFFFFFF, 0x80000000), (3, MASK), (0, MASK)):
        stamp = 4 * 100
        turn = (4 * (100 + secs)) & MASK
        c = cp_case(r, off=0, stamp=stamp, turn=turn, f738=0)
        c["bal"][179] = bal
        cases.append(c)
    # the stamp ahead of the turn (the difference wraps)
    cases.append(cp_case(r, off=0, stamp=4000, turn=40, f738=0))
    cases.append(cp_case(r, off=0, stamp=0x80000000, turn=0, f738=0))
    for status in STATUS:
        for off in (1, 0xFF):
            for onbus in (-1, 0, 1, 0x23, 0x24, 0x3C, 0x3D, 0x7FFFFFFF, 0x80000000, MASK):
                c = cp_case(r, off=off, onbus=onbus)
                c["q"]["status"] = [status, status, r.choice(STATUS)]
                cases.append(c)
    for status in STATUS:
        for wait in (0, 1, -1):
            c = cp_case(r, off=0, onbus=r.choice([0, 1, 5]), turn=0, stamp=0)
            c["bal"][179] = MASK   # no bus starts
            c["q"]["status"] = [status, status, status]
            c["q"]["wait"] = [wait]
            cases.append(c)
    for status in STATUS:   # a bus starts, then every status
        c = cp_case(r, off=0, turn=4000, stamp=0, f738=0)
        c["bal"][179] = 10
        c["q"]["status"] = [status, status, status]
        cases.append(c)
    for onbus in (0, 1, 2):   # dropping off, with the people left
        c = cp_case(r, off=1, onbus=onbus)
        c["q"]["status"] = [2, 2, 2]
        c["q"]["wait"] = [1]
        cases.append(c)
    return cases


def cp_rnd(r):
    c = cp_case(r)
    c["off"] = r.choice([0, 1, 2, 0xFF])
    if r.random() < 0.5:
        c["turn"] = (c["stamp"] + 4 * r.randrange(0, 12)) & MASK
        c["bal"][179] = r.choice([0, 3, 5, 11, 12])
    return c


# ---- HowManyPeople
def hm_case(r, **kw):
    return base_case(r, **kw)


def hm_in_range(c):
    """False when the float product leaves the int range: the emulator's fctiwz wraps where the processor saturates, so those cases say nothing."""
    if c["f738"] == 4:
        return True
    total = T.s32(T.s32(c["q"]["points"][0]) + c["bal"][181])
    factor = 0.8 if T.s32(c["wx"]) > 0 else 1.2
    return abs(T.f32(T.f32(factor) * T.f32(total))) < 2147483000.0


def hm_edges():
    return [c for c in hm_edges_all() if hm_in_range(c)]


def hm_edges_all():
    r = random.Random(31)
    cases = []
    cases.append(hm_case(r, f738=4))
    cases.append(hm_case(r, f738=5))
    for wx in (0, 1, 0x7FFFFFFF, 0x80000000, MASK, 0x3F800000):
        for points in (0, 1, 1000, 123457):
            c = hm_case(r, wx=wx, f738=0)
            c["q"]["points"] = [points]
            c["bal"][181] = 7
            cases.append(c)
    for div in (MASK, 0x80000000, 0, 1, 2, 3):
        c = hm_case(r, f738=0)
        c["bal"][182] = div
        c["q"]["points"] = [1000]
        cases.append(c)
    for points, bonus in ((16777216, 0), (16777217, 0), (16777215, 0), (0x7FFFFFFF, 0), (0x7FFFFFFF, 1), (0x40000000, 0x40000000), (MASK, 0), (0x80000000, 0),
                          (3, 0), (1, 0), (5, 0), (7, MASK - 10), (0x1000001, 0), (-5 & MASK, 0), (0x7FFFFF00, 0)):
        for wx in (1, 0):
            c = hm_case(r, f738=0, wx=wx)
            c["q"]["points"] = [s32(points)]
            c["bal"][181] = bonus
            c["bal"][182] = 1
            cases.append(c)
    for typ in (0, 1, 2, 3, MASK):
        for visitors, minimum in ((0, 0), (499, 1), (500, 1), (501, 1), (400, 100), (400, 101), (1499, 1), (1500, 1), (1501, 1), (1400, 100), (1400, 101), (-1 & MASK, 5),
                                  (0x7FFFFFFF, 1), (0x80000000, 3), (500, 0x7FFFFFF0), (600, 900), (0, 0x5DC), (0, 0x5DD), (0, 0x1F4), (0, 0x1F5)):
            c = hm_case(r, f738=0, type=typ)
            c["q"]["visitors"] = [s32(visitors)]
            c["q"]["points"] = [0]
            c["bal"][178] = minimum
            c["bal"][181] = 0
            cases.append(c)
    return cases


def hm_rnd(r):
    c = hm_case(r)
    if r.random() < 0.7:
        c["f738"] = r.choice([0, 1, 2, 3])
    if r.random() < 0.6:
        c["q"]["points"] = [s32(r.randrange(0, 200000))]
        c["bal"][181] = r.randrange(0, 2000)
        c["bal"][182] = r.choice([0, 1, 2, 3, 5, 10, 50])
    if r.random() < 0.5:
        c["q"]["visitors"] = [r.choice([r.randrange(0, 2000), 480, 495, 1490, 1500, 1520])]
        c["bal"][178] = r.choice([0, 1, 5, 20, 100, 400, 600])
    c["type"] = r.choice([0, 1, 1, 2, 3, MASK])
    while not hm_in_range(c):
        c["q"]["points"] = [s32(r.randrange(0, 200000))]
        c["bal"][181] = r.randrange(0, 2000)
    return c


# ---- CreatePerson
def pp_edges():
    r = random.Random(41)
    cases = []
    for sea in (0, 1, -1, 0x100, 0x7FFFFFFF):
        for cell in (0, 1, 255, 256, 257, 0x3FFF, 0x4000, 0x7FFF, 0x8000, 0xFFFF, 0xFF00):
            c = base_case(r)
            c["q"]["sea"] = [sea]
            c["q"]["cell"] = [cell]
            cases.append(c)
    return cases


def pp_rnd(r):
    return base_case(r)


# ---- the static initializer
def sinit_ppc(e, c):
    e.reset_scratch()
    be32(e, 0x1034C1A4, c["a"])
    be32(e, 0x1034C1A8, c["b"])
    T.call_ppc(e, A["sinit"], [])
    return dict(words=[e.u32(0x1034C1A4), e.u32(0x1034C1A8)])


def sinit_host(lib, c):
    bind(lib)
    out = (C.c_uint32 * 2)()
    lib.tpw_avc_sinit(out)
    return dict(words=list(out))


def sinit_cases():
    return [dict(a=0, b=0), dict(a=MASK, b=MASK), dict(a=0x10000, b=0), dict(a=1, b=2)]


# ---- CControlManager::GetObjectControl (both)
def ctl_case(r, count=None, **kw):
    c = base_case(r, lbase=1, **kw)
    n = r.choice([0, 1, 2, 5, 20, 150, 149]) if count is None else count
    pool = r.sample(POOL, 7)
    ids = [r.choice(pool) if r.random() < 0.8 else r.getrandbits(16) for _ in range(150)]
    c.update(ids=ids, count=n & MASK if n >= 0 else n & MASK, key=r.choice(pool + [0]), cache=r.choice([0, 0, 1, 2, 5, 150]), pool=pool)
    return c


def get_case(r, constant, **kw):
    c = ctl_case(r, **kw)
    ids = c["ids"]
    n = s32(c["count"]) if s32(c["count"]) > 0 else 0
    mode = r.randrange(6)
    if mode == 0 and n > 0:
        wanted = ids[r.randrange(n)]
    elif mode == 1 and n > 0:
        wanted = ids[n - 1]
    elif mode == 2 and n > 0:
        wanted = ids[0]
    elif mode == 3:
        wanted = c["key"]
    else:
        wanted = r.choice(c["pool"] + [r.getrandbits(16)])
    c.update(wanted=wanted, flag=r.choice([0, 1, 1, 2, -1]), constant=constant, low=r.getrandbits(16))
    return c


def get_edges(constant):
    def edges():
        r = random.Random(51 + constant)
        cases = []
        for count in (0, 1, 2, 5, 150, -1, 0x80000000 - 2**32):
            for flag in (0, 1):
                for cache in (0, 1, 3):
                    for mode in range(5):
                        c = get_case(r, constant, count=count)
                        n = max(s32(c["count"]), 0)
                        c["flag"] = flag
                        c["cache"] = cache
                        c["wanted"] = [c["ids"][0] if n else 5, c["ids"][n - 1] if n else 6, c["key"], 0xABCD, 0][mode]
                        cases.append(c)
        # the first of two equal ids wins; a hit when the cache holds the key; the cached NULL
        c = get_case(r, constant, count=5)
        c["ids"][1] = c["ids"][3] = 0x1111
        c["wanted"], c["key"], c["cache"] = 0x1111, 0x2222, 0
        cases.append(c)
        c = get_case(r, constant, count=5)
        c["wanted"], c["key"], c["cache"] = 0x2222, 0x2222, 0
        cases.append(c)
        return cases
    return edges


def get_rnd(constant):
    return lambda r: get_case(r, constant)


def get_args(v, e, c):
    return [v.ctl + G, ((c["wanted"] & 0xFFFF) << 16) | c["low"], c["flag"] & MASK]


def get_test(name, entry, constant):
    def ppc(e, c):
        return run_ppc(e, c, entry, lambda v, e2: get_args(v, e2, c), "ctl")

    def host(lib, c):
        def call(lb):
            return lb.tpw_avc_get(constant, c["wanted"], s32(c["flag"]))
        out = run_host(lib, c, call)
        return out
    return Fn(name, ppc, host, edge=get_edges(constant), rnd=get_rnd(constant))


# ---- CControlManager::CheckAllControlsInitialised
def check_case(r, **kw):
    c = ctl_case(r, count=r.choice([0, 1, 5, 40, 100, 140]), **kw)
    n = r.choice([0, 1, 1, 2, 5, 10, 20])
    c["count"] = min(s32(c["count"]), 150 - n) & MASK if s32(c["count"]) >= 0 else c["count"]
    pool = c["pool"]
    rides = []
    for i in range(n):
        rides.append(dict(seed=r.getrandbits(32), id=r.choice(pool) if r.random() < 0.6 else r.getrandbits(16), fatigue=r.getrandbits(32),
                          ripoff=r.getrandbits(32), research=r.choice([0, 0, 1, MASK, 0x100, 0x80000000, r.getrandbits(32)]), cost=r.getrandbits(32)))
    c["rides"] = rides
    return c


def check_edges():
    r = random.Random(61)
    cases = []
    for n in (0, 1, 2, 5, 20):
        for count in (0, 1, 100, 130):
            c = check_case(r)
            c["rides"] = c["rides"][:0] + [dict(seed=r.getrandbits(32), id=r.choice(c["pool"]), fatigue=r.getrandbits(32), ripoff=r.getrandbits(32),
                                                  research=r.choice([0, 1, MASK]), cost=r.getrandbits(32)) for _ in range(n)]
            c["count"] = count
            cases.append(c)
    c = check_case(r)   # the cached control of the last ride, a cached NULL, the table filled to 150
    c["count"] = 145
    c["rides"] = [dict(seed=i, id=0x3000 + i, fatigue=i, ripoff=-i & MASK, research=i % 2, cost=1000 + i) for i in range(5)]
    c["ids"] = [0x100 + i for i in range(150)]
    c["key"], c["cache"] = 0x3001, 0
    cases.append(c)
    c = check_case(r)
    c["count"] = 6
    c["ids"] = [0x10, 0x10, 0x11, 0x12, 0x13, 0x14] + [0] * 144
    c["rides"] = [dict(seed=1, id=0x14, fatigue=1, ripoff=2, research=0, cost=3), dict(seed=2, id=0x10, fatigue=4, ripoff=5, research=1, cost=6),
                  dict(seed=3, id=0x99, fatigue=7, ripoff=8, research=0, cost=9), dict(seed=4, id=0x99, fatigue=7, ripoff=8, research=0, cost=9)]
    c["key"], c["cache"] = 0x14, 0
    cases.append(c)
    return cases


# ---- CControlManager::Serialise
def cm_case(r, save, version):
    c = ctl_case(r, count=r.choice([0, 1, 5, 150]))
    c.update(save=save, version=version, unused=r.getrandbits(32), file=bytes(fill(r.getrandbits(32), 150 * 0x20 + 8)))
    c["q"] = dict(c["q"], lv=[r.choice([0, 1, 7, 150, 151, MASK, 0x80000000]), r.getrandbits(16) if r.random() < 0.7 else r.choice([0, 0xFFFF]), 3, 4])
    return c


def cm_ppc(e, c):
    return run_ppc(e, c, A["cmser"], lambda v, e2: [v.ctl + G, FILE_HANDLE, c["save"] & MASK, c["version"] & MASK, c["unused"]], "int")


def cm_host(lib, c):
    return run_host(lib, c, lambda lb: lb.tpw_avc_cm_serialise(addr(lb, 6), c["save"], s32(c["version"]), c["unused"]))


def cm_edges():
    r = random.Random(71)
    cases = []
    for save in (0, 1, 2, -1):
        for version in (-1, 0, 1, 2, 3, MASK, 0x7FFFFFFF):
            cases.append(cm_case(r, save, version))
    for save in (0, 1):
        for k in (0, 1, 2, 148, 149):   # a file call failing at the k-th control
            c = cm_case(r, save, 1)
            call = "LbFile_Read" if save == 0 else "LbFile_Write"
            c["q"][call] = [0] * k + [-1]
            cases.append(c)
            c = cm_case(r, save, 0)
            c["q"][call + ":xfer"] = [0x20] * k + [0x10]
            cases.append(c)
        for ans in ([5], [0, 5], [0, -1], [0, 0]):
            c = cm_case(r, save, 1)
            c["q"]["ans"] = ans
            cases.append(c)
    short = cm_case(r, 0, 1)
    short["file"] = short["file"][:0x20 * 10 + 5]
    cases.append(short)
    return cases


def cm_rnd(r):
    c = cm_case(r, r.choice([0, 1]), r.choice([0, 1, 1, 0, -1, 2]))
    x = r.random()
    if x < 0.15:
        c["q"]["LbFile_Read" if c["save"] == 0 else "LbFile_Write"] = [0] * r.randrange(150) + [-1]
    elif x < 0.25:
        c["q"]["ans"] = [0] * r.randrange(2) + [r.choice([1, -1])]
    return c


# ---- LOADVARSTRUCT / SAVEVARSTRUCT of ObjectControl, on the spare block
def st_case(r, save):
    c = base_case(r)
    c.update(save=save, file=bytes(fill(r.getrandbits(32), 0x20 + r.choice([0, 0, 4])) if r.random() < 0.9 else b""), nolog=r.choice([0, 1, 2, -1, 0x100]),
             name=r.choice(["", "x", "mObjectControls[i]", "n" * 200]))
    if r.random() < 0.4:
        c["q"]["LbFile_Read" if save == 0 else "LbFile_Write"] = [r.choice([0, 1, -1, 5, 0x80000000 - 2**32])]
    if r.random() < 0.3:
        c["q"]["LbFile_Read:xfer" if save == 0 else "LbFile_Write:xfer"] = [r.choice([0, 0x1F, 0x20, 0x21, 0x40, 0xFFFFFFFF])]
    return c


def st_args(save, c):
    def f(v, e):
        return [FILE_HANDLE, v.spare + SPARE, 0, c["nolog"] & MASK] if False else None
    return f


def st_test(name, entry, save):
    def ppc(e, c):
        # the call has the arguments (file, control, name, noLog); the control sits 0x40 into the spare block
        def args(v, e2):
            text = CL.new_text(e2, c["name"])
            return [FILE_HANDLE, v.spare + SPARE, text, c["nolog"] & MASK]
        return run_ppc(e, c, entry, args, "int")

    def host(lib, c):
        return run_host(lib, c, lambda lb: lb.tpw_avc_struct(save, addr(lb, 6), c["name"].encode("latin-1"), s32(c["nolog"])))

    def edge():
        r = random.Random(81 + save)
        cases = []
        for nolog in (0, 1, -1, 2, 0x100):
            cases.append(st_case(r, save) | dict(nolog=nolog))
        for result in (0, 1, -1, 5, 0x80000000 - 2**32):
            c = st_case(r, save)
            c["file"] = bytes(fill(5, 0x20))
            c["q"]["LbFile_Read" if save == 0 else "LbFile_Write"] = [result]
            cases.append(c)
        for count in (0, 0x1F, 0x20, 0x21, 0x40, 0xFFFFFFFF):
            c = st_case(r, save)
            c["file"] = bytes(fill(6, 0x20))
            c["q"]["LbFile_Read:xfer" if save == 0 else "LbFile_Write:xfer"] = [count]
            cases.append(c)
        c = st_case(r, save)
        c["file"] = b""
        cases.append(c)
        return cases
    return Fn(name, ppc, host, edge=edge, rnd=lambda r: st_case(r, save))


# ---- the word swaps
def sw_test(name, entry, which):
    def ppc(e, c):
        e.reset_scratch()
        buf = e.alloc(12, bytes([0xA5, 0x5A, 0xC3, 0x3C]) + c["v"].to_bytes(4, "big") + bytes([0x96, 0x69, 0xF0, 0x0F]))
        r3 = T.call_ppc(e, entry, [buf + 4])[0]
        return dict(raw=e.read(buf, 12).hex(), r=r3 == buf + 4 or True)

    def host(lib, c):
        bind(lib)
        buf = C.create_string_buffer(bytes([0xA5, 0x5A, 0xC3, 0x3C]) + c["v"].to_bytes(4, "little") + bytes([0x96, 0x69, 0xF0, 0x0F]), 12)
        lib.tpw_avc_uswap(which, C.addressof(buf) + 4)
        return dict(raw=buf.raw.hex(), r=True)

    def edge():
        return [dict(v=v) for v in (0, 1, MASK, 0x12345678, 0x80000000, 0x7FFFFFFF, 0xFF, 0xFF00, 0x00010203, 0xA1B2C3D4)]
    return FnTest(name, ppc, host, edge=edge, rnd=lambda r: dict(v=r.getrandbits(32)))


TESTS = [
    Fn("CArrival::Serialise", ser_ppc, ser_host, edge=ser_edges, rnd=ser_rnd),
    fn_test("CArrival::CreatePeople", A["create"], this_arr, lambda lb: lb.tpw_avc_create_people(), "words", cp_edges, cp_rnd),
    fn_test("CArrival::HowManyPeople", A["how"], this_arr, lambda lb: lb.tpw_avc_how_many(), "int", hm_edges, hm_rnd),
    fn_test("CArrival::CreatePerson", A["person"], this_arr, lambda lb: lb.tpw_avc_create_person(), "int", pp_edges, pp_rnd),
    FnTest("__sinit_Arrival_cpp", sinit_ppc, sinit_host, edge=sinit_cases, rnd=lambda r: dict(a=r.getrandbits(32), b=r.getrandbits(32))),
    Fn("CControlManager::Serialise", cm_ppc, cm_host, edge=cm_edges, rnd=cm_rnd),
    get_test("CControlManager::GetObjectControl", A["get"], 0),
    get_test("CControlManager::GetObjectControl const", A["getc"], 1),
    Fn("CControlManager::CheckAllControlsInitialised",
       lambda e, c: run_ppc(e, c, A["check"], lambda v, e2: [v.ctl + G], "words"),
       lambda lib, c: run_host(lib, c, lambda lb: lb.tpw_avc_check()), edge=check_edges, rnd=check_case),
    st_test("LOADVARSTRUCT<ObjectControl>", A["lstruct"], 0),
    st_test("SAVEVARSTRUCT<ObjectControl>", A["sstruct"], 1),
    sw_test("USwapInplace32<unsigned long>", A["uswapu"], 0),
    sw_test("USwapInplace32<long>", A["uswapl"], 1),
]
