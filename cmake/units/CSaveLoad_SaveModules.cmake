# Unit v5628: CSaveLoad::SaveModules (src/game/save/SaveLoad.cpp). The callees outside the unit are recording fakes in
# tests/unit_CSaveLoad_SaveModules.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_CSaveLoad_SaveModules src/game/save/SaveLoad.cpp platform/arena.cpp)
set(TPW_UNIT_CSaveLoad_SaveModules_KEEP
  __ZN9CSaveLoad11SaveModulesEPvNS_8LoadTypeE   # CSaveLoad::SaveModules(void*, CSaveLoad::LoadType)
  _gSaveModulesTagTestWords
)
