# Unit w184: CHandyman (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, UpdateTurn, ChooseNextAction, SetState, ModelState, the six Process
# states, FindNearestDirtyToilet and the static initialiser, src/Handyman.cpp). Every callee outside them (the CStaff, CPeep and CObject members, CWorld::Register and
# Unregister, the park analyser, the map cell calls, the script and sound calls, the string table, the message centre, the serialise instances, the trace and
# the assertion) is a recording fake in tests/unit_Handyman.cpp, which also defines the globals of other files.
set(TPW_UNIT_Handyman src/Handyman.cpp platform/arena.cpp)
