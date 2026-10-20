# Unit v5542: CFSOnlinePark constructor and destructor (src/CFSOnlinePark_CFSOnlinePark.cpp). The callees are recording fakes in
# tests/unit_CFSOnlinePark_CFSOnlinePark.cpp (the list members are strong symbols that replace the header templates' weak copies,
# so the source is compiled without inlining).
set(TPW_UNIT_CFSOnlinePark_CFSOnlinePark src/CFSOnlinePark_CFSOnlinePark.cpp platform/arena.cpp)
set_source_files_properties(src/CFSOnlinePark_CFSOnlinePark.cpp PROPERTIES COMPILE_OPTIONS "-fno-inline")
