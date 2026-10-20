# Unit w178: CEntertainer (both constructors, Init, ReceiveMessage, Serialise, FinalDestroy, UpdateTurn, ChooseNextAction, SetState, ModelState and the two
# statics, src/Entertainer.cpp), CStaff::~CStaff and the six CWorld::Unregister overloads (src/World_unregister.cpp). Every callee outside them (the CStaff and
# CPeep members, CWorld::Register, the map's effectors, the script and sound calls, the string table, the serialise instances) is a recording fake in
# tests/unit_Entertainer.cpp, which also defines the globals of other files.
set(TPW_UNIT_Entertainer src/Entertainer.cpp src/World_unregister.cpp platform/arena.cpp)
