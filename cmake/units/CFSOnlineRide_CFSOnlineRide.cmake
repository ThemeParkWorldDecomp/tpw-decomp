# Unit v5141: CFSOnlineRide::CFSOnlineRide (src/game/online/FS_OnlineList.cpp). The string callees are recording fakes in
# tests/unit_CFSOnlineRide_CFSOnlineRide.cpp; the list's AddTail is the header's real template.
set(TPW_UNIT_CFSOnlineRide_CFSOnlineRide src/game/online/FS_OnlineList.cpp platform/arena.cpp)
set(TPW_UNIT_CFSOnlineRide_CFSOnlineRide_KEEP
  __ZN13CFSOnlineRideC1EPK10ccRideInfoP23CFSOnlineSystemRideList   # CFSOnlineRide::CFSOnlineRide(ccRideInfo const*, CFSOnlineSystemRideList*)
  __ZN13CFSOnlineRideD1Ev   # CFSOnlineRide::~CFSOnlineRide()
)
