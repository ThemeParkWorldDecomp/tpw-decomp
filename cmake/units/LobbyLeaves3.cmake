# The lobby's small leaves, part 3 (unit w59): the ride database's one-argument GetTypeName and the two sign-name members, the theme
# gate names, FLY_Close and RSSE_Close (one test module, test_LobbyLeaves3). The strings are the real bullfrog strings; the string
# table, the macdoze compare, the heap, FLY_RemoveBase, KillScript and GAME_ASSERTMSG are fakes in the unit file.
set(TPW_UNIT_LobbyLeaves3 src/game/rides/RideDB.cpp src/game/save/SaveLoad.cpp src/game/Game.cpp src/game/ui/3DFont.cpp
    src/game/rides/FlyingRide.cpp src/game/rides/rsse.cpp src/bullfrog/FileSystem.cpp platform/msl.cpp)
set(TPW_UNIT_LobbyLeaves3_KEEP
  __Z10RSSE_Closev   # RSSE_Close()
  __Z10TPUniStrEqRK23TbDynamicStringTemplateIDsES2_   # TPUniStrEq(TbDynamicStringTemplate<char16_t> const&, TbDynamicStringTemplate<char16_t> const&)
  __Z15RemoveExtensionRK23TbDynamicStringTemplateIcERS0_   # RemoveExtension(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char>&)
  __Z17StringToUniStringR23TbDynamicStringTemplateIDsERKS_IcE   # StringToUniString(TbDynamicStringTemplate<char16_t>&, TbDynamicStringTemplate<char> const&)
  __Z17UniStringToStringR23TbDynamicStringTemplateIcERKS_IDsE   # UniStringToString(TbDynamicStringTemplate<char>&, TbDynamicStringTemplate<char16_t> const&)
  __Z7TPStrEqRK23TbDynamicStringTemplateIcES2_   # TPStrEq(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __Z8TPStrGtrRK23TbDynamicStringTemplateIcES2_   # TPStrGtr(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char> const&)
  __Z9FLY_Closev   # FLY_Close()
  __ZN11CRideNameDB7GetNameEiPDsi   # CRideNameDB::GetName(int, char16_t*, int)
  __ZN11CRideNameDB8GetNameAEiPDsib   # CRideNameDB::GetNameA(int, char16_t*, int, bool)
  __ZN11CRideNameDB8GetNameBEiPDsi   # CRideNameDB::GetNameB(int, char16_t*, int)
  __ZN12CRideBalance21GetBalanceDescriptionEi   # CRideBalance::GetBalanceDescription(int)
  __ZN12CRideBalance21GetBalanceValuesStartEv   # CRideBalance::GetBalanceValuesStart()
  __ZN13CRideDatabase11GetFileNameE12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CRideDatabase::GetFileName(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>)
  __ZN13CRideDatabase11GetTypeNameEt   # CRideDatabase::GetTypeName(unsigned short)
  __ZN13CRideDatabase11GetTypeNameEtPDsj   # CRideDatabase::GetTypeName(unsigned short, char16_t*, unsigned int)
  __ZN13CRideDatabase12GetSignNameAEtPDsj   # CRideDatabase::GetSignNameA(unsigned short, char16_t*, unsigned int)
  __ZN13CRideDatabase12GetSignNameBEtPDsj   # CRideDatabase::GetSignNameB(unsigned short, char16_t*, unsigned int)
  __ZN5CGame19GetCurrentThemeNameEv   # CGame::GetCurrentThemeName()
  __ZNK5CGame15GetCurrentThemeEv   # CGame::GetCurrentTheme() const
  __ZNK5CGame26GetLocalisedThemeGateNamesERK23TbDynamicStringTemplateIDsERS1_S4_   # CGame::GetLocalisedThemeGateNames(TbDynamicStringTemplate<char16_t> const&, TbDynamicStringTemplate<char16_t>&, TbDynamicStringTemplate<char16_t>&) const
  _gFlyActive
  _gFlyBases
  _gFlyInitialised
  _gFlySinTable
  _gGameThemeFantasy
  _gGameThemeHallow
  _gGameThemeJungle
  _gGameThemeSpace
  _gRSSEMallocCount
  _gRSSEState
  _gRideBalanceDescriptions
  _gRideMissingName
  _gRideNameTable
  __ZN22TbStaticStringTemplateIcE8ResizeToEj   # template void TbStaticStringTemplate<char>::ResizeTo(uint32_t) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEC1EPcji   # template TbStaticStringTemplate<char>::TbStaticStringTemplate(char*, uint32_t, int32_t) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcED1Ev   # template TbStaticStringTemplate<char>::~TbStaticStringTemplate() (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEaSEPKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const char*) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEaSERK12TbStringBaseIcE   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const TbStringBase<char>&) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEaSERKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const char&) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEpLEPKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const char*) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEpLERK12TbStringBaseIcE   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const TbStringBase<char>&) (explicit instantiation)
  __ZN22TbStaticStringTemplateIcEpLERKc   # template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const char&) (explicit instantiation)
  __ZTS12TbStringBaseIDsE   # template class TbStringBase<char16_t> (explicit instantiation)
  __ZTS12TbStringBaseIcE   # template class TbStringBase<char> (explicit instantiation)
  __ZTS23TbDynamicStringTemplateIDsE   # template class TbDynamicStringTemplate<char16_t> (explicit instantiation)
  __ZTS23TbDynamicStringTemplateIcE   # template class TbDynamicStringTemplate<char> (explicit instantiation)
  tmpl_10f1a43abff8@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbString.cpp had before the regroup)
  tmpl_12b48e599676@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_13aaac678a90@bullfrog/FileSystem.cpp   # ~TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_156c9453e350@bullfrog/FileSystem.cpp   # ResizeTo (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_177fef366926@bullfrog/FileSystem.cpp   # SBCstrcpy (a template definition src/TbString.cpp had before the regroup)
  tmpl_1b2c80ece410@bullfrog/FileSystem.cpp   # FindOneOf (a template definition src/TbString.cpp had before the regroup)
  tmpl_1eedb09deef3@bullfrog/FileSystem.cpp   # SBCstrncat (a template definition src/TbString.cpp had before the regroup)
  tmpl_1fb948d1a8d7@bullfrog/FileSystem.cpp   # operator== (a template definition src/TbString.cpp had before the regroup)
  tmpl_31520eb2cde0@bullfrog/FileSystem.cpp   # operator< (a template definition src/TbString.cpp had before the regroup)
  tmpl_3b898e0ec42d@bullfrog/FileSystem.cpp   # Length (a template definition src/TbString.cpp had before the regroup)
  tmpl_3f76b5cf6898@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
  tmpl_4209e9eb338a@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_444e69a72b25@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_47746be482c6@bullfrog/FileSystem.cpp   # Right (a template definition src/TbString.cpp had before the regroup)
  tmpl_5072774eb760@bullfrog/FileSystem.cpp   # Find (a template definition src/TbString.cpp had before the regroup)
  tmpl_56b8c8679dbb@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
  tmpl_58045e5a0a73@bullfrog/FileSystem.cpp   # AllocBuffer (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_5e6789bb3ff3@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_62d628fca008@bullfrog/FileSystem.cpp   # FindReverseOneOf (a template definition src/TbString.cpp had before the regroup)
  tmpl_67728c1f6bc2@bullfrog/FileSystem.cpp   # ResizeTo (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_6fee3b71ae69@bullfrog/FileSystem.cpp   # LockBuffer (a template definition src/TbString.cpp had before the regroup)
  tmpl_7a81918ce0ed@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_7d7b5671d9c8@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbString.cpp had before the regroup)
  tmpl_7f7c954a7476@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_8078b9a106a6@bullfrog/FileSystem.cpp   # Find (a template definition src/TbString.cpp had before the regroup)
  tmpl_89c56dab9bda@bullfrog/FileSystem.cpp   # Left (a template definition src/TbString.cpp had before the regroup)
  tmpl_90c753af4ad8@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_923c787ed96b@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbString.cpp had before the regroup)
  tmpl_9329dea9cdff@bullfrog/FileSystem.cpp   # operator== (a template definition src/TbString.cpp had before the regroup)
  tmpl_966b985e3b04@bullfrog/FileSystem.cpp   # TbDynamicStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_a2ea62013ee7@bullfrog/FileSystem.cpp   # SBCstrlen (a template definition src/TbString.cpp had before the regroup)
  tmpl_a65764511fd4@bullfrog/FileSystem.cpp   # ReleaseBuffer (a template definition src/TbString.cpp had before the regroup)
  tmpl_aa572ba6c8bb@bullfrog/FileSystem.cpp   # ~TbStaticStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_b04e950cbb16@bullfrog/FileSystem.cpp   # FindReverse (a template definition src/TbString.cpp had before the regroup)
  tmpl_be6e319ad7a7@bullfrog/FileSystem.cpp   # TbStaticStringTemplate<Char> (a template definition src/TbString.cpp had before the regroup)
  tmpl_bf7094367fc1@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
  tmpl_c2f8bf00de94@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_c4b2c4a972d6@bullfrog/FileSystem.cpp   # Mid (a template definition src/TbString.cpp had before the regroup)
  tmpl_cd6077897b68@bullfrog/FileSystem.cpp   # operator!= (a template definition src/TbString.cpp had before the regroup)
  tmpl_d3aef466ce77@bullfrog/FileSystem.cpp   # SBCstrcat (a template definition src/TbString.cpp had before the regroup)
  tmpl_d3d969d3322f@bullfrog/FileSystem.cpp   # SBCstrncpy (a template definition src/TbString.cpp had before the regroup)
  tmpl_d4a4fb92aa5f@bullfrog/FileSystem.cpp   # Mid (a template definition src/TbString.cpp had before the regroup)
  tmpl_d809f9651e41@bullfrog/FileSystem.cpp   # operator+= (a template definition src/TbStringBuffer.cpp had before the regroup)
  tmpl_feba63a511e0@bullfrog/FileSystem.cpp   # operator= (a template definition src/TbString.cpp had before the regroup)
)
