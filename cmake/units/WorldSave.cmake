# Unit w205: the world's save (CWorld::Save, the two enum SAVEVAR instances, the two headquarters getters, the macro layer's destructor, CThing's
# serialise constructor; src/World_save.cpp) and the two chunk reader destructors (src/ChunkFile.cpp). The callees outside the unit are recording
# fakes in tests/unit_WorldSave.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_WorldSave src/World_save.cpp src/ChunkFile.cpp src/Serialise_basic.cpp)
set(TPW_UNIT_WorldSave_OWN_CALLLOG ON)
