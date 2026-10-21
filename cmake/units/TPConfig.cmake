set(TPW_UNIT_TPConfig src/game/TPConfig.cpp src/game/rides/TPRide.cpp src/game/save/TPQuickload.cpp
    src/game/LMatrix.cpp)
set(TPW_UNIT_TPConfig_KEEP
  __Z12FindFilePathPKcjPi   # FindFilePath(char const*, unsigned int, int*)
  __Z13ClosedownGamev   # ClosedownGame()
  __Z13DoesFileExistPcj   # DoesFileExist(char*, unsigned int)
  __Z14TPSaveRegistryv   # TPSaveRegistry()
  __Z17CloseQuickloadingv   # CloseQuickloading()
  __Z18LoadQuickloadIndexP15sQuickloadIndexPvj   # LoadQuickloadIndex(sQuickloadIndex*, void*, unsigned int)
  __Z19BuildGeometryTablesj   # BuildGeometryTables(unsigned int)
  __Z20FreeQuickloadIndexesv   # FreeQuickloadIndexes()
  __Z20InitialiseGameStage1v   # InitialiseGameStage1()
  __Z20InitialiseGameStage2v   # InitialiseGameStage2()
  __Z20InitialiseGameStage3v   # InitialiseGameStage3()
  __Z20LoadQuickloadIndexesPKci   # LoadQuickloadIndexes(char const*, int)
  __Z21FindQuickloadFilenamePKci   # FindQuickloadFilename(char const*, int)
  __Z22CheckIndexedFileExistsPKcii   # CheckIndexedFileExists(char const*, int, int)
  __Z22InitialiseQuickloadingv   # InitialiseQuickloading()
  __Z25SetupDirectoryForMeshLoadPcS_S_   # SetupDirectoryForMeshLoad(char*, char*, char*)
  __Z26DoesQuickloadFilenameExistPKcii   # DoesQuickloadFilenameExist(char const*, int, int)
  _gFilePathBuffer
  _gPathLength
  _gPathString
  _gPaths
  _gQuickloadEnabled
  _gQuickloadIndexes
  _gQuickloadInitialised
  _gQuickloadPathCount
  _gQuickloadTextureIndexes
  _gTPConfigLanguage
  _gTPConfigLanguages
  _gTPConfigTitle
)
