# Unit w188: CResearcher (both constructors, Init, Serialise, FinalDestroy, ReceiveMessage, UpdateTurn, ModelState, ChooseNextAction, the three Process states,
# DoSomeResearch and the static initialiser) and CRegionEffectCell's operator== and Serialise, src/Researcher.cpp. Every callee outside them (the CStaff and CPeep members,
# CWorld::Register and Unregister, the research lab, the script and sound calls, the string table, the serialise instances, memcmp, the trace and the assertion) is a
# recording fake in tests/unit_Researcher.cpp, which also defines the globals of other files.
set(TPW_UNIT_Researcher src/Researcher.cpp platform/arena.cpp)
# The test fakes memcmp (operator== is memcmp of ten bytes on the Mac): keep the compiler from expanding the call inline.
set_source_files_properties(src/Researcher.cpp PROPERTIES COMPILE_OPTIONS -fno-builtin-memcmp)
