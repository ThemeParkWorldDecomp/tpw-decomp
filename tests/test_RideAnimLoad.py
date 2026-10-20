"""Ride loading, filename search, animation passes and native record expansion versus PPC."""
import ctypes as C
import re
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest
SHIM_LIBRARY = "unit_RideAnimLoad"
ENTRY = 0x1006840c
def setup(runner): T.install_std_stubs(runner.emu)
def ws(e, a, values): e.write(a, struct.pack(">" + "I" * len(values), *[v & 0xffffffff for v in values]))
class Side:
    def __init__(self, e, c):
        self.e = e; self.c = c; self.log = CL.PpcLog({}); e.cl = self.log
        self.header = e.alloc(0xb8); self.nodes = e.alloc(3 * 0xa0); self.field = e.alloc(0x30)
        ws(e, self.header + 0x30, [c["seed"][0]]); e.write(self.header + 0x44, b"\0\3")
        ws(e, self.header + 0x6c, [self.field, self.nodes]); ws(e, self.header + 0x78, [self.nodes])
        e.write(self.header + 0x18, b"test-base\0"); ws(e, self.field + 0x18, [3, 2])
        e.write(self.field + 4, struct.pack(">HH", 12, 6))
        heights = e.alloc(48, struct.pack(">12f", *[i * 0.5 for i in range(12)]))
        quads = e.alloc(24, struct.pack(">12H", *[v for i in range(6) for v in (i * 17, i * 9)]))
        ws(e, self.field + 0x28, [heights, quads])
        for i, name in enumerate(("Sky", "grass", "SKY")):
            ws(e, self.nodes + i * 0xa0, [0x40 + i]); ws(e, self.nodes + i * 0xa0 + 0x54, [e.alloc(8, name.encode() + b"\0")])
        self.mesh = e.alloc(0x40); ws(e, self.mesh + 4, [self.header])
        self.output = e.alloc(4); ws(e, self.output, [0x12121212])
        self.name = e.alloc(128, c["name"].encode() + b"\0"); self.loaded = e.alloc(1040, b"unchanged\0")
        self.path = e.alloc(32, b"disc:models:\0")
        self.alloc_index = self.load_index = 0
        self.saved = {0x102c79a0: e.read(0x102c79a0, 4)}
        # TPGlobals is 0x1029c7fc; mEngineFlags is +0x400c.
        self.global_flags = e.u32(0x10260660) + 0x400c
        self.saved[self.global_flags] = e.read(self.global_flags, 4)
        ws(e, self.global_flags, [c["seed"][1]]); ws(e, 0x102c79a0, [c["seed"][2]])
        st = self.st = CL.Stubs(); st.real.add(ENTRY)
        def strcat(e):
            destination = e.r(3); e.write(destination, (e.cstr(destination) + e.cstr(e.r(4))).encode() + b"\0")
        st.imp("strcat", strcat)
        def stricmp(e):
            a, b = e.cstr(e.r(3)).lower(), e.cstr(e.r(4)).lower()
            e.set_r(3, (a > b) - (a < b))
        st.imp("_stricmp__10NS_MacDozeFPCcPCc", stricmp)
        st.internal(0x100b88a0, lambda e: e.set_r(3, 0 if c["seed"][4] else self.path))
        def mesh(e):
            name = e.cstr(e.r(3)); e.cl.rec("Mesh", [e.r(5), e.r(6), e.r(7)], [name])
            mode = c["seed"][3]; e.set_r(3, self.mesh if mode and (mode != 2 or "TestBase" in name) else 0)
        st.internal(0x100b7d04, mesh)
        st.internal(0x100682fc, lambda e: e.cl.rec("Scratch"))
        st.internal(0x10059714, lambda e: (e.cl.rec("Exists", [e.r(4)], [e.cstr(e.r(3))]), e.set_r(3, self.exists(e.cstr(e.r(3)), False))))
        st.internal(0x10064bd4, self.alloc)
        st.imp("LoadM3D2Header__FPcUl", self.load)
        st.internal(0x10064d7c, lambda e: e.cl.rec("DestroyHeader", [self.log.lab(e.r(3))]))
        st.internal(0x100c3430, lambda e: e.cl.rec("InitType", [self.log.lab(e.r(4))]))
        st.internal(0x100c609c, lambda e: e.cl.rec("Channels"))
        st.internal(0x101ee150, lambda e: e.cl.rec("Strip", [int(e.r(4) != 0), CL.fbits(e.f(1)), int(e.r(6) != 0), int(e.r(7) != 0)]))
        st.internal(0x10067cf4, lambda e: e.cl.rec("CorrectFlags"))
        st.internal(0x100b7dd4, lambda e: e.cl.rec("TypeMesh"))
        st.internal(0x10064f40, lambda e: e.cl.rec("EngineMesh"))
        st.internal(0x100c7768, lambda e: (e.cl.rec("InitSet", [e.r(4), e.r(5), e.r(6)]), e.set_r(3, 1)))
        st.internal(0x101ff7e4, lambda e: e.cl.rec("Embedded"))
        def error(e):
            e.cl.rec("ErrorBox", (), [e.cstr(e.r(3)) % e.cstr(e.r(4))])
        st.internal(0x1006fd3c, error)
        self.old = st.activate(e)
    def exists(self, name, loading):
        match = re.search(r"([CDILSMEUWBRO])(\d*)\.md2$", name)
        if not match: return False
        cat = self.c["seed"][6 + "CDILSMEUWBRO".index(match[1])]
        return bool(cat & 16) if not match[2] else int(match[2]) <= (cat & 15) and not (loading and cat & 32)
    def alloc(self, e):
        i = self.alloc_index; self.alloc_index += 1
        e.cl.rec("Alloc", [i, e.r(3) // (4 if i < 2 else 8) if i < 3 else self.c["seed"][5] if i == 3 else e.r(3), e.r(4)])
        e.set_r(3, e.alloc(e.r(3)))
    def load(self, e):
        name = e.cstr(e.r(3)); e.cl.rec("AnimHeader", [e.r(4)], [name])
        if not self.exists(name, True): e.set_r(3, 0); return
        h, a = e.alloc(0xb8), e.alloc(0x40)
        ws(e, h + 0x98, [a]); self.log.labels[h] = 100 + self.load_index; self.log.labels[a] = 200 + self.load_index
        self.load_index += 1; e.set_r(3, h)
    def state(self):
        e = self.e; h = self.header
        out = [e.u32(h + 0x30)] + [e.u32(self.nodes + i * 0xa0) for i in range(3)] + list(e.read(h + 0xa8, 4))
        o = e.u32(self.output); valid = o != 0x12121212; out.append(int(valid))
        if not valid: return out
        count_words = struct.unpack(">HH", e.read(o + 12, 4))
        out += [int(e.u32(o) == o), e.u32(o + 4), int(e.u32(o + 8) == self.mesh), *count_words, int(e.u32(o + 16) == o + 0xfc), int(e.u32(o + 20) != 0)]
        for i in range(12):
            g = o + 0x18 + i * 12; flags, count, entries = e.u32(g), e.u32(g + 4), e.u32(g + 8)
            out += [flags, count, int(entries != 0)]
            for j in range(count): out += [e.u32(entries + j * 8), self.log.lab(e.u32(entries + j * 8 + 4))]
        count, animations, headers = e.u32(o + 0xa8), e.u32(o + 0xac), e.u32(o + 0xdc)
        if count > 36 or (count and (animations == 0 or headers == 0)):
            raise RuntimeError(f"unexpected animation arrays: {count=} {animations=:x} {headers=:x} {self.c=}")
        out.append(count)
        for i in range(count): out += [self.log.lab(e.u32(animations + i * 4)), self.log.lab(e.u32(headers + i * 4))]
        return out
    def close(self):
        CL.Stubs.deactivate(self.e, self.old)
        for a, b in self.saved.items(): self.e.write(a, bytes(b))
def ppc(e, c):
    s = Side(e, c)
    try:
        result = T.call_ppc(e, ENTRY, [s.name, c["flags"], s.output, s.loaded if c["want"] else 0, c["seed"][5]])[0]
        return result, s.state(), e.cstr(s.loaded), s.log.log
    finally: s.close()
def host(lib, c):
    CL.host_begin(lib, {})
    begin, run, state, name = lib.tpw_ral_begin, lib.tpw_ral_run, lib.tpw_ral_state, lib.tpw_ral_name
    begin.argtypes = [C.POINTER(C.c_uint32)]; begin.restype = None
    run.argtypes = [C.c_char_p, C.c_uint32, C.c_int32]; run.restype = C.c_int32
    state.argtypes = [C.POINTER(C.c_uint32)]; state.restype = C.c_int32
    name.restype = C.c_char_p
    begin((C.c_uint32 * 18)(*c["seed"])); result = run(c["name"].encode(), c["flags"], c["want"])
    out = (C.c_uint32 * 256)(); data = list(out[:state(out)])
    return result, data, name().decode(), CL.host_end(lib)
def case(flags=8, catalog=None, name="Base", header=0, quick=0, load=1, path=1, time=1, want=1):
    return dict(flags=flags, seed=[header, 8, quick, load, int(not path), time] + (catalog or [0] * 12), name=name, want=want)
def edges():
    for flags in (0, 2, 8, 6, 0x20, 0x40, 0x80, 0x100, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000, 0x800000, 0x20000, 0x40000, 0x8fffff): yield case(flags=flags, header=4)
    for name in ("Base", "Base.md2", ".Base", "a.b", "disc:.:Base"):
        for flags in (0, 8, 0x200): yield case(flags=flags, name=name)
    for count in (1, 2, 3, 16, 33, 34, 48):
        for quick in (0, 1): yield case(catalog=[count] * 12, quick=quick, time=3)
    yield case(load=0); yield case(load=0, path=0); yield case(want=0)
def rnd(r):
    return case(flags=r.getrandbits(24), catalog=[r.choice([0, 1, 2, 3, 16, 33, 34, 48]) for _ in range(12)],
                header=r.randrange(8), quick=r.randrange(2), load=r.randrange(5) != 0, time=r.randrange(1, 5), want=r.randrange(2))
TESTS = [FnTest("Ride mesh and animation loader", ppc, host, edge=edges, rnd=rnd)]
