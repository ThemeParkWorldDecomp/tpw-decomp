# The ESP sprite loader (unit w33): its file list, sorter, loader and the sprite lists' set-up; the lists' array is HLSprite.cpp's.
set(TPW_UNIT_ESPSprites src/game/HLSprite.cpp src/game/TPGlobals.cpp platform/arena.cpp)
set(TPW_UNIT_ESPSprites_KEEP
  __Z15ESP_LoadSpritesb   # ESP_LoadSprites(bool)
  __Z16ESP_FindESPFilesR17CTPSpriteResourceRK23TbDynamicStringTemplateIcE   # ESP_FindESPFiles(CTPSpriteResource&, TbDynamicStringTemplate<char> const&)
  __Z16ESP_SortFileListv   # ESP_SortFileList()
  __Z21ESP_GetParticleSpriteiii   # ESP_GetParticleSprite(int, int, int)
  __Z28ESP_InitialiseAndLoadSpritesbb   # ESP_InitialiseAndLoadSprites(bool, bool)
  __ZN11ESP_CSprite15LoadFromESPFileERN3bfl10FileSystem9CFileInfoERNS1_12CFileStorageE   # ESP_CSprite::LoadFromESPFile(bfl::FileSystem::CFileInfo&, bfl::FileSystem::CFileStorage&)
  __ZN11ESP_CSpriteC1Ev   # ESP_CSprite::ESP_CSprite()
  __ZN9TPGlobalsC1Ev   # TPGlobals::TPGlobals()
  __ZN9TPGlobalsD1Ev   # TPGlobals::~TPGlobals()
  __ZNK11ESP_CSprite13GetCustomDataEiPiP4ACTSS0_S0_   # ESP_CSprite::GetCustomData(int, int*, ACTS*, int*, int*) const
  __ZNK11ESP_CSprite15GetActionFramesEi   # ESP_CSprite::GetActionFrames(int) const
  _gESPCellsHigh
  _gESPCellsWide
  _gESPCompressed
  _gESPDrawEnabled
  _gESPF20
  _gESPF34
  _gESPF88
  _gESPFileCount
  _gESPFileInfo
  _gESPFiles
  _gESPFirstPerson
  _gESPGlobalInstances
  _gESPGlobalList
  _gESPGroups
  _gESPKidFiles
  _gESPListInstances
  _gESPMapHeight
  _gESPMapWidth
  _gESPPathBuffer
  _gESPSemitransparent
  _gESPSpriteCount
  _gESPSprites
  _gESPThirdPerson
  _gESPTotalSprites
  _gHLSpriteLists
  _gSpriteHotX
  _gSpriteHotY
)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_ESPSprites_OWN_CALLLOG ON)
