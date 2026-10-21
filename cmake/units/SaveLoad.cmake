set(TPW_UNIT_SaveLoad src/game/save/SaveLoad.cpp src/game/save/FS_LoadSave.cpp platform/arena.cpp)
set(TPW_UNIT_SaveLoad_KEEP
  __Z15PortIsEarlySavePKhj   # PortIsEarlySave(unsigned char const*, unsigned int)
  __ZN11CFSLoadSave14LoadMostRecentEv   # CFSLoadSave::LoadMostRecent()
  __ZN9CSaveLoad4LoadEPKcP15CSaveLoadHeaderNS_8LoadTypeE   # CSaveLoad::Load(char const*, CSaveLoadHeader*, CSaveLoad::LoadType)
  _gSaveLoadOpenTestWord
)
