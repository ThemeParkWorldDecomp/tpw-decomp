# Unit w146: the map save and litter half of MapDelta.cpp (CMapCell::PlaceLitter, the CMap, CMapCell and CMacroAI serialisers, the LitterType
# instances) with the real basic serialise instances (the file is the fake) and the three cell constructors (src/game/buildings/TrainingCentre.cpp, src/game/park/CellBase.cpp, src/game/park/MapDelta.cpp, src/game/people/Researcher.cpp, run real on both
# sides) and src/game/park/MapDelta.cpp (the eight compass deltas, whose values the test compares with __sinit_MapDelta_cpp's). Every other callee is a recording fake in tests/unit_MapSave.cpp, which includes calllog.cpp itself (the map walk logs many events).
set(TPW_UNIT_MapSave src/game/park/Map.cpp src/game/park/MapDelta.cpp src/game/ControlManager.cpp src/game/Object.cpp
    src/game/advisor/Advisor.cpp src/game/park/BankAccount.cpp src/game/park/analyser.cpp src/game/rides/FlyingRide.cpp
    src/game/save/FS_MasterSave.cpp src/game/buildings/TrainingCentre.cpp src/game/park/CellBase.cpp
    src/game/people/Researcher.cpp)
set(TPW_UNIT_MapSave_KEEP
  __Z13LOADVARSTRUCTI11TbTimeStampEiPvPT_PKci   # int LOADVARSTRUCT<TbTimeStamp>(void*, TbTimeStamp*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTI11TbTimeStampEiPvPT_PKci   # int SAVEVARSTRUCT<TbTimeStamp>(void*, TbTimeStamp*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>*, char const*, int)
  __Z7LOADVARI7MacLongEiPvPT_PKci   # int LOADVAR<MacLong>(void*, MacLong*, char const*, int)
  __Z7LOADVARI8MacULongEiPvPT_PKci   # int LOADVAR<MacULong>(void*, MacULong*, char const*, int)
  __Z7LOADVARIDsEiPvPT_PKci   # int LOADVAR<char16_t>(void*, char16_t*, char const*, int)
  __Z7LOADVARIN8CMapCell10LitterTypeEEiPvPT_PKci   # int LOADVAR<CMapCell::LitterType>(void*, CMapCell::LitterType*, char const*, int)
  __Z7LOADVARIbEiPvPT_PKci   # int LOADVAR<bool>(void*, bool*, char const*, int)
  __Z7LOADVARIcEiPvPT_PKci   # int LOADVAR<char>(void*, char*, char const*, int)
  __Z7LOADVARIhEiPvPT_PKci   # int LOADVAR<unsigned char>(void*, unsigned char*, char const*, int)
  __Z7LOADVARIiEiPvPT_PKci   # int LOADVAR<int>(void*, int*, char const*, int)
  __Z7LOADVARIjEiPvPT_PKci   # int LOADVAR<unsigned int>(void*, unsigned int*, char const*, int)
  __Z7LOADVARIsEiPvPT_PKci   # int LOADVAR<short>(void*, short*, char const*, int)
  __Z7LOADVARItEiPvPT_PKci   # int LOADVAR<unsigned short>(void*, unsigned short*, char const*, int)
  __Z7SAVEVARI7MacLongEiPvPT_PKci   # int SAVEVAR<MacLong>(void*, MacLong*, char const*, int)
  __Z7SAVEVARI8MacULongEiPvPT_PKci   # int SAVEVAR<MacULong>(void*, MacULong*, char const*, int)
  __Z7SAVEVARIDsEiPvPT_PKci   # int SAVEVAR<char16_t>(void*, char16_t*, char const*, int)
  __Z7SAVEVARIN8CMapCell10LitterTypeEEiPvPT_PKci   # int SAVEVAR<CMapCell::LitterType>(void*, CMapCell::LitterType*, char const*, int)
  __Z7SAVEVARIbEiPvPT_PKci   # int SAVEVAR<bool>(void*, bool*, char const*, int)
  __Z7SAVEVARIcEiPvPT_PKci   # int SAVEVAR<char>(void*, char*, char const*, int)
  __Z7SAVEVARIhEiPvPT_PKci   # int SAVEVAR<unsigned char>(void*, unsigned char*, char const*, int)
  __Z7SAVEVARIiEiPvPT_PKci   # int SAVEVAR<int>(void*, int*, char const*, int)
  __Z7SAVEVARIjEiPvPT_PKci   # int SAVEVAR<unsigned int>(void*, unsigned int*, char const*, int)
  __Z7SAVEVARIsEiPvPT_PKci   # int SAVEVAR<short>(void*, short*, char const*, int)
  __Z7SAVEVARItEiPvPT_PKci   # int SAVEVAR<unsigned short>(void*, unsigned short*, char const*, int)
  __ZN10CTrackCellC1Ev   # CTrackCell::CTrackCell()
  __ZN12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE8ByteswapEv   # tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>::Byteswap()
  __ZN12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE8ByteswapEv   # tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>::Byteswap()
  __ZN17CRegionEffectCellC1Ev   # CRegionEffectCell::CRegionEffectCell()
  __ZN4CMap9SerialiseEPviij   # CMap::Serialise(void*, int, int, unsigned int)
  __ZN8CMacroAI9SerialiseEPviij   # CMacroAI::Serialise(void*, int, int, unsigned int)
  __ZN8CMapCell11PlaceLitterENS_10LitterTypeE   # CMapCell::PlaceLitter(CMapCell::LitterType)
  __ZN8CMapCell9SerialiseEPviij   # CMapCell::Serialise(void*, int, int, unsigned int)
  __ZN8CMapCellC1Ev   # CMapCell::CMapCell()
  __ZN9CCellBase15RegisterMapGridEPK8CMapCelli   # CCellBase::RegisterMapGrid(CMapCell const*, int)
  __ZN9CCellBase17RegisterTrackGridEPK10CTrackCelli   # CCellBase::RegisterTrackGrid(CTrackCell const*, int)
  __ZNK8MapDelta6RotateEi   # MapDelta::Rotate(int) const
  _gMapDeltaMinusX
  _gMapDeltaMinusXMinusY
  _gMapDeltaMinusXPlusY
  _gMapDeltaMinusY
  _gMapDeltaPlusX
  _gMapDeltaPlusXMinusY
  _gMapDeltaPlusXPlusY
  _gMapDeltaPlusY
)
set(TPW_UNIT_MapSave_OWN_CALLLOG ON)
