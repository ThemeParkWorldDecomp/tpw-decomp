"""Engine subsystem start-up and shutdown (engine library): CDisplay, the render list and
render memory, the gamut, the height field, the light manager, the map lists, CSwitch,
the sprite manager, and the bit and sine helpers.

A case is a script of operations run on both sides. After every operation the test
compares its result, the events of the fakes (the engine allocator, operator new and
delete, the error handler, the objects the unit only destroys) and the state of every
subsystem object, read field by field in the same order as tests/unit_EngineStartup.cpp
writes the host's.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_EngineStartup"

# ---- the original's addresses
FN = dict(
    CreateSineTable=0x10038bf8, Gamut_Init=0x10009904, Gamut_Close=0x100099d0,
    LightManager_Init=0x10014278, LightManager_Close=0x10014334, RenderList_Init=0x10024650,
    RenderList_Close=0x10024704, HField_Init=0x1000bca4, HField_Close=0x1000bd60, MapWho_Init=0x10017318,
    MapWho_Close=0x100173d0, Switch_Init=0x100326f0, Switch_Close=0x100327a8, SpriteManager_Init=0x10031238,
    SpriteManager_Close=0x100312f8, RenderMemory_Init=0x100269d4, RenderMemory_Close=0x10026ac0,
    SetYClipper=0x10009b80, SetFinalCallback=0x100177cc, Reset=0x10026c34, GetVertexBufferPtr=0x100271a8,
    LoadInfo=0x1002720c, LoadInfo_CopyVDB=0x10027354, Close=0x1002707c, Display=0x10027de0, SetDisplay=0x10027f2c,
    SetWindow=0x1002806c, ResetWindow=0x10028200, SetBitPattern=0x1002827c, SetZSpace=0x10028470,
    ZSetup=0x100284c4, SetFogSpace=0x10028558, SetFogColor=0x100285c8, SetMaxBits=0x10028664,
    OutputDebugInfo=0x10028718, BitsSet=0x10038a78, ShiftFactor=0x10038ac0,
    Particle_Init=0x1001f21c, Particle_Close=0x1001f35c, RegisterLinkBase=0x10019700, FreeLinkBase=0x10019768,
    InitWaveLoad=0x10005c84, CloseWaveLoad=0x10005d68, Instance=0x1003ed0c, AddRef=0x10039598,
    GetRefCount=0x100395d8, Release=0x10039610, GetLinkType=0x1001fc08, GetData=0x1001fbd8)
# Members the entry points reach, which other modules may have left stubs on.
INNER = [0x10009a44, 0x100143b0, 0x1000bffc, 0x1000c068, 0x1000d8b8, 0x1000dce0, 0x100174f8, 0x10017720,
         0x10017a74, 0x1003287c, 0x10031538, 0x1003157c, 0x100316f8, 0x10026b48, 0x10026be4, 0x10026cdc,
         0x1002707c, 0x1001f3e4, 0x100394d0, 0x10039460, 0x1001980c]
INIT_CLOSE = ["Gamut", "LightManager", "RenderList", "HField", "MapWho", "Switch", "SpriteManager", "RenderMemory"]

G_GAMUT, G_LIGHT, GLOBAL_DIFFUSE, G_RLIST, G_HFIELD = 0x10052db8, 0x100af1b8, 0x100af1a8, 0x1016562c, 0x100af11c
G_MAPWHO, G_SWITCH, G_SPRITE, G_RMEM = 0x100af618, 0x10173180, 0x10173154, 0x10165630
DISPLAY, SYSTEM, SINE, P_MALLOC, P_FREE = 0x10165634, 0x10165f80, 0x101731c0, 0x10051f70, 0x10051f74
ERROR_PUSH, PROC_CLOSE, PROC_DTOR = 0x10008904, 0x10012b98, 0x100127f0
DISPLAY_WORDS = 0x93c // 4
MARKER = 0x7c0fd000          # stands for the function the game installs in the map
LINKS = [0x7c0fd100 + 16 * k for k in range(8)]     # stand for other objects' link bases (labels 20 + k)
G_PARTICLE, LINK_BASES, LINK_COUNT = 0x100b0508, 0x100af620, 0x100af61c
G_WAVELET, G_WORKSPACE, G_WAVELOAD, G_PROCBUFFER = 0x101735e8, 0x102b3614, 0x10052b04, 0x10052b00
NODES, INFOS, INFO_SIZE = 0x4800, 0x1000, 0x38

# Word indexes of CDisplay's float fields (a NaN compares as a NaN).
DISPLAY_FLOATS = {o // 4 for o in [0x04, 0x10, 0x14, 0x18, 0x1c, 0x8b8, 0x900, 0x938]
                  + list(range(0x30, 0x80, 4)) + list(range(0x8c0, 0x8ec, 4))}

# The PowerPC side's heap for a case: one start-up of the render memory takes 3.5 MB, more
# than the emulator's shared scratch area holds.
HEAP_BASE, HEAP_SIZE = 0x60000000, 0x4000000


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if not hasattr(e, "es_heap"):
        e.uc.mem_map(HEAP_BASE, HEAP_SIZE)
        e.es_heap = HEAP_BASE
        e.es_malloc = T.fake_function(e, lambda emu: emu.es_side.malloc_callback())
        e.es_free = T.fake_function(e, lambda emu: emu.es_side.free_callback())


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def canon(bits):
    """Float bits with every NaN made the same."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x007FFFFF) else bits


