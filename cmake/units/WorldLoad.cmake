# Unit w207: the world's load (CWorld::Load, the two enum LOADVAR instances, CThing::ForceThingId; src/World_load.cpp). The callees outside the unit
# are recording fakes in tests/unit_WorldLoad.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_WorldLoad src/World_load.cpp src/Serialise_basic.cpp)
set(TPW_UNIT_WorldLoad_OWN_CALLLOG ON)
