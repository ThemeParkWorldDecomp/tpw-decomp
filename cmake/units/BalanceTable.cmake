set(TPW_UNIT_BalanceTable src/game/balance/BalanceLoader.cpp src/game/balance/HoardingsBalance.cpp
    src/game/balance/MainBalance.cpp src/game/balance/OnlineBalance.cpp src/game/balance/OptionsBalance.cpp
    src/game/balance/ResolutionBalance.cpp src/game/balance/SoundBalance.cpp)
set(TPW_UNIT_BalanceTable_KEEP
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
  __ZN13CSoundBalance21GetBalanceDescriptionEi   # CSoundBalance::GetBalanceDescription(int)
  __ZN13CSoundBalance21GetBalanceValuesStartEv   # CSoundBalance::GetBalanceValuesStart()
  __ZN13CSoundBalanceD1Ev   # CSoundBalance::~CSoundBalance()
  __ZN14COnlineBalance21GetBalanceDescriptionEi   # COnlineBalance::GetBalanceDescription(int)
  __ZN14COnlineBalance21GetBalanceValuesStartEv   # COnlineBalance::GetBalanceValuesStart()
  __ZN14COnlineBalanceD1Ev   # COnlineBalance::~COnlineBalance()
  __ZN15COptionsBalance21GetBalanceDescriptionEi   # COptionsBalance::GetBalanceDescription(int)
  __ZN15COptionsBalance21GetBalanceValuesStartEv   # COptionsBalance::GetBalanceValuesStart()
  __ZN15COptionsBalanceD1Ev   # COptionsBalance::~COptionsBalance()
  __ZN17CHoardingsBalance21GetBalanceDescriptionEi   # CHoardingsBalance::GetBalanceDescription(int)
  __ZN17CHoardingsBalance21GetBalanceValuesStartEv   # CHoardingsBalance::GetBalanceValuesStart()
  __ZN17CHoardingsBalanceD1Ev   # CHoardingsBalance::~CHoardingsBalance()
  __ZN18CResolutionBalance21GetBalanceDescriptionEi   # CResolutionBalance::GetBalanceDescription(int)
  __ZN18CResolutionBalance21GetBalanceValuesStartEv   # CResolutionBalance::GetBalanceValuesStart()
  __ZN18CResolutionBalanceD1Ev   # CResolutionBalance::~CResolutionBalance()
  _gHoardingsBalanceDescriptions
  _gMainBalance
  _gMainBalanceDescriptions
  _gOnlineBalance
  _gOnlineBalanceDescriptions
  _gOptionsBalance
  _gOptionsBalanceDescriptions
  _gResolutionBalance
  _gResolutionBalanceDescriptions
  _gSoundBalance
  _gSoundBalanceDescriptions
)
