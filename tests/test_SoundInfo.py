"""Unit w77, the sound library's value classes, against libtpw_unit_SoundInfo.dylib (tests/unit_SoundInfo.cpp), in the program
"sound symbols shared" (the Mac's sound library, Code at 0x10000000, TOC base r2 = 0x1003abf0):
  src/TbSoundInfo.cpp  TbSoundPosition (18 functions, 0x1000c580 to 0x100246e4), TbSoundDistance (0x10001324, 0x10015950),
                       TbSoundReverbInfo (0x100012ac), TbSoundVolumeControl (0x10009d9c, 0x1000aac8, 0x10026f28, 0x10026f68),
                       TbSoundSampleInfo (18 functions, 0x1000dc60 to 0x100196f4; SetBank 0x1000df50 is w72's) and
                       TbSoundSystemInfo (23 functions, 0x1000ba78 to 0x10028c08): 66 functions in all

Each function runs as the original in the emulator and as the host's translation, over records built on both sides from the
same words in the Mac's layout, after a fill (random words, so that "left as it was" bytes are checked too). The whole of
every record the function can touch (the object, every other record it was given, the result record of operator+ and
operator-, the three outputs of Get), the answer and, where the function calls the sound library's imports, the calls the
imports saw are compared. The pointer members of the sample info, the system info and the volume control are compared as
labels: the index of the record they point at (the host's pointers are native, the Mac's are 32-bit words), anything else as
the 32-bit word. The imports (memcmp, __fpclassifyf, __isfinitef, sqrt) are emulator stubs by name; the host's Valid uses the
same classification (MathLib's fp.h numbering, a reading: an assumption the emulator cannot confirm).

Arithmetic results compare a NaN as one value (the PowerPC and the host pick different NaN payloads); every copy compares bits.
"""
import ctypes as C
import math
import struct
from collections import namedtuple

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SoundInfo"
MASK = 0xFFFFFFFF
LABEL = 0xCAFE0000
HOST_SPACE = 1024

# the record kinds: words in the Mac's layout; which words are floats (compared as one value for NaNs when the operation is
# arithmetic), which are pointers (compared as labels); the class kinds are loaded and dumped through the shim
WORDS = {"pos": 3, "dist": 3, "rev": 12, "vol": 2, "sample": 36, "sys": 19, "ftab": 8, "cfg": 4, "gout": 3, "res": 3}
FLOATS = {"pos": (0, 1, 2), "dist": (0, 1, 2), "rev": (4, 6, 8, 9, 11), "ftab": tuple(range(8)), "gout": (0, 1, 2),
          "res": (0, 1, 2), "sample": (8, 13, 14, 15, 24, 26, 28, 29, 31)}
POINTERS = {"vol": (0,), "sample": (1, 11, 12, 17, 18, 19, 32), "sys": (8, 9, 10, 11, 12, 13, 14, 15, 18)}
CLASS_KIND = {"vol": 0, "sample": 1, "sys": 2}
TOKEN_KIND = {"P": "pos", "D": "dist", "R": "rev", "V": "vol", "C": "cfg", "S": "sys", "F": "ftab", "G": "gout"}

Spec = namedtuple("Spec", "name addr self args ret loose res check")


def S(name, addr, self_kind, args="", ret="none", loose=False, res=False, check=None):
    return Spec(name, addr, self_kind, args, ret, loose, res, check)


