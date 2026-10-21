# CWorld::Allocate on its own (unit w113), with a fake AllocateThingHeap in the unit file so that the error branch (CExitError type 8)
# can be reached; the heap it builds is tested in the WorldInit library.
set(TPW_UNIT_WorldAlloc src/game/World.cpp src/game/save/BlockSave.cpp src/game/ExitErr.cpp)
set(TPW_UNIT_WorldAlloc_KEEP
  __ZN10CBlockSave17RegisterSaveBlockENS_9SaveBlockEPviPKc   # CBlockSave::RegisterSaveBlock(CBlockSave::SaveBlock, void*, int, char const*)
  __ZN10CExitError8SetErrorENS_9ErrorTypeEjPKc   # CExitError::SetError(CExitError::ErrorType, unsigned int, char const*)
  __ZN6CWorld8AllocateEv   # CWorld::Allocate()
  _gExitError
)
