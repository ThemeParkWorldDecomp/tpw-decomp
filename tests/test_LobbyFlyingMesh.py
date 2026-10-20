"""Differential test for src/lobby_flyingmesh.cpp (the lobby's flying meshes: CLBFlyingMeshInstance,
CLBFlyingMeshGroup, CLBFlyingMeshSystem's constructor, destructor and lists, CLBLobby::WindowMessagesFn and
PAR_GetEmitterParticleCount), against libtpw_unit_LobbyFlyingMesh.dylib (tests/unit_LobbyFlyingMesh.cpp).

A case is a world and a script. The world is the lobby (its generator's state and step), the LOBBYOBJECTS
option, and 27 ride sets whose meshes each hold a matrix; the lobby's real mesh system (src/lobby_meshes.cpp on
the host, the original on the PowerPC side) is built over them, so an instance's mesh is a real mesh instance and
its place and direction end up in a ride set's matrix, which the test reads. Only the ride and height field
loaders under the mesh system are fakes (they would load files). A script is a list of operations: make the
flying mesh system, make groups, tick and show them, make, tick, unlist, relist and destroy instances (also
with the destructor's flag of a derived class, 0 and -1: no delete), visit another group with a shown instance,
list and unlist groups, clear the lists (also with a NULL entry in them), RemoveAll and RemoveHead on the
lists, destroy groups and the system, send the lobby window's messages, ask for emitters' particle counts.
After every operation the test compares the answer, the calls the fakes recorded, the system's group list,
every live group and every live instance with all its words (the goal direction of an instance that never
ticked is not defined and is left out), the 27 matrices bit for bit (any NaN as one value), the two lobbies'
words, the lobby's generator and operator new's and delete's counts.

The harness runs the same bookkeeping on both sides (the Runner below): which groups are alive and listed, which
instances are alive, listed or only alive, so that a script never puts an object in a list twice or uses a
dead one. Instances are picked at run time by their number among those alive, so the choice does not depend on
how many a group made. Each object gets a label for the call log as soon as it is seen, so that a missing or
misplaced delete shows.

The lobby's random numbers are the original's own GetRand (and the group's inline step) on the PowerPC side. The
fused multiply-adds of the original run under HardwareRounding (copied from test_LobbyIslandsTick.py): Unicorn
rounds them twice.
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import test_FSSystem as FS
import test_LobbyMeshes as LM
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyFlyingMesh"

MASK = 0xFFFFFFFF
SETS = 28
FN = dict(
    inst_ctor=0x100AA5D4, inst_dtor=0x100AA9DC, inst_tick=0x100AAA80, grp_ctor=0x100AAE1C, grp_dtor=0x100AB008,
    grp_list_dtor=0x100AB100, grp_tick=0x100AB1A8, grp_show=0x100AB2FC, grp_clear=0x100AB3B4, grp_register=0x100AB464,
    grp_unregister=0x100AB4D8, sys_ctor=0x100AB578, sys_dtor=0x100AB620, sys_list_dtor=0x100AB6EC, sys_close=0x100AB874,
    sys_wmf=0x100AB8AC, sys_clear=0x100AB8F0, sys_register=0x100AB99C, sys_unregister=0x100ABA0C,
    g_ct=0x100ABAAC, g_dt=0x100ABB1C, g_find=0x100ABBD4, g_removeall=0x100ABC64, g_removeat=0x100ABD20,
    g_removehead=0x100ABE0C, g_addtail=0x100AC444, i_ct=0x100ABEE8, i_dt=0x100ABF5C, i_find=0x100AC018,
    i_removeall=0x100AC0B0, i_removeat=0x100AC170, i_removehead=0x100AC264, i_addtail=0x100AC348,
    lobby_wmf=0x100A6484, par=0x100BA170, get_rand=0x100AA98C)
# Callees outside the unit.
LOBBY_SYSTEM_CTOR, LOBBY_SYSTEM_DTOR, DEFAULT_HANDLER, MAIL_ICON = 0x100A56C8, 0x100A5744, 0x101DE318, 0x101E3698
# Globals: the lobby, the mesh system, the flying mesh system, the options table (LOBBYOBJECTS is the word at
# +0x74), the particle system's running flag and its block, the ride set tables.
G_LOBBY, G_MESHES, G_FLYING, G_OPTIONS = 0x102E4888, 0x102E48D0, 0x102E48D4, 0x102B4DA8
G_PAR_RUNNING, G_PAR_BLOCK, G_SETS, G_HEIGHTFIELD = 0x102E51FC, 0x102EDD68, 0x102C79A4, 0x102C6C74
LABEL_GROUP, LABEL_INSTANCE = 120, 200
CODE_RANGES = [(FN["inst_ctor"], FN["inst_ctor"] + 848), (FN["inst_tick"], FN["inst_tick"] + 872)]

MESSAGES = [0x10003, 0x10004, 0x10005, 0x14, 0x11, 0x22, 0x1000B, 0x10002, 0x10006, 0x13, 0x15, 0x21, 0x23, 0, -1,
            0x7FFFFFFF, -0x80000000, 0x1000A, 0x1000C, 0x12, 0x1001, 0x10007]
ARGS = [0, 1, 2, 3, -1, -2, 0x12345678, -0x7FFFFFFF, 0x7FFFFFFF, 0x10000, 0xFFFF, -0x80000000, 0x20010, 4, 7]
FOLDERS = ["data:lobby:globe", "DATA:Lobby:Globe", "data:lobby:fantasy", "data:lobby"]
NAMES = ["bird", "Bird", "cloud", "hot air balloon", "b"]


def fb(x):
    """A float32's bits."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bf(w):
    """The float32 with the bits w."""
    return struct.unpack(">f", struct.pack(">I", w & MASK))[0]


def canon(w):
    """A float's bits with every NaN the same."""
    return 0x7FC00000 if (w & 0x7F800000) == 0x7F800000 and (w & 0x007FFFFF) else w


def w32(e, a, v):
    """A big-endian word into the emulator's memory."""
    e.write(a, struct.pack(">I", v & MASK))


def f32(x):
    """x rounded to a float32."""
    return struct.unpack(">f", struct.pack(">f", x))[0]


def flight_pick(centre, extent, draw):
    """One coordinate of a random point of a box, as the code works it out (single precision, the difference fused)."""
    unit = f32(float(draw & 0x3FFFFFFF)) / 1073741824.0
    return f32(centre + float(LIBM.fmaf(f32(extent), f32(unit), -f32(0.5 * extent))))


def hash_text(text):
    """The FNV-1a hash of a latin-1 text (as the unit library's)."""
    h = 2166136261
    for ch in text.encode("latin-1"):
        h = ((h ^ ch) * 16777619) & MASK
    return h


# ----------------------------------------------------------------- the emulator's fused multiply-adds


LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Unicorn (QEMU 5.0) computes fmadds, fmsubs, fnmadds and fnmsubs in double precision and rounds the result
    to single, which can differ by one unit in the last place from the processor's single rounding of the exact
    result. A code hook over the functions that use them looks at each instruction before it runs and rewrites
    its target register just before the next one, with the C library's fmaf (one rounding)."""

    changed = 0    # results the correction replaced (all cases so far), for the notes
    seen = 0       # fused instructions it looked at

    def __init__(self, e):
        self.e, self.pending, self.hooks = e, None, []

    def __enter__(self):
        for begin, end in CODE_RANGES:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))
        return self

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
            if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                HardwareRounding.changed += 1
            uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        HardwareRounding.seen += 1
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = (rd, value)


