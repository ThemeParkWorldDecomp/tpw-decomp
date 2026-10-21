"""Unit v5L42, the window scripts of src/game/ui/ui_allStaffPanel.cpp, src/game/ui/ui_cityLobbyController.cpp, src/game/ui/ui_onlineCityInfoOverlay.cpp, src/game/ui/ui_onlineOutbox.cpp, src/game/ui/ui_onlineParkInfoPanel.cpp, src/game/ui/ui_parkStatus.cpp, against libtpw_unit_WindowScripts.dylib (tests/unit_WindowScripts.cpp), main app
("SimTheme Park"): the arrays the launcher had as all-zero stand-ins until devtools/gen_game_tables.py generated them (WINDOW_SCRIPTS). Also
the scripts that were two-word placeholders until then, which their panels' files define: each is read from that unit's library (ELSEWHERE).

No code runs. One test, "the window scripts": the host's array against the words of the program image, read from the data section's dump (so
from another reading of the program than the generator's). A case is one script and one block of 16 words (the last block is shorter); the edge
cases cover every word of every script. The word counts are the walks of ProcessLevel's grammar, each ending where the next named data begins.
"""
import ctypes as C
import struct

from tpwtest import FnTest

SHIM_LIBRARY = "unit_WindowScripts"

# (array, image address, words)
SCRIPTS = [
    ("gCongratsScript", 0x102A772C, 21),
    ("gBankruptScript", 0x102A7756, 21),
    ("gAllShowsScript", 0x102A8B42, 125),
    ("gLoadSaveScript", 0x102AA330, 186),
    ("gOptionsPanelScript", 0x102AADE4, 1090),
    ("gAddressBookDescription", 0x102AC1E6, 184),
    ("gYearEndScript", 0x102AF524, 478),
    ("gProgBarScript", 0x102AFE24, 43),
    ("gCityInfoOverlayScript", 0x102B0288, 58),
    ("gOutboxPanelData", 0x102B05C4, 311),
    ("gOnlineParkInfoScript", 0x102B0E58, 279),
]
BLOCK = 16
# the arrays another unit's library holds: (library, its address function, the name it takes)
ELSEWHERE = {
    "gCongratsScript": ("unit_UI_Congratulations", "tpw_u5022_addr", "cscript"),
    "gBankruptScript": ("unit_UI_Congratulations", "tpw_u5022_addr", "bscript"),
    "gLoadSaveScript": ("unit_CreateLoadPanel_2", "tpw_cl_addr", "script"),
    "gAddressBookDescription": ("unit_CreateAddressBook", "tpw_ab_addr", "desc"),
    "gYearEndScript": ("unit_CreateYearEndPanel", "tpw_yep_addr", "script"),
}

IMAGE = {}
LIBS = {}


def setup(runner):
    for name, (library, fn, key) in ELSEWHERE.items():
        LIBS[name] = (runner.shim(library), fn, key)
    data = next(b for b in runner.emu.blocks if b["initialized"] and not b["execute"] and b["name"] != "IMPORTS")
    IMAGE["start"], IMAGE["bytes"] = data["start"], (runner.emu.dir / data["file"]).read_bytes()


def block(case):
    name, address, words = SCRIPTS[case["script"]]
    first = case["block"] * BLOCK
    return name, address + 2 * first, min(BLOCK, words - first)


def ppc(e, case):
    _, at, n = block(case)
    return {"words": list(struct.unpack_from(">%dh" % n, IMAGE["bytes"], at - IMAGE["start"]))}


def host(lib, case):
    name, _, n = block(case)
    lib, fn, key = LIBS.get(name, (lib, "tpw_ws_addr", name))
    getattr(lib, fn).restype, getattr(lib, fn).argtypes = C.c_uint64, [C.c_char_p]
    base = getattr(lib, fn)(key.encode())
    assert base, name
    first = case["block"] * BLOCK
    return {"words": list(struct.unpack("<%dh" % n, C.string_at(base + 2 * first, 2 * n)))}


def edges():
    for i, (_, _, words) in enumerate(SCRIPTS):
        for b in range((words + BLOCK - 1) // BLOCK):
            yield dict(script=i, block=b)


def rnd(r):
    i = r.randrange(len(SCRIPTS))
    return dict(script=i, block=r.randrange((SCRIPTS[i][2] + BLOCK - 1) // BLOCK))


TESTS = [FnTest("the window scripts", ppc, host, edge=edges, rnd=rnd)]
