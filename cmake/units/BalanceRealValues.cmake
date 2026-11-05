# The real main balance table, loaded from the game's Standard.sam by the real reader, under the code that reads it: the visitors'
# happiness changes and the 2D map's sprite scales. The callees outside are recording fakes in tests/unit_BalanceRealValues.cpp.
set(TPW_UNIT_BalanceRealValues src/game/balance/BalanceLoader.cpp src/game/balance/MainBalance.cpp src/game/people/Person.cpp
    src/game/ui/ui_2dmap.cpp platform/arena.cpp)
set(TPW_UNIT_BalanceRealValues_KEEP
  __Z11read_stringPPcS0_   # read_string(char**, char**)
  __Z14read_array_rowPPcPi   # read_array_row(char**, int*)
  __Z16read_field_namesPPcPA64_cPi   # read_field_names(char**, char (*) [64], int*)
  __Z16read_float_valuePPcPf   # read_float_value(char**, float*)
  __Z17read_map_internalPPcPP9RideShapeS0_PK9SymbolKeyi   # read_map_internal(char**, RideShape**, char**, SymbolKey const*, int)
  __Z17recognise_delimitPPc   # recognise_delimit(char**)
  __Z18read_integer_valuePPcPi   # read_integer_value(char**, int*)
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN13CBalanceTable11AssignValueEPfif   # CBalanceTable::AssignValue(float*, int, float)
  __ZN13CBalanceTable11AssignValueEPiii   # CBalanceTable::AssignValue(int*, int, int)
  __ZN13CBalanceTable11FreeStringsEv   # CBalanceTable::FreeStrings()
  __ZN13CBalanceTable21LocateBalanceTableRowEPcS0_Piii   # CBalanceTable::LocateBalanceTableRow(char*, char*, int*, int, int)
  __ZN13CBalanceTable23InitialiseBalanceValuesENS_8InitTypeE   # CBalanceTable::InitialiseBalanceValues(CBalanceTable::InitType)
  __ZN13CBalanceTable27CountBalanceDescriptionRowsEv   # CBalanceTable::CountBalanceDescriptionRows()
  __ZN13CBalanceTable4InitEv   # CBalanceTable::Init()
  __ZN13CBalanceTable4LoadEPKcNS_8LoadModeE   # CBalanceTable::Load(char const*, CBalanceTable::LoadMode)
  __ZN13CBalanceTable4LoadEPKcRN3bfl10FileSystem5CFileENS_8LoadModeE   # CBalanceTable::Load(char const*, bfl::FileSystem::CFile&, CBalanceTable::LoadMode)
  __ZN13CBalanceTable5ParseEPKcPcNS_8LoadModeE   # CBalanceTable::Parse(char const*, char*, CBalanceTable::LoadMode)
  __ZN13CBalanceTable6UnInitEv   # CBalanceTable::UnInit()
  __ZN7CPerson14LowerHappinessENS_6AmountE   # CPerson::LowerHappiness(CPerson::Amount)
  __ZN7CPerson14RaiseHappinessENS_6AmountE   # CPerson::RaiseHappiness(CPerson::Amount)
  __ZNK7CPerson18GetHappinessAmountENS_6AmountE   # CPerson::GetHappinessAmount(CPerson::Amount) const
  __ZN7CPerson22ExtraExcitementEffectsEPK7CObject   # CPerson::ExtraExcitementEffects(CObject const*)
  __ZN8CUI2DMap16CreateMapSpritesEv   # CUI2DMap::CreateMapSprites()
  _gMainBalance
  _gMainBalanceDescriptions
)