# The operations, in the order of enum Op in the unit file. args: f float (bits), i int, o opaque pointer word, P position,
# D distance, R reverb, V volume control, C speaker config, S system info, F float table, G the three outputs of Get.
# ret: none, bool (0 or 1), word, ptr (a label), dbl (the bits of f1), this (the answer is the object), mask (self + 0x18).
OPS = [
    S("TbSoundPosition::TbSoundPosition()", 0x1000CB64, "pos"),
    S("TbSoundPosition::TbSoundPosition(int,int,int)", 0x100246E4, "pos", "iii"),
    S("TbSoundPosition::Set(float,float,float)", 0x1000C580, "pos", "fff"),
    S("TbSoundPosition::Set(int,int,int)", 0x1000C8E0, "pos", "iii"),
    S("TbSoundPosition::SetX", 0x1000C5C0, "pos", "f"),
    S("TbSoundPosition::SetY", 0x1000C5F4, "pos", "f"),
    S("TbSoundPosition::SetZ", 0x1000C628, "pos", "f"),
    S("TbSoundPosition::Get", 0x10015F3C, "pos", "G"),
    S("TbSoundPosition::length", 0x1000C65C, "pos", "", "dbl", loose=True, check="sqrt"),
    S("TbSoundPosition::DotProduct", 0x1000C754, "pos", "P", "dbl", loose=True),
    S("TbSoundPosition::Valid", 0x1000C7C4, "pos", "", "bool", check="valid_self"),
    S("TbSoundPosition::operator!=", 0x1000C920, "pos", "P", "bool", check="memcmp"),
    S("TbSoundPosition::operator*=", 0x1000C994, "pos", "f", "this", loose=True),
    S("TbSoundPosition::operator/=", 0x1000C9EC, "pos", "f", "this", loose=True),
    S("TbSoundPosition::operator-", 0x1000CA44, "pos", "P", loose=True, res=True),
    S("TbSoundPosition::operator+", 0x1000CBA4, "pos", "P", loose=True, res=True),
    S("TbSoundPosition::TbSoundPosition(const TbSoundPosition&)", 0x1000CB08, "pos", "P"),
    S("TbSoundPosition::operator=", 0x100177C0, "pos", "P", "this"),
    S("TbSoundDistance::TbSoundDistance()", 0x10015950, "dist"),
    S("TbSoundDistance::TbSoundDistance(float,float,float)", 0x10001324, "dist", "fff"),
    S("TbSoundReverbInfo::TbSoundReverbInfo", 0x100012AC, "rev"),
    S("TbSoundVolumeControl::TbSoundVolumeControl", 0x10009D9C, "vol"),
    S("TbSoundVolumeControl::GetVolume", 0x1000AAC8, "vol", "i", "dbl", loose=True),
    S("TbSoundVolumeControl::SetVolumes", 0x10026F28, "vol", "F"),
    S("TbSoundVolumeControl::Init", 0x10026F68, "vol", "i"),
    S("TbSoundSampleInfo::TbSoundSampleInfo", 0x100158BC, "sample"),
    S("TbSoundSampleInfo::SetNumber", 0x1000DF08, "sample", "i"),
    S("TbSoundSampleInfo::SetUserId", 0x1000DCAC, "sample", "i"),
    S("TbSoundSampleInfo::SetPan", 0x1000DEC0, "sample", "i"),
    S("TbSoundSampleInfo::SetVolume", 0x1000DD98, "sample", "i"),
    S("TbSoundSampleInfo::SetPitch", 0x1000DE2C, "sample", "i"),
    S("TbSoundSampleInfo::SetFrequency", 0x1000DE74, "sample", "f"),
    S("TbSoundSampleInfo::SetPriority", 0x1000DDE0, "sample", "i"),
    S("TbSoundSampleInfo::SetPosition", 0x1001851C, "sample", "P", check="valid_args"),
    S("TbSoundSampleInfo::SetVelocity", 0x1001847C, "sample", "P", check="valid_args"),
    S("TbSoundSampleInfo::SetDistance", 0x1001959C, "sample", "D"),
    S("TbSoundSampleInfo::SetLoopCount", 0x1000DD4C, "sample", "i"),
    S("TbSoundSampleInfo::SetCallback", 0x1000DCF4, "sample", "o"),
    S("TbSoundSampleInfo::SetCompletedCallback", 0x100196F4, "sample", "o"),
    S("TbSoundSampleInfo::SetUserData", 0x1000DC60, "sample", "o"),
    S("TbSoundSampleInfo::SetReverb", 0x1000E068, "sample", "R"),
    S("TbSoundSampleInfo::SetVolumeControl", 0x1000DFFC, "sample", "Vi"),
    S("TbSoundSampleInfo::SetStreamingBufferSize", 0x1000DFA4, "sample", "i"),
    S("TbSoundSystemInfo::TbSoundSystemInfo()", 0x10011D7C, "sys"),
    S("TbSoundSystemInfo::TbSoundSystemInfo(const TbSpeaker::Config&)", 0x10011DD4, "sys", "C"),
    S("TbSoundSystemInfo::TbSoundSystemInfo(const TbSoundSystemInfo&)", 0x1000BA78, "sys", "S"),
    S("TbSoundSystemInfo::SetHWnd", 0x10011E34, "sys", "o"),
    S("TbSoundSystemInfo::SetGuid", 0x10011E7C, "sys", "o"),
    S("TbSoundSystemInfo::SetClassId", 0x10011ED8, "sys", "o"),
    S("TbSoundSystemInfo::SetListenerPosition", 0x10011F34, "sys", "P", check="valid_args"),
    S("TbSoundSystemInfo::SetListenerDirection", 0x10011FDC, "sys", "PP", check="valid_args"),
    S("TbSoundSystemInfo::SetListenerVelocity", 0x100120BC, "sys", "P", check="valid_args"),
    S("TbSoundSystemInfo::SetSpeakerConfig", 0x10012164, "sys", "C"),
    S("TbSoundSystemInfo::SetBuffersToUse", 0x100121C8, "sys", "i"),
    S("TbSoundSystemInfo::SetMaxBuffers", 0x10012218, "sys", "i"),
    S("TbSoundSystemInfo::SetHalfRate", 0x10012264, "sys", "i"),
    S("TbSoundSystemInfo::SetHardwareDevice", 0x100122A0, "sys", "o"),
    S("TbSoundSystemInfo::GetBuffersToUse", 0x1001061C, "sys", "", "word"),
    S("TbSoundSystemInfo::GetSpeakerConfig", 0x10015E74, "sys", "", "ptr"),
    S("TbSoundSystemInfo::GetListenerVelocity", 0x10015EB8, "sys", "", "ptr"),
    S("TbSoundSystemInfo::GetTopDir", 0x10015F00, "sys", "", "ptr"),
    S("TbSoundSystemInfo::GetFrontDir", 0x10015F8C, "sys", "", "ptr"),
    S("TbSoundSystemInfo::GetListenerPosition", 0x10015FCC, "sys", "", "ptr"),
    S("TbSoundSystemInfo::GetMask", 0x10016014, "sys", "", "mask"),
    S("TbSoundSystemInfo::GetMaxBuffers", 0x10027120, "sys", "", "word"),
    S("TbSoundSystemInfo::GetSamplesPlaying", 0x10028C08, "sys", "", "word"),
]
BY_NAME = {spec.name: (index, spec) for index, spec in enumerate(OPS)}

