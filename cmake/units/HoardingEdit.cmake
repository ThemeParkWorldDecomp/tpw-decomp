# Unit w206: the park's inside filled from the locked hoarding and a hoarding edit checked for legality and cost (src/game/park/LLCellSystem.cpp) with
# the cell flag and kind tests (src/game/park/TrackCell.cpp, src/game/park/MapCell.cpp). The flag and kind tests of unit w201 (src/game/park/TrackCell.cpp,
# src/game/park/MapCell.cpp), the track cell's straight test (src/game/park/TrackCell.cpp), the cell bases and the track cell's neighbouring cell (src/game/park/CellBase.cpp,
# src/game/buildings/TrainingCentre.cpp) and the eight compass deltas (src/game/park/MapDelta.cpp) run for real; every other callee is a recording fake in tests/unit_HoardingEdit.cpp,
# which also defines the globals of other files.
set(TPW_UNIT_HoardingEdit src/game/park/LLCellSystem.cpp src/game/park/TrackCell.cpp src/game/park/MapCell.cpp
    src/game/park/CellBase.cpp src/game/buildings/TrainingCentre.cpp src/game/park/MapDelta.cpp
    src/game/ControlManager.cpp src/game/Object.cpp src/game/advisor/Advisor.cpp src/game/park/BankAccount.cpp
    src/game/park/analyser.cpp src/game/rides/FlyingRide.cpp src/game/save/FS_MasterSave.cpp)
