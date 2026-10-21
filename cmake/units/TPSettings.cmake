set(TPW_UNIT_TPSettings src/game/TPResource.cpp src/game/TPSettings.cpp src/game/balance/OptionsBalance.cpp
    src/game/balance/SoundBalance.cpp)
set(TPW_UNIT_TPSettings_KEEP
  __Z11SwapForFileRN15CPlayerSettings6VolumeE   # SwapForFile(CPlayerSettings::Volume&)
  __Z13LOADVARSTRUCTIN15CPlayerSettings6VolumeEEiPvPT_PKci   # int LOADVARSTRUCT<CPlayerSettings::Volume>(void*, CPlayerSettings::Volume*, char const*, int)
  __Z13RestartDialogv   # RestartDialog()
  __Z13SAVEVARSTRUCTIN15CPlayerSettings6VolumeEEiPvPT_PKci   # int SAVEVARSTRUCT<CPlayerSettings::Volume>(void*, CPlayerSettings::Volume*, char const*, int)
  __Z13UnswapForFileRN15CPlayerSettings6VolumeE   # UnswapForFile(CPlayerSettings::Volume&)
  __Z15QualifyFilenameR23TbDynamicStringTemplateIcES1_S1_   # QualifyFilename(TbDynamicStringTemplate<char>&, TbDynamicStringTemplate<char>&, TbDynamicStringTemplate<char>&)
  __Z16MakeFullFilenameRK23TbDynamicStringTemplateIcERS0_   # MakeFullFilename(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char>&)
  __Z18RestartCallback_OKPv   # RestartCallback_OK(void*)
  __Z22RestartCallback_CancelPv   # RestartCallback_Cancel(void*)
  __Z22strRemoveRootDirectoryR23TbDynamicStringTemplateIcE   # strRemoveRootDirectory(TbDynamicStringTemplate<char>&)
  __Z7LOADVARIN15CPlayerSettings10eVIDEOCARDEEiPvPT_PKci   # int LOADVAR<CPlayerSettings::eVIDEOCARD>(void*, CPlayerSettings::eVIDEOCARD*, char const*, int)
  __Z7LOADVARIN15CPlayerSettings11eRESOLUTIONEEiPvPT_PKci   # int LOADVAR<CPlayerSettings::eRESOLUTION>(void*, CPlayerSettings::eRESOLUTION*, char const*, int)
  __Z7LOADVARIN15CPlayerSettings7eDETAILEEiPvPT_PKci   # int LOADVAR<CPlayerSettings::eDETAIL>(void*, CPlayerSettings::eDETAIL*, char const*, int)
  __Z7LOADVARIN15CPlayerSettings9eRENDEREREEiPvPT_PKci   # int LOADVAR<CPlayerSettings::eRENDERER>(void*, CPlayerSettings::eRENDERER*, char const*, int)
  __Z7SAVEVARIN15CPlayerSettings10eVIDEOCARDEEiPvPT_PKci   # int SAVEVAR<CPlayerSettings::eVIDEOCARD>(void*, CPlayerSettings::eVIDEOCARD*, char const*, int)
  __Z7SAVEVARIN15CPlayerSettings11eRESOLUTIONEEiPvPT_PKci   # int SAVEVAR<CPlayerSettings::eRESOLUTION>(void*, CPlayerSettings::eRESOLUTION*, char const*, int)
  __Z7SAVEVARIN15CPlayerSettings7eDETAILEEiPvPT_PKci   # int SAVEVAR<CPlayerSettings::eDETAIL>(void*, CPlayerSettings::eDETAIL*, char const*, int)
  __Z7SAVEVARIN15CPlayerSettings9eRENDEREREEiPvPT_PKci   # int SAVEVAR<CPlayerSettings::eRENDERER>(void*, CPlayerSettings::eRENDERER*, char const*, int)
  __ZN13CSoundBalance21GetBalanceDescriptionEi   # CSoundBalance::GetBalanceDescription(int)
  __ZN13CSoundBalance21GetBalanceValuesStartEv   # CSoundBalance::GetBalanceValuesStart()
  __ZN13CSoundBalanceD1Ev   # CSoundBalance::~CSoundBalance()
  __ZN13CTPUIResourceC1ERK23TbDynamicStringTemplateIcES3_   # CTPUIResource::CTPUIResource(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __ZN13CTPUIResourceD1Ev   # CTPUIResource::~CTPUIResource()
  __ZN14CTPFMVResourceC1ERK23TbDynamicStringTemplateIcES3_   # CTPFMVResource::CTPFMVResource(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __ZN14CTPFMVResourceD1Ev   # CTPFMVResource::~CTPFMVResource()
  __ZN15COptionsBalance21GetBalanceDescriptionEi   # COptionsBalance::GetBalanceDescription(int)
  __ZN15COptionsBalance21GetBalanceValuesStartEv   # COptionsBalance::GetBalanceValuesStart()
  __ZN15COptionsBalanceD1Ev   # COptionsBalance::~COptionsBalance()
  __ZN15CPlayerSettings10LoadGlobalER23TbDynamicStringTemplateIcE   # CPlayerSettings::LoadGlobal(TbDynamicStringTemplate<char>&)
  __ZN15CPlayerSettings10SaveGlobalER23TbDynamicStringTemplateIcE   # CPlayerSettings::SaveGlobal(TbDynamicStringTemplate<char>&)
  __ZN15CPlayerSettings12SetVideoCardENS_10eVIDEOCARDE   # CPlayerSettings::SetVideoCard(CPlayerSettings::eVIDEOCARD)
  __ZN15CPlayerSettings12UpdateVolumeEv   # CPlayerSettings::UpdateVolume()
  __ZN15CPlayerSettings13ResetSettingsEv   # CPlayerSettings::ResetSettings()
  __ZN15CPlayerSettings13SetResolutionENS_11eRESOLUTIONE   # CPlayerSettings::SetResolution(CPlayerSettings::eRESOLUTION)
  __ZN15CPlayerSettings13StoreSettingsEv   # CPlayerSettings::StoreSettings()
  __ZN15CPlayerSettings14SerialiseLocalEPvij   # CPlayerSettings::SerialiseLocal(void*, int, unsigned int)
  __ZN15CPlayerSettings14UpdateSettingsEv   # CPlayerSettings::UpdateSettings()
  __ZN15CPlayerSettings15SerialiseGlobalEPvij   # CPlayerSettings::SerialiseGlobal(void*, int, unsigned int)
  __ZN15CPlayerSettings15SetRendererTypeENS_9eRENDERERE   # CPlayerSettings::SetRendererType(CPlayerSettings::eRENDERER)
  __ZN15CPlayerSettings15SetSoundQualityEi   # CPlayerSettings::SetSoundQuality(int)
  __ZN15CPlayerSettings17SetGraphicsDetailENS_7eDETAILE   # CPlayerSettings::SetGraphicsDetail(CPlayerSettings::eDETAIL)
  __ZN15CPlayerSettings19LoadGraphicsDetailsENS_7eDETAILE   # CPlayerSettings::LoadGraphicsDetails(CPlayerSettings::eDETAIL)
  __ZN15CPlayerSettings19SetGameRefreshStateENS_19eGAME_REFRESH_STATEE   # CPlayerSettings::SetGameRefreshState(CPlayerSettings::eGAME_REFRESH_STATE)
  __ZN15CPlayerSettings20SetGameContinueStateENS_20eGAME_CONTINUE_STATEE   # CPlayerSettings::SetGameContinueState(CPlayerSettings::eGAME_CONTINUE_STATE)
  __ZN15CPlayerSettings21ResetGameRefreshStateEv   # CPlayerSettings::ResetGameRefreshState()
  __ZN15CPlayerSettings22ResetGameContinueStateEv   # CPlayerSettings::ResetGameContinueState()
  __ZN15CPlayerSettings4InitEv   # CPlayerSettings::Init()
  __ZN15CPlayerSettingsD1Ev   # CPlayerSettings::~CPlayerSettings()
  __ZN15CTPBaseResource13AddWADStorageERK23TbDynamicStringTemplateIcE   # CTPBaseResource::AddWADStorage(TbDynamicStringTemplate<char> const&)
  __ZN15CTPBaseResource14AddAllStoragesERK23TbDynamicStringTemplateIcE   # CTPBaseResource::AddAllStorages(TbDynamicStringTemplate<char> const&)
  __ZN15CTPBaseResource14AddDiscStorageERK23TbDynamicStringTemplateIcE   # CTPBaseResource::AddDiscStorage(TbDynamicStringTemplate<char> const&)
  __ZN15CTPBaseResource15AddLocalStorageERK23TbDynamicStringTemplateIcE   # CTPBaseResource::AddLocalStorage(TbDynamicStringTemplate<char> const&)
  __ZN15CTPBaseResource16AddRemoteStorageERK23TbDynamicStringTemplateIcE   # CTPBaseResource::AddRemoteStorage(TbDynamicStringTemplate<char> const&)
  __ZN15CTPBaseResource17OpenQualifiedFileERN3bfl10FileSystem5CFileERK23TbDynamicStringTemplateIcE   # CTPBaseResource::OpenQualifiedFile(bfl::FileSystem::CFile&, TbDynamicStringTemplate<char> const&)
  __ZN15CTPBaseResource8FindNextERN3bfl10FileSystem9CFileInfoE   # CTPBaseResource::FindNext(bfl::FileSystem::CFileInfo&)
  __ZN15CTPBaseResource8OpenFileERN3bfl10FileSystem5CFileERK23TbDynamicStringTemplateIcERNS1_12CFileStorageEj   # CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, TbDynamicStringTemplate<char> const&, bfl::FileSystem::CFileStorage&, unsigned int)
  __ZN15CTPBaseResource8OpenFileERN3bfl10FileSystem5CFileERK23TbDynamicStringTemplateIcEj   # CTPBaseResource::OpenFile(bfl::FileSystem::CFile&, TbDynamicStringTemplate<char> const&, unsigned int)
  __ZN15CTPBaseResource9FindFirstERK23TbDynamicStringTemplateIcERN3bfl10FileSystem9CFileInfoE   # CTPBaseResource::FindFirst(TbDynamicStringTemplate<char> const&, bfl::FileSystem::CFileInfo&)
  __ZN15CTPBaseResourceD1Ev   # CTPBaseResource::~CTPBaseResource()
  __ZN15CTPFontResourceC1ERK23TbDynamicStringTemplateIcES3_   # CTPFontResource::CTPFontResource(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __ZN15CTPFontResourceD1Ev   # CTPFontResource::~CTPFontResource()
  __ZN15CTPIntermediateD1Ev   # CTPIntermediate::~CTPIntermediate()
  __ZN16CTPSoundResourceC1ERK23TbDynamicStringTemplateIcES3_   # CTPSoundResource::CTPSoundResource(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __ZN16CTPSoundResourceD1Ev   # CTPSoundResource::~CTPSoundResource()
  __ZN17CTPSpriteResourceC1ERK23TbDynamicStringTemplateIcES3_   # CTPSpriteResource::CTPSpriteResource(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __ZN17CTPSpriteResourceD1Ev   # CTPSpriteResource::~CTPSpriteResource()
  __ZN18CTPAdvisorResourceC1ERK23TbDynamicStringTemplateIcES3_   # CTPAdvisorResource::CTPAdvisorResource(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __ZN18CTPAdvisorResourceD1Ev   # CTPAdvisorResource::~CTPAdvisorResource()
  __ZN18CTPResourceManager12FindResourceERK23TbDynamicStringTemplateIcEPP15CTPBaseResource   # CTPResourceManager::FindResource(TbDynamicStringTemplate<char> const&, CTPBaseResource**)
  __ZN18CTPResourceManager16RegisterResourceEP15CTPBaseResource   # CTPResourceManager::RegisterResource(CTPBaseResource*)
  __ZN18CTPResourceManager16RegisterResourceEP24CTPSharedTextureResource   # CTPResourceManager::RegisterResource(CTPSharedTextureResource*)
  __ZN18CTPResourceManager18UnregisterResourceEP15CTPBaseResource   # CTPResourceManager::UnregisterResource(CTPBaseResource*)
  __ZN18CTPResourceManager19pSetCurrentResourceEP15CTPBaseResource   # CTPResourceManager::pSetCurrentResource(CTPBaseResource*)
  __ZN18CTPResourceManager8OpenFileERN3bfl10FileSystem5CFileERK23TbDynamicStringTemplateIcE   # CTPResourceManager::OpenFile(bfl::FileSystem::CFile&, TbDynamicStringTemplate<char> const&)
  __ZN18CTPResourceManagerC1Ev   # CTPResourceManager::CTPResourceManager()
  __ZN18CTPResourceManagerD1Ev   # CTPResourceManager::~CTPResourceManager()
  __ZN18CTPTerrainResourceC1ERK23TbDynamicStringTemplateIcES3_   # CTPTerrainResource::CTPTerrainResource(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __ZN18CTPTerrainResourceD1Ev   # CTPTerrainResource::~CTPTerrainResource()
  __ZN18CTPTextureResourceC1ERK23TbDynamicStringTemplateIcES3_   # CTPTextureResource::CTPTextureResource(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __ZN18CTPTextureResourceD1Ev   # CTPTextureResource::~CTPTextureResource()
  __ZN19CTPLanguageResourceC1ERK23TbDynamicStringTemplateIcES3_   # CTPLanguageResource::CTPLanguageResource(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __ZN19CTPLanguageResourceD1Ev   # CTPLanguageResource::~CTPLanguageResource()
  __ZN24CTPSharedTextureResourceC1ERK23TbDynamicStringTemplateIcES3_   # CTPSharedTextureResource::CTPSharedTextureResource(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __ZN24CTPSharedTextureResourceD1Ev   # CTPSharedTextureResource::~CTPSharedTextureResource()
  __ZNK15CPlayerSettings12GetSFXVolumeEv   # CPlayerSettings::GetSFXVolume() const
  __ZNK15CPlayerSettings14GetMovieVolumeEv   # CPlayerSettings::GetMovieVolume() const
  __ZNK15CPlayerSettings14GetMusicVolumeEv   # CPlayerSettings::GetMusicVolume() const
  __ZNK15CPlayerSettings15GetSpeechVolumeEv   # CPlayerSettings::GetSpeechVolume() const
  __ZNK15CPlayerSettings17HasNumKidsChangedEii   # CPlayerSettings::HasNumKidsChanged(int, int) const
  __ZNK15CPlayerSettings18HasMPEGRateChangedEii   # CPlayerSettings::HasMPEGRateChanged(int, int) const
  __ZNK15CPlayerSettings21HasTextureSizeChangedEii   # CPlayerSettings::HasTextureSizeChanged(int, int) const
  __ZNK15CTPBaseResource17OpenQualifiedFileERN3bfl10FileSystem5CFileERK23TbDynamicStringTemplateIcERNS1_12CFileStorageE   # CTPBaseResource::OpenQualifiedFile(bfl::FileSystem::CFile&, TbDynamicStringTemplate<char> const&, bfl::FileSystem::CFileStorage&) const
  __ZNK15CTPBaseResource24pGetCurrentSearchStorageEv   # CTPBaseResource::pGetCurrentSearchStorage() const
  __ZNK15CTPBaseResource24pGetLastFoundFileStorageEv   # CTPBaseResource::pGetLastFoundFileStorage() const
  __ZNK15CTPIntermediate10GetSamFileERN3bfl10FileSystem12CFileStorageERNS1_5CFileE   # CTPIntermediate::GetSamFile(bfl::FileSystem::CFileStorage&, bfl::FileSystem::CFile&) const
  __ZNK15CTPIntermediate16GetOnlineSamFileERN3bfl10FileSystem12CFileStorageERNS1_5CFileE   # CTPIntermediate::GetOnlineSamFile(bfl::FileSystem::CFileStorage&, bfl::FileSystem::CFile&) const
  __ZNK16CTPSoundResource13FindSoundFileERK23TbDynamicStringTemplateIcERS1_   # CTPSoundResource::FindSoundFile(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char>&) const
  __ZNK18CTPResourceManager19pGetCurrentResourceEv   # CTPResourceManager::pGetCurrentResource() const
  _gOptionsBalance
  _gOptionsBalanceDescriptions
  _gPlayerSettings
  _gSoundBalance
  _gSoundBalanceDescriptions
  _gTPSettingsLanguage
  _gTPSettingsLanguages
  _gTPSettingsTitle
  __ZTS7TbTListIP15CTPBaseResourceS1_E   # template class TbTList<CTPBaseResource*, CTPBaseResource*> (explicit instantiation)
  __ZTS7TbTListIPN3bfl10FileSystem12CFileStorageES3_E   # template class TbTList<bfl::FileSystem::CFileStorage*, bfl::FileSystem::CFileStorage*> (explicit instantiation)
  __ZTS9TbPtrListI15CTPBaseResourceE   # template class TbPtrList<CTPBaseResource> (explicit instantiation)
  __ZTS9TbPtrListIN3bfl10FileSystem12CFileStorageEE   # template class TbPtrList<bfl::FileSystem::CFileStorage> (explicit instantiation)
)
