# Unit v5542: CFSOnlinePark constructor and destructor (src/game/online/FS_OnlineList.cpp). The callees are recording fakes in
# tests/unit_CFSOnlinePark_CFSOnlinePark.cpp (the list members are strong symbols that replace the header templates' weak copies,
# so the source is compiled without inlining).
set(TPW_UNIT_CFSOnlinePark_CFSOnlinePark src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlinePark_CFSOnlinePark_KEEP
  __ZN13CFSOnlineParkC1EPK10ccParkInfoP23CFSOnlineSystemParkList   # CFSOnlinePark::CFSOnlinePark(ccParkInfo const*, CFSOnlineSystemParkList*)
  __ZN13CFSOnlineParkD1Ev   # CFSOnlinePark::~CFSOnlinePark()
)
tpw_test_source_flags(src/game/online/FS_OnlineList.cpp OPTIONS -fno-inline)