# ----------------------------------------------------------------- the stand-ins


STUBS = None


def build_stubs(emu):
    """The mesh system's stand-ins from test_LobbyMeshes.py, with the lobby system base class and the loader
    answering as the unit library does, and the new callees: the window's default handler and the mail icon."""
    S = LM.build_stubs(emu)
    R = CL.recorder
    S.real.update(FN.values())
    S.internal(LOBBY_SYSTEM_CTOR, lambda e: None if e.fm_quiet else e.cl.rec("CLBLobbySystem", (e.cl.newlab(e.r(3)),)))
    S.internal(LOBBY_SYSTEM_DTOR, lambda e: None if e.fm_quiet else e.cl.rec("~CLBLobbySystem", (e.cl.lab(e.r(3)),)))
    S.internal(0x1005BC44, lambda e: (e.cl.rec("LoadLevelHeightfield", (T.s32(e.r(3)), T.s32(e.r(4)))), e.set_r(3, 1)))
    S.internal(0x1006A3EC, R("DestroyRideInstance", "u"))
    S.internal(DEFAULT_HANDLER, R("Window.Default", "Liii", lambda e, L: L.ret("Default", 5) & MASK))
    S.internal(MAIL_ICON, R("UpdateOutboxMailIcon"))
    std_delete = emu.stubs["__dl__FPv"]

    def delete(e):
        p = e.r(3)
        std_delete(e)
        if e.cl.lab(p) > 0:
            e.cl.rec("delete", (e.cl.lab(p),))
            e.cl.forget(p)

    S.imp("__dl__FPv", delete)
    return S


def setup(runner):
    """The standard stubs, the string vtables, the recorders, and the fake systems' vtable: slot 0x1c is
    WindowMessagesFn."""
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    if not FS.VT:
        FS.make_vtables(e)
    STUBS = build_stubs(e)
    e.fm_quiet = False

    def fake_message(e):
        L = e.cl
        L.rec("System.WindowMessagesFn", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))

    vtable = T.persist_alloc(e, 0x40)
    w32(e, vtable + 0x1C, T.fake_function(e, fake_message))
    e.fm_vtable = vtable


# ----------------------------------------------------------------- PowerPC side


