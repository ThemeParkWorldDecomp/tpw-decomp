# Unit v5206: CSaveLoad::LoadModules (src/game/save/SaveLoad.cpp). The callees outside the unit are recording fakes in
# tests/unit_CSaveLoad_LoadModules.cpp, which also defines the globals of other files.
set(TPW_UNIT_CSaveLoad_LoadModules src/game/save/SaveLoad.cpp platform/arena.cpp)
set(TPW_UNIT_CSaveLoad_LoadModules_KEEP
  __ZN9CSaveLoad11LoadModulesERN3bfl10FileSystem5CFileENS_8LoadTypeE   # CSaveLoad::LoadModules(bfl::FileSystem::CFile&, CSaveLoad::LoadType)
)