set(TPW_UNIT_HoardingEdit_KEEP
  __Z13LOADVARSTRUCTI11TbTimeStampEiPvPT_PKci   # int LOADVARSTRUCT<TbTimeStamp>(void*, TbTimeStamp*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>*, char const*, int)
  __Z13LOADVARSTRUCTI12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtEEiPvPT_PKci   # int LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>*, char const*, int)
  __Z13LOADVARSTRUCTIN9CCellBase14CELL_IndexTypeEEiPvPT_PKci   # int LOADVARSTRUCT<CCellBase::CELL_IndexType>(void*, CCellBase::CELL_IndexType*, char const*, int)
  __Z13SAVEVARSTRUCTI11TbTimeStampEiPvPT_PKci   # int SAVEVARSTRUCT<TbTimeStamp>(void*, TbTimeStamp*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTI12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtEEiPvPT_PKci   # int SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>>(void*, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>*, char const*, int)
  __Z13SAVEVARSTRUCTIN9CCellBase14CELL_IndexTypeEEiPvPT_PKci   # int SAVEVARSTRUCT<CCellBase::CELL_IndexType>(void*, CCellBase::CELL_IndexType*, char const*, int)
  __Z21LLCC_FillInsideOfParkv   # LLCC_FillInsideOfPark()
  __Z24LLCC_IsHoardingEditLegaliiiib   # LLCC_IsHoardingEditLegal(int, int, int, int, bool)
  __Z7LOADVARI10CELL_MODESEiPvPT_PKci   # int LOADVAR<CELL_MODES>(void*, CELL_MODES*, char const*, int)
  __Z7LOADVARI7MacLongEiPvPT_PKci   # int LOADVAR<MacLong>(void*, MacLong*, char const*, int)
  __Z7LOADVARI8MacULongEiPvPT_PKci   # int LOADVAR<MacULong>(void*, MacULong*, char const*, int)
  __Z7LOADVARIDsEiPvPT_PKci   # int LOADVAR<char16_t>(void*, char16_t*, char const*, int)
  __Z7LOADVARIbEiPvPT_PKci   # int LOADVAR<bool>(void*, bool*, char const*, int)
  __Z7LOADVARIcEiPvPT_PKci   # int LOADVAR<char>(void*, char*, char const*, int)
  __Z7LOADVARIhEiPvPT_PKci   # int LOADVAR<unsigned char>(void*, unsigned char*, char const*, int)
  __Z7LOADVARIiEiPvPT_PKci   # int LOADVAR<int>(void*, int*, char const*, int)
  __Z7LOADVARIjEiPvPT_PKci   # int LOADVAR<unsigned int>(void*, unsigned int*, char const*, int)
  __Z7LOADVARIsEiPvPT_PKci   # int LOADVAR<short>(void*, short*, char const*, int)
  __Z7LOADVARItEiPvPT_PKci   # int LOADVAR<unsigned short>(void*, unsigned short*, char const*, int)
  __Z7SAVEVARI10CELL_MODESEiPvPT_PKci   # int SAVEVAR<CELL_MODES>(void*, CELL_MODES*, char const*, int)
  __Z7SAVEVARI7MacLongEiPvPT_PKci   # int SAVEVAR<MacLong>(void*, MacLong*, char const*, int)
  __Z7SAVEVARI8MacULongEiPvPT_PKci   # int SAVEVAR<MacULong>(void*, MacULong*, char const*, int)
  __Z7SAVEVARIDsEiPvPT_PKci   # int SAVEVAR<char16_t>(void*, char16_t*, char const*, int)
  __Z7SAVEVARIbEiPvPT_PKci   # int SAVEVAR<bool>(void*, bool*, char const*, int)
  __Z7SAVEVARIcEiPvPT_PKci   # int SAVEVAR<char>(void*, char*, char const*, int)
  __Z7SAVEVARIhEiPvPT_PKci   # int SAVEVAR<unsigned char>(void*, unsigned char*, char const*, int)
  __Z7SAVEVARIiEiPvPT_PKci   # int SAVEVAR<int>(void*, int*, char const*, int)
  __Z7SAVEVARIjEiPvPT_PKci   # int SAVEVAR<unsigned int>(void*, unsigned int*, char const*, int)
  __Z7SAVEVARIsEiPvPT_PKci   # int SAVEVAR<short>(void*, short*, char const*, int)
  __Z7SAVEVARItEiPvPT_PKci   # int SAVEVAR<unsigned short>(void*, unsigned short*, char const*, int)
  __ZN10CTrackCell19GetNeighbouringCellE8MapDeltai   # CTrackCell::GetNeighbouringCell(MapDelta, int)
  __ZN10CTrackCell19GetNeighbouringCellEhi   # CTrackCell::GetNeighbouringCell(unsigned char, int)
  __ZN10CTrackCell19HasBeenModifiedFromERKS_   # CTrackCell::HasBeenModifiedFrom(CTrackCell const&)
  __ZN10CTrackCell7GetFlagEv   # CTrackCell::GetFlag()
  __ZN10CTrackCell7SetFlagEt   # CTrackCell::SetFlag(unsigned short)
  __ZN10CTrackCell9SerialiseEPviij   # CTrackCell::Serialise(void*, int, int, unsigned int)
  __ZN10CTrackCell9UnSetFlagEt   # CTrackCell::UnSetFlag(unsigned short)
  __ZN12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE8ByteswapEv   # tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>::Byteswap()
  __ZN12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE8ByteswapEv   # tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>::Byteswap()
  __ZN8CMapCell9UnSetFlagEt   # CMapCell::UnSetFlag(unsigned short)
  __ZN9CCellBase15GetMapCellEquivEv   # CCellBase::GetMapCellEquiv()
  __ZN9CCellBase15RemoveDirectionEh   # CCellBase::RemoveDirection(unsigned char)
  __ZN9CCellBase16ModifyNeighboursEh   # CCellBase::ModifyNeighbours(unsigned char)
  __ZN9CCellBase17DecOverlapCounterEv   # CCellBase::DecOverlapCounter()
  __ZN9CCellBase17GetTrackCellEquivEv   # CCellBase::GetTrackCellEquiv()
  __ZN9CCellBase17SetOverlapCounterEi   # CCellBase::SetOverlapCounter(int)
  __ZN9CCellBase9SerialiseEPviij   # CCellBase::Serialise(void*, int, int, unsigned int)
  __ZNK10CTrackCell10IsHoardingEv   # CTrackCell::IsHoarding() const
  __ZNK10CTrackCell10IsStraightEv   # CTrackCell::IsStraight() const
  __ZNK10CTrackCell11IsCrossRoadEv   # CTrackCell::IsCrossRoad() const
  __ZNK10CTrackCell6IsFlagEt   # CTrackCell::IsFlag(unsigned short) const
  __ZNK10CTrackCell8IsCornerEv   # CTrackCell::IsCorner() const
  __ZNK10CTrackCell8IsTrackXEv   # CTrackCell::IsTrackX() const
  __ZNK8CMapCell10IsFreeformEv   # CMapCell::IsFreeform() const
  __ZNK8CMapCell6IsFlagEt   # CMapCell::IsFlag(unsigned short) const
  __ZNK8CMapCell7IsWaterEv   # CMapCell::IsWater() const
  __ZNK8MapDelta6RotateEi   # MapDelta::Rotate(int) const
  __ZNK9CCellBase12GetDirectionEv   # CCellBase::GetDirection() const
  __ZNK9CCellBase13GetNeighboursEv   # CCellBase::GetNeighbours() const
  __ZNK9CCellBase15GetMapCellEquivEv   # CCellBase::GetMapCellEquiv() const
  __ZNK9CCellBase17GetOverlapCounterEv   # CCellBase::GetOverlapCounter() const
  __ZNK9CCellBase17GetTrackCellEquivEv   # CCellBase::GetTrackCellEquiv() const
  __ZNK9CCellBase18NumberOfDirectionsEv   # CCellBase::NumberOfDirections() const
  __ZNK9CCellBase18NumberOfNeighboursEv   # CCellBase::NumberOfNeighbours() const
  __ZNK9CCellBase20NumberOfHVNeighboursEv   # CCellBase::NumberOfHVNeighbours() const
  _gLLCCEdgeCount
  _gLLCCEdgeMarks
  _gMapDeltaMinusX
  _gMapDeltaMinusXMinusY
  _gMapDeltaMinusXPlusY
  _gMapDeltaMinusY
  _gMapDeltaPlusX
  _gMapDeltaPlusXMinusY
  _gMapDeltaPlusXPlusY
  _gMapDeltaPlusY
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