class PpcSide:
    """The case's world as a memory image with the Mac's layout, the stand-ins active, and the calls under test.
    close() puts the globals it overwrote back."""

    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        del e.alloc_log[:]
        e.fm_quiet = True
        words = case["world"]
        self.saved = {}
        for address, size in ((G_LOBBY, 4), (G_MESHES, 4), (G_FLYING, 4), (G_HEIGHTFIELD, 4), (G_OPTIONS + 0x74, 4),
                              (G_PAR_RUNNING, 4), (G_PAR_BLOCK + 0x2C, 126 * 0x140), (G_SETS, 4 * SETS)):
            self.saved[address] = e.read(address, size)
        fixed = {}
        self.matrices = []
        for i in range(SETS):
            flags, c0, c4 = words[i * 3:i * 3 + 3]
            node = e.alloc(0xA0, b"\xa5" * 0xA0)
            e.write(node + 0x10, b"".join(struct.pack(">f", float(i * 100 + k)) for k in range(16)))
            header = e.alloc(0xB8, b"\xa5" * 0xB8)
            w32(e, header + 0x78, node)
            mesh = e.alloc(0x40, b"\xa5" * 0x40)
            w32(e, mesh + 4, header)
            rset = e.alloc(0xC8, b"\xa5" * 0xC8)
            e.write(rset + 4, struct.pack(">II", flags, mesh))
            e.write(rset + 0xC0, struct.pack(">II", c0, c4))
            w32(e, G_SETS + 4 * i, 0 if i == 0 else rset)
            if i == 0:
                w32(e, G_HEIGHTFIELD, rset)
            fixed[mesh], fixed[rset], fixed[node + 0x10] = 10 + i, 40 + i, 70 + i
            self.matrices.append(node + 0x10)
        w = words[SETS * 3:]
        w32(e, G_OPTIONS + 0x74, w[2])
        # the mesh system, built for real (quietly: the setup's events are not compared)
        self.system_meshes = e.lm_system = e.alloc(0x10, b"\xa5" * 0x10)
        e.write(self.system_meshes + 0xC, b"\0\0\0\0")
        fixed[self.system_meshes] = 5
        w32(e, G_MESHES, 0)
        self.windows, self.lobbies, self.fake = [], [], []
        for which in range(2):
            window = e.alloc(16, b"\xa5" * 16)
            fixed[window] = 3 + which
            lobby = e.alloc(0x38, b"\xa5" * 0x38)
            fixed[lobby] = 1 + which
            w32(e, lobby + 4, window)
            w32(e, lobby + 8, w[1])
            w32(e, lobby + 0x34, w[0] if which == 0 else w[0] ^ 0x5A5A5A5A)
            w32(e, lobby + 0x20, w[3 + which * 3])
            w32(e, lobby + 0x24, w[4 + which * 3])
            w32(e, lobby + 0x1C, w[5 + which * 3])
            systems = []
            for k in range(2):
                system = e.alloc(8, b"\xa5" * 8)
                w32(e, system, e.fm_vtable)
                fixed[system] = 6 + which * 2 + k
                systems.append(system)
            lst = e.alloc(0x10, b"\0" * 0x10)
            previous = 0
            for system in systems:
                node = e.alloc(12, struct.pack(">III", system, 0, previous))
                w32(e, (previous + 4) if previous else (lst + 4), node)
                previous = node
            w32(e, lst + 8, previous)
            w32(e, lst + 0xC, len(systems))
            w32(e, lobby + 0x18, lst)
            self.windows.append(window)
            self.lobbies.append(lobby)
        w32(e, G_LOBBY, self.lobbies[0])
        w32(e, G_FLYING, 0)
        w32(e, G_PAR_RUNNING, 0)
        e.cl = self.L = CL.PpcLog(fixed, case.get("q"))
        self.old = STUBS.activate(e)
        self.rounding = HardwareRounding(e)
        self.rounding.__enter__()
        self.call(LM.U["ctor"], self.system_meshes)
        w32(e, G_MESHES, self.system_meshes)
        e.fm_quiet = False
        del self.L.log[:]
        del e.alloc_log[:]

    def close(self):
        """Puts the stand-ins and the globals back."""
        e = self.e
        self.rounding.__exit__()
        CL.Stubs.deactivate(e, self.old)
        for address, data in self.saved.items():
            e.write(address, data)
        e.fm_quiet = False

    # ---- helpers
    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a if isinstance(a, T.F) else a & MASK for a in args])[0]

    def virtual(self, obj, slot, *args):
        e = self.e
        return self.call(e.u32(e.u32(e.u32(obj) + slot)), obj, *args)

    def text(self, value):
        return self.e.alloc(len(value) + 1, value.encode("latin-1") + b"\0")

    def point(self, bits):
        return self.e.alloc(12, struct.pack(">3I", *[b & MASK for b in bits]))

    def new(self, size):
        """operator new, as the original's code asks for it: counted, and filled with a byte the code must not use."""
        address = self.e.alloc(size, b"\xa5" * size)
        self.e.alloc_log.append(("new", address, size))
        return address

    def label(self, address, label):
        self.L.labels[address] = label

    def mark(self):
        return len(self.L.log)

    def log(self):
        return self.L.log

    # ---- the flying mesh system
    def system_new(self):
        e = self.e
        address = self.new(8)
        self.call(FN["sys_ctor"], address)
        w32(e, G_FLYING, address)
        return address

    def system_delete(self, flag):
        e = self.e
        self.virtual(e.u32(G_FLYING), 8, flag)
        w32(e, G_FLYING, 0)

    def system_register(self, group):
        self.virtual(self.e.u32(G_FLYING), 0x20, group)

    def system_unregister(self, group):
        self.virtual(self.e.u32(G_FLYING), 0x24, group)

    def system_clear(self):
        self.call(FN["sys_clear"], self.e.u32(G_FLYING))

    def system_frame(self, message, a, b):
        system = self.e.u32(G_FLYING)
        self.virtual(system, 0x18)
        self.virtual(system, 0x1C, message, a, b)

    def system_removeall(self):
        self.call(FN["g_removeall"], self.e.u32(self.e.u32(G_FLYING) + 4))

    def system_removehead(self):
        return self.call(FN["g_removehead"], self.e.u32(self.e.u32(G_FLYING) + 4))

    def system_groups(self):
        return self.walk(self.e.u32(self.e.u32(G_FLYING) + 4))

    def walk(self, lst):
        e, out, node = self.e, [], self.e.u32(lst + 4)
        while node:
            out.append(e.u32(node))
            node = e.u32(node + 4)
        return out

    # ---- groups
    def group_new(self, number, folder, name, position, direction, speed, limit):
        address = self.new(0x5C)
        self.call(FN["grp_ctor"], address, number, self.text(folder), self.text(name), self.point(position),
                  self.point(direction), T.F(bf(speed)), T.F(bf(limit)))
        return address

    def group_delete(self, group, flag):
        self.virtual(group, 8, flag)

    def group_tick(self, group):
        self.call(FN["grp_tick"], group)

    def group_show(self, group):
        self.call(FN["grp_show"], group)

    def group_clear(self, group):
        self.call(FN["grp_clear"], group)

    def group_register(self, group, instance):
        self.virtual(group, 0xC, instance)

    def group_unregister(self, group, instance):
        self.virtual(group, 0x10, instance)

    def group_removeall(self, group):
        self.call(FN["i_removeall"], self.e.u32(group + 0x58))

    def group_removehead(self, group):
        return self.call(FN["i_removehead"], self.e.u32(group + 0x58))

    def group_instances(self, group):
        return self.walk(self.e.u32(group + 0x58))

    # ---- instances
    def instance_new(self, group, folder, name, position, direction, speed, arrival):
        address = self.new(0x60)
        self.call(FN["inst_ctor"], address, group, self.text(folder), self.text(name), self.point(position),
                  self.point(direction), T.F(bf(speed)), T.F(bf(arrival)))
        return address

    def instance_delete(self, instance, flag):
        self.virtual(instance, 8, flag)

    def instance_tick(self, instance):
        self.call(FN["inst_tick"], instance)

    # ---- the lobby window and the particles
    def lobby_message(self, which, mode, window, message, a, b):
        e = self.e
        w32(e, G_LOBBY, (self.lobbies[0], self.lobbies[1], 0)[mode])
        answer = self.call(FN["lobby_wmf"], self.lobbies[which], self.windows[window], message, a, b)
        w32(e, G_LOBBY, self.lobbies[0])
        return T.s32(answer)

    def particles(self, running, in_use, counts):
        e = self.e
        w32(e, G_PAR_RUNNING, running)
        e.write(G_PAR_BLOCK + 0x2C, b"\xa5" * (126 * 0x140))
        for i, (use, count) in enumerate(zip(in_use, counts)):
            e.write(G_PAR_BLOCK + 0x2C + i * 0x140, struct.pack(">H", use & 0xFFFF))
            e.write(G_PAR_BLOCK + 0x2C + i * 0x140 + 8, struct.pack(">H", count & 0xFFFF))

    def particle_count(self, emitter):
        return T.s32(self.call(FN["par"], emitter))

    # ---- the state
    def text_of(self, string):
        e = self.e
        data = e.u32(string + 8)
        return hash_text(e.cstr(data, limit=400)) if data else 0

    def group_state(self, group):
        e = self.e
        out = [T.s32(e.u32(group + 4)), T.s32(e.u32(group + 8)), T.s32(e.u32(group + 0xC)),
               self.text_of(group + 0x10), self.text_of(group + 0x24)]
        out += [canon(e.u32(group + 0x38 + 4 * k)) for k in range(6)]
        out += [canon(e.u32(group + 0x50)), canon(e.u32(group + 0x54))]
        lst = e.u32(group + 0x58)
        out.append(T.s32(e.u32(lst + 0xC)))
        return out + self.walk(lst)

    def instance_state(self, instance, ticked):
        e = self.e
        mesh = e.u32(instance + 8)
        meshes = self.walk(e.u32(e.u32(G_MESHES) + 0xC))
        out = [e.u32(instance + 4), meshes.index(mesh) if mesh in meshes else -1, e.u32(mesh + 4), T.s32(e.u32(mesh + 8)),
               self.text_of(mesh + 0xC), self.text_of(mesh + 0x20), T.s32(e.u32(instance + 0xC))]
        words = [canon(e.u32(instance + 0x10 + 4 * k)) for k in range(17)]
        goal_direction = [canon(e.u32(instance + 0x54 + 4 * k)) if ticked else 0 for k in range(3)]
        return out + words + goal_direction

    def world_state(self):
        e = self.e
        out = []
        for matrix in self.matrices:
            out += [canon(v) for v in struct.unpack(">16I", e.read(matrix, 64))]
        for lobby in self.lobbies:
            out += [self.L.lab(e.u32(lobby + 4)), T.s32(e.u32(lobby + 0x20)), T.s32(e.u32(lobby + 0x24)),
                    T.s32(e.u32(lobby + 0x1C)), e.u32(lobby + 0x34)]
        out.append(sum(1 for kind, address, _ in e.alloc_log if kind == "new"))
        out.append(sum(1 for kind, address, _ in e.alloc_log if kind == "delete" and address))
        return out


# ----------------------------------------------------------------- host side