# ---------------------------------------------------------------- the data of the cases

# float bit patterns: zeros, ones, denormals, the extremes, infinities, quiet and signalling NaNs
FLOAT_EDGES = [0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x3F000000, 0x40000000, 0x00000001, 0x80000001, 0x007FFFFF,
               0x807FFFFF, 0x00800000, 0x80800000, 0x7F7FFFFF, 0xFF7FFFFF, 0x7F800000, 0xFF800000, 0x7FC00000, 0xFFC00000,
               0x7F800001, 0xFF800001, 0x7FFFFFFF, 0x5F800000, 0x1F800000, 0x7E800000, 0x20000000, 0x3FB504F3, 0x358637BD,
               0x4B000000, 0x40490FDB, 0xC2F6E979]
INT_EDGES = [0, 1, 2, 3, 7, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFF,
             0xFFFFFFFE, 0x12345678, 0xA5A5A5A5, 0x3F800000, 0x7FC00000, 0x7F800001, 0x00000001]
OPAQUE_EDGES = [0, 1, 0x1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xA5A5A5A5, 0x12345678, 0xCAFD0000, 0xCAFF0000]
BAD_FLOATS = [0x7FC00000, 0xFFC00000, 0x7F800001, 0xFF800001, 0x7F800000, 0xFF800000, 0x7FFFFFFF]
GOOD_FLOATS = [0, 0x80000000, 1, 0x007FFFFF, 0x00800000, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0xFF7FFFFF, 0x40490FDB]


