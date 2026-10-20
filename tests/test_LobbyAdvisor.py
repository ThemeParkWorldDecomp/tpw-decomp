"""Differential test for src/lobby_advisor.cpp (CLBScreenComponentAdvisor and its list of
responses), against libtpw_unit_LobbyAdvisor.dylib.

A case is a script over two advisor screens: Init, Tick, Add, ClearAll, the timeout line,
Show and Hide, Close, with the advisor talking or silent and a clock the test sets. After
every operation the answer, the advisor calls recorded, both screens' state (visible, the
waiting lines in order, the timeout fields) and how many blocks operator new and delete
have handled. A script may end by destroying a screen: the list frees its nodes, the lines
still waiting are left, the base class destructor is called.

On the PowerPC side the two screens are built as CLBScreenSystem::Init builds its advisor in
place (the vtables, a zero "visible" word, an empty list) over memory full of 0xa5.
"""
import ctypes as C
import random
import struct

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyAdvisor"

MINUS = 0xFFFFFFFF
w32 = FS.w32
F = FS.F
ADVISOR = dict(Init=0x100A6C98, Tick=0x100A6CE8, Render=0x100A6E10, Close=0x100A6E4C, WindowMessagesFn=0x100A6EA8,
               ProcessCancel=0x100A6EF0, Show=0x100A6F38, Hide=0x100A6FF4, Add=0x100A7074, ClearAll=0x100A7128,
               SetTimeoutResponse=0x100A71C8, dtor=0x100A7234, list_dtor=0x100A72F4, list_base_dtor=0x100A7398,
               RemoveAll=0x100A7448, RemoveHead=0x100A74FC, RemoveTail=0x100A75D4, AddHead=0x100A76AC, IsVisible=0x100A6FB8)
ADVISOR_VTABLE, LIST_VTABLE = 0x1029E474, 0x1029E4C0
BASE_DTOR = 0x100B6C2C
G_SCREENS, G_SPEAKING = 0x102E49B8, 0x102B354C + 0xC
SKIPPED = "(skipped)"


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(ADVISOR.values())
    S.imp("LbTime_GetClock__Fv", R("LbTime_GetClock", "", lambda e, L: L.ret("Clock", 0)))
    S.internal(0x1000887C, R("ADV_Call", "i"))
    S.internal(0x10008DD4, R("ADV_Slap"))
    S.internal(BASE_DTOR, R("~CLBScreenComponent"))
    return S


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        del e.alloc_log[:]
        self.advisors, self.destroyed = [], [False, False]
        for _ in range(2):
            block = e.alloc(0x2C, b"\xa5" * 0x2C)
            e.write(block, struct.pack(">II", ADVISOR_VTABLE, 0))
            e.write(block + 8, struct.pack(">IIII", LIST_VTABLE, 0, 0, 0))
            self.advisors.append(block)
        self.system = e.alloc(0x1C)
        w32(e, self.system + 0x14, self.advisors[0])
        w32(e, G_SCREENS, self.system)
        w32(e, G_SPEAKING, 0)

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def virtual(self, obj, slot, *args):
        e = self.e
        return self.call(e.u32(e.u32(e.u32(obj) + slot)), obj, *args)

    def op(self, op, a):
        e, advisor = self.e, self.advisors[a[0] & 1]
        if op not in (11, 12) and self.destroyed[a[0] & 1]:
            return -98
        if op == 1:
            self.virtual(advisor, 0xC)
        elif op == 2:
            self.virtual(advisor, 0x10)
        elif op == 3:
            self.call(ADVISOR["Add"], advisor, a[1], a[2])
        elif op == 4:
            self.call(ADVISOR["ClearAll"], advisor)
        elif op == 5:
            self.call(ADVISOR["SetTimeoutResponse"], advisor, a[1], a[2])
        elif op == 6:
            self.virtual(advisor, 0x24)
        elif op == 7:
            self.virtual(advisor, 0x28)
        elif op == 8:
            return T.s32(self.virtual(advisor, 0x2C))
        elif op == 9:
            self.virtual(advisor, 0x18)
        elif op == 10:
            self.virtual(advisor, 0x14)
            self.virtual(advisor, 0x1C, a[1], a[2], a[3])
            return self.virtual(advisor, 0x20)
        elif op == 11:
            w32(e, G_SPEAKING, a[1])
        elif op == 12:
            w32(e, self.system + 0x14, self.advisors[a[1] & 1])
        elif op == 13:
            w32(e, advisor + 4, a[1])
        elif op == 14:
            self.call(ADVISOR["dtor"], advisor, 0)
            self.destroyed[a[0] & 1] = True
        else:
            return -99
        return 0

    def state(self):
        e, out = self.e, []
        for advisor, destroyed in zip(self.advisors, self.destroyed):
            if destroyed:
                continue
            s = lambda o: T.s32(e.u32(advisor + o))   # noqa: E731
            out += [s(4), s(0x14)]
            node, n = e.u32(advisor + 0xC), 0
            while node and n < 40:
                out.append(T.s32(e.u32(e.u32(node))))
                node, n = e.u32(node + 4), n + 1
            out += [s(0x18), e.u32(advisor + 0x1C), e.u32(advisor + 0x20), s(0x24), s(0x28)]
        out.append(sum(1 for kind, address, _ in e.alloc_log if kind == "new"))
        out.append(sum(1 for kind, address, _ in e.alloc_log if kind == "delete" and address))
        return out


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = build_stubs(runner.emu)


