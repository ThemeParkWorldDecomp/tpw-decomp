# Unit w208: the park's hoarding written to the theme's hoardings balance file, and the static objects of LLCellSystem.cpp
# (src/LLCellSystem_hoardwrite.cpp). The callees outside the unit are recording fakes in tests/unit_HoardingWrite.cpp, which also defines
# the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_HoardingWrite src/LLCellSystem_hoardwrite.cpp platform/arena.cpp)
