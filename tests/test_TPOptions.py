"""TPOptions and inline members owned by Text, InterfaceFont and TimeSig headers.
Untranslated serialization, font and world functions use recording boundary fakes.
TPDialog getters are real on both sides, so their invalid manual return is unreachable.
"""
import ctypes as C
import random
import struct
import tpwtest as T
from batch3_support import *

OPTIONS, BALANCE, WORLD_PTR = 0x1038070c, 0x102b4da8, 0x1037ec0c

def case(r):
    vals = T.rwords(r, 28); vals[24] = T.u32(r.choice([-5, -1, 0, 1, 2, 3, 4, 5, T.rint(r)]))
    return dict(words=T.rwords(r, 306), font=T.rwords(r, 5), vt=T.rwords(r, 54),
                stamp=r.getrandbits(32), global_turn=r.getrandbits(32), turn=r.getrandbits(32),
                mask=r.getrandbits(32), answer=T.u32(r.choice([0, 1, -1, T.rint(r)])),
                id=T.rint(r), save=T.rint(r), version=r.choice([-1, 0, 1, 2, T.rint(r)]),
                serial_flags=r.getrandbits(32), flags=r.getrandbits(32), values=vals,
                flag=r.choice([0, 1, -1, 0x7fff, 0x8000, 0x10000, 0x1ffff, T.rint(r)]),
                file=r.randbytes(32), raw=r.randbytes(16), record=T.rwords(r, 3))

def edges():
    r = random.Random(32); out = []
    for save in (0, 1, -1):
        for version in (-0x80000000, -1, 0, 1, 2, 0x7fffffff):
            for answer in (0, 1, 0xffffffff):
                c = case(r); c.update(save=save, version=version, answer=answer); out.append(c)
    for stamp in (0, 1, 2, 3, 4, 5, 0x7fffffff, 0x80000000, 0xfffffffc, 0xffffffff):
        for turn in (0, 1, 3, 4, 5, 0x80000000, 0xffffffff):
            c = case(r); c.update(stamp=stamp, turn=turn, global_turn=turn, mask=0); out.append(c)
    return out

def option_edges():
    r = random.Random(33); out = []
    # Every combination of the flag priorities and manual sprite selection bits.
    bits = [0x2000, 0x1000, 0x800, 0x4000, 8, 16, 32]
    for n in range(128):
        for value in (-0x80000000, -1, 0, 1, 2, 3, 4, 0x7fffffff):
            c = case(r); c["flags"] = sum(b for i, b in enumerate(bits) if n & (1 << i))
            c["values"][24] = T.u32(value); out.append(c)
    return out

def font_ppc(ctor):
    def run(e, c):
        raw = c["font"][:4] + [c["vt"][0]] + c["font"][4:]
        a = e.alloc(24, struct.pack(">6I", *raw)); begin_ppc(e, c, [("font", a, 24)])
        ret = T.call_ppc(e, e.func("__ct__20InterfaceFontWrapperFv" if ctor else "SetID__13InterfaceFontFi"), [a] if ctor else [a, T.I(c["id"])])[0]
        return dict(words=font_values(e, a), trace=e.b3_trace, allocs=allocs(e),
                    abi_ok=e.u32(a + 16) == c["vt"][0], ret_this=ret == a if ctor else None)
    return run

def font_host(ctor):
    def run(lib, c):
        begin_host(lib, c); w = (U32 * 5)(*c["font"])
        a = F(lib, "tpw_Font_create", VP, VP)(w); start_host(lib)
        if ctor: F(lib, "tpw_Font_ctor", None, VP)(a)
        else: F(lib, "tpw_Font_SetID", None, VP, I32)(a, c["id"])
        F(lib, "tpw_Font_read", None, VP, VP)(a, w)
        out = dict(words=list(w), abi_ok=True, ret_this=True if ctor else None, **end_host(lib, [("font", a, 1)]))
        F(lib, "tpw_Font_release", None, VP)(a); return out
    return run

def seed_text(e, a, c):
    w = c["words"]; put(e, a, w[:3]); put(e, a + 12, [w[3], c["vt"][53], w[4], w[5]])
    for i in range(52):
        f = w[6 + i * 5:11 + i * 5]; put(e, a + 28 + i * 24, f[:4] + [c["vt"][i]] + f[4:])
    put(e, a + 0x4fc, [c["vt"][52]] + w[266:285]); put(e, a + 0x54c, text_words(c)[285:])

