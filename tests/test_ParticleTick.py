"""Unit w26c: the particle system's per-tick work and drawing list (DoEmitters, DoEffecters, DoAParticle, MoveEmitter,
MoveEffecter, PAR_BuildDrawingList; src/rsse_tick.cpp) and the effect manager (EM_Init, EM_SetEmitersOn, EM_SetEffectTranslation,
EM_SetEffectOrbit, EM_Tick; src/effect_manager.cpp), against libtpw_unit_ParticleTick.dylib (tests/unit_ParticleTick.cpp).

Each case builds the particle system's state block on both sides from the same Python values: the header (the counts, the seed),
120 emitters of 0x140 bytes, 20 effecters of 0x68, the chain heads and a pool of 0x34-byte particles, with real chains (used and
free lists, each emitter's particles) and every field filled from a distribution that reaches the branches (all the other bytes
random). The PowerPC side writes the Mac's big-endian image at the block's real address (0x102edd68) and saves and restores it
(other modules read it); the host side copies the same fields little-endian into gParState. After the call the whole state, the
pool (with a margin of 0x80 bytes of 0xcc round it), the drawing list's block and the globals are compared byte by byte (each
field's bytes put in the Mac's order), with the call log: every callee outside the unit (the spawning functions, PAR_Malloc,
ESP_GetParticleSprite, the height callback, the assert, RealTimePauseAdjusted, HOST_Sin and HOST_Cos, PAR_SetEmitterPos...) is a fake
on both sides that records its arguments and answers from the case's queues. The drawing list is compared record by record (the
host's PTCL is 0x20 bytes, the Mac's 0x1c; PAR_Malloc is recorded as a count of records).
"""
import ctypes as C
import math
import os
import pathlib
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ParticleTick"

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent

MASK = 0xFFFFFFFF
STATE = 0x102EDD68
STATE_LEN = 0x9E68
GLOBALS = 0x102E51FC          # running, the drawing list's busy flag, the drawing list's block
EFFECTS = 0x102DAB10          # the effect manager's three effects of 0x44 bytes
EFFECTS_LEN = 0xCC
SIN_PTR = 0x102E49F4
MARGIN = 0x80
POOL_MAX = 48

# The entries of the originals under test and of the callees the PowerPC side fakes.
ENTRY = {"DoEmitters": 0x100BCBB4, "DoEffecters": 0x100BD1F0, "DoAParticle": 0x100BD63C, "MoveEmitter": 0x100BC9F0,
         "MoveEffecter": 0x100BD02C, "Build": 0x100B9DD4, "EM_Init": 0x1007A3D0, "EM_On": 0x1007A454,
         "EM_Translation": 0x1007A558, "EM_Orbit": 0x1007A63C, "EM_Tick": 0x1007A6B8}
HEIGHT_ENTRY = 0x100BDBF0     # DummyHeightCallBack: the descriptor the state's callback points at is made to run the fake here


# ---------------------------------------------------------------------------------------------------------- records

class Layout:
    """A record of the state: named fields at offsets (struct characters b B h H i I); every other byte is a raw byte."""

    def __init__(self, size, fields):
        items = []
        pos = 0
        for name, off, ch in sorted(fields, key=lambda f: f[1]):
            while pos < off:
                items.append((None, pos, "B"))
                pos += 1
            items.append((name, off, ch))
            pos = off + struct.calcsize(ch)
        while pos < size:
            items.append((None, pos, "B"))
            pos += 1
        self.size = size
        self.items = items
        self.chars = "".join(c for _, _, c in items)
        self.index = {name: i for i, (name, _, _) in enumerate(items) if name}
        # The host's little-endian bytes in the Mac's order: each field's bytes reversed.
        perm = []
        for _, off, ch in items:
            width = struct.calcsize(ch)
            perm.extend(range(off + width - 1, off - 1, -1))
        self.perm = perm

    def pack(self, endian, values):
        return struct.pack(endian + self.chars, *values)

    def to_big(self, little):
        """Little-endian record bytes (any number of records) as the Mac's bytes."""
        if len(little) == self.size:
            return bytes(little[j] for j in self.perm)
        return b"".join(self.to_big(little[o:o + self.size]) for o in range(0, len(little), self.size))

    def unpack(self, data):
        return list(struct.unpack(">" + self.chars, data))


def emitter_fields():
    f = [("inuse", 0x00, "h"), ("paused", 0x02, "h"), ("kind", 0x04, "b"), ("twod", 0x05, "b"), ("first", 0x06, "h"),
         ("count", 0x08, "H"), ("serial", 0x0A, "H"), ("special", 0x0E, "h"), ("endtype", 0x10, "h"), ("x", 0x14, "i"),
         ("y", 0x18, "i"), ("z", 0x1C, "i"), ("life", 0x20, "i"), ("drag", 0x24, "h"), ("noexpire", 0x26, "h"),
         ("gravity", 0x28, "h"), ("mask", 0x2A, "h"), ("vx", 0x2C, "i"), ("vy", 0x30, "i"), ("vz", 0x34, "i"),
         ("depth", 0x52, "h"), ("angle", 0x54, "i"), ("colourmode", 0x58, "i"), ("maxp", 0x64, "h"),
         ("countdown", 0x66, "h"), ("rate0", 0x68, "b"), ("rate1", 0x69, "b"), ("rate2", 0x6A, "b"), ("rate3", 0x6B, "b"),
         ("accel", 0x6F, "b"), ("pflags", 0x70, "I"), ("size0", 0x74, "h"), ("bounce", 0x76, "h"), ("lifebase", 0x78, "i"),
         ("pdrag", 0x7C, "i"), ("pgrav", 0x80, "i"), ("ax", 0x84, "i"), ("ay", 0x88, "i"), ("az", 0x8C, "i"),
         ("sprite", 0x94, "h"), ("frames", 0x96, "h"), ("death", 0x98, "i"), ("ignored", 0x9C, "i"), ("scale", 0xA2, "h"),
         ("size1", 0xA6, "h"), ("diewith", 0xAA, "b"), ("child", 0xAB, "b"), ("linked", 0xB4, "i"), ("follow", 0xB8, "h"),
         ("linkeff", 0xBA, "b"), ("endeff", 0xBB, "b"), ("killlinked", 0xBC, "h"), ("spinmin", 0xBE, "h"),
         ("spinmax", 0xC6, "h"), ("offspin", 0xC8, "i"), ("next", 0xD0, "h"), ("prev", 0xD2, "h"), ("lifestart", 0xD4, "i")]
    f += [("c%d" % k, 0xD8 + 4 * k, "I") for k in range(16)]
    f += [("b%d" % k, 0x128 + 4 * k, "i") for k in range(6)]
    return f


def effecter_fields():
    return [("inuse", 0x00, "i"), ("group", 0x04, "i"), ("serial", 0x08, "H"), ("linked", 0x0A, "B"), ("bounce", 0x0B, "b"),
            ("x", 0x0C, "i"), ("y", 0x10, "i"), ("z", 0x14, "i"), ("vx", 0x18, "i"), ("vy", 0x1C, "i"), ("vz", 0x20, "i"),
            ("drag", 0x24, "i"), ("gravity", 0x28, "i"), ("radius", 0x2C, "i"), ("mode", 0x30, "i"), ("sx", 0x34, "i"),
            ("sy", 0x38, "i"), ("sz", 0x3C, "i"), ("strength", 0x40, "i"), ("endtype", 0x46, "h"), ("endeff", 0x49, "b"),
            ("noexpire", 0x4A, "h"), ("life", 0x4C, "i"), ("next", 0x60, "h"), ("prev", 0x62, "h")]


def particle_fields():
    return [("x", 0x00, "i"), ("y", 0x04, "i"), ("z", 0x08, "i"), ("vx", 0x0C, "i"), ("vy", 0x10, "i"), ("vz", 0x14, "i"),
            ("colour", 0x18, "I"), ("total", 0x1C, "i"), ("left", 0x20, "i"), ("state", 0x24, "h"), ("emitter", 0x26, "h"),
            ("size", 0x28, "h"), ("kind", 0x2A, "h"), ("frame", 0x2C, "H"), ("spin", 0x2E, "H"), ("next", 0x30, "h"),
            ("prev", 0x32, "h")]


