# Unit w200: a ride opens, closes, runs and asks for an upgrade, and its messages (CObject::IsOpenable ... ReceiveMessage in src/Object_opening.cpp, the ride
# messages in src/ThingMessages2.cpp, CRideDatabase::GetRideInfo in src/RideDB_lookup.cpp). The callees outside the unit are recording fakes in tests/unit_ObjectOpening.cpp.
set(TPW_UNIT_ObjectOpening src/Object_opening.cpp src/ThingMessages2.cpp src/RideDB_lookup.cpp platform/arena.cpp)