def text_words(c):
    # The final two resource pointers are also preserved by the destructor.
    return c["words"] + list(struct.unpack("<2I", c["raw"][:8]))

def read_text(e, a):
    out = words(e, a, 3) + [e.u32(a + off) for off in (12, 20, 24)]
    for i in range(52): out += font_values(e, a + 28 + i * 24)
    return out + words(e, a + 0x500, 19) + words(e, a + 0x54c, 23)

def text_ppc(e, c):
    a = e.alloc(0x5a8); seed_text(e, a, c)
    bases = [("font%d" % i, a + 28 + i * 24, 24) for i in range(52)] + [("params", a + 0x4fc, 80), ("manager", a + 12, 16), ("text", a, 0x5a8)]
    begin_ppc(e, c, bases)
    ret = T.call_ppc(e, e.func("__dt__5CTextFv"), [a, T.I(c["flag"])])[0]
    vts = [e.u32(a + 28 + i * 24 + 16) for i in range(52)] + [e.u32(a + 0x4fc), e.u32(a + 16)]
    return dict(words=read_text(e, a), trace=e.b3_trace, allocs=allocs(e), ret_this=ret == a, abi_ok=vts == c["vt"])

def text_host(lib, c):
    begin_host(lib, c); a = F(lib, "tpw_Text_create", VP, VP)((U32 * 308)(*text_words(c)))
    bases = [("font%d" % i, F(lib, "tpw_Text_subobject", VP, VP, I32)(a, i), 1) for i in range(52)]
    bases += [("params", F(lib, "tpw_Text_subobject", VP, VP, I32)(a, 52), 1), ("manager", F(lib, "tpw_Text_subobject", VP, VP, I32)(a, 53), 1), ("text", a, 1)]
    out = (U32 * 308)(); start_host(lib)
    ret = F(lib, "tpw_Text_destroy", VP, VP, C.c_int16, VP)(a, T.s16(c["flag"]), out)
    ans = dict(words=list(out), ret_this=ret == a, abi_ok=True, **end_host(lib, bases))
    if T.s16(c["flag"]) <= 0: F(lib, "tpw_Text_release", None, VP)(a)
    return ans

TIME = [("Serialise", "FPviiUl"), ("SetTimeSig", "Fv"), ("TurnsSinceLastUpdate", "CFv"), ("GetTimeSecond", "CFv"), ("SecondsSinceLastUpdate", "CFv")]

def time_ppc(name, suffix):
    def run(e, c):
        a = e.alloc(16, struct.pack(">4I", c["stamp"], *c["words"][:3])); file = e.alloc(32, c["file"])
        gw, rw = e.alloc(0x1da710), e.alloc(0x1da710)
        put(e, gw + 0x1da70c, [c["global_turn"]]); put(e, rw + 0x1da70c, [c["turn"]]); put(e, WORLD_PTR, [gw])
        begin_ppc(e, c, [("stamp", a, 16), ("file", file, 32)]); e.b3_world = rw
        args = [a] + ([file, T.I(c["save"]), T.I(c["version"]), T.I(c["serial_flags"])] if name == "Serialise" else [])
        ret = T.call_ppc(e, e.func(name + "__8CTimeSig" + suffix), args)[0]
        return dict(ret=None if name == "SetTimeSig" else T.u32(ret), words=words(e, a, 4), file=e.read(file, 32),
                    world=[e.u32(gw + 0x1da70c), e.u32(rw + 0x1da70c)], world_ptr_ok=e.u32(WORLD_PTR) == gw,
                    trace=e.b3_trace, allocs=allocs(e))
    return run

def time_host(name, suffix):
    def run(lib, c):
        begin_host(lib, c); a = (U32 * 4)(c["stamp"], *c["words"][:3]); file = C.create_string_buffer(c["file"], 32)
        F(lib, "tpw_TimeSig_worlds", None, U32, U32)(c["global_turn"], c["turn"]); start_host(lib)
        types = [VP] + ([VP, I32, I32, U32] if name == "Serialise" else [])
        args = [a] + ([C.addressof(file), c["save"], c["version"], c["serial_flags"]] if name == "Serialise" else [])
        ret = F(lib, "tpw_TimeSig_" + name, None if name == "SetTimeSig" else U32, *types)(*args)
        return dict(ret=ret, words=list(a), file=file.raw, world=[F(lib, "tpw_TimeSig_world_read", U32, I32)(i) for i in range(2)],
                    world_ptr_ok=True, **end_host(lib, [("stamp", C.addressof(a), 16), ("file", C.addressof(file), 32)]))
    return run

