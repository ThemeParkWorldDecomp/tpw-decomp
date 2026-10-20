# Unit w182: CGuard (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, ProsecutePerp, IsAvailable, IsCarrying, UpdateTurn, ChooseNextAction,
# ModelState, SetState and the five Process states, src/Guard.cpp). Every callee outside them (the CStaff, CPeep and CPerson members, CWorld::Register and
# Unregister, the map's effectors and entrance cells, the park analyser, the script and sound calls, the string table, the message centre, the serialise
# instances, the trace and the assertion) is a recording fake in tests/unit_Guard.cpp, which also defines the globals of other files.
set(TPW_UNIT_Guard src/Guard.cpp platform/arena.cpp)
