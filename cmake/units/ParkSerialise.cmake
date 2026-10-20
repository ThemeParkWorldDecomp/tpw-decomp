# Unit w162: the park analyser's Serialise (src/ParkAnalyser_serialise.cpp: CParkAnalyser::Serialise). Every callee (CThing::Serialise, the cell, value and
# history members, LOADVAR / SAVEVAR<int> and the TbTimeStamp pair) is a recording fake in tests/unit_ParkSerialise.cpp. List only the sources the library must compile.
set(TPW_UNIT_ParkSerialise src/ParkAnalyser_serialise.cpp platform/arena.cpp)
set(TPW_UNIT_ParkSerialise_OWN_CALLLOG ON)
