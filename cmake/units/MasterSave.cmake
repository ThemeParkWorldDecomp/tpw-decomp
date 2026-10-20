# Unit w89b: the master save file (Save, Load, Serialise) and the park record's loader constructor and Serialise. The master save and park
# record of units w32 and w89, the bullfrog strings and the 16-bit C library are real; the serialisation instances, the player settings'
# serialiser, the file functions, the trace output, the swear filter and the path getters are fakes in the unit file.
set(TPW_UNIT_MasterSave src/FS_MasterSave_file.cpp src/FS_MasterSave_park.cpp src/FS_MasterSave_tickets.cpp src/FS_MasterSave_signs.cpp
    src/FS_MasterSave_global.cpp src/FS_MasterSave_global_ctor.cpp src/RideDB_names.cpp src/TbString.cpp src/TbStringBuffer.cpp
    platform/msl.cpp)