def is_nan_bits(w):
    return (w & 0x7F800000) == 0x7F800000 and (w & 0x007FFFFF) != 0


def is_finite_bits(w):
    return (w & 0x7F800000) != 0x7F800000


def quiet(w):
    """A float argument travels through an FPR as a double: a signalling NaN would be quieted, so cases use the quiet form."""
    return w | 0x00400000 if is_nan_bits(w) else w


def rand_float(r):
    k = r.random()
    if k < 0.2:
        return r.choice(FLOAT_EDGES)
    if k < 0.45:
        return r.getrandbits(32)
    return struct.unpack("<I", struct.pack("<f", r.uniform(-1000.0, 1000.0)))[0]


def rand_finite(r):
    while True:
        w = rand_float(r)
        if is_finite_bits(w):
            return w


def rand_arg_float(r):
    return quiet(rand_float(r))


def rand_int(r):
    k = r.random()
    if k < 0.3:
        return r.choice(INT_EDGES)
    if k < 0.6:
        return r.getrandbits(32)
    return r.randrange(0, 0x400)


def rand_opaque(r):
    k = r.random()
    return r.choice(OPAQUE_EDGES) if k < 0.3 else r.getrandbits(32)


def garbage(r):
    """A pointer word that is not a label."""
    w = r.getrandbits(32)
    return w ^ 0x00010000 if LABEL <= w < LABEL + 0x100 else w


def rand_pos(r):
    k = r.random()
    if k < 0.45:
        return [rand_finite(r) for _ in range(3)]
    if k < 0.6:
        words = [rand_finite(r) for _ in range(3)]
        words[r.randrange(3)] = r.choice(BAD_FLOATS)
        return words
    return [rand_float(r) for _ in range(3)]


def rand_dist(r):
    return [rand_float(r) for _ in range(3)]


def rand_rev(r):
    return [r.getrandbits(32) if r.random() < 0.7 else r.choice(FLOAT_EDGES) for _ in range(12)]


def rand_cfg(r):
    return [r.getrandbits(32) for _ in range(4)]


def rand_ftab(r):
    return [rand_float(r) for _ in range(8)]


def rand_pointer(r):
    k = r.random()
    if k < 0.2:
        return 0
    if k < 0.65:
        return LABEL + r.randrange(2)
    return garbage(r)


def rand_vol(r):
    return [rand_pointer(r), rand_int(r)]


def rand_sys(r):
    words = [rand_int(r) for _ in range(19)]
    for i in POINTERS["sys"]:
        words[i] = rand_pointer(r)
    return words


def rand_gout(r):
    return [r.getrandbits(32) for _ in range(3)]


RAND_TOKEN = {"pos": rand_pos, "dist": rand_dist, "rev": rand_rev, "vol": rand_vol, "cfg": rand_cfg, "sys": rand_sys,
              "ftab": rand_ftab, "gout": rand_gout}


def rand_self(kind, r, style):
    """The record the function works on, before the call: random words, a recognisable pattern or zeros."""
    n = WORDS[kind]
    if style == "a5":
        words = [0xA5A5A5A5] * n
    elif style == "zero":
        words = [0] * n
    else:
        words = [rand_int(r) for _ in range(n)]
        for i in FLOATS.get(kind, ()):
            words[i] = rand_float(r)
        for i in POINTERS.get(kind, ()):
            words[i] = rand_pointer(r)
    return words


def pos_variants():
    """The positions that exercise Valid: valid ones, each component bad in turn, two bad, all bad."""
    out = [[g, g, g] for g in GOOD_FLOATS]
    out.append([0x3F800000, 0xBF800000, 0x40000000])
    for comp in range(3):
        for bad in BAD_FLOATS:
            words = [0x3F800000, 0x40000000, 0xC0000000]
            words[comp] = bad
            out.append(words)
    for a, b in ((0, 1), (0, 2), (1, 2)):
        words = [0x3F800000] * 3
        words[a] = 0x7FC00000
        words[b] = 0x7F800000
        out.append(words)
        words = [0x3F800000] * 3
        words[a] = 0x7F800001
        words[b] = 0xFF800000
        out.append(words)
    out.append([0x7FC00000, 0x7F800000, 0xFF800001])
    return out


