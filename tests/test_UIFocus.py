"""Original PowerPC UI focus behavior, including repeated calls and held buttons."""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_UIFocus"
ROOT, ACTIVE, BUTTONS = 0x10395c24, 0x102af924, 0x10395bbe
ACTIVATE, DEACTIVATE, GET_BUTTONS, NOTIFY = 0x101ca5e0, 0x101ca658, 0x101ca944, 0x101ca30c


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    saved_data = {ROOT: e.read(ROOT, 4), ACTIVE: e.read(ACTIVE, 4), BUTTONS: e.read(BUTTONS, 2)}
    window = e.alloc(4)
    log = e.cl = CL.PpcLog({window: 1})
    st = CL.Stubs()
    st.real = {ACTIVATE, DEACTIVATE, GET_BUTTONS}

    def notify(e):
        log.rec("activateUI", [log.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.u32(ACTIVE)),
                                struct.unpack(">h", e.read(BUTTONS, 2))[0]])

    st.internal(NOTIFY, notify)
    saved_stubs = st.activate(e)
    results, states = [], []
    try:
        e.write(ROOT, struct.pack(">I", window if case["root"] else 0))
        e.write(ACTIVE, struct.pack(">I", case["active"] & 0xffffffff))
        e.write(BUTTONS, struct.pack(">h", case["buttons"]))
        for op in case["ops"]:
            result = T.call_ppc(e, {1: ACTIVATE, 2: DEACTIVATE, 3: GET_BUTTONS}[op], [])[0]
            results.append(T.s32(result) if op == 3 else 0)
            states.append([int(e.u32(ROOT) != 0), T.s32(e.u32(ACTIVE)),
                           struct.unpack(">h", e.read(BUTTONS, 2))[0]])
        return dict(results=results, states=states, log=log.log)
    finally:
        CL.Stubs.deactivate(e, saved_stubs)
        for address, data in saved_data.items():
            e.write(address, bytes(data))


def host(lib, case):
    CL.host_begin(lib, {})
    seed = lib.tpw_ui_focus_seed
    seed.argtypes, seed.restype = [C.c_int32] * 3, None
    op_fn = lib.tpw_ui_focus_op
    op_fn.argtypes, op_fn.restype = [C.c_int32, C.POINTER(C.c_int64)], C.c_int32
    results, states = [], []
    state = (C.c_int64 * 3)()
    try:
        seed(case["root"], case["active"], case["buttons"])
        for op in case["ops"]:
            results.append(op_fn(op, state))
            states.append(list(state))
    finally:
        log = CL.host_end(lib)
    return dict(results=results, states=states, log=log)


def edges():
    for root in (0, 1):
        for active in (0, 1, -1, 2):
            for buttons in (0, 1, 3, -1, -32768, 32767):
                for ops in ([3, 1, 1, 2, 2, 3, 1, 3], [3, 2, 3, 2, 1, 1, 3]):
                    yield dict(root=root, active=active, buttons=buttons, ops=ops)


TESTS = [FnTest("UI_Activate, UI_DeActivate, UI_GetButtonFlags", ppc, host, edge=edges,
                rnd=lambda r: dict(root=r.randrange(2), active=r.choice([0, 1, -1, 2]),
                                   buttons=r.randint(-32768, 32767),
                                   ops=[r.choice([1, 2, 3]) for _ in range(r.randint(1, 20))]))]
