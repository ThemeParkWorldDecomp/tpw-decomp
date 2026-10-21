# The leaves under LevelInit2 and LevelDeInit (unit w112a): the cell system's four mesh loaders and destroyers, its click stack
# and illegal placement words, the coaster extruder's storage, the object table's set-up, the dynamic mesh texture sets and the
# engine height field's procedural texture window setter.
set(TPW_UNIT_LevelLeaves src/game/park/LLCellMouse.cpp src/game/park/LLCellSystem.cpp src/game/DynamicMeshSet.cpp
    src/game/rides/CoasterExtrude.cpp src/game/rides/RidePlacement.cpp src/engine/tb_hfield.cpp)
set(TPW_UNIT_LevelLeaves_KEEP
  __Z14DMFreeTexturesP12sDTextureSet   # DMFreeTextures(sDTextureSet*)
  __Z14DMLoadTexturesP12sDTextureSet   # DMLoadTextures(sDTextureSet*)
  __Z16LLCM_LoadQMeshesv   # LLCM_LoadQMeshes()
  __Z19LLCM_DestroyQMeshesv   # LLCM_DestroyQMeshes()
  __Z21LLCC_ResetBuyLandListv   # LLCC_ResetBuyLandList()
  __Z21LLCM_LoadKTrackMeshesv   # LLCM_LoadKTrackMeshes()
  __Z21LLCM_LoadWTrackMeshesv   # LLCM_LoadWTrackMeshes()
  __Z23LLCM_LoadHoardingMeshesv   # LLCM_LoadHoardingMeshes()
  __Z24CreateExtrudeFaceStoragejj   # CreateExtrudeFaceStorage(unsigned int, unsigned int)
  __Z24LLCM_DestroyKTrackMeshesv   # LLCM_DestroyKTrackMeshes()
  __Z24LLCM_DestroyWTrackMeshesv   # LLCM_DestroyWTrackMeshes()
  __Z24LLCM_SetIllegalPlacementi   # LLCM_SetIllegalPlacement(int)
  __Z24SetupObjectInstanceTableRjS_S_   # SetupObjectInstanceTable(unsigned int&, unsigned int&, unsigned int&)
  __Z26LLCC_SystemResetClickStackv   # LLCC_SystemResetClickStack()
  __Z26LLCM_DestroyHoardingMeshesv   # LLCM_DestroyHoardingMeshes()
  __ZN7CHField16ProcTextureRangeEiiii   # CHField::ProcTextureRange(int, int, int, int)
  _gDynamicCellTextureEntries
  _gDynamicCellTextures
  _gExtrudeFaceStoreA
  _gExtrudeFaceStoreB
  _gExtrudeFaceStoreCount
  _gLLCCClickStackCount
  _gLLCMHoardingMeshes
  _gLLCMIllegalPlacement
  _gLLCMKTrackMeshes
  _gLLCMQMeshes
  _gLLCMQSize
  _gLLCMTrackSize
  _gLLCMWTrackMeshes
  _gRideHoardingTextureEntries
  _gRideHoardingTextures
)
