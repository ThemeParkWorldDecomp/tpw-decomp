# Unit v51125: small leaves of the main app (src/std_exception_what.cpp) and sAlignedBox::Byteswap (src/m3d2_load.cpp, tested where it is).
# The callees outside the unit are recording fakes in tests/unit_std_exception_what.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_exception_what src/std_exception_what.cpp src/m3d2_load.cpp platform/arena.cpp)