def options_ppc(name):
    def run(e, c):
        a = e.alloc(16, c["raw"]); put(e, BALANCE + 8, c["values"]); put(e, 0x103bc664, [c["flags"]])
        begin_ppc(e, c, [("object", a, 16)])
        ret = T.call_ppc(e, e.func(name + "__8COptionsCFv"), [a])[0]
        return dict(ret=ret, raw=e.read(a, 16), flags=e.u32(0x103bc664), values=words(e, BALANCE + 8, 28), trace=e.b3_trace, allocs=allocs(e))
    return run

def options_host(name):
    def run(lib, c):
        begin_host(lib, c); a = C.create_string_buffer(c["raw"], 16); vals = (U32 * 28)(*c["values"])
        F(lib, "tpw_Options_balance_seed", None, VP)(vals)
        flags = U32.from_address(F(lib, "tpw_TPDialog_options", VP)()); flags.value = c["flags"]
        start_host(lib); ret = F(lib, "tpw_Options_" + name, I32, VP)(a)
        F(lib, "tpw_Options_balance_read", None, VP)(vals)
        return dict(ret=ret, raw=a.raw, flags=flags.value, values=list(vals), **end_host(lib, []))
    return run

def dtor_ppc(e, c):
    a = e.alloc(16, c["raw"]); begin_ppc(e, c, [("object", a, 16)])
    ret = T.call_ppc(e, e.func("__dt__8COptionsFv"), [a, T.I(c["flag"])])[0]
    return dict(raw=e.read(a, 16), ret_this=ret == a, trace=e.b3_trace, allocs=allocs(e))

def dtor_host(lib, c):
    begin_host(lib, c); a = F(lib, "tpw_Options_create", VP, VP)(C.create_string_buffer(c["raw"], 16)); out = C.create_string_buffer(16)
    start_host(lib); ret = F(lib, "tpw_Options_destroy", VP, VP, C.c_int16, VP)(a, T.s16(c["flag"]), out)
    ans = dict(raw=out.raw, ret_this=ret == a, **end_host(lib, [("object", a, 16)]))
    if T.s16(c["flag"]) <= 0: F(lib, "tpw_Options_release", None, VP)(a)
    return ans

def sinit_ppc(e, c):
    e.write(OPTIONS, c["raw"][:4]); put(e, 0x10380710, c["record"]); begin_ppc(e, c, [])
    T.call_ppc(e, e.func("__sinit_TPOptions_cpp"), [])
    assert e.registered == [(OPTIONS, 0x102672c0, 0x10380710)]
    assert (e.u32(0x102672c0), e.u32(0x102672c4)) == (e.func("__dt__8COptionsFv"), e.toc)
    return dict(raw=e.read(OPTIONS, 4), trace=e.b3_trace, allocs=allocs(e))

def sinit_host(lib, c):
    begin_host(lib, c); a = F(lib, "tpw_Options_global", VP)()
    # The empty class has one native byte. Its original four-byte storage is ABI padding.
    C.memmove(a, c["raw"][:1], 1); start_host(lib); F(lib, "tpw_sinit_TPOptions", None)()
    return dict(raw=C.string_at(a, 1) + c["raw"][1:4], **end_host(lib, []))

TESTS = [T.FnTest("CText::~CText", text_ppc, text_host, edge=edges, rnd=case),
         T.FnTest("InterfaceFont::SetID", font_ppc(False), font_host(False), edge=edges, rnd=case),
         T.FnTest("InterfaceFontWrapper::InterfaceFontWrapper()", font_ppc(True), font_host(True), edge=edges, rnd=case)]
TESTS += [T.FnTest("CTimeSig::" + name, time_ppc(name, suffix), time_host(name, suffix), edge=edges, rnd=case) for name, suffix in TIME]
TESTS += [T.FnTest("COptions::~COptions", dtor_ppc, dtor_host, edge=edges, rnd=case)]
TESTS += [T.FnTest("COptions::" + name, options_ppc(name), options_host(name), edge=option_edges, rnd=case) for name in ("GetNumKidSprites", "GetNumStaffSprites")]
TESTS += [T.FnTest("__sinit_TPOptions_cpp", sinit_ppc, sinit_host, edge=edges, rnd=case)]