class HostSide:
    """The same operations on the unit library (its objects are addresses, as on the PowerPC side)."""

    def __init__(self, lib, case):
        self.lib = lib
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
        CL.host_begin(lib, {}, case.get("q"))
        words = (C.c_uint32 * len(case["world"]))(*[v & MASK for v in case["world"]])
        F = FS.F
        F(lib, "tpw_fm_begin", None, C.POINTER(C.c_uint32))(words)

    def fn(self, name, res, *args):
        return FS.F(self.lib, name, res, *args)

    def close(self):
        pass

    @staticmethod
    def pt(bits):
        return (C.c_uint32 * 3)(*[b & MASK for b in bits])

    def label(self, address, label):
        self.fn("tpw_fm_label", None, C.c_int32, C.c_void_p)(label, address)

    def mark(self):
        return C.c_int32.in_dll(self.lib, "tpw_cl_n").value

    def log(self):
        return CL.host_end(self.lib)

    def system_new(self):
        return self.fn("tpw_fm_system_new", C.c_size_t)()

    def system_delete(self, flag):
        self.fn("tpw_fm_system_delete", None, C.c_int32)(flag)

    def system_register(self, group):
        self.fn("tpw_fm_system_register", None, C.c_size_t)(group)

    def system_unregister(self, group):
        self.fn("tpw_fm_system_unregister", None, C.c_size_t)(group)

    def system_clear(self):
        self.fn("tpw_fm_system_clear", None)()

    def system_frame(self, message, a, b):
        self.fn("tpw_fm_system_frame", None, C.c_int32, C.c_int32, C.c_int32)(T.s32(message & MASK), T.s32(a & MASK), T.s32(b & MASK))

    def system_removeall(self):
        self.fn("tpw_fm_system_removeall", None)()

    def system_removehead(self):
        return self.fn("tpw_fm_system_removehead", C.c_size_t)()

    def system_groups(self):
        buffer = (C.c_size_t * 64)()
        n = self.fn("tpw_fm_system_groups", C.c_int32, C.POINTER(C.c_size_t))(buffer)
        return list(buffer[:n])

    def group_new(self, number, folder, name, position, direction, speed, limit):
        return self.fn("tpw_fm_group_new", C.c_size_t, C.c_int32, C.c_char_p, C.c_char_p, C.POINTER(C.c_uint32),
                       C.POINTER(C.c_uint32), C.c_uint32, C.c_uint32)(
            T.s32(number & MASK), folder.encode("latin-1"), name.encode("latin-1"), self.pt(position), self.pt(direction),
            speed & MASK, limit & MASK)

    def group_delete(self, group, flag):
        self.fn("tpw_fm_group_delete", None, C.c_size_t, C.c_int32)(group, flag)

    def group_tick(self, group):
        self.fn("tpw_fm_group_tick", None, C.c_size_t)(group)

    def group_show(self, group):
        self.fn("tpw_fm_group_show", None, C.c_size_t)(group)

    def group_clear(self, group):
        self.fn("tpw_fm_group_clear", None, C.c_size_t)(group)

    def group_register(self, group, instance):
        self.fn("tpw_fm_group_register", None, C.c_size_t, C.c_size_t)(group, instance)

    def group_unregister(self, group, instance):
        self.fn("tpw_fm_group_unregister", None, C.c_size_t, C.c_size_t)(group, instance)

    def group_removeall(self, group):
        self.fn("tpw_fm_group_removeall", None, C.c_size_t)(group)

    def group_removehead(self, group):
        return self.fn("tpw_fm_group_removehead", C.c_size_t, C.c_size_t)(group)

    def group_instances(self, group):
        buffer = (C.c_size_t * 64)()
        n = self.fn("tpw_fm_group_instances", C.c_int32, C.c_size_t, C.POINTER(C.c_size_t))(group, buffer)
        return list(buffer[:n])

    def instance_new(self, group, folder, name, position, direction, speed, arrival):
        return self.fn("tpw_fm_instance_new", C.c_size_t, C.c_size_t, C.c_char_p, C.c_char_p, C.POINTER(C.c_uint32),
                       C.POINTER(C.c_uint32), C.c_uint32, C.c_uint32)(
            group, folder.encode("latin-1"), name.encode("latin-1"), self.pt(position), self.pt(direction),
            speed & MASK, arrival & MASK)

    def instance_delete(self, instance, flag):
        self.fn("tpw_fm_instance_delete", None, C.c_size_t, C.c_int32)(instance, flag)

    def instance_tick(self, instance):
        self.fn("tpw_fm_instance_tick", None, C.c_size_t)(instance)

    def lobby_message(self, which, mode, window, message, a, b):
        return self.fn("tpw_fm_lobby_message", C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32)(
            which, mode, window, T.s32(message & MASK), T.s32(a & MASK), T.s32(b & MASK))

    def particles(self, running, in_use, counts):
        n = len(in_use)
        self.fn("tpw_fm_particles", None, C.c_int32, C.c_int32, C.POINTER(C.c_int32), C.POINTER(C.c_int32))(
            running, n, (C.c_int32 * max(n, 1))(*in_use), (C.c_int32 * max(n, 1))(*counts))

    def particle_count(self, emitter):
        return self.fn("tpw_fm_particle_count", C.c_int32, C.c_uint32)(emitter & MASK)

    def group_state(self, group):
        buffer = (C.c_int64 * 128)()
        n = self.fn("tpw_fm_group_state", C.c_int32, C.c_size_t, C.POINTER(C.c_int64))(group, buffer)
        return list(buffer[:n])

    def instance_state(self, instance, ticked):
        buffer = (C.c_int64 * 64)()
        n = self.fn("tpw_fm_instance_state", C.c_int32, C.c_size_t, C.c_int32, C.POINTER(C.c_int64))(instance, int(ticked), buffer)
        return list(buffer[:n])

    def world_state(self):
        buffer = (C.c_int64 * 1024)()
        n = self.fn("tpw_fm_world_state", C.c_int32, C.POINTER(C.c_int64))(buffer)
        return list(buffer[:n])


# ----------------------------------------------------------------- the same bookkeeping on both sides


class Obj:
    def __init__(self, address):
        self.address = address
        self.alive = True
        self.listed = True      # in its owner's list (the system's for a group, the group's for an instance)
        self.owner = None       # an instance's group
        self.ticked = False
        self.shown = False      # a group: Show has run (the model of its mShown)


