# Unit v51006: CSaveLoad::SaveHeader, PostLoadOperations and PreLoadOperations (src/game/save/SaveLoad.cpp). The callees outside
# the unit are recording fakes in tests/unit_CSaveLoad_SaveHeader_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_CSaveLoad_SaveHeader_2 src/game/save/SaveLoad.cpp platform/arena.cpp)
set(TPW_UNIT_CSaveLoad_SaveHeader_2_KEEP
  __ZN9CSaveLoad10SaveHeaderEPvP15CSaveLoadHeader   # CSaveLoad::SaveHeader(void*, CSaveLoadHeader*)
  __ZN9CSaveLoad17PreLoadOperationsEv   # CSaveLoad::PreLoadOperations()
  __ZN9CSaveLoad18PostLoadOperationsEv   # CSaveLoad::PostLoadOperations()
)
