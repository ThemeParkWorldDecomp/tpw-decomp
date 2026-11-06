#!/usr/bin/env python3
"""The answer of the Mac's CTrackCell::ModifyType (0x100a0828) against the port's CTrackCell::ModifyTypeBody, on top of the state test_MapCells
already compares. CTrackCell::Modify stores the word at 0x102dc21c when this answer is nonzero, so a different answer is a different word."""
import ctypes as C

import test_MapCells as M
import tpwtest as T

SHIM_LIBRARY = "unit_MapCells_ModifyAnswer"
STUBS = M.STUBS
setup = getattr(M, "setup", None)


def real_valid(c):
    """ValidTypeChange's answer (0x100a149c, tested by test_CTrackCell_ValidTypeChange's unit) for the cell the case builds, whose flags are random."""
    t, m = c["type"], c["mode"]
    c["valid"] = 1 if t in (0x19, 7, 0) or m == 0 or m == t else 0
    return c


def edges(modes=(0, 1, 7, 0xB, 0x10, 0x19, 3)):
    for c in M.edges_modify():
        yield real_valid(c)
    # the hoarding stroke over every kind of occupied cell
    r = M.random.Random(2200)
    for t in (1, 2, 3, 4, 5, 9, 10, 0xB, 0xC, 0xD, 0x10, 0x11, 0x12):
        for m in modes:
            yield real_valid(M.mk_modify(r, mode=m, type=t, free=0, rec=0, balance=1000000, kart=250, water=400))


def rnd(r):
    return real_valid(M.mk_modify(r))


class AnswerTest(M.ModifyTest):
    def __init__(self):
        M.Base.__init__(self, "CTrackCell::ModifyType answer", self.ppc_case, self.host_case, edge=lambda: list(edges()), rnd=rnd)

    def ppc_case(self, e, c):
        answers = []
        real = T.call_ppc

        def spy(*a, **k):
            r = real(*a, **k)
            answers.append(T.s32(r[0]))
            return r
        T.call_ppc = spy
        try:
            out = super().ppc_case(e, c)
        finally:
            T.call_ppc = real
        out["answer"] = answers[-1]
        return out

    def host_case(self, lib, c):
        answers = []
        modify = lib.tpw_mc_modify_answer
        modify.argtypes = [C.c_int32]
        modify.restype = C.c_int32

        class Call:
            argtypes = None

            def __call__(self, mode):
                answers.append(modify(mode))

        class Shim:
            tpw_mc_modify = Call()

            def __getattr__(self, name):
                return getattr(lib, name)
        shim = Shim()
        out = super().host_case(shim, c)
        out["answer"] = answers[-1]
        return out


TESTS = [AnswerTest()]