E = Layout(0x140, emitter_fields())
F = Layout(0x68, effecter_fields())
P = Layout(0x34, particle_fields())


def library_emitter_fields():
    """The library file's emitter record (PARLibraryEmitter in include/rsse.h): every field the loader swaps, at its width."""
    f = [("S00", 0x00, "h"), ("S02", 0x02, "h"), ("S06", 0x06, "h"), ("U08", 0x08, "H"), ("U0A", 0x0A, "H"), ("S0C", 0x0C, "h"),
         ("S0E", 0x0E, "h"), ("S10", 0x10, "h"), ("S12", 0x12, "h")]
    f += [("L%02X" % o, o, "i") for o in (0x14, 0x18, 0x1C, 0x20)]
    f += [("S%02X" % o, o, "h") for o in (0x24, 0x26, 0x28, 0x2A)]
    f += [("L%02X" % o, o, "i") for o in range(0x2C, 0x50, 4)]
    f += [("S50", 0x50, "h"), ("S52", 0x52, "h")] + [("L%02X" % o, o, "i") for o in (0x54, 0x58, 0x5C, 0x60)]
    f += [("S64", 0x64, "h"), ("S66", 0x66, "h"), ("L70", 0x70, "i"), ("S74", 0x74, "h"), ("S76", 0x76, "h")]
    f += [("L%02X" % o, o, "i") for o in range(0x78, 0x94, 4)]
    f += [("S94", 0x94, "h"), ("S96", 0x96, "h"), ("L98", 0x98, "i"), ("L9C", 0x9C, "i")]
    f += [("S%02X" % o, o, "h") for o in (0xA0, 0xA2, 0xA4, 0xA6, 0xA8)]
    f += [("L%02X" % o, o, "i") for o in (0xAC, 0xB0, 0xB4)] + [("SB8", 0xB8, "h"), ("SBC", 0xBC, "h"), ("SBE", 0xBE, "h")]
    f += [("SC4", 0xC4, "h"), ("SC6", 0xC6, "h"), ("LC8", 0xC8, "i"), ("LCC", 0xCC, "i"), ("SD0", 0xD0, "h"), ("SD2", 0xD2, "h"),
          ("LD4", 0xD4, "i")]
    f += [("D%d" % k, 0xD8 + 4 * k, "i") for k in range(16)]
    f += [("L%02X" % o, o, "i") for o in range(0x128, 0x140, 4)]
    return f


def library_effecter_fields():
    f = [("L00", 0x00, "i"), ("L04", 0x04, "i"), ("U08", 0x08, "H")] + [("L%02X" % o, o, "i") for o in range(0x0C, 0x44, 4)]
    return f + [("S46", 0x46, "h"), ("S4A", 0x4A, "h"), ("L4C", 0x4C, "i"), ("S60", 0x60, "h"), ("S62", 0x62, "h")]


LIB_E = Layout(0x140, library_emitter_fields())
LIB_F = Layout(0x68, library_effecter_fields())
PLB = ROOT / "extracted" / "disc-hfs" / "Theme Park Data" / "data" / "Particle" / "Tp2.plb"
REAL = None


# ---------------------------------------------------------------------------------------------------------- numbers

BITS = {"b": 8, "B": 8, "h": 16, "H": 16, "i": 32, "I": 32}


def wrap(value, ch):
    bits = BITS[ch]
    value &= (1 << bits) - 1
    if ch in "bhi" and value >= 1 << (bits - 1):
        value -= 1 << bits
    return value


def rnum(r, ch):
    """A random number for a field of type `ch`: mostly small, some edge values, some of any size."""
    k = r.random()
    if k < 0.45:
        v = r.randint(-8, 8)
    elif k < 0.7:
        v = r.randint(-300, 300)
    elif k < 0.8:
        v = r.choice([0, 1, -1, 0x7F, 0x80, 0xFF, 0x7FFF, 0x8000, 0xFFFF, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x10000])
    else:
        v = r.getrandbits(BITS[ch])
    return wrap(v, ch)


def pick(r, *choices):
    return r.choice(choices)


def coord(r):
    """A position: mostly small, some large, some anything."""
    k = r.random()
    if k < 0.6:
        return r.randint(-4000, 4000)
    if k < 0.85:
        return r.randint(-(1 << 20), 1 << 20)
    return wrap(r.choice([r.getrandbits(32), 0x7FFFFFFF, -0x80000000, 0x7FFFFFF0, 0x08000000, -0x08000000]), "i")


def speed(r):
    k = r.random()
    if k < 0.15:
        return 0
    if k < 0.6:
        return r.randint(-200, 200)
    if k < 0.85:
        return r.randint(-100000, 100000)
    return wrap(r.choice([r.getrandbits(32), 0x7FFFFFFF, -0x80000000]), "i")


def range_word(r):
    """A random range for `rand % range`: often 0 (none), mostly small, sometimes negative or huge."""
    return pick(r, 0, 0, 1, 2, 7, 100, 1000, 20000, -3, -1, 70000, 0x7FFFFFFF, -0x80000000, r.randint(-50, 50))


def emitter_value(r, name, ch):
    """The distribution of one named emitter field."""
    if name in ("inuse", "serial", "first", "count", "next", "prev"):
        return rnum(r, ch)         # structural fields are set afterwards
    if name == "paused":
        return pick(r, 0, 0, 0, 0, 0, 0, 1, -1, 0x100)
    if name == "kind":
        return pick(r, -1, 0, 0, 1, 2, 3, -2, 127, -128)
    if name == "twod":
        return pick(r, 0, 0, 0, 1, -1, 5)
    if name == "special":
        return pick(r, 0, 0, 0, 0, 0, 1, -1, 7)
    if name == "endtype":
        return pick(r, -1, -1, 0, 1, 5, 104, -2, 32767, -32768)
    if name in ("x", "y", "z"):
        return coord(r)
    if name == "life":
        return pick(r, -3, -2, -1, -1, -1, 0, 1, 2, 3, 10, 100, r.randint(-5, 20), rnum(r, "i"))
    if name == "drag":
        return pick(r, 0, 0, 100, 512, 1024, -50, 32767, -32768, 1, 2000, r.randint(-1100, 1100))
    if name == "noexpire":
        return pick(r, 0, 0, 0, 1, -1)
    if name == "gravity":
        return pick(r, 0, 0, 1, 5, 100, -7, 30000, -32768, r.randint(-40, 40))
    if name == "mask":
        return pick(r, 0, 0, 0, 0, 0, 1, -1, 256)
    if name in ("vx", "vy", "vz", "offspin"):
        return speed(r)
    if name == "depth":
        return rnum(r, "h")
    if name == "angle":
        return pick(r, 0, 1, 0xFFF, 0x1000, -1, 4095, r.randint(0, 0xFFF), rnum(r, "i"))
    if name == "colourmode":
        return pick(r, 0, 0, 1, 2, 3, -1, 0x10001, 0x80000000 - 2**32, 255)
    if name == "maxp":
        return pick(r, 0, 1, 5, 30, 1000, -1, 32767, -32768, r.randint(0, 20))
    if name == "countdown":
        return pick(r, 0, 1, 2, -1, 5, 0x7FFF, -0x8000, r.randint(-3, 6))
    if name.startswith("rate"):
        return pick(r, 0, 0, 1, 2, 3, -1, -2, -5, 4, 10, -128, 127, r.randint(-6, 6))
    if name == "accel":
        return pick(r, 0, 0, 1, -1, 5)
    if name == "pflags":
        return pick(r, 0, 0x4000, 0x54000, r.getrandbits(32), 0xFFFFFFFF, r.getrandbits(32) & 0x7FFFF)
    if name == "size0" or name == "size1":
        return pick(r, 0, 1, 1023, 1024, 2048, -1, -1024, -1025, 32767, -32768, r.randint(-5000, 5000))
    if name == "bounce":
        return pick(r, 0, 1, 1, -1, 0x100)
    if name == "lifebase":
        return pick(r, 0, 0, -5, 1, 30, 100, 5000, 0x7FFFFFFF, -0x80000000, r.randint(-3, 40))
    if name in ("pdrag",):
        return pick(r, 0, 0, 0, 100, 512, 1024, -50, 2000, r.randint(-1100, 1100), rnum(r, "i"))
    if name == "pgrav":
        return pick(r, 0, 0, 1, 5, 100, -7, rnum(r, "i"), r.randint(-40, 40))
    if name in ("ax", "ay", "az"):
        return range_word(r)
    if name == "sprite":
        return pick(r, 0, 1, 7, 100, -1, 32767, -32768)
    if name == "frames":
        return pick(r, 0, 0, 1, 2, 4, 5, 16, 40, -3, 32767, -32768)
    if name == "death":
        return pick(r, -1, -1, 0, 1, 5, 104, 0x7FFFFFFF, -0x80000000)
    if name == "ignored":
        return pick(r, 0, 0, 0, 1, -1, 7)
    if name == "scale":
        return pick(r, 1000, 1000, 1000, 1000, 0, 1, 999, 1001, -1000, 2000, 32767, -32768, 500, r.randint(-1500, 1500))
    if name == "diewith":
        return pick(r, 0, 0, 1, -1, 5)
    if name == "child":
        return pick(r, 0, 0, 0, 1, -1, 5)
    if name == "linked":
        return pick(r, -1, -1, -2, 0x00010003, 0x7FFF0000, r.getrandbits(32) & 0x7FFFFFFF, r.getrandbits(32))
    if name in ("follow", "killlinked"):
        return pick(r, 0, 0, 1, -1, 0x100, 5)
    if name in ("linkeff", "endeff"):
        return pick(r, 0, 0, 1, -1, 5)
    if name in ("spinmin", "spinmax"):
        return pick(r, 0, 0, 1, 5, -5, 100, 32767, -32768, r.randint(-300, 300))
    if name == "lifestart":
        return pick(r, 0, 0, 100, 50, -7, 3, 1, 4, 8, 1000, rnum(r, "i"), -1, r.randint(1, 60))
    return rnum(r, ch)


