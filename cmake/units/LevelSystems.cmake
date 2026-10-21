# The park's second-stage systems that LevelInit2 and LevelDeInit call (unit w111): the attribute map, the poly search table, the
# heightmap words, the level lights' teardown, the highlight buffers, the coaster types, the cell system's start and end, the
# script system, the dynamic cell meshes and the engine's PT_SetRange. Light.cpp is the lights' constructor and destructor
# (DestroyLevelLights deletes real lights).
set(TPW_UNIT_LevelSystems src/game/MeshLoad.cpp src/game/TPLevel.cpp src/game/park/TPHField.cpp
    src/game/park/TPHoardings.cpp src/game/rides/CoasterType.cpp src/game/rides/RidePreview.cpp
    src/game/rides/TPCoaster.cpp src/game/park/LLCellSystem.cpp src/game/Script.cpp src/game/park/DynamicCell.cpp
    src/engine/tb_hfield.cpp src/game/Light.cpp)
set(TPW_UNIT_LevelSystems_KEEP
  __Z11PT_SetRangeiiii   # PT_SetRange(int, int, int, int)
  __Z15LLCC_SystemInitii   # LLCC_SystemInit(int, int)
  __Z16DataErrorHandlerPc   # DataErrorHandler(char*)
  __Z17LLCC_SystemDeInitv   # LLCC_SystemDeInit()
  __Z18DestroyLevelLightsv   # DestroyLevelLights()
  __Z18SetupForPolySearchv   # SetupForPolySearch()
  __Z19DestroyCoasterTypesv   # DestroyCoasterTypes()
  __Z20SCR_InitScriptSystemv   # SCR_InitScriptSystem()
  __Z20SetupHeightmapSystemv   # SetupHeightmapSystem()
  __Z21LoadLevelAttributeMapv   # LoadLevelAttributeMap()
  __Z21SetdownFromPolySearchv   # SetdownFromPolySearch()
  __Z22ClosedownRideHoardingsv   # ClosedownRideHoardings()
  __Z22InitialiseCoasterTypesv   # InitialiseCoasterTypes()
  __Z24DC_FreeDynamicCellMeshesv   # DC_FreeDynamicCellMeshes()
  __Z25DC_SetupDynamicCellMeshesv   # DC_SetupDynamicCellMeshes()
  __Z28CreateHighlightColourBuffersv   # CreateHighlightColourBuffers()
  __Z29DestroyHighlightColourBuffersv   # DestroyHighlightColourBuffers()
  __ZN6CLight11GetLinkTypeEv   # CLight::GetLinkType()
  __ZN6CLight7GetDataEv   # CLight::GetData()
  __ZN6CLight7PrepareEv   # CLight::Prepare()
  __ZN6CLightC1Ev   # CLight::CLight()
  __ZN6CLightD1Ev   # CLight::~CLight()
  _gBlueprintMesh
  _gCoasterTextureBase
  _gCoasterTypeCount
  _gCoasterTypes
  _gDynamicCellF9BD4
  _gDynamicCellF9D94
  _gDynamicCellF9D98
  _gDynamicCellF9D9C
  _gDynamicCellF9DA0
  _gDynamicCellFBDBC
  _gDynamicCellFlag
  _gDynamicCellRandom
  _gDynamicCellsReady
  _gExtrudeFaceCountA
  _gExtrudeFaceCountB
  _gFlagsMesh
  _gHeightmapScaleA
  _gHeightmapScaleB
  _gHighlightFaceColours
  _gHighlightNodeColours
  _gLLCC47C4
  _gLLCC47C8
  _gLLCC47CC
  _gLLCC47E0
  _gLLCC47E4
  _gLLCC47E8
  _gLLCC47EC
  _gLLCC4800
  _gLLCC4804
  _gLLCC4808
  _gLLCC480C
  _gLLCC4810
  _gLLCC4814
  _gLLCC4818
  _gLLCCC200
  _gLLCCC204
  _gLLCCC208
  _gLLCCC210
  _gLLCCC214
  _gLLCCDB68
  _gLLCCDB6C
  _gLLCCDE0E2
  _gLLCCHeight
  _gLLCCWidth
  _gPolySearchTable
  _gScriptF7C2C
  _gScriptInitialised
  _gScriptTable
  _gScriptTableSize
  _gWaterMesh
  TRAINS_SWAP_FILE_WORDS@game/rides/TPCoaster.cpp   # the #define TRAINS_SWAP_FILE_WORDS of src/game/rides/TPCoaster.cpp (a guard macro the regroup brought into it)
)