def display_pattern(seed):
    """What the display holds before a script: words that read as ordinary floats, and a
    plausible screen size (the emulator's fctiwz returns 0x80000000 where the real
    processor saturates to 0x7fffffff, so SetWindow must not be given a size whose
    products overflow)."""
    words = [0x40000000 | ((seed * 2654435761 + i * 40503) & 0x00FFFFFF) for i in range(DISPLAY_WORDS)]
    words[2] = 100 + seed % 1900
    words[3] = 100 + (seed // 7) % 1500
    return words


# ----------------------------------------------------------------- PowerPC side


class PpcSide:
    def __init__(self, e, case):
        fixed = {MARKER: 9}
        fixed.update({a: 20 + k for k, a in enumerate(LINKS)})
        self.e, self.L = e, CL.PpcLog(fixed, case.get("q"))
        e.cl = self.L
        e.es_side = self
        e.es_heap = HEAP_BASE
        self.entries = 0          # entries of the sprite manager's planted texture array
        self.free_list = (0, -1, 1)
        self.marked = None        # the render memory's marked-records sum, when known

    def heap(self, size):
        e = self.e
        a = (e.es_heap + 15) & ~15
        e.es_heap = a + size + 64
        if e.es_heap > HEAP_BASE + HEAP_SIZE:
            raise RuntimeError("the case's PowerPC heap is used up")
        return a

    def malloc(self, size):
        L = self.L
        L.rec("Malloc", (size,))
        if not L.ret("Malloc", 1):
            return 0
        skew = L.ret("Skew", 0) if size == 0x10000F else 0
        block = self.heap(size + 16) + skew
        if size <= 0x40000:
            self.e.write(block, bytes(size))
        L.newlab(block)
        return block

    def malloc_callback(self):
        self.e.set_r(3, self.malloc(self.e.r(3)))

    def free_callback(self):
        e, L = self.e, self.L
        where = e.r(3)
        block = e.u32(where)
        L.rec("Free", (L.lab(block),))
        L.forget(block)
        w32(e, where, 0)

    def op_new(self, e, array=False):
        size = e.r(3)
        if not array and not self.L.ret("New", 1):     # an allocation made to fail
            e.set_r(3, 0)
            return
        block = self.heap(size)
        if not array:
            e.write(block, b"\xa5" * size)
        self.L.rec("new[]" if array else "new", (self.L.newlab(block),))
        e.set_r(3, block)

    def op_delete(self, e):
        block = e.r(3)
        if self.L.lab(block) > 0:
            self.L.rec("delete", (self.L.lab(block),))
        self.L.forget(block)

    def destroy_new_array(self, e):
        block = e.r(3)
        for _ in range(self.entries):
            self.L.rec("Entry.dtor")
        self.L.rec("delete", (self.L.lab(block),))
        self.L.forget(block)

    def stubs(self):
        st = CL.Stubs()
        st.real = set(FN.values()) | set(INNER)
        st.internal(ERROR_PUSH, CL.recorder("Error", "xii"))
        st.internal(PROC_CLOSE, CL.recorder("Proc.Close", "L", ret=lambda e, L: 1))
        st.internal(PROC_DTOR, CL.recorder("Proc.dtor", "D"))
        st.imp("__nw__FUl", self.op_new)
        st.imp("__nwa__FUl", lambda e: self.op_new(e, True))
        st.imp("__dl__FPv", self.op_delete)
        st.imp("__dla__FPv", self.op_delete)
        st.imp("__destroy_new_array", self.destroy_new_array)
        st.imp("__assertion_failed", lambda e: self.L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
        return st

    def reset(self, case):
        e = self.e
        for a in (G_GAMUT, G_LIGHT, G_RLIST, G_HFIELD, G_MAPWHO, G_SWITCH, G_SPRITE, G_RMEM, G_PARTICLE, LINK_COUNT,
                  G_WAVELET, G_WORKSPACE, G_WAVELOAD, G_PROCBUFFER):
            w32(e, a, 0)
        e.write(DISPLAY, struct.pack(f">{DISPLAY_WORDS}I", *display_pattern(case["seed"])))
        e.write(SINE, bytes(1024))
        e.write(GLOBAL_DIFFUSE, bytes(16))
        w32(e, SYSTEM + 0x1c, 3)
        w32(e, P_MALLOC, e.es_malloc)
        w32(e, P_FREE, e.es_free)

    def call(self, name, *args):
        return T.call_ppc(self.e, FN[name], list(args))[0]

    def op(self, op, a):
        e, u = self.e, self.e.u32
        fl = lambda bits: T.F(T.bits_f32(bits))
        if op == 0:
            self.call("CreateSineTable")
            return 0
        if 1 <= op <= 16:
            name = INIT_CLOSE[(op - 1) // 2] + ("_Init" if op % 2 else "_Close")
            result = T.s32(self.call(name))
            if op == 16 and result:        # they pointed into the pool that is gone
                w32(e, G_WORKSPACE, 0)
                w32(e, G_PROCBUFFER, 0)
            return result
        if op >= 42:
            return self.op_stage2(op, a)
        if op == 17:
            if not u(G_GAMUT):
                return -9
            self.call("SetYClipper", u(G_GAMUT), fl(a[0]), fl(a[1]))
            return 0
        if op == 18:
            h = u(G_HFIELD)
            if not h:
                return -9
            if a[0] & 1:
                block = self.heap(8)
                self.L.rec("new", (self.L.newlab(block),))
                w32(e, h + 0x230, block)
            if a[0] & 2:
                w32(e, h + 0x12970, self.malloc(64))
            return 0
        if op == 19:
            if not u(G_MAPWHO):
                return -9
            w32(e, u(G_MAPWHO) + 0x10, self.malloc(64))
            return 0
        if op == 20:
            if not u(G_MAPWHO):
                return -9
            self.call("SetFinalCallback", u(G_MAPWHO), MARKER if a[0] else 0)
            return 0
        if op == 21:
            s = u(G_SPRITE)
            if not s:
                return -9
            block = self.heap(a[0] * 0x5c + 8)
            self.L.rec("new[]", (self.L.newlab(block),))
            self.entries = a[0]
            w32(e, s + 0x2400, block)
            return 0
        m = u(G_RMEM)
        if op == 22:
            if not m or not u(m + 0x44):
                return -9
            self.call("Reset", m, a[0])
            return 0
        if op == 23:
            if not m or not u(m + 0x58):
                return -9
            r = self.call("GetVertexBufferPtr", m, a[0])
            return -1 if r == 0 else T.s32(r - u(m + 0x58))
        if op in (24, 25):
            if not m or not u(m + 0x44):
                return -9
            r = self.call("LoadInfo" if op == 24 else "LoadInfo_CopyVDB", m, a[0])
            return T.s32(r - u(m + 0x44)) // INFO_SIZE
        if op == 26:
            if not m or not u(m + 0x44):
                return -9
            pool = m + 8 + a[0] * 0x18
            w32(e, pool + 4, u(pool + 4) + a[1])
            w32(e, pool + 0x10, u(pool + 0x10) + a[2])
            if a[0] == 0:
                w32(e, m + 0x54, u(m + 0x54) - a[3])
            else:
                w32(e, m + 0x50, u(m + 0x50) + a[3])
            return 0
        if op == 27:
            self.call("Display", DISPLAY)
        elif op == 28:
            w32(e, SYSTEM + 0x1c, a[3])
            self.call("SetDisplay", DISPLAY, a[0], a[1], a[2])
        elif op == 29:
            self.call("SetWindow", DISPLAY, fl(a[0]), fl(a[1]), fl(a[2]), fl(a[3]))
        elif op == 30:
            self.call("ResetWindow", DISPLAY)
        elif op == 31:
            w32(e, DISPLAY + 0x24, a[12])
            self.call("SetBitPattern", DISPLAY, *a[:12])
        elif op == 32:
            self.call("SetZSpace", DISPLAY, fl(a[0]))
        elif op == 33:
            self.call("ZSetup", DISPLAY, fl(a[0]), fl(a[1]), fl(a[2]))
        elif op == 34:
            self.call("SetFogSpace", DISPLAY, fl(a[0]), fl(a[1]))
        elif op == 35:
            self.call("SetFogColor", DISPLAY, a[0])
        elif op == 36:
            for offset, value in zip((0x904, 0x90c, 0x914), a):
                w32(e, DISPLAY + offset, value)
            self.call("SetMaxBits", DISPLAY)
        elif op == 37:
            return T.s32(self.call("BitsSet", a[0]))
        elif op == 38:
            return T.s32(self.call("ShiftFactor", a[0]))
        elif op == 39:
            self.call("OutputDebugInfo", DISPLAY)
        elif op == 40:
            if not m or not u(m + 0x44):
                return -9
            infos = u(m + 0x44)
            for i in range(INFOS):
                w32(e, infos + i * INFO_SIZE, 0x1000 + i)
        elif op == 41:
            if not m:
                return -9
            result = T.s32(self.call("Close", m))
            w32(e, G_WORKSPACE, 0)
            w32(e, G_PROCBUFFER, 0)
            return result
        else:
            return -99
        return 0

    def op_stage2(self, op, a):
        """The particle object, the link bases and the wavelet loader."""
        e, u = self.e, self.e.u32
        rm = u(G_RMEM)
        instance = u(G_WAVELET)            # the CWavelet inside the object, 0x10 bytes in
        if op == 42:
            return T.s32(self.call("Particle_Init"))
        if op == 43:
            return T.s32(self.call("Particle_Close"))
        if op == 44:
            return T.s32(self.call("RegisterLinkBase", LINKS[a[0]]))
        if op == 45:
            return T.s32(self.call("FreeLinkBase", LINKS[a[0]]))
        if op in (46, 48):
            if not rm or not u(rm + 0x44):
                return -9
            if op == 46:
                return T.s32(self.call("InitWaveLoad"))
            return 1 if self.call("Instance") else 0
        if op == 47:
            waveload = u(G_WAVELOAD)
            last = waveload != 0 and T.s32(u(waveload - 0x10 + 8)) <= 1
            result = T.s32(self.call("CloseWaveLoad"))
            if last:
                w32(e, G_WAVELET, 0)
            return result
        if op in (49, 50, 51):
            if not instance:
                return -9
            result = T.s32(self.call({49: "AddRef", 50: "GetRefCount", 51: "Release"}[op], instance - 0x10))
            if op == 51 and result < 1:
                w32(e, G_WAVELET, 0)
                w32(e, G_WAVELOAD, 0)
            return result
        if op in (52, 53):
            particle = u(G_PARTICLE)
            if not particle:
                return -9
            result = self.call("GetLinkType" if op == 52 else "GetData", particle)
            return T.s32(result) if op == 52 else int(result != 0)
        return -99

    def dynamic_offset(self, pointer):
        u = self.e.u32
        if pointer == 0:
            return -1
        rm = u(G_RMEM)
        if not rm or not u(rm + 0x20):
            return -2
        return T.s32(pointer - u(rm + 0x20))

    def state(self, op):
        e, L, u = self.e, self.L, self.e.u32
        s = lambda a: T.s32(u(a))
        out = []
        g = u(G_GAMUT)
        out.append(L.lab(g))
        if g:
            out += [u(g + 0x30), u(g + 0x34)] + [u(g + o) for o in range(0x3c, 0x54, 4)]
            out += [s(g + o) for o in (0x54, 0x58, 0x5c, 0x60, 0x6c, 0xc0, 0xc4, 0xc8, 0x1cc, 0x1d0)]
        light = u(G_LIGHT)
        out.append(L.lab(light))
        if light:
            out += [s(light), s(light + 4)]
        out += [u(GLOBAL_DIFFUSE + 4 * i) for i in range(4)]
        rl = u(G_RLIST)
        out.append(L.lab(rl))
        if rl:
            out.append(s(rl))
        h = u(G_HFIELD)
        out.append(L.lab(h))
        if h:
            out += [s(h + o) for o in (0, 4, 0x20, 0x24, 0x128, 0x12c)] + [L.lab(u(h + 0x230))]
            out += [s(h + o) for o in (0x234, 0x238, 0x23c, 0x240, 0x2958, 0x12968)] + [L.lab(u(h + 0x12970))]
        m = u(G_MAPWHO)
        out.append(L.lab(m))
        if m:
            nodes = u(m)
            out += [L.lab(nodes), s(m + 8), s(m + 0xc), L.lab(u(m + 0x10))]
            out += [s(m + o) for o in range(0x14, 0x30, 4)]
            out += [s(m + o) for o in (0x30, 0x34, 0x38, 0x3c, 0x40, 0x70, 0x74)] + [L.lab(u(m + 0x78))]
            if op in (9, 10):      # the only operations that touch the entries
                length, first, ordered = 0, -1, 1
                if nodes and L.lab(nodes) > 0:
                    words = struct.unpack(f">{NODES * 2}I", e.read(nodes, NODES * 8))
                    node = u(m + 4)
                    while node and length <= NODES:
                        index = (node - nodes) // 8
                        if length == 0:
                            first = index
                        nxt = words[index * 2 + 1] if 0 <= index < NODES else 0
                        if nxt and nxt != node - 8:
                            ordered = 0
                        length += 1
                        node = nxt
                self.free_list = (length, first, ordered)
            out += list(self.free_list)
        sw = u(G_SWITCH)
        out.append(L.lab(sw))
        if sw:
            out.append(s(sw))
        sp = u(G_SPRITE)
        out.append(L.lab(sp))
        if sp:
            out += [1 if u(sp + 0x2400) else 0, s(sp + 0x2404), s(sp + 0x2408), s(sp + 0x240c)]
        rm = u(G_RMEM)
        out.append(L.lab(rm))
        if rm:
            out += [s(rm), s(rm + 4)]
            for pool in (rm + 8, rm + 0x20):
                out += [L.lab(u(pool)), s(pool + 4), s(pool + 8), L.lab(u(pool + 0xc)), s(pool + 0x10), s(pool + 0x14)]
            infos = u(rm + 0x44)
            out += [L.lab(infos), s(rm + 0x48), s(rm + 0x4c), s(rm + 0x50), s(rm + 0x54)]
            block, scratch = u(rm + 0x60), u(rm + 0x58)
            out.append(L.lab(block))
            if block and L.lab(block) > 0:
                out += [-1 if scratch == 0 else scratch - block, T.s32(u(rm + 0x5c) - scratch),
                        T.s32(u(rm + 0x3c) - scratch)]
            else:
                out += [-3, -3, -3]
            current = u(rm + 0x38)
            if infos and L.lab(infos) > 0 and infos <= current < infos + INFOS * INFO_SIZE:
                out.append((current - infos) // INFO_SIZE)
                data, index = u(current + 0x28), u(current + 0x2c)
                where_data = where_index = -4
                for p, pool in enumerate((rm + 8, rm + 0x20)):
                    if u(pool) and u(pool) <= data <= u(pool) + 0x100000:
                        where_data = p * 0x1000000 + data - u(pool)
                    if u(pool + 0xc) and u(pool + 0xc) <= index <= u(pool + 0xc) + 0x20000:
                        where_index = p * 0x1000000 + index - u(pool + 0xc)
                out += [-1 if data == 0 else where_data, -1 if index == 0 else where_index, u(current)]
            else:
                out.append(-5)
            if op in (15, 16, 22, 24, 25, 40, 41) or self.marked is None:
                self.marked = 0
                if infos and L.lab(infos) > 0:
                    raw = e.read(infos, INFOS * INFO_SIZE)
                    self.marked = sum(i + 1 for i in range(INFOS) if raw[i * INFO_SIZE:i * INFO_SIZE + 4] != b"\0\0\0\0")
            out.append(self.marked)
        particle = u(G_PARTICLE)
        out.append(L.lab(particle))
        if particle:
            out += [s(particle + 4), s(particle + 8)] + [int(u(particle + o) != 0) for o in range(0x84, 0xa0, 4)]
        count = s(LINK_COUNT)
        out.append(count)
        own = u(particle) if particle else 0          # the object's first word points at its link base
        for i in range(min(max(count, 0), 0x200)):
            entry = u(LINK_BASES + 4 * i)
            out.append(1 if entry and entry == own else L.lab(entry))
        instance = u(G_WAVELET)
        out.append(int(instance != 0))
        if instance:
            out += [s(instance - 0x10 + 8), L.lab(u(instance - 0x10 + 0xc))]
        out += [self.dynamic_offset(u(G_WORKSPACE)), int(u(G_WAVELOAD) != 0), self.dynamic_offset(u(G_PROCBUFFER))]
        display = struct.unpack(f">{DISPLAY_WORDS}I", e.read(DISPLAY, DISPLAY_WORDS * 4))
        out += [canon(w) if i in DISPLAY_FLOATS else w for i, w in enumerate(display)]
        out += [canon(w) for w in struct.unpack(">256I", e.read(SINE, 1024))]
        return out


def ppc(e, case):
    side = PpcSide(e, case)
    saved_pointers = (e.u32(P_MALLOC), e.u32(P_FREE))
    saved = side.stubs().activate(e)
    try:
        side.reset(case)
        results, states, marks = [], [], []
        for op, args in case["script"]:
            results.append(side.op(op, args))
            states.append(side.state(op))
            marks.append(len(side.L.log))
    finally:
        CL.Stubs.deactivate(e, saved)
        w32(e, P_MALLOC, saved_pointers[0])
        w32(e, P_FREE, saved_pointers[1])
    return dict(results=results, log=side.L.log, marks=marks, states=states)


# ----------------------------------------------------------------- host side


def capacity_ok(lib):
    """The render memory's capacity: the Mac's 4096 records and pools of 1 MB and 64 KB of
    indices, or built with the port fixes on (unit v5L47) at least eight times as many records
    and pools at least eight times as large, which the state shows at the Mac's sizes."""
    if not hasattr(lib, "es_capacity_ok"):
        lib.tpw_es_capacity.restype, lib.tpw_es_capacity.argtypes = C.c_int32, []
        lib.tpw_es_portfix.restype, lib.tpw_es_portfix.argtypes = C.c_int32, []
        records, scale = lib.tpw_es_capacity() >> 4, lib.tpw_es_capacity() & 15
        if lib.tpw_es_portfix() == 1:
            lib.es_capacity_ok = records >= 0x8000 and scale >= 8
        else:
            lib.es_capacity_ok = records == 0x1000 and scale == 1
    return lib.es_capacity_ok


def host(lib, case):
    reset = lib["tpw_es_reset"]
    reset.restype, reset.argtypes = None, [C.POINTER(C.c_uint32)]
    op_fn = lib["tpw_es_op"]
    op_fn.restype, op_fn.argtypes = C.c_int64, [C.c_int32, C.POINTER(C.c_int32)]
    state_fn = lib["tpw_es_state"]
    state_fn.restype, state_fn.argtypes = C.c_int32, [C.POINTER(C.c_int64), C.c_int32]
    marker = lib["tpw_es_marker"]
    marker.restype = C.c_void_p

    reset((C.c_uint32 * DISPLAY_WORDS)(*display_pattern(case["seed"])))
    link = lib["tpw_es_link"]
    link.restype, link.argtypes = C.c_void_p, [C.c_int32]
    fixed = {9: marker()}
    fixed.update({20 + k: link(k) for k in range(8)})
    CL.host_begin(lib, fixed, case.get("q"))
    results, states, marks = [], [], []
    buffer = (C.c_int64 * 2048)()
    try:
        for op, args in case["script"]:
            padded = (C.c_int32 * 16)(*[T.s32(v) for v in args])
            results.append(op_fn(op, padded))
            count = state_fn(buffer, 2048)
            state = list(buffer[:count])
            # The display and the sine table are the last 0x24f + 256 numbers.
            base = count - DISPLAY_WORDS - 256
            for i in range(DISPLAY_WORDS):
                if i in DISPLAY_FLOATS:
                    state[base + i] = canon(state[base + i])
            for i in range(count - 256, count):
                state[i] = canon(state[i])
            states.append(state)
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    if not capacity_ok(lib):
        results.append("render memory capacity")
    return dict(results=results, log=log, marks=marks, states=states)


# ----------------------------------------------------------------- scripts


def fb(x):
    return T.f32_bits(x)


def rfloat(r):
    return fb(r.choice([0.0, 1.0, -1.0, 0.5, 2.0, 10.0, 20.0, 100.0, 130.0, 1000.0, 0.25, 0.75, -0.5, 1e-3,
                        r.uniform(-2, 2), r.uniform(0, 500)]))


def display_op(r):
    kind = r.choice([27, 28, 28, 29, 29, 29, 30, 31, 31, 32, 33, 34, 34, 35, 36, 36, 39])
    if kind == 28:
        w, h = r.choice([(640, 480), (800, 600), (1024, 768), (320, 200), (1, 1), (0, 0), (0, 5), (7, 0),
                         (r.randrange(1, 2000), r.randrange(1, 2000)), (-640, 480)])
        return (28, [w, h, r.choice([16, 24, 32, 8, 0]), r.choice([3, 3, 4, 0, 1])])
    if kind == 29:
        if r.random() < 0.5:
            return (29, [fb(r.choice([-1.0, -0.5, 0.0])), fb(r.choice([-1.0, 0.0, 0.5])),
                         fb(r.choice([2.0, 1.0, 0.5])), fb(r.choice([2.0, 1.0, 0.25]))])
        return (29, [rfloat(r) for _ in range(4)])
    if kind == 31:
        def channel(bits_choices):
            return [r.getrandbits(32), r.randrange(0, 32), r.choice(bits_choices)]
        common = [0, 1, 4, 5, 5, 5, 6, 6, 8]
        wild = common + [9, 16, 24, 31, 32, 33, 40, 63, 64, -1]
        pick = common if r.random() < 0.7 else wild
        return (31, channel(pick) + channel(pick) + channel(pick) + channel(pick) + [r.choice([16, 16, 16, 24, 32, 15])])
    if kind == 32:
        return (32, [rfloat(r)])
    if kind == 33:
        return (33, [rfloat(r), rfloat(r), rfloat(r)])
    if kind == 34:
        return (34, [rfloat(r), rfloat(r)])
    if kind == 35:
        return (35, [r.choice([0, 0xFFFFFFFF, 0x40008000, 0x80000000, r.getrandbits(32)])])
    if kind == 36:
        def mask():
            return r.choice([0, 1, 0x80000000, 0xF800, 0x7E0, 0x1F, 0xFF0000, 0xFF00, 0xFF, 0x7C00, 0x3E0,
                             r.getrandbits(32), 1 << r.randrange(32)])
        return (36, [mask(), mask(), mask()])
    return (kind, [])


def memory_op(r):
    kind = r.choice([22, 22, 23, 23, 24, 24, 24, 25, 25, 26, 26, 40, 41])
    if kind in (40, 41):
        return (kind, [])
    if kind == 22:
        return (22, [r.randrange(8)])
    if kind == 23:
        return (23, [r.choice([0, 1, 16, 0x1000, 0xFFFF, 0x10000, 0x10001, 0x8000, -1, -16, r.randrange(0x12000)])])
    if kind in (24, 25):
        return (kind, [r.choice([1, 2]) | r.choice([0, 0, 4]) | (r.getrandbits(8) << 3 if r.random() < 0.3 else 0)])
    return (26, [r.randrange(2), r.randrange(0, 0x4000), r.randrange(0, 0x800), r.randrange(0, 4)])


def subsystem_op(r):
    kind = r.random()
    if kind < 0.6:
        return (r.randrange(1, 17), [])
    if kind < 0.68:
        return (17, [rfloat(r), rfloat(r)])
    if kind < 0.78:
        return (18, [r.randrange(1, 4)])
    if kind < 0.86:
        return (19, [])
    if kind < 0.92:
        return (20, [r.randrange(2)])
    return (21, [r.randrange(0, 5)])


def stage2_op(r):
    kind = r.choice([42, 42, 43, 44, 44, 45, 45, 46, 46, 47, 47, 48, 49, 50, 51, 51, 52, 53, 15, 16, 24, 26])
    if kind in (44, 45):
        return (kind, [r.randrange(8)])
    if kind == 24:
        return (24, [r.choice([1, 2])])
    if kind == 26:
        return (26, [1, r.randrange(0, 0x4000), r.randrange(0, 0x800), r.randrange(0, 3)])
    return (kind, [])


def edge_stage2():
    r = random.Random(74)
    yield dict(seed=1, script=[(42, []), (42, []), (52, []), (53, []), (44, [0]), (44, [1]), (45, [0]), (45, [0]),
                               (43, []), (43, []), (45, [1]), (45, [7])])
    # The registry keeps 0x200 entries and silently drops the rest.
    yield dict(seed=2, script=[(44, [k % 8]) for k in range(0x202)] + [(42, []), (45, [3]), (43, [])])
    yield dict(seed=3, script=[(15, []), (26, [1, 0x1234, 0x56, 1]), (46, []), (46, []), (50, []), (48, []), (50, []),
                               (47, []), (47, []), (51, []), (51, []), (51, []), (46, []), (16, [])])
    yield dict(seed=4, script=[(46, []), (48, []), (47, []), (49, []), (50, []), (51, [])])
    # The decompressor's own allocation failing (the render memory's comes first).
    yield dict(seed=5, q={"New": [1, 0, 1]}, script=[(15, []), (46, []), (46, []), (47, []), (16, [])])
    yield dict(seed=5, q={"New": [1, 0, 1]}, script=[(15, []), (48, []), (48, []), (50, []), (16, [])])
    yield dict(seed=6, q={"New": [0, 1]}, script=[(42, []), (42, []), (43, [])])
    yield dict(seed=7, script=[(15, []), (48, []), (51, []), (51, []), (51, []), (48, []), (50, []), (16, [])])
    for _ in range(40):
        yield case_of(r, [stage2_op], 18, with_memory=r.random() < 0.7)
    for _ in range(10):
        yield case_of(r, [stage2_op], 18, fail=True, with_memory=True)


def helper_op(r):
    kind = r.choice([0, 37, 37, 37, 38, 38, 38])
    if kind == 0:
        return (0, [])
    return (kind, [r.choice([0, 1, 2, 0x80000000, 0xFFFFFFFF, 0xF800, 0x7E0, 0x1F, 0xFF000000, 1 << r.randrange(32),
                             r.getrandbits(32)])])


def queues(r, fail):
    q = {"Skew": [r.randrange(16) for _ in range(4)]}
    if fail:
        q["Malloc"] = [1 if r.random() < 0.8 else 0 for _ in range(r.randrange(1, 14))] + [1]
        if r.random() < 0.5:
            q["New"] = [1 if r.random() < 0.7 else 0 for _ in range(r.randrange(1, 10))] + [1]
    return q


def close_all():
    return [(op, []) for op in (47, 43, 2, 4, 6, 8, 10, 12, 14, 16)]


def case_of(r, makers, length, fail=False, with_memory=False):
    script = []
    if with_memory:
        script.append((15, []))
    inits = 1 if with_memory else 0
    loaded = False               # whether LoadInfo has run since the render memory started
    for _ in range(length):
        op = r.choice(makers)(r)
        if op[0] == 15:          # each start-up of the render memory takes 3.5 MB of the case's heap
            inits += 1
            if inits > 8:
                continue
        if op[0] in (15, 16, 41):
            loaded = False
        # LoadInfo_CopyVDB reads the pool LoadInfo last used; before any LoadInfo that
        # field is whatever the allocation held.
        if op[0] == 25 and not loaded:
            op = (24, op[1])
        if op[0] == 24:
            loaded = True
        script.append(op)
    return dict(seed=r.getrandbits(20), q=queues(r, fail), script=script + close_all())


def edge_display():
    r = random.Random(71)
    base = [(27, []), (28, [640, 480, 16, 3]), (31, [0x8000, 15, 1, 0x7C00, 10, 5, 0x3E0, 5, 5, 0x1F, 0, 5, 16]),
            (36, [0xF800, 0x7E0, 0x1F]), (29, [fb(-1.0), fb(-1.0), fb(2.0), fb(2.0)]), (30, []), (39, [])]
    yield dict(seed=1, script=base)
    for renderer in (0, 3, 4):
        yield dict(seed=2, script=[(27, []), (28, [800, 600, 32, renderer])])
    for bits in range(0, 12):
        yield dict(seed=3, script=[(31, [1, 2, 3, 4, 5, bits, 7, 8, bits, 10, 11, bits, 16])])
    for depth in (15, 16, 24, 32):
        yield dict(seed=4, script=[(31, [1, 2, 3, 4, 5, 5, 7, 8, 6, 10, 11, 5, depth])])
    for near, far in ((100.0, 130.0), (5.0, 5.0), (9.0, 3.0), (0.0, 1.0)):
        yield dict(seed=5, script=[(34, [fb(near), fb(far)])])
    for _ in range(40):
        yield case_of(r, [display_op], 12)


def edge_memory():
    r = random.Random(72)
    yield dict(seed=1, script=[(15, []), (24, [1]), (26, [0, 0x100, 0x20, 1]), (24, [1]), (25, [1]), (24, [2]),
                               (26, [1, 0x300, 0x40, 1]), (25, [2]), (24, [6]), (25, [2]), (22, [0]), (22, [1]),
                               (22, [2]), (22, [3]), (23, [16]), (23, [0x10000]), (23, [1]), (15, []), (16, []),
                               (16, [])])
    # Every allocation of the start-up failing in turn (six of them), with each rounding.
    for failing in range(6):
        yield dict(seed=2, q={"Malloc": [1] * failing + [0, 1]}, script=[(15, []), (15, []), (16, [])])
    for skew in range(16):
        yield dict(seed=3, q={"Skew": [skew]}, script=[(15, []), (23, [3]), (16, [])])
    for pools in range(4):
        yield dict(seed=8, script=[(15, []), (24, [1]), (26, [0, 64, 8, 2]), (24, [1]), (40, []), (22, [pools]),
                                   (24, [2]), (16, [])])
    yield dict(seed=9, script=[(15, []), (23, [4]), (41, []), (23, [4]), (24, [1]), (22, [3]), (41, []), (16, [])])
    for _ in range(30):
        yield case_of(r, [memory_op], 14, with_memory=True)
    for _ in range(10):
        yield case_of(r, [memory_op, lambda r: (r.choice([15, 16]), [])], 10, fail=True)


def edge_subsystems():
    r = random.Random(73)
    everything = [(op, []) for op in range(1, 17, 2)]
    yield dict(seed=1, script=everything + everything + close_all() + close_all())
    yield dict(seed=2, script=everything + [(17, [fb(1.5), fb(-2.5)]), (18, [3]), (19, []), (20, [1]), (20, [0]),
                                            (21, [3])] + close_all())
    for planted in (1, 2, 3):
        yield dict(seed=3, script=[(7, []), (18, [planted]), (8, []), (8, [])])
    for entries in (0, 1, 4):
        yield dict(seed=4, script=[(13, []), (21, [entries]), (14, [])])
    yield dict(seed=5, q={"Malloc": [0]}, script=[(9, []), (19, []), (10, [])])
    # operator new failing for each subsystem in turn (the original checks for NULL).
    for init in range(1, 17, 2):
        yield dict(seed=7, q={"New": [0, 1]}, script=[(init, []), (init, []), (init + 1, []), (init + 1, [])])
    yield dict(seed=6, script=[(17, [0, 0]), (18, [1]), (19, []), (20, [1]), (21, [1]), (22, [1]), (23, [1]), (24, [1])])
    for _ in range(40):
        yield case_of(r, [subsystem_op], 16)
    for _ in range(10):
        yield case_of(r, [subsystem_op], 16, fail=True)


def edge_helpers():
    yield dict(seed=1, script=[(0, [])])
    yield dict(seed=2, script=[(37, [m]) for m in (0, 1, 0x80000000, 0xFFFFFFFF, 0xF800, 0x7E0, 0x1F)]
               + [(38, [m]) for m in (0, 1, 0x80000000, 0xFFFFFFFF, 0xF800, 0x7E0, 0x1F)])


TESTS = [
    FnTest("CDisplay", ppc, host, edge=edge_display, rnd=lambda r: case_of(r, [display_op], 14)),
    FnTest("CRenderMemory", ppc, host, edge=edge_memory,
           rnd=lambda r: case_of(r, [memory_op, memory_op, lambda r: (r.choice([15, 16]), [])], 14,
                                 fail=r.random() < 0.3, with_memory=True)),
    FnTest("engine subsystem Init/Close", ppc, host, edge=edge_subsystems,
           rnd=lambda r: case_of(r, [subsystem_op], 18, fail=r.random() < 0.3)),
    FnTest("BitsSet, ShiftFactor, CreateSineTable", ppc, host, edge=edge_helpers,
           rnd=lambda r: case_of(r, [helper_op], 8)),
    FnTest("particle object, link bases, wavelet loader", ppc, host, edge=edge_stage2,
           rnd=lambda r: case_of(r, [stage2_op], 20, fail=r.random() < 0.3, with_memory=r.random() < 0.7)),
    FnTest("engine start-up, mixed scripts", ppc, host,
           rnd=lambda r: case_of(r, [display_op, memory_op, subsystem_op, subsystem_op, helper_op, stage2_op], 24,
                                 fail=r.random() < 0.3)),
]
