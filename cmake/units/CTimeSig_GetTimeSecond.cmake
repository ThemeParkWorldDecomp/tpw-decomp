# Unit v51118: small stand-ins (CTimeSig inline members, CSaveLoad logs, destructors and constructors; src/game/park/SkyObjects.cpp, src/game/save/SaveLoad.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/MessageCentre.cpp).
# The callees outside the unit are recording fakes in tests/unit_CTimeSig_GetTimeSecond.cpp, which also defines the globals of other files.
set(TPW_UNIT_CTimeSig_GetTimeSecond src/game/park/SkyObjects.cpp src/game/save/SaveLoad.cpp
    src/game/ui/InterfaceFont.cpp src/game/ui/MessageCentre.cpp platform/arena.cpp)
set(TPW_UNIT_CTimeSig_GetTimeSecond_KEEP
  __ZN11CSkyObjectsD1Ev   # CSkyObjects::~CSkyObjects()
  __ZN13CMsgCheatModeC1Eb   # CMsgCheatMode::CMsgCheatMode(bool)
  __ZN18fontwrapper_callee18GetVerticalSpacingEP7TbIFont   # fontwrapper_callee::GetVerticalSpacing(TbIFont*)
  __ZN19ParkThemeTreeCallee11PairDestroyEP13ParkThemeTreePNSt3__14pairIK23TbDynamicStringTemplateIcEP17CFSGlobalParkDataEE   # ParkThemeTreeCallee::PairDestroy(ParkThemeTree*, std::__1::pair<TbDynamicStringTemplate<char> const, CFSGlobalParkData*>*)
  __ZN19ParkThemeTreeCallee14NodeDeallocateEP13ParkThemeTreePNS0_4nodeEm   # ParkThemeTreeCallee::NodeDeallocate(ParkThemeTree*, ParkThemeTree::node*, unsigned long)
  __ZN19ParkThemeTreeCallee8PairDtorEPNSt3__14pairIK23TbDynamicStringTemplateIcEP17CFSGlobalParkDataEEs   # ParkThemeTreeCallee::PairDtor(std::__1::pair<TbDynamicStringTemplate<char> const, CFSGlobalParkData*>*, short)
  __ZN20InterfaceFontWrapper9GetHeightEv   # InterfaceFontWrapper::GetHeight()
  __ZN27CMsgResearchCategoryUpdatedC1E11ResearchCat   # CMsgResearchCategoryUpdated::CMsgResearchCategoryUpdated(ResearchCat)
  __ZN6v5111810U16SetDtorEP9MacU16Sets   # v51118::U16SetDtor(MacU16Set*, short)
  __ZN6v5111813KeyStringDtorEPvs   # v51118::KeyStringDtor(void*, short)
  __ZN6v5111814OperatorDeleteEPv   # v51118::OperatorDelete(void*)
  __ZN6v5111814SkyObjectsDtorEP11CSkyObjectss   # v51118::SkyObjectsDtor(CSkyObjects*, short)
  __ZN6v5111815RideBalanceDtorEP12CRideBalances   # v51118::RideBalanceDtor(CRideBalance*, short)
  __ZN6v5111815WStringListCtorEP14MacWStringList   # v51118::WStringListCtor(MacWStringList*)
  __ZN6v5111815WStringListDtorEP14MacWStringLists   # v51118::WStringListDtor(MacWStringList*, short)
  __ZN6v5111816RideDatabaseDtorEP13CRideDatabases   # v51118::RideDatabaseDtor(CRideDatabase*, short)
  __ZN6v5111817ParkThemeTreeCtorEP13ParkThemeTreePKh   # v51118::ParkThemeTreeCtor(ParkThemeTree*, unsigned char const*)
  __ZN9CSaveLoad11OpenLogfileEi   # CSaveLoad::OpenLogfile(int)
  __ZN9CSaveLoad12CloseLogfileEv   # CSaveLoad::CloseLogfile()
  __ZN9CSaveLoad17PreSaveOperationsEv   # CSaveLoad::PreSaveOperations()
  __ZN9CSaveLoad17WriteSerialiseLogEiPKcNS_12LogfileStateE   # CSaveLoad::WriteSerialiseLog(int, char const*, CSaveLoad::LogfileState)
  _gParkTreeCtorWord
  _gWStringListCtorWord
)
