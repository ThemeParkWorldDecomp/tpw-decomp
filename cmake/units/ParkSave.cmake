# Unit w158: the park analyser's value and history save templates (CParkAnalyserVal<int/float>::Serialise, CHistory<T,N>::Serialise for seven
# instances and CHistory<ObjectId,3>::SerialiseStruct; src/ParkAnalyser_save.cpp). LOADVAR, SAVEVAR and the STRUCT pair are recording fakes in
# tests/unit_ParkSave.cpp. List only the sources the library must compile.
set(TPW_UNIT_ParkSave src/ParkAnalyser_save.cpp platform/arena.cpp)
set(TPW_UNIT_ParkSave_OWN_CALLLOG ON)
