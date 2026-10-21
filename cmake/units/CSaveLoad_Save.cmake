# Unit v51020: CSaveLoad::Save (src/game/save/SaveLoad.cpp). The callees outside the unit are recording fakes in
# tests/unit_CSaveLoad_Save.cpp, which also defines the globals of other files.
set(TPW_UNIT_CSaveLoad_Save src/game/save/SaveLoad.cpp platform/arena.cpp)
set(TPW_UNIT_CSaveLoad_Save_KEEP
  __ZN9CSaveLoad4SaveEPKcP15CSaveLoadHeaderNS_8LoadTypeE   # CSaveLoad::Save(char const*, CSaveLoadHeader*, CSaveLoad::LoadType)
)
