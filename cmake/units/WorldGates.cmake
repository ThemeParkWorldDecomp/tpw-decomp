# The park gates and the arrival vehicle (unit w193): CWorld::SetParkOpenOrClosed, ParkGates_Open, ParkGates_Close and the six
# ArrivalVehicle_* functions (src/World_gates.cpp). Every callee outside them (the asserts and traces, the thing virtuals, the script
# variables, the ride database, the message centre and its event, CObject's constructor, the map's bus stop, _wcsicmp) is a fake in the unit file.
set(TPW_UNIT_WorldGates src/World_gates.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
# The unit file includes calllog.cpp itself (it observes the allocations).
set(TPW_UNIT_WorldGates_OWN_CALLLOG ON)
