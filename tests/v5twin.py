"""The byte-twin judge.

A function left untranslated (N) whose machine code equals a translated function's (D) once the b/bl targets and the
TOC displacements are masked is a byte twin. Only twins that are instances of the
same template in the same program are taken, so N's explicit instantiation compiles the very source D's does. Byte
twins are still not proof, so N is credited only when D's own differential test passes with N's ORIGINAL code in D's
place: every entry to D on the PowerPC side is sent to N (a code hook at D's entry sets the pc to N) while the host side
runs D's translation. N runs with its own relocations (its own callees and TOC slots), so a callee of N that behaves
differently from D's, or that the module fakes at D's callee's address only, shows as a mismatch.

A wrapper module (tests/test_V5Twin_*.py names the module it borrows and,
for each twin, the tests of that module that enter D; each such test is run again renamed after N and fails a case set
in which N never ran.
"""
import difftest
from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as P

N_RANDOM = difftest.Runner.run_test.__defaults__[0]     # the random cases run_test adds to a test's edge cases


class Redirect:
    """Code hooks at each D on one emulator: hits[D] counts the entries, target[D] (None: count only) receives them."""

    def __init__(self, emu, ds):
        self.emu, self.target, self.hits = emu, {d: None for d in ds}, {d: 0 for d in ds}
        for d in ds:
            emu.uc.hook_add(UC_HOOK_CODE, self._hook, None, d, d)

    def _hook(self, uc, address, size, user):
        if address not in self.target or address in getattr(self.emu, "internal_stubs", {}):
            return                     # a module that fakes D itself never runs D, so it cannot run N either
        self.hits[address] += 1
        if self.target[address] is not None:
            uc.reg_write(P.UC_PPC_REG_PC, self.target[address])


class Twin:
    """A test of the borrowed module, run with D sent to N and named after N."""

    def __init__(self, inner, name, d, n, state):
        self.inner, self.name, self.d, self.n, self.state = inner, name, d, n, state
        self.left = self.entered = 0

    def edge_cases(self):
        edges = list(self.inner.edge_cases())
        self.left, self.entered = len(edges) + N_RANDOM, 0
        return edges

    def random_case(self, rnd):
        return self.inner.random_case(rnd)

    def run_case(self, runner, case):
        red = self.state["redirect"]
        before = red.hits[self.d]
        red.target[self.d] = self.n
        try:
            out = difftest.Runner.run_case(runner, self.inner, case)
        finally:
            red.target[self.d] = None
        self.entered += red.hits[self.d] > before
        self.left -= 1
        if not out and self.left == 0 and self.entered == 0:
            return [f"twin 0x{self.n:08x} never entered"]
        return out


def wrap(module, twins):
    """module: the borrowed test module; twins: [(test name, D, N, N's name)].
    Returns (PROGRAM, SHIM_LIBRARY, setup, TESTS) for the wrapper module."""
    state = {}
    by_name = {t.name: t for t in module.TESTS}
    tests = [Twin(by_name[t], f"{name} [0x{n:08x} as 0x{d:08x} in {t}]", d, n, state) for t, d, n, name in twins]

    def setup(runner):
        if hasattr(module, "setup"):
            module.setup(runner)
        state["redirect"] = Redirect(runner.emu, sorted({d for _, d, _, _ in twins}))

    return getattr(module, "PROGRAM", difftest.DEFAULT_PROGRAM), getattr(module, "SHIM_LIBRARY", None), setup, tests