class Runner:
    """Runs a script on one side, keeping the model of which groups and instances are alive and listed. The
    operations that pick an instance take a number and pick the n-th suitable one."""

    def __init__(self, side):
        self.side = side
        self.system = False
        self.groups, self.instances, self.slot_of = [], [], {}

    # ---- objects
    def add_group(self, address):
        group = Obj(address)
        self.groups.append(group)
        self.slot_of[("g", address)] = len(self.groups) - 1
        self.side.label(address, LABEL_GROUP + len(self.groups) - 1)
        return group

    def add_instance(self, address, owner):
        instance = Obj(address)
        instance.owner = owner
        self.instances.append(instance)
        self.slot_of[("i", address)] = len(self.instances) - 1
        self.side.label(address, LABEL_INSTANCE + len(self.instances) - 1)
        return instance

    def instance_at(self, address):
        slot = self.slot_of.get(("i", address))
        return self.instances[slot] if slot is not None else None

    def scan(self):
        """Gives the instances a group made on its own a slot and a label, in list order."""
        for group in self.groups:
            if group.alive:
                for address in self.side.group_instances(group.address):
                    if ("i", address) not in self.slot_of:
                        self.add_instance(address, group)

    def die(self, kind, obj):
        """An object ends: its address may be handed out again by the host's heap, so it is forgotten."""
        obj.alive = obj.listed = False
        self.slot_of.pop((kind, obj.address), None)

    def kill_listed(self, group):
        """The instances in the group's list end with it."""
        for address in self.side.group_instances(group.address):
            instance = self.instance_at(address)
            if instance is not None:
                self.die("i", instance)

    def pick(self, n, test):
        suitable = [instance for instance in self.instances if instance.alive and test(instance)]
        return suitable[n % len(suitable)] if suitable else None

    def group(self, index):
        if index < len(self.groups) and self.groups[index].alive:
            return self.groups[index]
        return None

    # ---- the operations
    def do(self, step):
        side, op = self.side, step[0]
        if op == "sys_new":
            if self.system:
                return -1
            side.system_new()
            self.system = True
        elif op == "sys_delete":
            if not self.system:
                return -1
            self.sys_ends()
            side.system_delete(step[1] if len(step) > 1 else 1)
            self.system = False
        elif op == "sys_clear":
            if not self.system:
                return -1
            self.sys_ends()
            side.system_clear()
        elif op == "sys_clear_null":
            # a NULL in the list: the clearing skips it (RemoveHead answers NULL)
            if not self.system:
                return -1
            side.system_register(0)
            self.sys_ends()
            side.system_clear()
        elif op == "sys_removeall":
            if not self.system:
                return -1
            for address in side.system_groups():
                self.groups[self.slot_of[("g", address)]].listed = False
            side.system_removeall()
        elif op == "sys_removehead":
            if not self.system:
                return -1
            listed = side.system_groups()
            head = side.system_removehead()
            if listed:
                self.groups[self.slot_of[("g", listed[0])]].listed = False
            return self.slot_of.get(("g", head), -2 if head == 0 else -3)
        elif op == "sys_frame":
            if not self.system:
                return -1
            side.system_frame(*step[1:])
        elif op == "group_new":
            if not self.system:
                return -1
            _, number, folder, name, position, direction, speed, limit = step
            self.add_group(side.group_new(number, folder, name, position, direction, speed, limit))
        elif op in ("group_delete", "group_tick", "group_show", "group_clear", "group_clear_null", "group_removeall",
                    "group_removehead", "sys_register", "sys_unregister"):
            group = self.group(step[1])
            if group is None or (op in ("group_delete", "sys_register", "sys_unregister") and not self.system):
                return -1
            if op == "group_delete":
                self.kill_listed(group)
                side.group_delete(group.address, step[2] if len(step) > 2 else 1)
                self.die("g", group)
            elif op == "group_tick":
                side.group_tick(group.address)
                self.scan()
                for address in side.group_instances(group.address):
                    self.instance_at(address).ticked = True
            elif op == "group_show":
                side.group_show(group.address)
            elif op in ("group_clear", "group_clear_null"):
                if op == "group_clear_null":
                    side.group_register(group.address, 0)
                self.kill_listed(group)
                side.group_clear(group.address)
            elif op == "group_removeall":
                for address in side.group_instances(group.address):
                    self.instance_at(address).listed = False
                side.group_removeall(group.address)
            elif op == "group_removehead":
                listed = side.group_instances(group.address)
                head = side.group_removehead(group.address)
                if listed:
                    self.instance_at(listed[0]).listed = False
                return self.slot_of.get(("i", head), -2 if head == 0 else -3)
            elif op == "sys_register":
                if group.listed:
                    return -1
                side.system_register(group.address)
                group.listed = True
            else:
                side.system_unregister(group.address)
                group.listed = False
        elif op == "inst_new":
            group = self.group(step[1])
            if group is None:
                return -1
            _, _, folder, name, position, direction, speed, arrival = step
            instance = self.add_instance(side.instance_new(group.address, folder, name, position, direction, speed, arrival), group)
            instance.listed = True
        elif op == "inst_tick":
            instance = self.pick(step[1], lambda i: True)
            if instance is None:
                return -1
            side.instance_tick(instance.address)
            instance.ticked = True
        elif op == "inst_delete":
            instance = self.pick(step[1], lambda i: i.owner.alive)
            if instance is None:
                return -1
            side.instance_delete(instance.address, step[2] if len(step) > 2 else 1)
            self.die("i", instance)
        elif op == "inst_unregister":
            instance = self.pick(step[1], lambda i: i.listed and i.owner.alive)
            if instance is None:
                return -1
            side.group_unregister(instance.owner.address, instance.address)
            instance.listed = False
        elif op == "inst_unregister_again":
            instance = self.pick(step[1], lambda i: not i.listed and i.owner.alive)
            if instance is None:
                return -1
            side.group_unregister(instance.owner.address, instance.address)
        elif op == "inst_register":
            instance = self.pick(step[1], lambda i: not i.listed and i.owner.alive)
            if instance is None:
                return -1
            side.group_register(instance.owner.address, instance.address)
            instance.listed = True
        elif op == "inst_visit":
            # A listed instance that has been shown is put in the list of a group that has not, which is shown (so
            # the group's Show finds a shown instance), and goes back to its own group's list.
            instance = self.pick(step[1], lambda i: i.listed and i.owner.alive and i.owner.shown)
            target = self.group(step[2])
            if instance is None or target is None or target is instance.owner or target.shown:
                return -1
            side.group_unregister(instance.owner.address, instance.address)
            side.group_register(target.address, instance.address)
            side.group_show(target.address)
            target.shown = True
            side.group_unregister(target.address, instance.address)
            side.group_register(instance.owner.address, instance.address)
        elif op == "lobby":
            return side.lobby_message(*step[1:])
        elif op == "particles":
            side.particles(step[1], step[2], step[3])
        elif op == "particle_count":
            return side.particle_count(step[1])
        else:
            raise ValueError(op)
        if op == "group_show":
            group.shown = True
        return 0

    def sys_ends(self):
        """The groups in the system's list end with it, and so do their instances."""
        for address in self.side.system_groups():
            if address == 0:
                continue
            group = self.groups[self.slot_of[("g", address)]]
            self.kill_listed(group)
            self.die("g", group)

    # ---- the state
    def state(self):
        side, out = self.side, []
        out.append(int(self.system))
        if self.system:
            listed = side.system_groups()
            out.append(len(listed))
            out += [self.slot_of.get(("g", address), -1) for address in listed]
        for group in self.groups:
            if group.alive:
                fields = side.group_state(group.address)
                count = fields[13]
                out += fields[:14] + [self.slot_of.get(("i", address), -1) for address in fields[14:14 + count]]
        for instance in self.instances:
            if instance.alive:
                fields = side.instance_state(instance.address, instance.ticked)
                # (its group by the model: the host may hand a dead group's address out again)
                out += [self.groups.index(instance.owner), int(fields[0] == instance.owner.address)] + fields[1:]
        return out + side.world_state()


def run_script(side, script):
    runner = Runner(side)
    answers, states, marks = [], [], []
    for step in script:
        answers.append(runner.do(step))
        states.append(runner.state())
        marks.append(side.mark())
    return dict(answers=answers, states=states, marks=marks)


def flying_ppc(e, case):
    side = PpcSide(e, case)
    try:
        result = run_script(side, case["script"])
        result["log"] = side.log()
        return result
    finally:
        side.close()


def flying_host(lib, case):
    side = HostSide(lib, case)
    try:
        result = run_script(side, case["script"])
    finally:
        log = side.log()
    result["log"] = log
    return result


# ----------------------------------------------------------------- the cases


def pick(r, *values):
    return r.choice(values)


def fl(r, lo=-300.0, hi=300.0):
    """A float's bits: mostly a plain number, now and then a special one."""
    k = r.random()
    if k < 0.06:
        return fb(r.choice([0.0, -0.0, 1.0, -1.0, 0.5, 100.0]))
    if k < 0.9:
        return fb(r.uniform(lo, hi))
    if k < 0.96:
        return fb(r.uniform(-1e-3, 1e-3))
    return fb(r.choice([1e30, -1e30, 1e-30, 3e38]))


def size(r):
    """The size of a box: positive mostly, sometimes nothing."""
    k = r.random()
    if k < 0.1:
        return fb(0.0)
    if k < 0.15:
        return fb(-r.uniform(0, 100))
    return fb(r.uniform(0.0, 500.0))


def world(r, pct=None, units=None, seed=None):
    words = []
    for _ in range(SETS):
        words += [r.choice([0, 0x4000, 0x4000, 0x20, 0xFFFFBFFF, MASK, 0x4020]), r.getrandbits(32), r.getrandbits(32)]
    words.append(r.getrandbits(32) if seed is None else seed)
    words.append(fb(r.choice([0.0, 0.25, 0.5, 1.0, 1.0, 2.0, 5.0, r.uniform(0.0, 10.0), 100.0, 1e-30, -1.0])) if units is None else units)
    words.append(r.choice([0, 1, 10, 25, 50, 50, 75, 100, 100, 100, r.randrange(0, 101), 150, -5]) if pct is None else pct)
    for _ in range(2):
        words += [r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)]
    return words


