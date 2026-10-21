set(TPW_UNIT_TPEngine src/game/TPEngine.cpp src/game/TPInstance.cpp src/game/TPMesh.cpp src/game/park/TPHField.cpp
    src/game/rides/TPRide.cpp platform/arena.cpp)
set(TPW_UNIT_TPEngine_KEEP
  __Z11EngineTick1v   # EngineTick1()
  __Z11EngineTick2v   # EngineTick2()
  __Z11EngineTick3v   # EngineTick3()
  __Z12AnimateRidesj   # AnimateRides(unsigned int)
  __Z12FindFilePathPKci   # FindFilePath(char const*, int)
  __Z12MacReadClockv   # MacReadClock()
  __Z13EngineStartupv   # EngineStartup()
  __Z13MReleaseFreeFPv   # MReleaseFreeF(void*)
  __Z14MReleaseAllocFjj   # MReleaseAllocF(unsigned int, unsigned int)
  __Z14MacUpdateMousev   # MacUpdateMouse()
  __Z15EngineCloseDownv   # EngineCloseDown()
  __Z16MacIsFirstPersonv   # MacIsFirstPerson()
  __Z16TPForce3DRefreshj   # TPForce3DRefresh(unsigned int)
  __Z19EngineStartup_Finaliiii   # EngineStartup_Final(int, int, int, int)
  __Z20EngineStartup_Dialogv   # EngineStartup_Dialog()
  __Z20InitialiseInstancingj   # InitialiseInstancing(unsigned int)
  __Z20MacLoadDebugSettingsv   # MacLoadDebugSettings()
  __Z22InitialiseMemorySystemv   # InitialiseMemorySystem()
  __Z22ResetCursorOverSpritesP8s3DPoint   # ResetCursorOverSprites(s3DPoint*)
  __Z22ResetWorldBoundingAreav   # ResetWorldBoundingArea()
  __Z23CreateScratchGeomBufferi   # CreateScratchGeomBuffer(int)
  __Z24DestroyScratchGeomBufferv   # DestroyScratchGeomBuffer()
  __Z24EngineStartup_SetOptionsv   # EngineStartup_SetOptions()
  __Z25EnableProceduralTexturingv   # EnableProceduralTexturing()
  __Z25EngineStartup_OptionsFilej   # EngineStartup_OptionsFile(unsigned int)
  __Z26DisableProceduralTexturingv   # DisableProceduralTexturing()
  __Z26EngineStartup_SettingsFilev   # EngineStartup_SettingsFile()
  __Z27IsProceduralTexturingActivev   # IsProceduralTexturingActive()
  __Z27UpdateScratchGeomBufferSizeP9tag_sMesh   # UpdateScratchGeomBufferSize(tag_sMesh*)
  __Z7MacFreeRPv   # MacFree(void*&)
  __Z7MacOpenPKcS0_   # MacOpen(char const*, char const*)
  __Z7MacReadPvjjS_   # MacRead(void*, unsigned int, unsigned int, void*)
  __Z7MacSeekPvjj   # MacSeek(void*, unsigned int, unsigned int)
  __Z7MacSizePv   # MacSize(void*)
  __Z8HNewHeapjjjPP9sHeapInfo   # HNewHeap(unsigned int, unsigned int, unsigned int, sHeapInfo**)
  __Z8MacClosePv   # MacClose(void*)
  __Z9HFreeHeapP9sHeapInfo   # HFreeHeap(sHeapInfo*)
  __Z9MacMallocj   # MacMalloc(unsigned int)
  _g3DRefreshFlags
  _gCursorOverSprite
  _gCursorSpritePosition
  _gIdxDirectDraw
  _gMemorySystemInitialised
  _gOpenGL
  _gScratchGeomBuffer
  _gScratchGeomVertices
  _gSmallTexturesOnly
)
