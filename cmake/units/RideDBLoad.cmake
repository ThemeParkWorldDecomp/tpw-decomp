# Unit w119b: ride database loading, shape and text, custom effects and coaster setup.
# Every external call is a recording fake; the eleven functions call one another for real.
set(TPW_UNIT_RideDBLoad src/RideDB_load.cpp src/RideDB_scan.cpp src/RideDB_shape.cpp src/RideDB_info.cpp
    src/RideDB_balance.cpp src/CoasterBalance.cpp src/TPCoaster_load.cpp platform/msl.cpp)
set(TPW_UNIT_RideDBLoad_OWN_CALLLOG ON)