def queues(r):
    sets = list(range(1, SETS))
    if r.random() < 0.5:
        r.shuffle(sets)
    return {"Instance": sets, "RideType": [r.choice([0, 1, 7, 300, MASK]) for _ in range(r.randrange(1, 6))],
            "Default": [r.choice([0, 1, 5, -1, 0x12345678 & MASK]) for _ in range(r.randrange(1, 6))]}


class Steps:
    """A script under construction. It knows how many groups there are and how many are alive, for the
    operations that name a group."""

    def __init__(self):
        self.steps = []
        self.alive = []         # per group: alive
        self.system = False

    def add(self, *step):
        self.steps.append(tuple(step))

    def start(self):
        self.system = True
        self.add("sys_new")

    def group(self, r=None, number=3, folder=FOLDERS[0], name=NAMES[0], position=None, direction=None, speed=None, limit=None):
        position = position if position is not None else [fb(0.0), fb(10.0), fb(0.0)]
        direction = direction if direction is not None else [fb(100.0), fb(20.0), fb(100.0)]
        self.add("group_new", number, folder, name, list(position), list(direction), fb(1.0) if speed is None else speed,
                 fb(500.0) if limit is None else limit)
        self.alive.append(True)

    def kill(self, g):
        self.alive[g] = False

    def live(self):
        return [g for g, a in enumerate(self.alive) if a]


def random_group(r, steps):
    steps.group(number=r.choice([-3, -1, 0, 1, 2, 3, 4, 5, 6, 8, 12]), folder=r.choice(FOLDERS), name=r.choice(NAMES),
                position=[fl(r, -600, 600) for _ in range(3)], direction=[size(r) for _ in range(3)],
                speed=fb(r.choice([0.0, 1.0, 2.0, r.uniform(0.0, 5.0), r.uniform(0.0, 5.0), -1.0, 50.0])),
                limit=fb(r.choice([500.0, 500.0, 500.0, 0.0, 1e30, 100.0, -1.0, 4.0])))


def random_script(r, length):
    steps = Steps()
    steps.start()
    for _ in range(r.randrange(1, 3)):
        random_group(r, steps)
    for _ in range(length):
        k = r.random()
        live = steps.live()
        g = r.choice(live) if live else 0
        if k < 0.17:
            steps.add("group_tick", g)
        elif k < 0.24:
            steps.add("inst_tick", r.randrange(40))
        elif k < 0.29:
            steps.add("group_show", g)
        elif k < 0.33 and len(steps.alive) < 4:
            random_group(r, steps)
        elif k < 0.38:
            steps.add("inst_new", g, r.choice(FOLDERS), r.choice(NAMES), [fl(r, -600, 600) for _ in range(3)],
                      [size(r) for _ in range(3)], fb(r.uniform(0.0, 5.0)), fb(r.choice([500.0, 0.0, 1e30, 100.0])))
        elif k < 0.42:
            steps.add("group_delete", g, r.choice([1, 1, 1, 0, -1]))
            if live:
                steps.kill(g)
        elif k < 0.45:
            steps.add(r.choice(["group_clear", "group_clear", "group_clear_null"]), g)
        elif k < 0.47:
            steps.add(r.choice(["sys_clear", "sys_clear", "sys_clear_null"]))
        elif k < 0.51:
            steps.add("inst_delete", r.randrange(40), r.choice([1, 1, 1, 0, -1]))
        elif k < 0.57:
            steps.add("inst_unregister", r.randrange(40))
        elif k < 0.60:
            steps.add("inst_unregister_again", r.randrange(40))
        elif k < 0.65:
            steps.add("inst_register", r.randrange(40))
        elif k < 0.69:
            steps.add("sys_unregister", r.randrange(max(len(steps.alive), 1)))
        elif k < 0.73:
            steps.add("sys_register", r.randrange(max(len(steps.alive), 1)))
        elif k < 0.75:
            steps.add("sys_removeall")
        elif k < 0.77:
            steps.add("sys_removehead")
        elif k < 0.79:
            steps.add("group_removeall", g)
        elif k < 0.82:
            steps.add("group_removehead", g)
        elif k < 0.85:
            steps.add("inst_visit", r.randrange(40), r.randrange(max(len(steps.alive), 1)))
        elif k < 0.88:
            steps.add("sys_frame", r.choice(MESSAGES), r.choice(ARGS), r.choice(ARGS))
        elif k < 0.92:
            steps.add("lobby", r.randrange(2), r.randrange(3), r.randrange(2), r.choice(MESSAGES), r.choice(ARGS), r.choice(ARGS))
        else:
            steps.add("inst_tick", r.randrange(40))
    if r.random() < 0.6:
        steps.add("sys_delete", r.choice([1, 1, 1, 0, -1]))
    return steps


def flying_case(r):
    seed = r.getrandbits(32)
    return dict(world=world(r, seed=seed), q=queues(r), script=random_script(r, r.randrange(6, 36)).steps)


