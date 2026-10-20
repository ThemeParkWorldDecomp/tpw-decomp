# Unit w89: the master save's ticket bookkeeping, the current theme and the park sign names. The unit file fakes the balance table,
# the trace output and the string table; the master save and park record of unit w32, the ride names, the strings and the 16-bit C
# library are real.
set(TPW_UNIT_MasterSaveTickets src/RideDB_balance.cpp src/FS_MasterSave_tickets.cpp src/FS_MasterSave_signs.cpp src/FS_MasterSave_global.cpp
    src/FS_MasterSave_global_ctor.cpp src/RideDB_names.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
