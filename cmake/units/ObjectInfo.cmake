# Unit w210: a ride's names, kind, excitement and stand point, and its final destroy (CObject::UpgradesWhat ... FinalDestroy in src/Object_info.cpp,
# CMsgRideDeleted's constructor in src/ThingMessages2.cpp, CRideDatabase::GetFileName in src/RideDB_names.cpp). The callees outside the unit are recording
# fakes in tests/unit_ObjectInfo.cpp; the strings and the 16-bit C library (swprintf, wcslen, wcsncpy) are real.
set(TPW_UNIT_ObjectInfo src/Object_info.cpp src/gameplay_leaves.cpp src/ThingMessages2.cpp src/RideDB_names.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/arena.cpp platform/msl.cpp)