def rand_emitter(r):
    rec = []
    for name, _, ch in E.items:
        rec.append(r.randrange(256) if name is None else emitter_value(r, name, ch))
    fix_emitter_link(r, rec)
    return rec


def fix_emitter_link(r, rec):
    """The handle of a linked emitter or effecter must name an emitter (0..119) or an effecter (0..19) when its status is nonzero."""
    ix = E.index
    handle = rec[ix["linked"]]
    if handle >= 0 or r.random() < 0.1:
        low = r.randrange(20 if rec[ix["linkeff"]] != 0 else 120)
        handle = (r.getrandbits(15) << 16) | low
        if r.random() < 0.08:
            handle = wrap(handle | 0x80000000, "i")
        rec[ix["linked"]] = wrap(handle, "i")


def effecter_value(r, name, ch):
    if name in ("inuse", "serial", "next", "prev"):
        return rnum(r, ch)
    if name == "group":
        return pick(r, -1, -1, -1, 0, 1, 2, 3, -2, 127, -128, r.randint(-3, 3))
    if name == "linked":
        return pick(r, 0, 0, 0, 1, 255)
    if name == "bounce":
        return pick(r, 0, 0, 1, -1, 5)
    if name in ("x", "y", "z"):
        return coord(r)
    if name in ("vx", "vy", "vz"):
        return speed(r)
    if name == "drag":
        return pick(r, 0, 0, 100, 512, 1024, -50, 2000, r.randint(-1100, 1100), rnum(r, "i"))
    if name == "gravity":
        return pick(r, 0, 0, 1, 5, 100, -7, rnum(r, "i"), r.randint(-40, 40))
    if name == "radius":
        return pick(r, 0, 0, 1, 16, 64, 1000, -5, 40000, rnum(r, "i"), r.randint(1, 300))
    if name == "mode":
        return pick(r, 0, 2, 2, 2, 1, 3, -1, 0x10002)
    if name in ("sx", "sy", "sz"):
        return range_word(r)
    if name == "strength":
        return pick(r, 0, 1, 10, 100, 4000, -100, -1, rnum(r, "i"), r.randint(-500, 500))
    if name == "endtype":
        return pick(r, 0, 1, 5, 19, 104, -1, 32767)
    if name == "endeff":
        return pick(r, 0, 0, 1, -1, 5)
    if name == "noexpire":
        return pick(r, 0, 0, 0, 1, -1)
    if name == "life":
        return pick(r, -3, -2, -1, -1, 0, 1, 2, 3, 10, 100, r.randint(-5, 20), rnum(r, "i"))
    return rnum(r, ch)


def rand_effecter(r):
    return [r.randrange(256) if name is None else effecter_value(r, name, ch) for name, _, ch in F.items]


def rand_particle(r):
    out = []
    for name, _, ch in P.items:
        if name is None:
            out.append(r.randrange(256))
        elif name in ("x", "y", "z"):
            out.append(coord(r))
        elif name in ("vx", "vy", "vz"):
            out.append(speed(r))
        elif name == "state":
            out.append(pick(r, 0, 1, 2, 2, 2, 5, -1))
        elif name == "size":
            out.append(pick(r, 0, 1, 1023, 1024, 2048, -1, -1024, -1025, 32767, -32768, r.randint(-5000, 5000)))
        elif name == "kind":
            out.append(pick(r, -1, 0, 0, 1, 2, 3, -2, 127, -128))
        elif name == "total":
            out.append(pick(r, 1, 2, 5, 15, 16, 30, 100, 255, r.randint(1, 60)))
        elif name == "left":
            out.append(pick(r, -2, -1, 0, 1, 2, 3, 10, 50, r.randint(-1, 40)))
        else:
            out.append(rnum(r, ch))
    return out


# ---------------------------------------------------------------------------------------------------------- the world

class World:
    """The state block as Python values: the header words, the records (lists of field values) and the chain heads."""

    def __init__(self):
        self.header = [0, 0, 0, 40, 1, 1, 1, 1, 1024]    # f00, ticks, lowspec, max particles, seed, f14, f18, f1a, density
        self.emitters = []
        self.effecters = []
        self.particles = []
        self.heads = [-1, -1, 0, 0, 0]                    # emitters used, effecters used, emitters free, effecters free, particles free
        self.head_raw = bytes(range(0x12))
        self.running = 1
        self.busy = 0
        self.fill = 0xA5                                  # the drawing list block's bytes


def chain(records, layout, order):
    """Links the records `order` names into one chain; answers its first (-1 for none)."""
    nxt, prv = layout.index["next"], layout.index["prev"]
    previous = -1
    for i in order:
        records[i][prv] = previous
        if previous >= 0:
            records[previous][nxt] = i
        previous = i
    if order:
        records[order[-1]][nxt] = -1
    return order[0] if order else -1


def real_library():
    """The game's own particle library (Tp2.plb, read at run time, never copied): its 105 emitters and 20 effecters as the values of
    the state's records (the file's little-endian fields put in the Mac's order and read with the layouts of the state). None when
    the file is not there."""
    global REAL
    if REAL is None:
        REAL = ([], [])
        if PLB.exists():
            data = PLB.read_bytes()
            count, size = struct.unpack_from("<II", data, 0)
            assert size == 0x140 and count >= 105
            emitters = [E.unpack(LIB_E.pack(">", struct.unpack("<" + LIB_E.chars, data[8 + k * 0x140:8 + (k + 1) * 0x140])))
                        for k in range(105)]
            at = 8 + count * size
            count2, size2 = struct.unpack_from("<II", data, at)
            assert size2 == 0x68 and count2 >= 20
            effecters = [F.unpack(LIB_F.pack(">", struct.unpack("<" + LIB_F.chars, data[at + 8 + k * 0x68:at + 8 + (k + 1) * 0x68])))
                         for k in range(20)]
            REAL = (emitters, effecters)
    return REAL


def use_real(r, w, emitters, effecters):
    """Puts a library emitter into each of the used emitters' records and a library effecter into each used effecter's (their
    chain links, counts and handles stay)."""
    library_e, library_f = real_library()
    if not library_e:
        return
    ix, fx = E.index, F.index
    keep = [ix[n] for n in ("inuse", "serial", "first", "count", "next", "prev")]
    for i in emitters:
        record = list(r.choice(library_e))
        for k in keep:
            record[k] = w.emitters[i][k]
        record[ix["inuse"]] = 1
        record[ix["lifestart"]] = record[ix["life"]]
        record[ix["accel"]] = 1 if (record[ix["ax"]] or record[ix["ay"]] or record[ix["az"]]) else 0
        record[ix["paused"]] = 0
        if r.random() < 0.5:
            record[ix["life"]] = pick(r, -2, -1, 0, 1, r.randint(1, max(record[ix["lifestart"]], 2)))
        fix_emitter_link(r, record)
        w.emitters[i] = record
    keep = [fx[n] for n in ("inuse", "serial", "next", "prev")]
    for i in effecters:
        record = list(r.choice(library_f))
        for k in keep:
            record[k] = w.effecters[i][k]
        record[fx["inuse"]] = 1
        if r.random() < 0.5:
            record[fx["life"]] = pick(r, -1, 0, 1, r.randint(1, 30))
        w.effecters[i] = record


