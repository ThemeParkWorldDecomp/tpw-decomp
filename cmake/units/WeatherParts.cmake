# Unit w181: CWeather (FinalDestroy, Serialise, ReceiveMessage, FindLightningCoords, SetQuality, the wind setters and UpdateWindInfoToEngine),
# CWeatherForecast (Serialise, Invalidate, Set) and the four save instances over D3DVECTOR_SWAPABLE and PrecipitationType, with the real basic
# serialise instances (the file is the fake), the real message classes (the test's message is a subclass) and the real balance storage. Every
# other callee (the base thing, the random numbers, the funny time, the time span, the rain, the precipitation type, the file) is a fake in
# tests/unit_WeatherParts.cpp.
set(TPW_UNIT_WeatherParts src/game/park/Weather.cpp src/game/ControlManager.cpp src/game/Object.cpp
    src/game/advisor/Advisor.cpp src/game/park/BankAccount.cpp src/game/park/analyser.cpp src/game/rides/FlyingRide.cpp
    src/game/save/FS_MasterSave.cpp src/game/park/ChallengeHQ.cpp src/game/ui/MessageCentre.cpp
    src/game/balance/MainBalance.cpp)
set(TPW_UNIT_WeatherParts_KEEP
  __Z13LOADVARSTRUCTI11TbTimeStampEiPvPT_PKci   # int LOADVARSTRUCT<TbTimeStamp>(void*, TbTimeStamp*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>*, char const*, int)
  __Z13LOADVARSTRUCTI18D3DVECTOR_SWAPABLEEiPvPT_PKci   # int LOADVARSTRUCT<D3DVECTOR_SWAPABLE>(void*, D3DVECTOR_SWAPABLE*, char const*, int)
  __Z13SAVEVARSTRUCTI11TbTimeStampEiPvPT_PKci   # int SAVEVARSTRUCT<TbTimeStamp>(void*, TbTimeStamp*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTI18D3DVECTOR_SWAPABLEEiPvPT_PKci   # int SAVEVARSTRUCT<D3DVECTOR_SWAPABLE>(void*, D3DVECTOR_SWAPABLE*, char const*, int)
  __Z7LOADVARI17PrecipitationTypeEiPvPT_PKci   # int LOADVAR<PrecipitationType>(void*, PrecipitationType*, char const*, int)
  __Z7LOADVARI7MacLongEiPvPT_PKci   # int LOADVAR<MacLong>(void*, MacLong*, char const*, int)
  __Z7LOADVARI8MacULongEiPvPT_PKci   # int LOADVAR<MacULong>(void*, MacULong*, char const*, int)
  __Z7LOADVARIDsEiPvPT_PKci   # int LOADVAR<char16_t>(void*, char16_t*, char const*, int)
  __Z7LOADVARIbEiPvPT_PKci   # int LOADVAR<bool>(void*, bool*, char const*, int)
  __Z7LOADVARIcEiPvPT_PKci   # int LOADVAR<char>(void*, char*, char const*, int)
  __Z7LOADVARIfEiPvPT_PKci   # int LOADVAR<float>(void*, float*, char const*, int)
  __Z7LOADVARIhEiPvPT_PKci   # int LOADVAR<unsigned char>(void*, unsigned char*, char const*, int)
  __Z7LOADVARIiEiPvPT_PKci   # int LOADVAR<int>(void*, int*, char const*, int)
  __Z7LOADVARIjEiPvPT_PKci   # int LOADVAR<unsigned int>(void*, unsigned int*, char const*, int)
  __Z7LOADVARIsEiPvPT_PKci   # int LOADVAR<short>(void*, short*, char const*, int)
  __Z7LOADVARItEiPvPT_PKci   # int LOADVAR<unsigned short>(void*, unsigned short*, char const*, int)
  __Z7SAVEVARI17PrecipitationTypeEiPvPT_PKci   # int SAVEVAR<PrecipitationType>(void*, PrecipitationType*, char const*, int)
  __Z7SAVEVARI7MacLongEiPvPT_PKci   # int SAVEVAR<MacLong>(void*, MacLong*, char const*, int)
  __Z7SAVEVARI8MacULongEiPvPT_PKci   # int SAVEVAR<MacULong>(void*, MacULong*, char const*, int)
  __Z7SAVEVARIDsEiPvPT_PKci   # int SAVEVAR<char16_t>(void*, char16_t*, char const*, int)
  __Z7SAVEVARIbEiPvPT_PKci   # int SAVEVAR<bool>(void*, bool*, char const*, int)
  __Z7SAVEVARIcEiPvPT_PKci   # int SAVEVAR<char>(void*, char*, char const*, int)
  __Z7SAVEVARIfEiPvPT_PKci   # int SAVEVAR<float>(void*, float*, char const*, int)
  __Z7SAVEVARIhEiPvPT_PKci   # int SAVEVAR<unsigned char>(void*, unsigned char*, char const*, int)
  __Z7SAVEVARIiEiPvPT_PKci   # int SAVEVAR<int>(void*, int*, char const*, int)
  __Z7SAVEVARIjEiPvPT_PKci   # int SAVEVAR<unsigned int>(void*, unsigned int*, char const*, int)
  __Z7SAVEVARIsEiPvPT_PKci   # int SAVEVAR<short>(void*, short*, char const*, int)
  __Z7SAVEVARItEiPvPT_PKci   # int SAVEVAR<unsigned short>(void*, unsigned short*, char const*, int)
  __ZN11CMsgWeatherC1Ei   # CMsgWeather::CMsgWeather(int)
  __ZN11CMsgWeatherD1Ev   # CMsgWeather::~CMsgWeather()
  __ZN12CMainBalance21GetBalanceDescriptionEi   # CMainBalance::GetBalanceDescription(int)
  __ZN12CMainBalance21GetBalanceValuesStartEv   # CMainBalance::GetBalanceValuesStart()
  __ZN12CMainBalanceD1Ev   # CMainBalance::~CMainBalance()
  __ZN12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE8ByteswapEv   # tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>::Byteswap()
  __ZN12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE8ByteswapEv   # tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>::Byteswap()
  __ZN13CMsgChallengeC1Ei14AdvisorMessagebiiiii12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtES3_9StaffType   # CMsgChallenge::CMsgChallenge(int, AdvisorMessage, bool, int, int, int, int, int, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>, StaffType)
  __ZN13CMsgChallengeD1Ev   # CMsgChallenge::~CMsgChallenge()
  __ZN16CWeatherForecast10InvalidateEv   # CWeatherForecast::Invalidate()
  __ZN16CWeatherForecast3SetEii   # CWeatherForecast::Set(int, int)
  __ZN16CWeatherForecast9SerialiseEPviij   # CWeatherForecast::Serialise(void*, int, int, unsigned int)
  __ZN18CMsgWeatherWarningC1Eii   # CMsgWeatherWarning::CMsgWeatherWarning(int, int)
  __ZN18CMsgWeatherWarningD1Ev   # CMsgWeatherWarning::~CMsgWeatherWarning()
  __ZN8CWeather10SetQualityEi   # CWeather::SetQuality(int)
  __ZN8CWeather12FinalDestroyEv   # CWeather::FinalDestroy()
  __ZN8CWeather12SetWindForceEf   # CWeather::SetWindForce(float)
  __ZN8CWeather14ReceiveMessageERK4CMsg   # CWeather::ReceiveMessage(CMsg const&)
  __ZN8CWeather16SetWindDirectionEf   # CWeather::SetWindDirection(float)
  __ZN8CWeather19FindLightningCoordsER10_D3DVECTORS1_i   # CWeather::FindLightningCoords(_D3DVECTOR&, _D3DVECTOR&, int)
  __ZN8CWeather22UpdateWindInfoToEngineEv   # CWeather::UpdateWindInfoToEngine()
  __ZN8CWeather9SerialiseEPviij   # CWeather::Serialise(void*, int, int, unsigned int)
  __ZNK11CMsgWeather7GetTypeEv   # CMsgWeather::GetType() const
  __ZNK13CMsgChallenge7GetTypeEv   # CMsgChallenge::GetType() const
  __ZNK18CMsgWeatherWarning7GetTypeEv   # CMsgWeatherWarning::GetType() const
  _gMainBalance
  _gMainBalanceDescriptions
)
set(TPW_UNIT_WeatherParts_OWN_CALLLOG ON)
