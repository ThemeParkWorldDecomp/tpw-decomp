# Unit w221: the registry and remaining file wrappers, outside callees faked in the unit.
set(TPW_UNIT_RegistryFiles src/bullfrog/TbRegistryKey.cpp src/bullfrog/LbApp.cpp src/bullfrog/LbFile.cpp
    platform/msl.cpp)
set(TPW_UNIT_RegistryFiles_KEEP
  __Z11LbFile_OpenPPvPKcj   # LbFile_Open(void**, char const*, unsigned int)
  __Z11LbFile_SavePKcPvj   # LbFile_Save(char const*, void*, unsigned int)
  __Z12LbFile_FlushPv   # LbFile_Flush(void*)
  __Z12LbMaxPointerRKPcS1_   # LbMaxPointer(char* const&, char* const&)
  __Z13LbMem_ReAllocPvjj   # LbMem_ReAlloc(void*, unsigned int, unsigned int)
  __Z14LbFile_FindEndP10TbFileFind   # LbFile_FindEnd(TbFileFind*)
  __Z15LbFile_FindNextP10TbFileFind   # LbFile_FindNext(TbFileFind*)
  __Z15LbFile_MakePathPKc   # LbFile_MakePath(char const*)
  __Z16LbFile_AllocLoadPKcPvPi   # LbFile_AllocLoad(char const*, void*, int*)
  __Z16LbFile_FindFirstPKcP10TbFileFindj   # LbFile_FindFirst(char const*, TbFileFind*, unsigned int)
  __Z18LbFile_GetPositionPv   # LbFile_GetPosition(void*)
  __Z19LbFile_GetFileStampPvP11TbFileStampS1_S1_   # LbFile_GetFileStamp(void*, TbFileStamp*, TbFileStamp*, TbFileStamp*)
  __Z19LbFile_SetFileStampPvPK11TbFileStampS2_S2_   # LbFile_SetFileStamp(void*, TbFileStamp const*, TbFileStamp const*, TbFileStamp const*)
  __Z22LbFile_GetCurrentDrivev   # LbFile_GetCurrentDrive()
  __Z24LbFile_GetFreeDriveSpacecP11TbDriveInfo   # LbFile_GetFreeDriveSpace(char, TbDriveInfo*)
  __Z25LbFile_GetProjectFilenamePKc   # LbFile_GetProjectFilename(char const*)
  __ZN13TbRegistryKey10DeleteItemEPKc   # TbRegistryKey::DeleteItem(char const*)
  __ZN13TbRegistryKey10InitialiseEv   # TbRegistryKey::Initialise()
  __ZN13TbRegistryKey11_OpenSubKeyEPKcj   # TbRegistryKey::_OpenSubKey(char const*, unsigned int)
  __ZN13TbRegistryKey12CreateSubKeyEPKcRS_   # TbRegistryKey::CreateSubKey(char const*, TbRegistryKey&)
  __ZN13TbRegistryKey13OpenEAGameKeyEPKci   # TbRegistryKey::OpenEAGameKey(char const*, int)
  __ZN13TbRegistryKey13_CreateSubKeyEPKcji   # TbRegistryKey::_CreateSubKey(char const*, unsigned int, int)
  __ZN13TbRegistryKey19OpenBullfrogGameKeyEPKci   # TbRegistryKey::OpenBullfrogGameKey(char const*, int)
  __ZN13TbRegistryKey7OpenKeyEPKc   # TbRegistryKey::OpenKey(char const*)
  __ZN13TbRegistryKey8CloseKeyEv   # TbRegistryKey::CloseKey()
  __ZN13TbRegistryKey9CreateKeyEPKc   # TbRegistryKey::CreateKey(char const*)
  __ZN13TbRegistryKey9ListItemsER16TbCharStringListj   # TbRegistryKey::ListItems(TbCharStringList&, unsigned int)
  __ZN13TbRegistryKeyC1ERKS_   # TbRegistryKey::TbRegistryKey(TbRegistryKey const&)
  __ZN13TbRegistryKeyC1Ev   # TbRegistryKey::TbRegistryKey()
  __ZN13TbRegistryKeyD1Ev   # TbRegistryKey::~TbRegistryKey()
  __ZN13TbRegistryKeyaSERKS_   # TbRegistryKey::operator=(TbRegistryKey const&)
  __ZNK13TbRegistryKey10OpenSubKeyEPKcRS_   # TbRegistryKey::OpenSubKey(char const*, TbRegistryKey&) const
  __ZNK13TbRegistryKey11GetItemTypeEPKc   # TbRegistryKey::GetItemType(char const*) const
  __ZNK13TbRegistryKey12GetValueGUIDEPKcRN10NS_MacDoze5_GUIDE   # TbRegistryKey::GetValueGUID(char const*, NS_MacDoze::_GUID&) const
  __ZNK13TbRegistryKey12GetValueGUIDEPKcRN10NS_MacDoze5_GUIDERKS3_   # TbRegistryKey::GetValueGUID(char const*, NS_MacDoze::_GUID&, NS_MacDoze::_GUID const&) const
  __ZNK13TbRegistryKey12GetValueSizeEPKc   # TbRegistryKey::GetValueSize(char const*) const
  __ZNK13TbRegistryKey12SetValueGUIDEPKcRKN10NS_MacDoze5_GUIDE   # TbRegistryKey::SetValueGUID(char const*, NS_MacDoze::_GUID const&) const
  __ZNK13TbRegistryKey13GetValueULONGEPKcRj   # TbRegistryKey::GetValueULONG(char const*, unsigned int&) const
  __ZNK13TbRegistryKey13GetValueULONGEPKcRjj   # TbRegistryKey::GetValueULONG(char const*, unsigned int&, unsigned int) const
  __ZNK13TbRegistryKey13SetValueULONGEPKcj   # TbRegistryKey::SetValueULONG(char const*, unsigned int) const
  __ZNK13TbRegistryKey14GetValueStringEPKcPcjS1_   # TbRegistryKey::GetValueString(char const*, char*, unsigned int, char const*) const
  __ZNK13TbRegistryKey14SetValueStringEPKcS1_   # TbRegistryKey::SetValueString(char const*, char const*) const
  __ZNK13TbRegistryKey8GetValueEPKcPvj   # TbRegistryKey::GetValue(char const*, void*, unsigned int) const
  __ZNK13TbRegistryKey8SetValueEPKc13TbRegItemTypePKvj   # TbRegistryKey::SetValue(char const*, TbRegItemType, void const*, unsigned int) const
)
set(TPW_UNIT_RegistryFiles_OWN_CALLLOG ON)