def make_world(r, used_em=None, used_ef=None, used_p=None, pool=None, strict=False, wild=False, quiet=False, real=None):
    """A state with proper chains. strict: no emitter has fewer particles in its count than in its chain (the drawing list would
    overrun); wild: the particles' links and owners are anything in range; quiet: the emitters make few particles."""
    w = World()
    if pool is None:
        pool = r.choice([8, 20, 33, 40, 48])
    if used_em is None:
        used_em = r.choice([0, 1, 1, 2, 3, 4, 6, 10])
    if used_ef is None:
        used_ef = r.choice([0, 1, 1, 2, 3, 5])
    w.header = [r.randint(0, 200), rnum(r, "i"), rnum(r, "i"), pool, wrap(r.getrandbits(32), "i"), rnum(r, "i"), rnum(r, "h"),
                rnum(r, "h"), pick(r, 1, 1024, 512, rnum(r, "i"))]
    w.head_raw = bytes(r.randrange(256) for _ in range(0x12))
    w.emitters = [rand_emitter(r) for _ in range(120)]
    w.effecters = [rand_effecter(r) for _ in range(20)]
    w.particles = [rand_particle(r) for _ in range(pool)]
    ix = E.index
    if quiet:
        for rec in w.emitters:
            for k in range(4):
                rec[ix["rate%d" % k]] = r.randint(-2, 2)
    # emitters
    ids = r.sample(range(120), used_em)
    rest = [i for i in range(120) if i not in ids]
    r.shuffle(rest)
    w.heads[0] = chain(w.emitters, E, ids)
    w.heads[2] = chain(w.emitters, E, rest)
    for i in ids:
        w.emitters[i][ix["inuse"]] = pick(r, 1, 1, 1, 1, 1, 1, 1, 1, 5, 0)
        w.emitters[i][ix["serial"]] = r.randint(1, 0xFFFF)
    for i in rest:
        w.emitters[i][ix["inuse"]] = pick(r, 0, 0, 0, 1)
        w.emitters[i][ix["first"]] = -1
        w.emitters[i][ix["count"]] = 0
    # effecters
    fids = r.sample(range(20), used_ef)
    frest = [i for i in range(20) if i not in fids]
    r.shuffle(frest)
    w.heads[1] = chain(w.effecters, F, fids)
    w.heads[3] = chain(w.effecters, F, frest)
    if real is None:
        real = r.random() < 0.3
    if real:
        use_real(r, w, ids, fids)
    # particles: each is in the chain of one used emitter or in the free chain
    if used_p is None:
        used_p = r.randint(0, pool)
    if not ids:
        used_p = 0
    pids = r.sample(range(pool), min(used_p, pool))
    pfree = [i for i in range(pool) if i not in pids]
    r.shuffle(pfree)
    owners = {}
    for p in pids:
        owners.setdefault(r.choice(ids), []).append(p)
    w.heads[4] = chain(w.particles, P, pfree)
    px = P.index
    for i in ids:
        plist = owners.get(i, [])
        w.emitters[i][ix["first"]] = chain(w.particles, P, plist)
        count = len(plist)
        k = r.random()
        if not strict and k < 0.05:
            count = pick(r, 0, max(count - 1, 0))
        elif k < 0.15 and plist:
            count += r.randint(1, 5)
        w.emitters[i][ix["count"]] = count
        for p in plist:
            w.particles[p][px["emitter"]] = i
    for p in pfree:
        w.particles[p][px["emitter"]] = r.randrange(120)
    for p in w.particles:
        owner = w.emitters[p[px["emitter"]]]
        if owner[ix["lifebase"]] > 0:
            keep_colour_in_range(p, px)
    if wild:
        for p in w.particles:
            p[px["next"]] = r.randint(-1, pool - 1)
            p[px["prev"]] = r.randint(-1, pool - 1)
            p[px["emitter"]] = r.randrange(120)
        for i in ids:
            w.emitters[i][ix["first"]] = r.randint(-1, pool - 1)
        for p in w.particles:
            owner = w.emitters[p[px["emitter"]]]
            if owner[ix["lifebase"]] > 0:
                keep_colour_in_range(p, px)
    w.running = pick(r, 1, 1, 1, 1, 0, 2)
    w.busy = pick(r, 0, 0, 0, 1)
    w.fill = 0xA5
    return w


def keep_colour_in_range(p, px):
    """DoAParticle takes the colour at life * 15 / total in the emitter's 16 colour words: keep it inside them. (A particle whose
    emitter ends has its life set to -1 first, which with a total under 15 reads the words before the colours: the one just before
    is the emitter's mLifeAtStart, the others are fields of other widths, which the byte orders of the two sides read differently, so
    a total of 8 or more keeps the place at -1 or above.)"""
    p[px["total"]] = max(p[px["total"]], 8)
    p[px["left"]] = max(-2, min(p[px["left"]], p[px["total"]] + 1))


def make_queues(r):
    """What the fakes answer: handles, statuses, ground heights, sprites and times."""
    return {"PAR_InitEmitter": [r.getrandbits(32) for _ in range(6)],
            "PAR_InitEffecter": [r.getrandbits(32) for _ in range(6)],
            "PAR_GetEmitterStatus": [r.randrange(2) for _ in range(8)],
            "PAR_GetEffecterStatus": [r.randrange(2) for _ in range(8)],
            "ESP_GetParticleSprite": [r.randint(1, 0x7FFFFFFF) for _ in range(8)],
            "RealTimePauseAdjusted": [r.getrandbits(32) for _ in range(4)],
            "Height": [wrap(coord(r), "i") for _ in range(4)]}


SINE = [int(256 * math.sin(2 * math.pi * i / 512)) for i in range(512)]


def world_bytes(w, endian):
    em = b"".join(E.pack(endian, rec) for rec in w.emitters)
    ef = b"".join(F.pack(endian, rec) for rec in w.effecters)
    heads = struct.pack(endian + "5h", *w.heads) + w.head_raw
    pool = b"".join(P.pack(endian, rec) for rec in w.particles)
    return em, ef, heads, pool


# ---------------------------------------------------------------------------------------------------------- the PowerPC side

def setup(runner):
    T.install_std_stubs(runner.emu)


def modf_stub(e):
    x = e.f(1)
    fraction, whole = math.modf(x)
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f(1, fraction)


def sine_stub(offset):
    def stub(e):
        angle = T.s32(e.r(3))
        e.cl.rec("HOST_Sin" if offset == 0 else "HOST_Cos", [angle])
        e.set_r(3, (SINE[(angle + offset) & 511] << 2) & MASK)
    return stub


