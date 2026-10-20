# Unit w187: CMechanic (both constructors, Init, FinalDestroy, ReceiveMessage, Serialise, SetState, ModelState, UpdateTurn, ChooseNextAction, the four
# Process states and the static initialiser, src/Mechanic.cpp). Every callee outside them (the CStaff, CPeep and CObject members, CWorld::Register and
# Unregister, GetRand, GetMechanicHQ and GetAdvisor, CMechanicHQ::GetNewJob, the advisor request and its queue, the script and sound calls, the string table,
# the message centre, the serialise instances, the trace and the assertion) is a recording fake in tests/unit_Mechanic.cpp, which also defines the globals
# of other files.
set(TPW_UNIT_Mechanic src/Mechanic.cpp platform/arena.cpp)
