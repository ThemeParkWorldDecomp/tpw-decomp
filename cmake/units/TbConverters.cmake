set(TPW_UNIT_TbConverters src/ltms/TbFontRenderParams.cpp src/ltms/TbIRLE4bitFont.cpp src/ltms/TbITable.cpp
    src/ltms/TbIUnicodeToMB.cpp src/ltms/StaticUnicodeBuffer.cpp src/ltms/TbIResourceIndex.cpp)
set(TPW_UNIT_TbConverters_KEEP
  __ZN14TbIMBToUnicode13ConvertStringEPDsPKht   # TbIMBToUnicode::ConvertString(char16_t*, unsigned char const*, unsigned short)
  __ZN14TbIMBToUnicodeD1Ev   # TbIMBToUnicode::~TbIMBToUnicode()
  __ZN14TbIRLE4bitFont11GetCharSizeEt   # TbIRLE4bitFont::GetCharSize(unsigned short)
  __ZN14TbIRLE4bitFont12GetCharCountEv   # TbIRLE4bitFont::GetCharCount()
  __ZN14TbIRLE4bitFont12GetCharIndexERPKh   # TbIRLE4bitFont::GetCharIndex(unsigned char const*&)
  __ZN14TbIRLE4bitFont14GetMBToUnicodeEv   # TbIRLE4bitFont::GetMBToUnicode()
  __ZN14TbIRLE4bitFont18GetVerticalSpacingEv   # TbIRLE4bitFont::GetVerticalSpacing()
  __ZN14TbIRLE4bitFont7GetCapsEv   # TbIRLE4bitFont::GetCaps()
  __ZN14TbIRLE4bitFont7GetTypeEv   # TbIRLE4bitFont::GetType()
  __ZN14TbIRLE4bitFontC1EPhP16TbIResourceIndex   # TbIRLE4bitFont::TbIRLE4bitFont(unsigned char*, TbIResourceIndex*)
  __ZN14TbIRLE4bitFontC1Ev   # TbIRLE4bitFont::TbIRLE4bitFont()
  __ZN14TbIRLE4bitFontD1Ev   # TbIRLE4bitFont::~TbIRLE4bitFont()
  __ZN14TbIUnicodeToMB11LocateValueEDs   # TbIUnicodeToMB::LocateValue(char16_t)
  __ZN14TbIUnicodeToMB13ConvertStringEPhPKDst   # TbIUnicodeToMB::ConvertString(unsigned char*, char16_t const*, unsigned short)
  __ZN14TbIUnicodeToMBD1Ev   # TbIUnicodeToMB::~TbIUnicodeToMB()
  __ZN16TbIResourceIndex10InitialiseEPKcPvj   # TbIResourceIndex::Initialise(char const*, void*, unsigned int)
  __ZN16TbIResourceIndex11GetResourceEPvPS0_   # TbIResourceIndex::GetResource(void*, void**)
  __ZN16TbIResourceIndex12OpenResourceEPNS_17resourceIndexItemEPPv   # TbIResourceIndex::OpenResource(TbIResourceIndex::resourceIndexItem*, void**)
  __ZN16TbIResourceIndex15GetResourceTypeEPv   # TbIResourceIndex::GetResourceType(void*)
  __ZN16TbIResourceIndex16OpenResourceFileERN3bfl10FileSystem5CFileEPNS_17resourceIndexItemE   # TbIResourceIndex::OpenResourceFile(bfl::FileSystem::CFile&, TbIResourceIndex::resourceIndexItem*)
  __ZN16TbIResourceIndex20GetPreOpenedResourceEPvPS0_   # TbIResourceIndex::GetPreOpenedResource(void*, void**)
  __ZN16TbIResourceIndex21GetResourceItemHandleEj   # TbIResourceIndex::GetResourceItemHandle(unsigned int)
  __ZN16TbIResourceIndex5CloseEv   # TbIResourceIndex::Close()
  __ZN16TbIResourceIndex7GetItemEj   # TbIResourceIndex::GetItem(unsigned int)
  __ZN16TbIResourceIndexC1Ev   # TbIResourceIndex::TbIResourceIndex()
  __ZN16TbIResourceIndexD1Ev   # TbIResourceIndex::~TbIResourceIndex()
  __ZN7TbIFontD1Ev   # TbIFont::~TbIFont()
  __ZN8TbITable6AddRefEv   # TbITable::AddRef()
  __ZN8TbITable7ReleaseEv   # TbITable::Release()
  __ZN8TbITableC1EjPvP16TbIResourceIndex   # TbITable::TbITable(unsigned int, void*, TbIResourceIndex*)
  __ZN8TbITableD1Ev   # TbITable::~TbITable()
  __ZN9TbIMBInfoD1Ev   # TbIMBInfo::~TbIMBInfo()
  __ZNK12TableFactory15CreateInterfaceERN3bfl10FileSystem5CFileEPPvjP16TbIResourceIndex   # TableFactory::CreateInterface(bfl::FileSystem::CFile&, void**, unsigned int, TbIResourceIndex*) const
  __ZNK13MBInfoFactory15CreateInterfaceERN3bfl10FileSystem5CFileEPPvjP16TbIResourceIndex   # MBInfoFactory::CreateInterface(bfl::FileSystem::CFile&, void**, unsigned int, TbIResourceIndex*) const
  __ZNK14TbIRLE4bitFont11GetCharDataEt   # TbIRLE4bitFont::GetCharData(unsigned short) const
  __ZNK14TbIRLE4bitFont13GetCharHeaderEt   # TbIRLE4bitFont::GetCharHeader(unsigned short) const
  __ZNK18MBToUnicodeFactory15CreateInterfaceERN3bfl10FileSystem5CFileEPPvjP16TbIResourceIndex   # MBToUnicodeFactory::CreateInterface(bfl::FileSystem::CFile&, void**, unsigned int, TbIResourceIndex*) const
  __ZNK18RLE4bitFontFactory15CreateInterfaceERN3bfl10FileSystem5CFileEPPvjP16TbIResourceIndex   # RLE4bitFontFactory::CreateInterface(bfl::FileSystem::CFile&, void**, unsigned int, TbIResourceIndex*) const
  __ZNK18UnicodeToMBFactory15CreateInterfaceERN3bfl10FileSystem5CFileEPPvjP16TbIResourceIndex   # UnicodeToMBFactory::CreateInterface(bfl::FileSystem::CFile&, void**, unsigned int, TbIResourceIndex*) const
  __ZNK9TbIMBInfo8GetIndexERPKh   # TbIMBInfo::GetIndex(unsigned char const*&) const
  _gLtmsResourceFactories
  __ZN12_GLOBAL__N_12mbE   # mb (static object)
  __ZN12_GLOBAL__N_14fontE   # font (static object)
  __ZN12_GLOBAL__N_14infoE   # info (static object)
  __ZN12_GLOBAL__N_15tableE   # table (static object)
  __ZN12_GLOBAL__N_17unicodeE   # unicode (static object)
)