def arith_pos_variants(r):
    """Positions for the arithmetic: extremes whose squares or products overflow or underflow, mixed signs, NaNs."""
    out = []
    for a in (0x7F7FFFFF, 0x5F800000, 0x1F800000, 0x00000001, 0x007FFFFF, 0x3F800000, 0xBF800000, 0x7F800000, 0x7FC00000):
        out.append([a, a, a])
        out.append([a, 0x3F800000, 0x80000000])
        out.append([0x40000000, a, 0xC0400000])
    for perm in ((0x3F800000, 0x39800000, 0x39800000), (0x39800000, 0x3F800000, 0x39800000), (0x39800000, 0x39800000, 0x3F800000)):
        out.append(list(perm))
        out.append([w ^ 0x80000000 for w in perm])
    for _ in range(12):
        out.append([rand_float(r) for _ in range(3)])
    return out


# ---------------------------------------------------------------- cases

def make_case(index, r, over=None, style="rand"):
    """One case of operation `index`: tokens 0 and 1 are the pool the pointer members of the fill point at, then one token per
    record argument. over: {argument position: value or a token (kind, words) or "alias" or "null"}."""
    spec = OPS[index]
    over = over or {}
    tokens = [("ftab", rand_ftab(r)), ("pos", rand_pos(r))]
    args = []
    for j, k in enumerate(spec.args):
        if k == "f":
            args.append(over[j] if j in over else rand_arg_float(r))
        elif k == "i":
            args.append(over[j] if j in over else rand_int(r))
        elif k == "o":
            args.append(over[j] if j in over else rand_opaque(r))
        else:
            kind = TOKEN_KIND[k]
            value = over[j] if j in over else (kind, RAND_TOKEN[kind](r))
            if value == "alias":
                args.append(-1)
            elif value == "null":
                args.append(-2)
            else:
                tokens.append(value)
                args.append(len(tokens) - 1)
    words = rand_self(spec.self, r, style)
    if spec.name == "TbSoundVolumeControl::GetVolume":
        words = [LABEL, rand_int(r)]
        args[0] = over[0] if 0 in over else r.randrange(8)
    return dict(op=index, self=words, tokens=tokens, args=args)


def edge_cases_of(index):
    import random
    r = random.Random(0x77 * 1000 + index)
    spec = OPS[index]
    out = []
    styles = ("rand", "a5", "zero")
    if not spec.args:
        for style in styles:
            for _ in range(4):
                out.append(make_case(index, r, style=style))
        return out
    for j, k in enumerate(spec.args):
        if k == "f":
            variants = [quiet(w) for w in FLOAT_EDGES]
        elif k == "i":
            variants = list(range(8)) if spec.name == "TbSoundVolumeControl::GetVolume" else INT_EDGES
        elif k == "o":
            variants = OPAQUE_EDGES
        elif k == "P":
            variants = [("pos", w) for w in pos_variants()]
            if spec.self == "pos":
                variants = [("pos", w) for w in arith_pos_variants(r)] + variants + ["alias"]
        elif k == "D":
            variants = [("dist", [w, w2, w]) for w in FLOAT_EDGES[:12] for w2 in (0, 0x3F800000)]
        elif k == "R":
            variants = [("rev", [w] * 12) for w in (0, 0xFFFFFFFF, 0xA5A5A5A5, 0x3F800000, 0x7F800001)]
            variants += [("rev", [0x11111111 * (i + 1) & MASK for i in range(12)])]
        elif k == "V":
            variants = [("vol", [0, 0]), ("vol", [LABEL, 8]), ("vol", [0xFFFFFFFF, 0xFFFFFFFF]), ("vol", [LABEL + 1, 3])]
        elif k == "C":
            variants = [("cfg", [0] * 4), ("cfg", [0xFFFFFFFF] * 4), ("cfg", [1, 2, 3, 4])]
        elif k == "S":
            variants = [("sys", [0] * 19), ("sys", [0xFFFFFFFF] * 19), ("sys", [0xA5A5A5A5] * 19),
                        ("sys", [i + 1 for i in range(19)])]
            variants += [("sys", [LABEL if i in POINTERS["sys"] else 0x01010101 * (i % 5) for i in range(19)])]
        elif k == "F":
            variants = [("ftab", [0] * 8), "null"]
        else:
            variants = [("gout", [0] * 3), ("gout", [0xA5A5A5A5] * 3)]
        for variant in variants:
            for style in styles[: 2 if len(variants) > 20 else 3]:
                out.append(make_case(index, r, {j: variant}, style))
    if spec.name == "TbSoundSystemInfo::SetListenerDirection":
        good = ("pos", [0x3F800000, 0, 0])
        bad = ("pos", [0, 0x7FC00000, 0])
        out.append(make_case(index, r, {0: bad, 1: good}))
        out.append(make_case(index, r, {0: good, 1: bad}))
        out.append(make_case(index, r, {0: bad, 1: bad}))
        out.append(make_case(index, r, {0: good, 1: good}))
        out.append(make_case(index, r, {0: good, 1: "alias" if False else good}))
        for a in pos_variants()[::3]:
            for b in pos_variants()[1::5]:
                out.append(make_case(index, r, {0: ("pos", a), 1: ("pos", b)}))
    return out


