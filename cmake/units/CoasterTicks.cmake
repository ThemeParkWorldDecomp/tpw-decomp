# Unit w137: the coasters' per-frame ticks and the track editor's pointer input (CoasterTickPreRender, CoasterTickPostRender,
# CoasterUserInputTick; src/TPCoaster_ticks.cpp). Every callee is a recording fake in tests/unit_CoasterTicks.cpp, which also defines the
# globals of other files. The strings (src/TbString.cpp, src/TbStringBuffer.cpp, platform/msl.cpp) are there for gTPGlobals' default
# names, never read.
set(TPW_UNIT_CoasterTicks src/TPCoaster_ticks.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