def flying_edges():
    r = random.Random(151)
    # --- the maximum: number x option / 100, at least 1, at most number (1 for a number below 1)
    numbers = [-5, -1, 0, 1, 2, 3, 7, 10, 99, 100, 101, 1000, 0x7FFFFFFF, -0x80000000, 0x40000000, 21474837]
    options = [0, 1, 5, 33, 50, 99, 100, 101, -3, 1000, 0x7FFFFFFF, -0x80000000, 21474836]
    for number in numbers:
        for pct in options:
            s = Steps()
            s.start()
            s.group(number=number)
            yield dict(world=world(r, pct=pct), q=queues(r), script=s.steps)
    # --- how many instances a tick makes: the generator's low three bits run through every state; the maximum
    # stops it too
    for seed in range(8):
        for number in (1, 2, 3, 6, 12):
            s = Steps()
            s.start()
            s.group(number=number)
            s.add("group_tick", 0)
            s.add("group_tick", 0)
            s.add("group_tick", 0)
            yield dict(world=world(r, pct=100, units=fb(1.0), seed=seed | 0x12345600), q=queues(r), script=s.steps)
    # --- showing: before the instances exist, after, twice, with new ones made afterwards, a direct instance
    for order in ("show_first", "show_late", "show_twice", "show_mid"):
        s = Steps()
        s.start()
        s.group(number=5)
        s.group(number=3, folder=FOLDERS[1], name=NAMES[1])
        if order == "show_first":
            s.add("group_show", 0)
            s.add("group_tick", 0)
            s.add("group_tick", 0)
        elif order == "show_late":
            s.add("group_tick", 0)
            s.add("group_tick", 0)
            s.add("group_show", 0)
            s.add("group_tick", 0)
        elif order == "show_twice":
            s.add("group_tick", 0)
            s.add("group_show", 0)
            s.add("group_show", 0)
            s.add("inst_new", 0, FOLDERS[0], NAMES[0], [fb(0.0)] * 3, [fb(10.0)] * 3, fb(1.0), fb(500.0))
            s.add("group_show", 0)
        else:
            s.add("inst_new", 1, FOLDERS[2], NAMES[2], [fb(5.0), fb(6.0), fb(7.0)], [fb(30.0)] * 3, fb(2.0), fb(500.0))
            s.add("group_show", 1)
            s.add("inst_new", 1, FOLDERS[2], NAMES[2], [fb(5.0), fb(6.0), fb(7.0)], [fb(30.0)] * 3, fb(2.0), fb(500.0))
            s.add("group_tick", 1)
            s.add("group_show", 1)
        s.add("group_delete", 0)
        s.add("sys_delete")
        yield dict(world=world(r, pct=100, seed=r.getrandbits(32)), q=queues(r), script=s.steps)
    # --- the flight: boxes, speeds, goals and steps of every kind, each ticked a few times
    boxes = [([0.0, 10.0, 0.0], [100.0, 20.0, 100.0]), ([0.0, 0.0, 0.0], [0.0, 0.0, 0.0]), ([5.0, 5.0, 5.0], [0.0, 10.0, 0.0]),
             ([5.0, 5.0, 5.0], [10.0, 0.0, 0.0]), ([5.0, 5.0, 5.0], [0.0, 0.0, 10.0]), ([-300.0, 70.0, 250.0], [-50.0, -10.0, 40.0]),
             ([0.0, 0.0, 0.0], [1e-3, 1e-3, 1e-3]), ([1e6, 1e6, 1e6], [1e6, 1e6, 1e6]), ([0.0, 0.0, 0.0], [1e30, 1e30, 1e30]),
             ([0.0, 0.0, 0.0], [3.0e19, 4.0e19, 1.0e19])]
    for centre, extent in boxes:
        for speed in (0.0, 1.0, 3.5, -2.0, 1000.0):
            for arrival in (500.0, 0.0, 1e30, -1.0, 1.0):
                if r.random() < 0.55:
                    continue
                s = Steps()
                s.start()
                s.group(number=2, position=[fb(v) for v in centre], direction=[fb(v) for v in extent], speed=fb(speed), limit=fb(arrival))
                for _ in range(4):
                    s.add("group_tick", 0)
                yield dict(world=world(r, pct=100, units=fb(r.choice([0.5, 1.0, 2.0, 10.0, 100.0]))), q=queues(r), script=s.steps)
    for bits in (0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000, 0x00000001, 0x7F7FFFFF):
        for place in ("centre", "extent", "speed", "arrival", "units"):
            s = Steps()
            s.start()
            centre, extent, speed, arrival = [fb(0.0), fb(10.0), fb(0.0)], [fb(100.0), fb(20.0), fb(100.0)], fb(1.0), fb(500.0)
            units = fb(1.0)
            axis = r.randrange(3)
            if place == "centre":
                centre[axis] = bits
            elif place == "extent":
                extent[axis] = bits
            elif place == "speed":
                speed = bits
            elif place == "arrival":
                arrival = bits
            else:
                units = bits
            s.group(number=2, position=centre, direction=extent, speed=speed, limit=arrival)
            s.add("group_tick", 0)
            s.add("group_tick", 0)
            s.add("inst_new", 0, FOLDERS[0], NAMES[0], centre, extent, speed, arrival)
            s.add("inst_tick", 0)
            s.add("inst_tick", 2)
            yield dict(world=world(r, pct=100, units=units), q=queues(r), script=s.steps)
    # --- an instance next to its goal and far from it, at the edge of the arrival test (a box with no size puts
    # the goal where the instance is, so the squared distance is 0 and the limit decides)
    for arrival in (0.0, 1e-30, 1.0, 500.0, -0.0, 1e30):
        for speed in (0.0, 1.0):
            s = Steps()
            s.start()
            s.group(number=1, position=[fb(3.0), fb(4.0), fb(5.0)], direction=[fb(0.0)] * 3, speed=fb(speed), limit=fb(arrival))
            s.add("group_tick", 0)
            s.add("group_tick", 0)
            yield dict(world=world(r, pct=100, units=fb(1.0)), q=queues(r), script=s.steps)
    # --- an instance that lands exactly on its goal in one tick, so that the squared distance is 0 and the arrival
    # limit is 0: the goal is kept (0 < 0 is false). The box is flat in x and z, so the flight is along y; the
    # lobby's step is the whole distance and the speed 1, and the centre is where the subtraction is exact.
    # (The cases are worked out here from the generator's first six draws; one that does not land is left out.)
    landed = 0
    for seed in range(0x00000000, 0x00000000 + 0x3000000, 0x1234567):
        for extent in (2.0, 0.5, 8.0):
            draws, state = [], seed
            for _ in range(6):
                state = (state * 1664525 + 1013904223) & MASK
                draws.append(state)
            py, gy = flight_pick(100.0, extent, draws[1]), flight_pick(100.0, extent, draws[4])
            dy = f32(gy - py)
            if dy == 0.0 or f32(py + dy) != gy:
                continue
            landed += 1
            s = Steps()
            s.start()
            s.group(number=1)
            s.add("inst_new", 0, FOLDERS[0], NAMES[0], [fb(0.0), fb(100.0), fb(0.0)], [fb(0.0), fb(extent), fb(0.0)], fb(1.0), fb(0.0))
            s.add("inst_tick", 0)
            s.add("inst_tick", 0)
            yield dict(world=world(r, units=fb(abs(dy)), seed=seed), q=queues(r), script=s.steps)
    assert landed >= 6, landed
    # --- the lists: instances and groups unlisted, listed again, listed twice, destroyed by hand and with the group
    s = Steps()
    s.start()
    s.group(number=4)
    s.group(number=4, folder=FOLDERS[1], name=NAMES[1])
    for _ in range(3):
        s.add("group_tick", 0)
        s.add("group_tick", 1)
    s.add("inst_unregister", 0)
    s.add("inst_unregister", 3)
    s.add("inst_unregister_again", 0)
    s.add("inst_tick", 0)
    s.add("inst_register", 0)
    s.add("inst_register", 1)
    s.add("inst_register", 5)
    s.add("inst_delete", 2)
    s.add("inst_delete", 0)
    s.add("sys_unregister", 0)
    s.add("sys_unregister", 0)
    s.add("group_tick", 0)
    s.add("sys_register", 0)
    s.add("sys_register", 0)
    s.add("group_clear", 1)
    s.add("group_tick", 1)
    s.add("sys_clear")
    s.add("sys_new")
    yield dict(world=world(r, pct=100, seed=0x00000007), q=queues(r), script=s.steps)
    # --- a group that the system's end does not reach (unlisted), then the system's end
    s = Steps()
    s.start()
    s.group(number=3)
    s.group(number=3)
    s.add("group_tick", 0)
    s.add("group_tick", 1)
    s.add("sys_unregister", 1)
    s.add("inst_unregister", 0)
    s.add("sys_delete")
    s.add("inst_tick", 0)
    s.add("inst_delete", 0)
    yield dict(world=world(r, pct=100, seed=0x00000003), q=queues(r), script=s.steps)
    # --- the system made and destroyed with nothing in it; its virtual members
    s = Steps()
    s.add("sys_frame", 1, 2, 3)
    s.start()
    s.add("sys_frame", 1, 2, 3)
    s.add("sys_clear")
    s.add("sys_delete")
    s.add("sys_new")
    s.add("sys_new")
    s.add("sys_delete")
    yield dict(world=world(r), q=queues(r), script=s.steps)
    # --- one mesh asked for again and again, in other letter cases: the lobby's mesh system shares its type
    s = Steps()
    s.start()
    s.group(number=3, folder=FOLDERS[0], name=NAMES[0])
    s.group(number=3, folder=FOLDERS[1], name=NAMES[1])
    s.group(number=3, folder=FOLDERS[0], name=NAMES[2])
    for _ in range(3):
        s.add("group_tick", 0)
        s.add("group_tick", 1)
        s.add("group_tick", 2)
    s.add("sys_delete")
    yield dict(world=world(r, pct=100, seed=0x00000005), q=queues(r), script=s.steps)
    # --- the destructors with the flag a derived class's destructor passes (0 and below: no delete)
    for flag in (1, 0, -1):
        s = Steps()
        s.start()
        s.group(number=3)
        s.group(number=2, folder=FOLDERS[1], name=NAMES[1])
        s.add("group_tick", 0)
        s.add("group_tick", 1)
        s.add("inst_delete", 0, flag)
        s.add("inst_delete", 1, flag)
        s.add("group_delete", 1, flag)
        s.add("group_tick", 0)
        s.add("group_delete", 0, flag)
        s.add("group_new", 2, FOLDERS[0], NAMES[3], [fb(1.0)] * 3, [fb(50.0)] * 3, fb(1.0), fb(500.0))
        s.add("group_tick", 2)
        s.add("sys_delete", flag)
        yield dict(world=world(r, pct=100, seed=0x00000009), q=queues(r), script=s.steps)
    # --- a shown instance in a group that is not shown yet, when that group is shown
    for seed in (0x00000003, 0x00000007, 0x12345678):
        s = Steps()
        s.start()
        s.group(number=6)
        s.group(number=4, folder=FOLDERS[1], name=NAMES[1])
        s.group(number=2, folder=FOLDERS[2], name=NAMES[2])
        s.add("group_show", 0)
        s.add("group_tick", 0)
        s.add("group_tick", 0)
        s.add("group_tick", 1)
        s.add("inst_visit", 0, 1)
        s.add("inst_visit", 1, 1)
        s.add("inst_visit", 0, 2)
        s.add("inst_visit", 0, 0)
        s.add("group_tick", 1)
        s.add("group_clear", 1)
        s.add("sys_delete")
        yield dict(world=world(r, pct=100, seed=seed), q=queues(r), script=s.steps)
    # --- a NULL in a list when it is cleared; RemoveAll and RemoveHead, on empty lists and full ones
    s = Steps()
    s.start()
    s.group(number=4)
    s.group(number=4, folder=FOLDERS[1], name=NAMES[1])
    s.add("group_clear_null", 0)
    s.add("group_tick", 0)
    s.add("group_tick", 0)
    s.add("group_clear_null", 0)
    s.add("group_removehead", 0)
    s.add("group_removeall", 0)
    s.add("group_tick", 0)
    s.add("group_tick", 1)
    s.add("group_removehead", 0)
    s.add("inst_register", 0)
    s.add("group_removeall", 1)
    s.add("inst_register", 1)
    s.add("inst_register", 0)
    s.add("sys_removehead")
    s.add("sys_removeall")
    s.add("sys_removehead")
    s.add("sys_register", 1)
    s.add("sys_register", 0)
    s.add("group_tick", 1)
    s.add("sys_clear_null")
    s.add("sys_clear_null")
    s.add("group_new", 2, FOLDERS[0], NAMES[0], [fb(0.0)] * 3, [fb(5.0)] * 3, fb(1.0), fb(500.0))
    s.add("sys_removeall")
    s.add("sys_delete")
    yield dict(world=world(r, pct=100, seed=0x00000007), q=queues(r), script=s.steps)
    for _ in range(40):
        yield flying_case(r)