def malloc_stub(e):
    size = e.r(3)
    address = e.alloc(size + 64, b"\xa5" * (size + 64))
    e.cl.rec("PAR_Malloc", [size // 0x1C, e.cl.newlab(address)])
    e.set_r(3, address)


def make_stubs():
    R = CL.recorder
    st = CL.Stubs()
    st.internal(0x100BEC30, R("PAR_InitEmitter", "iiii", ret="i"))
    st.internal(0x100BF248, R("PAR_InitEffecter", "iiii", ret="i"))
    st.internal(0x100BBFD4, R("SpawnParticle", "i"))
    st.internal(0x100BF42C, R("DoSpecialEmitter", "ii"))
    st.internal(0x100BF110, R("PAR_GetEmitterStatus", "u", ret="i"))
    st.internal(0x100BF1B0, R("PAR_GetEffecterStatus", "u", ret="i"))
    st.internal(0x100BE1E8, R("PAR_KillEmitter", "i"))
    st.internal(0x100BE150, R("PAR_SetEmitterPos", "iiii"))
    st.internal(0x1000DDE8, R("GAME_ASSERTMSG", "is"))
    st.internal(0x1007CEFC, R("ESP_GetParticleSprite", "iii", ret="i"))
    st.internal(0x10007C68, R("RealTimePauseAdjusted", "", ret="i"))
    st.internal(0x100BABF8, malloc_stub)
    st.internal(HEIGHT_ENTRY, R("Height", "ii", ret="i"))
    st.internal(0x100218F4, sine_stub(0))
    st.internal(0x10021928, sine_stub(128))
    st.internal(0x100259D0, R("BMP_GenerateSinTab", ""))
    st.imp("modf", modf_stub)
    st.real = set(ENTRY.values())
    return st


class PpcRun:
    """One case on the PowerPC side: the state written at its real address, the fakes installed, everything put back afterwards."""

    def __init__(self, e, w, queues, effects=None):
        self.e, self.w = e, w
        self.saved = (e.read(STATE, STATE_LEN), e.read(GLOBALS, 12), e.read(EFFECTS, EFFECTS_LEN))
        em, ef, heads, pool = world_bytes(w, ">")
        self.pool = e.alloc(len(pool) + 2 * MARGIN, b"\xcc" * MARGIN + pool + b"\xcc" * MARGIN) + MARGIN
        self.draw = e.alloc(0xAF0 + 64, bytes([w.fill]) * (0xAF0 + 64))
        self.desc = e.alloc(8, struct.pack(">II", HEIGHT_ENTRY, e.toc))
        h = w.header
        header = struct.pack(">iiiiiihhIiII", h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], SIN_PTR, h[8], self.desc, self.pool)
        e.write(STATE, header + em + ef + heads)
        e.write(GLOBALS, struct.pack(">iiI", w.running, w.busy, self.draw))
        if effects is not None:
            e.write(EFFECTS, effects)
        e.cl = CL.PpcLog({self.draw: 7, self.pool: 8}, queues)
        self.stubs = make_stubs()
        self.old = self.stubs.activate(e)

    def collect(self):
        e = self.e
        words = struct.unpack(">iiiiiihhIiII", e.read(STATE, 0x2C))
        raw = e.read(self.pool - MARGIN, len(self.w.particles) * 0x34 + 2 * MARGIN)
        out = {"header": list(words[:8]) + [words[9], words[10] == self.desc, words[11] == self.pool],
               "emitters": e.read(STATE + 0x2C, 0x9600), "effecters": e.read(STATE + 0x962C, 0x820),
               "heads": e.read(STATE + 0x9E4C, 0x1C), "pool": raw,
               "globals": [e.s32(GLOBALS), e.s32(GLOBALS + 4), e.cl.lab(e.u32(GLOBALS + 8))],
               "log": e.cl.log}
        return out

    def drawing_intact(self):
        return self.e.read(self.draw, 0xAF0 + 64) == bytes([self.w.fill]) * (0xAF0 + 64)

    def close(self):
        CL.Stubs.deactivate(self.e, self.old)
        self.e.write(STATE, self.saved[0])
        self.e.write(GLOBALS, self.saved[1])
        self.e.write(EFFECTS, self.saved[2])


# ---------------------------------------------------------------------------------------------------------- the host side

def region(lib, which):
    size = C.c_int32()
    lib.tpw_pt_region.restype = C.c_void_p
    lib.tpw_pt_region.argtypes = [C.c_int32, C.POINTER(C.c_int32)]
    address = lib.tpw_pt_region(which, C.byref(size))
    return address, size.value


class HostRun:
    """The same case on the host: the state copied little-endian into gParState."""

    def __init__(self, lib, w, queues):
        self.lib, self.w = lib, w
        em, ef, heads, pool = world_bytes(w, "<")
        n = len(w.particles)
        self.pool_buf = C.create_string_buffer(b"\xcc" * MARGIN + pool + b"\xcc" * MARGIN, len(pool) + 2 * MARGIN)
        self.pool = C.addressof(self.pool_buf) + MARGIN
        self.draw_buf = C.create_string_buffer(bytes([w.fill]) * (100 * 32 + 64), 100 * 32 + 64)
        self.draw = C.addressof(self.draw_buf)
        CL.host_begin(lib, {7: self.draw, 8: self.pool}, queues)
        lib.tpw_pt_set_header.argtypes = [C.POINTER(C.c_int32)]
        lib.tpw_pt_set_header((C.c_int32 * 9)(*[wrap(v, "i") for v in w.header]))
        lib.tpw_pt_attach.argtypes = [C.c_void_p, C.c_void_p, C.c_int32, C.c_int32]
        lib.tpw_pt_attach(self.pool, self.draw, w.running, w.busy)
        lib.tpw_pt_set_sine.argtypes = [C.POINTER(C.c_int32)]
        lib.tpw_pt_set_sine((C.c_int32 * 512)(*SINE))
        for which, data in ((0, em), (1, ef), (2, heads)):
            address, size = region(lib, which)
            assert size == len(data), (which, size, len(data))
            C.memmove(address, data, size)

    def collect(self):
        lib = self.lib
        header = (C.c_int64 * 11)()
        lib.tpw_pt_get_header.argtypes = [C.POINTER(C.c_int64)]
        lib.tpw_pt_get_header(header)
        glob = (C.c_int64 * 3)()
        lib.tpw_pt_globals.argtypes = [C.POINTER(C.c_int64)]
        lib.tpw_pt_globals(glob)
        n = len(self.w.particles)
        raw_pool = C.string_at(self.pool - MARGIN, n * 0x34 + 2 * MARGIN)
        pool = raw_pool[:MARGIN] + P.to_big(raw_pool[MARGIN:MARGIN + n * 0x34]) + raw_pool[MARGIN + n * 0x34:]
        a0, s0 = region(lib, 0)
        a1, s1 = region(lib, 1)
        a2, s2 = region(lib, 2)
        heads = C.string_at(a2, s2)
        heads = bytes(heads[1::-1] + heads[3:1:-1] + heads[5:3:-1] + heads[7:5:-1] + heads[9:7:-1]) + heads[10:]
        out = {"header": [int(v) for v in header[:9]] + [int(header[9]) == 1, int(header[10]) == 8],
               "emitters": E.to_big(C.string_at(a0, s0)), "effecters": F.to_big(C.string_at(a1, s1)),
               "heads": heads, "pool": pool, "globals": [int(v) for v in glob], "log": CL.host_end(lib)}
        return out

    def drawing_intact(self):
        return C.string_at(self.draw, 100 * 32 + 64) == bytes([self.w.fill]) * (100 * 32 + 64)

    def close(self):
        self.lib.tpw_pt_free_blocks()


# ---------------------------------------------------------------------------------------------------------- the particle functions

def trunc_div(a, b):
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


def steer_heights(r, rec_pos_y, rec_vy, count=3):
    """Ground heights (as the callback answers them, in 1/1024 units) that sit round a moved position and the bounce's edge."""
    out = []
    for _ in range(count):
        k = r.random()
        if k < 0.35:
            h = rec_pos_y + r.randint(-3, 3)
        elif k < 0.6:
            h = rec_pos_y - rec_vy + r.randint(-2, 2)
        elif k < 0.8:
            h = rec_pos_y + r.randint(-5000, 5000)
        else:
            h = coord(r)
        out.append(wrap((h << 4) + r.randrange(16), "i"))
    return out


def run_ppc_function(key, with_call):
    def ppc(e, case):
        run = PpcRun(e, case["world"], case["queues"])
        try:
            result = with_call(e, run, case)
            out = run.collect()
            out["intact"] = run.drawing_intact()
            out.update(result or {})
            return out
        finally:
            run.close()
    return ppc


def run_host_function(with_call):
    def host(lib, case):
        run = HostRun(lib, case["world"], case["queues"])
        try:
            result = with_call(lib, run, case)
            out = run.collect()
            out["intact"] = run.drawing_intact()
            out.update(result or {})
            return out
        finally:
            run.close()
    return host


def ppc_plain(key, address_of=None):
    def call(e, run, case):
        args = case["args"]
        if address_of is not None:
            args = [address_of(a) for a in args]
        T.call_ppc(e, ENTRY[key], args)
    return call


def host_plain(name):
    def call(lib, run, case):
        fn = getattr(lib, name)
        fn.argtypes = [C.c_int32] * len(case["args"])
        fn(*case["args"])
    return call


def case_of(w, queues, *args):
    return {"world": w, "queues": queues, "args": list(args)}


# DoEmitters

def do_emitters_case(r, **kw):
    quiet = kw.pop("quiet", None)
    w = make_world(r, quiet=quiet if quiet is not None else r.random() < 0.3, **kw)
    return case_of(w, make_queues(r))


def do_emitters_rnd(r):
    return do_emitters_case(r)


def do_emitters_edges():
    r = random.Random(11)
    out = []
    # all 120 in use, quiet; none in use; one in use
    out.append(do_emitters_case(r, used_em=120, used_p=40, quiet=True, real=False))
    out.append(do_emitters_case(r, used_em=0))
    out.append(do_emitters_case(r, used_em=1, used_p=0))
    out.append(do_emitters_case(r, used_em=1, used_p=0))
    # an emitter at each stage of its ending, with and without linked ones and particles
    for life in (-3, -2, -1, 0):
        for count in (0, 2):
            for endtype, endeff in ((-1, 0), (3, 0), (3, 1), (0, 0)):
                w = make_world(r, used_em=1, used_p=0)
                e = w.emitters[w.heads[0]]
                ix = E.index
                e[ix["paused"]] = 0
                e[ix["noexpire"]] = 1
                e[ix["life"]] = life
                e[ix["count"]] = count
                e[ix["endtype"]] = endtype
                e[ix["endeff"]] = endeff
                out.append(case_of(w, make_queues(r)))
    # the rate's quarter at the minimum divided by -1 (the emulator answers the dividend), and a start life of 0
    for lifestart in (-1, 0, 1):
        w = make_world(r, used_em=1, used_p=0)
        e = w.emitters[w.heads[0]]
        ix = E.index
        e[ix["paused"]] = 0
        e[ix["noexpire"]] = 1
        e[ix["life"]] = 0x20000000
        e[ix["lifestart"]] = lifestart
        e[ix["special"]] = 0
        e[ix["maxp"]] = 100
        e[ix["count"]] = 0
        out.append(case_of(w, make_queues(r)))
    # a living emitter that moves for one reason only: each of its velocities, its gravity, or none of them, with the child flag set or not
    for reason in ("vx", "vy", "vz", "gravity", None):
        for child in (0, 1):
            w = make_world(r, used_em=1, used_p=0)
            e = w.emitters[w.heads[0]]
            ix = E.index
            for name in ("vx", "vy", "vz", "gravity"):
                e[ix[name]] = 0
            if reason:
                e[ix[reason]] = r.choice([-9, 3, 70])
            e[ix["paused"]] = 0
            e[ix["noexpire"]] = 1
            e[ix["life"]] = 50
            e[ix["child"]] = child
            e[ix["special"]] = 0
            out.append(case_of(w, make_queues(r)))
    return out


# DoEffecters

def do_effecters_case(r, **kw):
    kw.setdefault("used_ef", r.choice([1, 1, 2, 3, 5]))
    kw.setdefault("used_em", r.choice([0, 1, 2, 3, 5]))
    w = make_world(r, **kw)
    ex = F.index
    # most effecters are in the group the particles are in, or none, so that the pull happens
    for rec in w.effecters:
        if r.random() < 0.5:
            rec[ex["group"]] = pick(r, -1, -1, 0, 1, 2)
    return case_of(w, make_queues(r))


def do_effecters_edges():
    r = random.Random(12)
    out = []
    for mode in (0, 2, 3):
        for group in (-1, 0, 1, -2):
            for life in (-1, 0, 5):
                w = make_world(r, used_ef=1, used_em=2, used_p=12)
                f = w.effecters[w.heads[1]]
                ex = F.index
                f[ex["mode"]] = mode
                f[ex["group"]] = group
                f[ex["life"]] = life
                f[ex["noexpire"]] = 1
                f[ex["radius"]] = pick(r, 0, 1, 16, 300)
                out.append(case_of(w, make_queues(r)))
    out.append(do_effecters_case(r, used_ef=20, used_em=6, used_p=30))
    out.append(do_effecters_case(r, used_ef=0))
    # the pull's divisor (distance squared over 256, plus 10) at 0 and at -1 (the squares wrap): the quotients the emulator gives for them
    for deltas, strength in (((19, 358, 65535), 1000), ((19, 358, 65535), -77), ((1, 625, 65533), -0x80000000),
                             ((1, 625, 65533), 5)):
        w = make_world(r, used_ef=1, used_em=1, used_p=1)
        f = w.effecters[w.heads[1]]
        ex, px = F.index, P.index
        em = w.emitters[w.heads[0]]
        particle = w.particles[em[E.index["first"]]]
        em[E.index["paused"]] = 0
        em[E.index["kind"]] = 0
        em[E.index["ignored"]] = 0
        f[ex["group"]] = -1
        f[ex["mode"]] = 0
        f[ex["linked"]] = 1
        f[ex["noexpire"]] = 1
        f[ex["life"]] = 5
        f[ex["sx"]] = f[ex["sy"]] = f[ex["sz"]] = 0
        f[ex["strength"]] = strength
        for axis, d in zip("xyz", deltas):
            f[ex[axis]] = wrap(particle[px[axis]] + 4 * d, "i")
        out.append(case_of(w, make_queues(r)))
    # a particle exactly at the edge of the sphere (distance 36, radius 96: 96 * 96 / 256 = 36)
    for radius in (96, 95, 97):
        w = make_world(r, used_ef=1, used_em=1, used_p=1)
        f = w.effecters[w.heads[1]]
        ex, px = F.index, P.index
        em = w.emitters[w.heads[0]]
        particle = w.particles[em[E.index["first"]]]
        em[E.index["paused"]] = 0
        em[E.index["kind"]] = 0
        em[E.index["ignored"]] = 0
        particle[px["kind"]] = 0
        f[ex["group"]] = -1
        f[ex["mode"]] = 2
        f[ex["radius"]] = radius
        f[ex["linked"]] = 1
        f[ex["noexpire"]] = 1
        f[ex["life"]] = 5
        f[ex["sx"]] = f[ex["sy"]] = f[ex["sz"]] = 0
        f[ex["strength"]] = 100
        for axis, d in zip("xyz", (16, 80, 0)):
            f[ex[axis]] = wrap(particle[px[axis]] + 4 * d, "i")
        out.append(case_of(w, make_queues(r)))
    # a living effecter that moves for one reason only: each of its velocities, its gravity, or none, linked or not
    for reason in ("vx", "vy", "vz", "gravity", None):
        for linked in (0, 1):
            w = make_world(r, used_ef=1, used_em=1, used_p=4)
            f = w.effecters[w.heads[1]]
            ex = F.index
            for name in ("vx", "vy", "vz", "gravity"):
                f[ex[name]] = 0
            if reason:
                f[ex[reason]] = r.choice([-9, 3, 70])
            f[ex["noexpire"]] = 1
            f[ex["life"]] = 50
            f[ex["linked"]] = linked
            out.append(case_of(w, make_queues(r)))
    return out


# DoAParticle

def do_particle_case(r, **kw):
    w = make_world(r, used_p=r.randint(1, 40), used_em=r.choice([1, 2, 3, 6]), wild=r.random() < 0.15, **kw)
    index = r.randrange(len(w.particles))
    px, ix = P.index, E.index
    p = w.particles[index]
    owner = w.emitters[p[px["emitter"]]]
    # most particles are alive and their emitters are not over
    if r.random() < 0.8:
        p[px["left"]] = max(p[px["left"]], 1)
        if owner[ix["lifebase"]] > 0:
            p[px["total"]] = max(p[px["total"]], p[px["left"]] - 1, 1)
    if r.random() < 0.5:
        owner[ix["accel"]] = 1
    return case_of(w, make_queues(r), index)


def do_particle_edges():
    r = random.Random(13)
    out = []
    for left in (-1, 0, 1, 2):
        for death in (-1, 4):
            for first in ("this", "other"):
                w = make_world(r, used_p=10, used_em=1)
                ix, px = E.index, P.index
                e = w.emitters[w.heads[0]]
                chain_ids = []
                i = e[ix["first"]]
                while i >= 0:
                    chain_ids.append(i)
                    i = w.particles[i][px["next"]]
                if not chain_ids:
                    continue
                target = chain_ids[0] if first == "this" else chain_ids[-1]
                w.particles[target][px["left"]] = left
                e[ix["death"]] = death
                out.append(case_of(w, make_queues(r), target))
    # a total life of 0 with nothing left: the quotients are divisions by 0 (the emulator answers the dividend)
    for frames in (0, 3):
        for lifebase in (0, 30):
            w = make_world(r, used_p=10, used_em=1)
            ix, px = E.index, P.index
            e = w.emitters[w.heads[0]]
            target = e[ix["first"]]
            if target < 0:
                continue
            w.particles[target][px["total"]] = 0
            w.particles[target][px["left"]] = 1
            e[ix["frames"]] = frames
            e[ix["lifebase"]] = lifebase
            e[ix["colourmode"]] = 0
            e[ix["life"]] = 5
            out.append(case_of(w, make_queues(r), target))
    # the middle of a chain, the only particle of one
    for k in range(6):
        out.append(do_particle_case(r))
    return out


# MoveEmitter and MoveEffecter

def move_case(r, emitter):
    w = make_world(r, used_em=r.choice([0, 1]), used_ef=r.choice([0, 1]))
    index = r.randrange(120 if emitter else 20)
    layout = E if emitter else F
    rec = (w.emitters if emitter else w.effecters)[index]
    ix = layout.index
    names = ("vx", "vy", "vz")
    if r.random() < 0.7:
        for name in names + ("x", "y", "z"):
            rec[ix[name]] = r.randint(-3000, 3000) if name[0] == "v" else r.randint(-60000, 60000)
    if r.random() < 0.8:
        rec[ix["bounce"]] = 1
    if r.random() < 0.5:
        rec[ix["drag"]] = r.randint(0, 600)
    if r.random() < 0.5:
        rec[ix["gravity"]] = r.randint(-30, 60)
    # where the ground is, round the moved position
    drag = rec[ix["drag"]]
    vy = rec[ix["vy"]]
    vy = wrap(vy - (rec[ix["gravity"]] + trunc_div(wrap(vy * drag, "i"), 1024)), "i")
    y = wrap(rec[ix["y"]] + vy, "i")
    queues = make_queues(r)
    queues["Height"] = steer_heights(r, y, vy)
    return case_of(w, queues, index)


def move_emitter_rnd(r):
    return move_case(r, True)


def move_effecter_rnd(r):
    return move_case(r, False)


def move_edges(emitter):
    def edges():
        r = random.Random(14 if emitter else 15)
        return [move_case(r, emitter) for _ in range(40)]
    return edges


# PAR_BuildDrawingList

def build_case(r, **kw):
    pool = kw.pop("pool", None)
    w = make_world(r, strict=True, pool=pool, used_em=kw.pop("used_em", r.choice([1, 2, 3, 5])), **kw)
    ix = E.index
    used = []
    i = w.heads[0]
    while i >= 0:
        used.append(i)
        i = w.emitters[i][ix["next"]]
    k = r.random()
    if used and k < 0.75:
        target = r.choice(used)
    else:
        target = r.randrange(120)
    # many emitters draw a 2D screen, a scale, a mask, sprites: the distributions do that
    return case_of(w, make_queues(r), target)


def build_edges():
    r = random.Random(16)
    out = []
    for twod in (0, 1):
        for scale in (1000, 999, 0):
            for frames in (0, 3):
                w = make_world(r, strict=True, used_em=1, used_p=10, pool=20)
                e = w.emitters[w.heads[0]]
                ix = E.index
                e[ix["twod"]] = twod
                e[ix["scale"]] = scale
                e[ix["frames"]] = frames
                e[ix["mask"]] = 0
                e[ix["inuse"]] = 1
                w.running = 1
                w.busy = 0
                out.append(case_of(w, make_queues(r), w.heads[0]))
    # the system's block is busy, the list is longer than 100, exactly 100, one
    for count, busy in ((101, 0), (100, 0), (100, 1), (1, 1), (1, 0), (60, 0)):
        w = make_world(r, strict=True, used_em=1, used_p=min(count, 130), pool=130)
        e = w.emitters[w.heads[0]]
        e[E.index["mask"]] = 0
        e[E.index["inuse"]] = 1
        w.busy = busy
        w.running = 1
        out.append(case_of(w, make_queues(r), w.heads[0]))
    return out


# ---------------------------------------------------------------------------------------------------------- the effect manager

TRIVIAL = None


def trivial_world():
    global TRIVIAL
    if TRIVIAL is None:
        TRIVIAL = make_world(random.Random(5), used_em=0, used_ef=0, used_p=0, pool=8)
    return TRIVIAL


def f32_bits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bits_f32(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def rfloat_bits(r, lo=-3.0, hi=3.0):
    """The bits of a float32: mostly in [lo, hi], sometimes an edge value (never a NaN other than the quiet one)."""
    k = r.random()
    if k < 0.7:
        return f32_bits(r.uniform(lo, hi))
    if k < 0.85:
        return f32_bits(r.choice([0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 1e-30, 3.0e38, -3.0e38, 1e6, 65536.0, 0.001]))
    if k < 0.93:
        return r.choice([0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x80000001])
    return f32_bits(r.uniform(-1e5, 1e5))


def effect_words(r, counts=None, tick=False):
    """The three effects as 51 words: floats as bits, the counts and the emitter handles as integers."""
    words = []
    for fx in range(3):
        count = counts[fx] if counts else pick(r, 0, 0, 1, 2, 3, 4, 5)
        w = [rfloat_bits(r) for _ in range(11)]
        if tick:
            now = r.getrandbits(32)
            signed = now - (1 << 32) if now >= 1 << 31 else now
            w[4] = f32_bits(float(signed - r.randint(-100, 600)))        # the start
            w[5] = pick(r, f32_bits(r.uniform(1, 500)), f32_bits(r.uniform(1, 500)), f32_bits(r.uniform(1, 500)), 0,
                        0x80000000, 0x7FC00000, f32_bits(-5.0), 0x7F800000, rfloat_bits(r))   # the duration
            w[6] = pick(r, f32_bits(r.uniform(0, 3)), f32_bits(r.uniform(-3, 3)), rfloat_bits(r))  # the turns
            w[0] = f32_bits(r.uniform(-1, 1)) if r.random() < 0.8 else rfloat_bits(r)
            w[1] = f32_bits(r.uniform(-1, 1)) if r.random() < 0.8 else rfloat_bits(r)
            w[2] = f32_bits(r.uniform(-1, 1)) if r.random() < 0.8 else rfloat_bits(r)
            w[3] = f32_bits(r.uniform(-1, 1)) if r.random() < 0.8 else rfloat_bits(r)
        words += w + [count & MASK] + [r.getrandbits(32) for _ in range(5)]
    return words


def effect_bytes(words, endian):
    return struct.pack(endian + "51I", *words)


def effects_case(r, op, tick=False, **kw):
    words = kw.pop("words", None) or effect_words(r, tick=tick)
    case = case_of(trivial_world(), make_queues(r), *kw.pop("args", []))
    case.update(op=op, words=words)
    case.update(kw)
    return case


def ppc_effects(e, case):
    run = PpcRun(e, case["world"], case["queues"], effect_bytes(case["words"], ">"))
    try:
        op, args = case["op"], case["args"]
        ret = 0
        if op == "init":
            T.call_ppc(e, ENTRY["EM_Init"], [])
        elif op == "on":
            ret = T.s32(T.call_ppc(e, ENTRY["EM_On"], args)[0])
        elif op in ("translation", "orbit"):
            effect, index, a, b, c = args
            T.call_ppc(e, ENTRY["EM_Translation" if op == "translation" else "EM_Orbit"],
                       [T.I(effect), T.I(index), T.F(a), T.F(b), T.F(c)])
        else:
            T.call_ppc(e, ENTRY["EM_Tick"], [])
        out = {"table": e.read(EFFECTS, EFFECTS_LEN), "log": e.cl.log, "ret": ret}
        return out
    finally:
        run.close()


def host_effects(lib, case):
    run = HostRun(lib, case["world"], case["queues"])
    try:
        lib.tpw_pt_effects.restype = C.c_void_p
        address = lib.tpw_pt_effects()
        data = effect_bytes(case["words"], "<")
        C.memmove(address, data, EFFECTS_LEN)
        op, args = case["op"], case["args"]
        ret = 0
        if op == "init":
            lib.tpw_pt_em_init()
        elif op == "on":
            lib.tpw_pt_em_on.restype = C.c_int32
            ret = lib.tpw_pt_em_on(*[C.c_int32(a) for a in args])
        elif op in ("translation", "orbit"):
            fn = lib.tpw_pt_em_translation if op == "translation" else lib.tpw_pt_em_orbit
            fn.argtypes = [C.c_int32, C.c_int32, C.c_float, C.c_float, C.c_float]
            fn(*args)
        else:
            lib.tpw_pt_em_tick()
        raw = C.string_at(address, EFFECTS_LEN)
        table = b"".join(raw[k + 3:k - 1 if k else None:-1] if k else raw[3::-1] for k in range(0, EFFECTS_LEN, 4))
        return {"table": table, "log": emulator_saturation(CL.host_end(lib)), "ret": ret}
    finally:
        run.close()


def emulator_saturation(log):
    """fctiwz of a number above 2^31 - 1 answers 0x7fffffff on the hardware (and in ppc_fctiwz) and 0x80000000 in the emulator
    (QEMU answers that for every invalid conversion). A float cannot be 2^31 - 1 itself, so every 0x7fffffff the host puts in a
    position is such a saturation: it is shown as the emulator's answer."""
    out = []
    for name, ints, texts in log:
        if name == "PAR_SetEmitterPos":
            ints = tuple(-0x80000000 if v == 0x7FFFFFFF else v for v in ints)
        out.append((name, ints, texts))
    return out


def float_arg(r):
    return bits_f32(rfloat_bits(r))


def em_init_rnd(r):
    return effects_case(r, "init")


def em_on_rnd(r):
    effect = r.randrange(3)
    count = pick(r, 0, 1, 2, 3, 4, 5, 5, r.randint(0, 5))
    if r.random() < 0.1 and effect < 2:
        count = pick(r, 6, 7)                      # spills into the next effect's words
    words = effect_words(r)
    case = effects_case(r, "on", words=words, args=[effect, count, pick(r, 0x2C, 0x31, 0, 104, -1, r.randrange(105))])
    case["queues"]["PAR_InitEmitter"] = [pick(r, 0, r.getrandbits(32)) for _ in range(8)]
    return case


def em_on_edges():
    r = random.Random(21)
    out = []
    for effect in range(3):
        for have in (0, 1, 3, 5):
            for count in (0, 1, 2, 3, 5):
                for zeros in (False, True):
                    words = effect_words(r, counts=[have] * 3)
                    if zeros:
                        for fx in range(3):
                            for s in range(5):
                                if r.random() < 0.5:
                                    words[fx * 17 + 12 + s] = 0
                    out.append(effects_case(r, "on", words=words, args=[effect, count, 0x2C]))
    return out


def em_translation_rnd(r):
    effect = r.randrange(3)
    z = pick(r, float_arg(r), float_arg(r), 0.0, -0.0, 1.5, 100.0, float("nan"), float("inf"))
    args = [effect, pick(r, 0, 0, 1, -1, 7), float_arg(r), float_arg(r), z]
    return effects_case(r, "translation", args=args)


def em_orbit_rnd(r):
    effect = r.randrange(3)
    args = [effect, pick(r, 0, 0, 1, -1, 7), float_arg(r), float_arg(r), float_arg(r)]
    return effects_case(r, "orbit", args=args)


def em_tick_rnd(r):
    return effects_case(r, "tick", tick=True)


def em_tick_edges():
    r = random.Random(22)
    out = []
    for counts in ([0, 0, 0], [1, 0, 0], [0, 2, 0], [0, 0, 5], [5, 5, 5], [3, 1, 4], [7, 0, 0]):
        for _ in range(3):
            words = effect_words(r, counts=counts, tick=True)
            out.append(effects_case(r, "tick", words=words))
    # many more random ticks: a fused and an unfused multiply-add differ in the last place of a few
    for _ in range(300):
        out.append(em_tick_rnd(r))
    # a move that has just ended, one that is still to start, a duration of 0, a turn exactly at the table's edges
    for duration in (0.0, 1.0, 100.0):
        for age in (-10, 0, 99, 100, 101, 1000):
            words = effect_words(r, counts=[2, 2, 2], tick=True)
            case = effects_case(r, "tick", words=words)
            now = case["queues"]["RealTimePauseAdjusted"][0]
            signed = now - (1 << 32) if now >= 1 << 31 else now
            for fx in range(3):
                case["queues"]["RealTimePauseAdjusted"] = [now] * 4
                words[fx * 17 + 4] = f32_bits(float(signed - age))
                words[fx * 17 + 5] = f32_bits(duration)
            out.append(case)
    return out


# ---------------------------------------------------------------------------------------------------------- the tests

def build_ppc(e, run, case):
    address, _, _ = None, None, None
    emitter = case["args"][0]
    count = case["world"].emitters[emitter][E.index["count"]]
    ret = T.call_ppc(e, ENTRY["Build"], [emitter])[0]
    out = {"label": e.cl.lab(ret), "records": []}
    if ret != 0:
        data = e.read(ret, count * 0x1C)
        for k in range(count):
            x, y, z, colour, tex, size, angle, flags = struct.unpack(">iiiIIHHI", data[k * 0x1C:(k + 1) * 0x1C])
            out["records"].append((x, y, z, colour, tex, size, angle, flags))
    return out


def build_host(lib, run, case):
    emitter = case["args"][0]
    count = case["world"].emitters[emitter][E.index["count"]]
    lib.tpw_pt_build.restype = C.c_void_p
    lib.tpw_pt_build.argtypes = [C.c_int32]
    ptr = lib.tpw_pt_build(emitter)
    lib.tpw_cl_lab.argtypes = [C.c_void_p]
    out = {"label": lib.tpw_cl_lab(ptr) if ptr else 0, "records": []}
    if ptr:
        data = C.string_at(ptr, count * 0x20)
        for k in range(count):
            x, y, z, colour, tex, size, angle, flags = struct.unpack("<iiiIQHHI", data[k * 0x20:(k + 1) * 0x20])
            out["records"].append((x, y, z, colour, tex & MASK, size, angle, flags))
    return out


def move_address(emitter):
    return (lambda index: STATE + 0x2C + index * 0x140) if emitter else (lambda index: STATE + 0x962C + index * 0x68)


TESTS = [
    FnTest("MoveEmitter", run_ppc_function("MoveEmitter", ppc_plain("MoveEmitter", move_address(True))),
           run_host_function(host_plain("tpw_pt_move_emitter")), edge=move_edges(True), rnd=move_emitter_rnd),
    FnTest("MoveEffecter", run_ppc_function("MoveEffecter", ppc_plain("MoveEffecter", move_address(False))),
           run_host_function(host_plain("tpw_pt_move_effecter")), edge=move_edges(False), rnd=move_effecter_rnd),
    FnTest("DoAParticle", run_ppc_function("DoAParticle", ppc_plain("DoAParticle")),
           run_host_function(host_plain("tpw_pt_do_particle")), edge=do_particle_edges, rnd=do_particle_case),
    FnTest("DoEmitters", run_ppc_function("DoEmitters", ppc_plain("DoEmitters")),
           run_host_function(host_plain("tpw_pt_do_emitters")), edge=do_emitters_edges, rnd=do_emitters_rnd),
    FnTest("DoEffecters", run_ppc_function("DoEffecters", ppc_plain("DoEffecters")),
           run_host_function(host_plain("tpw_pt_do_effecters")), edge=do_effecters_edges, rnd=do_effecters_case),
    FnTest("PAR_BuildDrawingList", run_ppc_function("Build", build_ppc), run_host_function(build_host), edge=build_edges,
           rnd=build_case),
    FnTest("EM_Init", ppc_effects, host_effects, rnd=em_init_rnd),
    FnTest("EM_SetEmitersOn", ppc_effects, host_effects, edge=em_on_edges, rnd=em_on_rnd),
    FnTest("EM_SetEffectTranslation", ppc_effects, host_effects, rnd=em_translation_rnd),
    FnTest("EM_SetEffectOrbit", ppc_effects, host_effects, rnd=em_orbit_rnd),
    FnTest("EM_Tick", ppc_effects, host_effects, edge=em_tick_edges, rnd=em_tick_rnd),
]
