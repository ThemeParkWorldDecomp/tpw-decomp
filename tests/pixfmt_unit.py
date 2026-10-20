"""Combined coverage entry point for the bullfrog TbPixFmt.cpp range (0x10014bf0..0x10016538)
and TbFPUFlag's inline destructor (0x1006abdc). test_bullfrog_CPU's setup goes last: it
replaces the standard Gestalt stub that the other setups install."""
import test_LbExeKey
import test_LbList
import test_LbLog
import test_TbFPUPrecision
import test_TbPixFmt
import test_TbRefRelease
import test_TbTimeDiff
import test_bullfrog_CPU

MODULES = [test_LbExeKey, test_LbList, test_LbLog, test_TbRefRelease, test_TbTimeDiff, test_TbFPUPrecision,
           test_TbPixFmt, test_bullfrog_CPU]
PROGRAM = "bullfrog symbols shared"
TESTS = [t for m in MODULES for t in m.TESTS]


def setup(runner):
    for m in MODULES:
        m.setup(runner)