def advisor_ppc(e, case):
    saved = STUBS.activate(e)
    try:
        side = PpcSide(e, case)
        answers, states, marks = [], [], []
        for op, a in case["script"]:
            answers.append(side.op(op, list(a)))
            states.append(side.state())
            marks.append(len(side.L.log))
        return dict(answers=answers, states=states, marks=marks, log=side.L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def advisor_host(lib, case):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    lib["tpw_la_begin"]()
    CL.host_begin(lib, {}, case.get("q"))
    op_fn = F(lib, "tpw_la_op", C.c_int64, C.c_int32, C.POINTER(C.c_int32))
    state_fn = F(lib, "tpw_la_state", C.c_int32, C.POINTER(C.c_int64))
    answers, states, marks = [], [], []
    values = (C.c_int64 * 256)()
    try:
        for op, a in case["script"]:
            answers.append(op_fn(op, (C.c_int32 * 8)(*[T.s32(v & MINUS) for v in a])))
            states.append(list(values[:state_fn(values)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(answers=answers, states=states, marks=marks, log=log)


def op(code, *a):
    return (code, (list(a) + [0] * 8)[:8])


def random_script(r, length):
    script = [op(1, 0), op(1, 1)] if r.random() < 0.9 else []
    for _ in range(length):
        k = r.random()
        which = r.randrange(2)
        if k < 0.04:
            script.append(op(1, which))
        elif k < 0.40:
            script.append(op(2, which))
        elif k < 0.60:
            script.append(op(3, which, r.randrange(-2, 60), r.choice([0, 0, 0, 1, 5])))
        elif k < 0.65:
            script.append(op(4, which))
        elif k < 0.73:
            script.append(op(5, which, r.randrange(100), r.choice([0, 1, 100, 5000, MINUS, 0x7FFFFFFF])))
        elif k < 0.79:
            script.append(op(r.choice([6, 7, 8]), which))
        elif k < 0.82:
            script.append(op(9, which))
        elif k < 0.84:
            script.append(op(10, which, 1, 2, 3))
        elif k < 0.93:
            script.append(op(11, 0, r.choice([0, 0, 1, 7])))
        elif k < 0.96:
            script.append(op(12, 0, r.randrange(2)))
        else:
            script.append(op(13, which, r.choice([0, 1, 2, -1])))
    for which in range(2):
        if r.random() < 0.3:
            script.append(op(14, which))
    if r.random() < 0.3:
        script.append(op(2, r.randrange(2)))
    return script


def advisor_case(r):
    return dict(q={"Clock": [r.choice([0, 1, 99, 100, 101, 5000, 5001, 10000, 0x7FFFFFFF, 0x80000000, MINUS, r.getrandbits(14)])
                             for _ in range(r.randrange(1, 12))]},
                script=random_script(r, r.randrange(6, 36)))


def advisor_edge():
    r = random.Random(17)
    # Lines are said oldest first, one a tick, and only while the advisor is silent.
    yield dict(script=[op(1, 0), op(3, 0, 11, 0), op(3, 0, 12, 0), op(3, 0, 13, 0), op(11, 0, 1), op(2, 0), op(2, 0), op(11, 0, 0),
                       op(2, 0), op(2, 0), op(2, 0), op(2, 0), op(2, 0)])
    # Add with "clear first", ClearAll and Close, with the advisor talking and not.
    for speaking in (0, 1):
        yield dict(script=[op(1, 0), op(3, 0, 1, 0), op(3, 0, 2, 0), op(11, 0, speaking), op(3, 0, 3, 1), op(3, 0, 4, 0), op(4, 0),
                           op(4, 0), op(3, 0, 5, 0), op(9, 0), op(2, 0)])
    # The timeout line: after silence only, measured from the last tick that had something
    # to say, given to the screen system's advisor (this screen or the other).
    for target in (0, 1):
        for clocks in ([100, 600, 1099, 1101, 1102, 5000], [0, 0, 0, 0], [MINUS, 0, 5, 10], [1000, 1500, 2001, 2002, 3003]):
            yield dict(q={"Clock": clocks}, script=[op(1, 0), op(1, 1), op(12, 0, target), op(5, 0, 77, 1000), op(2, 0), op(2, 0),
                                                    op(2, 0), op(2, 0), op(2, 1), op(2, 0), op(2, 0), op(3, 0, 5, 0), op(2, 0),
                                                    op(2, 0), op(2, 0)])
    yield dict(q={"Clock": [10, 20]}, script=[op(1, 0), op(5, 0, 9, 0), op(2, 0), op(2, 0), op(5, 0, 8, MINUS), op(2, 0)])
    # Show and Hide ask IsVisible first; the empty members.
    yield dict(script=[op(8, 0), op(6, 0), op(8, 0), op(6, 0), op(7, 0), op(8, 0), op(7, 0), op(13, 0, 2), op(6, 0), op(8, 0),
                       op(7, 0), op(13, 0, -1), op(7, 0), op(10, 0, 0x1000B, 0x1B, 0), op(10, 1, 1, 2, 3)])
    yield dict(script=[op(2, 0), op(3, 1, 4, 0), op(2, 1)])        # before Init: the fields are whatever the memory held
    # Destroyed with lines waiting: the nodes go, the lines stay.
    yield dict(script=[op(1, 0), op(1, 1), op(3, 0, 1, 0), op(3, 0, 2, 0), op(3, 1, 3, 0), op(14, 0), op(2, 1), op(14, 1)])
    yield dict(script=[op(14, 0), op(14, 1)])
    for _ in range(80):
        yield advisor_case(r)


TESTS = [FnTest("CLBScreenComponentAdvisor", advisor_ppc, advisor_host, edge=advisor_edge, rnd=advisor_case)]