# ---- the lobby window's messages


def lobby_case(r):
    s = Steps()
    for _ in range(r.randrange(1, 8)):
        s.add("lobby", r.randrange(2), r.randrange(3), r.randrange(2), r.choice(MESSAGES), r.choice(ARGS) if r.random() < 0.8 else r.getrandbits(32) - (1 << 31),
              r.choice(ARGS))
    return dict(world=world(r), q=queues(r), script=s.steps)


def lobby_edges():
    r = random.Random(152)
    for which in range(2):
        for mode in range(3):
            for message in MESSAGES:
                for a in ARGS:
                    if r.random() < 0.55:
                        continue
                    s = Steps()
                    s.add("lobby", which, mode, r.randrange(2), message, a, r.choice(ARGS))
                    yield dict(world=world(r), q=queues(r), script=s.steps)
    # the bits of the word at 0x1c, set and cleared in turn, from every starting value
    for start in (0, 1, 2, 4, 7, MASK, 0x80000000, 0x12345678):
        s = Steps()
        for message in (0x10005, 0x10004):
            for a in (0, 1, 2, 3, -1, 1, 2, 0, 3):
                s.add("lobby", 0, 0, 0, message, a, 0)
        s.add("lobby", 0, 0, 0, 0x22, 0, 0)
        w = world(r)
        w[SETS * 3 + 5] = start
        yield dict(world=w, q=queues(r), script=s.steps)
    # the size message: halves of the word
    for a in (0, 1, 0x10000, 0x3000200, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x0001FFFF):
        s = Steps()
        s.add("lobby", 0, 0, 0, 0x10003, a, 0)
        s.add("lobby", 1, 2, 1, 0x10003, a, 0)
        yield dict(world=world(r), q=queues(r), script=s.steps)


# ---- the particle counts


def particles_case(r):
    s = Steps()
    count = r.randrange(0, 126)
    in_use = [r.choice([0, 0, 1, -1, 0x8000, 0x7FFF, 0xA5A5, 2, 0x10000]) for _ in range(count)]
    counts = [r.choice([0, 1, 5, 0xFFFF, 0x8000, 0x7FFF, 100, r.getrandbits(16)]) for _ in range(count)]
    s.add("particles", r.choice([0, 1, 1, 1, 2, -1]), in_use, counts)
    for _ in range(r.randrange(1, 10)):
        s.add("particle_count", (r.getrandbits(16) << 16 if r.random() < 0.3 else 0) | r.randrange(0, 126))
    if r.random() < 0.3:
        s.add("particles", r.choice([0, 1]), [], [])
        s.add("particle_count", r.randrange(0, 126))
    return dict(world=world(r), q=queues(r), script=s.steps)


def particles_edges():
    r = random.Random(153)
    for running in (0, 1, 2, -1, 0x100):
        for use in (0, 1, -1, 0x8000, 0xFFFF, 0x7FFF, 0x10000, 0xA5A5):
            for particles in (0, 1, 0xFFFF, 0x8000, 0x7FFF, 0x10000):
                s = Steps()
                s.add("particles", running, [use] * 3, [particles] * 3)
                for emitter in (0, 1, 2, 3, 0x10001, 0xFFFF0002, 0x00010000, 0x70005):
                    s.add("particle_count", emitter)
                yield dict(world=world(r), q=queues(r), script=s.steps)
    s = Steps()
    s.add("particle_count", 0)
    s.add("particle_count", 125)
    s.add("particles", 1, [1] * 126, list(range(126)))
    for emitter in (0, 125, 0x1007D, 0xFFFF007D, 77):
        s.add("particle_count", emitter)
    yield dict(world=world(r), q=queues(r), script=s.steps)


TESTS = [
    FnTest("CLBFlyingMeshGroup, CLBFlyingMeshInstance and CLBFlyingMeshSystem (scripts)", flying_ppc, flying_host,
           edge=flying_edges, rnd=flying_case),
    FnTest("CLBLobby::WindowMessagesFn", flying_ppc, flying_host, edge=lobby_edges, rnd=lobby_case),
    FnTest("PAR_GetEmitterParticleCount", flying_ppc, flying_host, edge=particles_edges, rnd=particles_case),
]