def random_case_of(index):
    def make(r):
        return make_case(index, r, style=r.choice(("rand", "rand", "rand", "a5")))
    return make


# ---------------------------------------------------------------- the PowerPC side

def to_words_be(raw):
    return list(struct.unpack(">%dI" % (len(raw) // 4), raw))


def stub_classify(e):
    v = e.f(1)
    bits = struct.unpack(">Q", struct.pack(">d", v))[0]
    if math.isnan(v):
        cls = 1 if bits & (1 << 51) else 0
    elif math.isinf(v):
        cls = 2
    elif v == 0.0:
        cls = 3
    elif abs(v) < 2.0 ** -126:
        cls = 5
    else:
        cls = 4
    e.si_calls.append(("c", "nan" if math.isnan(v) else v, cls))
    e.set_r(3, cls)


def stub_isfinite(e):
    v = e.f(1)
    ok = 0 if math.isnan(v) or math.isinf(v) else 1
    e.si_calls.append(("f", "nan" if math.isnan(v) else v, ok))
    e.set_r(3, ok)


def stub_memcmp(e):
    a, b, n = e.r(3), e.r(4), e.r(5)
    x, y = e.read(a, n), e.read(b, n)
    diff = 0
    for p, q in zip(x, y):
        if p != q:
            diff = p - q
            break
    e.si_calls.append(("memcmp", a, b, n))
    e.set_r(3, diff & MASK)


def stub_sqrt(e):
    v = e.f(1)
    e.si_calls.append(("sqrt", "nan" if math.isnan(v) else v))
    e.set_f1(math.nan if (math.isnan(v) or v < 0) else math.sqrt(v))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.si_calls = []
    e.stub("__fpclassifyf", stub_classify)
    e.stub("__isfinitef", stub_isfinite)
    e.stub("memcmp", stub_memcmp)
    e.stub("sqrt", stub_sqrt)
    lib = runner.lib
    lib.tpw_si_op_count.restype = C.c_int32
    assert lib.tpw_si_op_count() == len(OPS), "the operation tables differ"
    lib.tpw_si_size.restype = C.c_int32
    lib.tpw_si_size.argtypes = [C.c_int32]
    for kind in CLASS_KIND:
        assert lib.tpw_si_size(CLASS_KIND[kind]) <= HOST_SPACE


def label_of(word, addrs):
    """The label of a PowerPC address among the case's tokens, else the word itself."""
    for i, a in enumerate(addrs):
        if word == a:
            return LABEL + i
    return word


def ppc_image(e, kind, addr, addrs):
    words = to_words_be(e.read(addr, 4 * WORDS[kind]))
    for i in POINTERS.get(kind, ()):
        words[i] = label_of(words[i], addrs)
    return words


def ppc_words(kind, words, addrs):
    """A record's words with the labels replaced by the PowerPC addresses."""
    out = list(words)
    for i in POINTERS.get(kind, ()):
        w = out[i]
        if LABEL <= w < LABEL + len(addrs):
            out[i] = addrs[w - LABEL]
    return out


def canon(kind, words, loose):
    """Words ready to compare: with `loose`, every NaN in a float word becomes one value."""
    words = list(words)
    if loose:
        for i in FLOATS.get(kind, ()):
            if is_nan_bits(words[i]):
                words[i] = 0x7FC00000
    return tuple(words)


def float_of(word):
    return struct.unpack("<f", struct.pack("<I", word))[0]


def same_arg(recorded, word):
    value = float_of(word)
    if recorded == "nan":
        return math.isnan(value)
    return not math.isnan(value) and recorded == value


def valid_trace_ok(calls, position_words):
    """The imports Valid calls, for each position in turn (the next is looked at only if the one before was valid): the six
    calls in the order __fpclassifyf x, y, z, then __isfinitef x, y, z, each only when the ones before did not fail."""
    order = (("c", 0), ("c", 1), ("c", 2), ("f", 0), ("f", 1), ("f", 2))
    i = 0
    for words in position_words:
        passed = True
        for name, index in order:
            if i >= len(calls):
                return False
            got = calls[i]
            i += 1
            if got[0] != name or not same_arg(got[1], words[index]):
                return False
            if (got[2] == 1) if name == "c" else (got[2] == 0):
                passed = False
                break
        if not passed:
            break
    return i == len(calls)


def ppc_case(index):
    spec = OPS[index]

    def run(e, c):
        e.si_calls = []
        tokens = c["tokens"]
        addrs = [e.alloc(4 * WORDS[kind] + 16) for kind, _ in tokens]
        self_addr = e.alloc(4 * WORDS[spec.self] + 16)
        for (kind, words), addr in zip(tokens, addrs):
            e.write(addr, struct.pack(">%dI" % len(words), *ppc_words(kind, words, addrs)))
        e.write(self_addr, struct.pack(">%dI" % len(c["self"]), *ppc_words(spec.self, c["self"], addrs)))
        res_addr = e.alloc(16)
        e.write(res_addr, struct.pack(">3I", 0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5))
        args = [T.I(res_addr)] if spec.res else []
        args.append(T.I(self_addr))
        words_args = []
        for k, a in zip(spec.args, c["args"]):
            if k == "f":
                args.append(T.F(T.bits_f32(a)))
            elif k in "io":
                args.append(T.I(a))
            elif k == "G":
                base = addrs[a]
                args += [T.I(base), T.I(base + 4), T.I(base + 8)]
            else:
                args.append(T.I(self_addr if a == -1 else 0 if a == -2 else addrs[a]))
        r3, r4, f1 = T.call_ppc(e, spec.addr, args)
        out = dict(self=canon(spec.self, ppc_image(e, spec.self, self_addr, addrs), spec.loose))
        for i, (kind, _) in enumerate(tokens):
            out["token%d" % i] = canon(kind, ppc_image(e, kind, addrs[i], addrs), spec.loose)
        if spec.res:
            out["result"] = canon("res", to_words_be(e.read(res_addr, 12)), spec.loose)
        if spec.ret in ("bool", "word"):
            out["ret"] = r3 & MASK
        elif spec.ret == "ptr":
            out["ret"] = label_of(r3, addrs)
        elif spec.ret == "this":
            out["ret"] = 1 if r3 == self_addr else 0
        elif spec.ret == "mask":
            out["ret"] = 1 if r3 == self_addr + 0x18 else 0
        elif spec.ret == "dbl":
            bits = struct.unpack(">Q", struct.pack(">d", f1))[0]
            out["ret"] = "nan" if math.isnan(f1) else bits
        out["trace"] = trace_ok(spec, c, e, addrs, self_addr)
        return out
    return run


def trace_ok(spec, c, e, addrs, self_addr):
    calls = e.si_calls
    tokens = c["tokens"]
    if spec.check == "valid_self":
        return valid_trace_ok(calls, [c["self"]])
    if spec.check == "valid_args":
        positions = []
        for k, a in zip(spec.args, c["args"]):
            if k == "P":
                positions.append(tokens[a][1])
        return valid_trace_ok(calls, positions)
    if spec.check == "memcmp":
        other = c["args"][0]
        expected = [("memcmp", self_addr, self_addr if other == -1 else addrs[other], 12)]
        return calls == expected
    if spec.check == "sqrt":
        return len(calls) == 1 and calls[0][0] == "sqrt"
    return calls == []


# ---------------------------------------------------------------- the host side

def host_case(index):
    spec = OPS[index]

    def run(lib, c):
        tokens = c["tokens"]
        buffers = [C.create_string_buffer(HOST_SPACE) for _ in tokens]
        self_buffer = C.create_string_buffer(HOST_SPACE)
        res_buffer = C.create_string_buffer(16)
        res_buffer.raw = struct.pack("<3I", 0xA5A5A5A5, 0xA5A5A5A5, 0xA5A5A5A5) + b"\0" * 4
        pointers = (C.c_void_p * len(tokens))(*[C.addressof(b) for b in buffers])
        lib.tpw_si_load.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32]
        lib.tpw_si_dump.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32]

        def load(kind, buffer, words):
            if kind in CLASS_KIND:
                array = (C.c_uint32 * len(words))(*words)
                lib.tpw_si_load(CLASS_KIND[kind], C.addressof(buffer), array, pointers, len(tokens))
            else:
                buffer.raw = struct.pack("<%dI" % len(words), *words) + buffer.raw[4 * len(words):]

        def dump(kind, buffer):
            if kind in CLASS_KIND:
                array = (C.c_uint32 * WORDS[kind])()
                lib.tpw_si_dump(CLASS_KIND[kind], C.addressof(buffer), array, pointers, len(tokens))
                return list(array)
            return list(struct.unpack("<%dI" % WORDS[kind], buffer.raw[: 4 * WORDS[kind]]))

        for (kind, words), buffer in zip(tokens, buffers):
            load(kind, buffer, words)
        load(spec.self, self_buffer, c["self"])
        pointer_args = []
        word_args = []
        for k, a in zip(spec.args, c["args"]):
            if k in "fio":
                word_args.append(a)
            elif a == -1:
                pointer_args.append(C.addressof(self_buffer))
            elif a == -2:
                pointer_args.append(0)
            else:
                pointer_args.append(C.addressof(buffers[a]))
        if spec.res:
            pointer_args.append(C.addressof(res_buffer))
        while len(pointer_args) < 2:
            pointer_args.append(0)
        word_args += [0] * (3 - len(word_args))
        lib.tpw_si_call.restype = C.c_int64
        lib.tpw_si_call.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint32,
                                    C.POINTER(C.c_double)]
        dret = C.c_double(0.0)
        ret = lib.tpw_si_call(index, C.addressof(self_buffer), pointer_args[0], pointer_args[1], word_args[0], word_args[1],
                              word_args[2], C.byref(dret))
        out = dict(self=canon(spec.self, dump(spec.self, self_buffer), spec.loose))
        for i, (kind, _) in enumerate(tokens):
            out["token%d" % i] = canon(kind, dump(kind, buffers[i]), spec.loose)
        if spec.res:
            out["result"] = canon("res", dump("res", res_buffer), spec.loose)
        if spec.ret in ("bool", "word"):
            out["ret"] = ret & MASK
        elif spec.ret == "ptr":
            word = ret & MASK
            for i, b in enumerate(buffers):
                if ret == C.addressof(b):
                    word = LABEL + i
            out["ret"] = word
        elif spec.ret in ("this", "mask"):
            out["ret"] = ret
        elif spec.ret == "dbl":
            bits = struct.unpack(">Q", struct.pack(">d", dret.value))[0]
            out["ret"] = "nan" if math.isnan(dret.value) else bits
        out["trace"] = True
        return out
    return run


TESTS = [
    FnTest("%s 0x%08x" % (spec.name, spec.addr), ppc_case(i), host_case(i), edge=(lambda i=i: edge_cases_of(i)),
           rnd=random_case_of(i))
    for i, spec in enumerate(OPS)
]
