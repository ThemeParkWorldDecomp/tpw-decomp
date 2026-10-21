# Unit v5742: the FS_Online.cpp static initialiser (src/game/online/FS_Online.cpp). The callees
# are recording fakes in tests/unit_sinit_FS_Online_cpp.cpp, which also defines the globals.
set(TPW_UNIT_sinit_FS_Online_cpp src/game/online/FS_Online.cpp platform/arena.cpp)
set(TPW_UNIT_sinit_FS_Online_cpp_KEEP
  __Z19sinit_FS_Online_cppv   # sinit_FS_Online_cpp()
)
