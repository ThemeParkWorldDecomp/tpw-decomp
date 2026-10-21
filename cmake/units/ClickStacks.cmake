# Unit w204: the click stacks of the queue and track tools (backtracking, the queue walk, the two track walks), the track shape tests and the
# track end offsets: src/game/park/LLCellSystem.cpp, src/game/park/TrackCell.cpp, src/game/park/MapCell.cpp, src/game/park/LLCellRide.cpp. The cell base,
# the map delta and track cell files run for real (GetNeighbours, NumberOfNeighbours, RemoveNeighbour, SetOverlapCounter, OPPOSITE, ADD_DIRECTION,
# IsXYValid, the two GetNeighbouringCell, CTrackCell::IsCorner); src/game/park/CellBase.cpp, src/game/park/LLCellSystem.cpp, src/game/park/MapCell.cpp, src/game/park/TPHField.cpp, src/game/park/TrackCell.cpp is only here for CCellBase::RemoveNeighbour. Every other callee is
# a recording fake in tests/unit_ClickStacks.cpp (what MapReset.cpp and TrackCell.cpp drag along is a no-op there, never called).
set(TPW_UNIT_ClickStacks src/game/park/LLCellSystem.cpp src/game/park/TrackCell.cpp src/game/park/MapCell.cpp
    src/game/park/LLCellRide.cpp src/game/park/CellBase.cpp src/game/park/TPHField.cpp src/game/park/Map.cpp
    src/game/park/MapDelta.cpp src/game/buildings/TrainingCentre.cpp platform/arena.cpp)
set(TPW_UNIT_ClickStacks_KEEP
  __Z13ADD_DIRECTIONhh   # ADD_DIRECTION(unsigned char, unsigned char)
  __Z13LLCC_ResetMapii   # LLCC_ResetMap(int, int)
  __Z13LOADVARSTRUCTIN9CCellBase14CELL_IndexTypeEEiPvPT_PKci   # int LOADVARSTRUCT<CCellBase::CELL_IndexType>(void*, CCellBase::CELL_IndexType*, char const*, int)
  __Z13SAVEVARSTRUCTIN9CCellBase14CELL_IndexTypeEEiPvPT_PKci   # int SAVEVARSTRUCT<CCellBase::CELL_IndexType>(void*, CCellBase::CELL_IndexType*, char const*, int)
  __Z16HFieldCellStatusii   # HFieldCellStatus(int, int)
  __Z20LLCC_SystemBacktrackP7CObjectb   # LLCC_SystemBacktrack(CObject*, bool)
  __Z21LLCR_OffsetEndOfTrackPiS_i   # LLCR_OffsetEndOfTrack(int*, int*, int)
  __Z23LLCR_OffsetStartOfTrackPiS_i   # LLCR_OffsetStartOfTrack(int*, int*, int)
  __Z26MAP_count_walking_distance12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtES1_7NavMode   # MAP_count_walking_distance(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, NavMode)
  __Z35LLCC_SystemConstructQueueClickStackP7CObject   # LLCC_SystemConstructQueueClickStack(CObject*)
  __Z35LLCC_SystemConstructTrackClickStackP7CObject   # LLCC_SystemConstructTrackClickStack(CObject*)
  __Z42LLCC_SystemConstructReverseTrackClickStackP7CObject   # LLCC_SystemConstructReverseTrackClickStack(CObject*)
  __Z7LOADVARI10CELL_MODESEiPvPT_PKci   # int LOADVAR<CELL_MODES>(void*, CELL_MODES*, char const*, int)
  __Z7SAVEVARI10CELL_MODESEiPvPT_PKci   # int SAVEVAR<CELL_MODES>(void*, CELL_MODES*, char const*, int)
  __Z8OPPOSITEh   # OPPOSITE(unsigned char)
  __ZN10CTrackCell19GetNeighbouringCellE8MapDeltai   # CTrackCell::GetNeighbouringCell(MapDelta, int)
  __ZN10CTrackCell19GetNeighbouringCellEhi   # CTrackCell::GetNeighbouringCell(unsigned char, int)
  __ZN10CTrackCell19HasBeenModifiedFromERKS_   # CTrackCell::HasBeenModifiedFrom(CTrackCell const&)
  __ZN10CTrackCell5ResetEv   # CTrackCell::Reset()
  __ZN10CTrackCell7GetFlagEv   # CTrackCell::GetFlag()
  __ZN10CTrackCell7SetFlagEt   # CTrackCell::SetFlag(unsigned short)
  __ZN10CTrackCell9SerialiseEPviij   # CTrackCell::Serialise(void*, int, int, unsigned int)
  __ZN10CTrackCell9UnSetFlagEt   # CTrackCell::UnSetFlag(unsigned short)
  __ZN4CMap11CleanUpParkEv   # CMap::CleanUpPark()
  __ZN4CMap12GetTimeStampE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CMap::GetTimeStamp(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN4CMap12SetTimeStampEj12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CMap::SetTimeStamp(unsigned int, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN4CMap16GetTimeStampCellEjj   # CMap::GetTimeStampCell(unsigned int, unsigned int)
  __ZN4CMap16MakeNewTimeStampEv   # CMap::MakeNewTimeStamp()
  __ZN4CMap17DeleteMapInEngineEv   # CMap::DeleteMapInEngine()
  __ZN4CMap17UpdateMapInEngineEv   # CMap::UpdateMapInEngine()
  __ZN4CMap19AddRegionalEffectorE14RegionEffector12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CMap::AddRegionalEffector(RegionEffector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN4CMap21GetRegionalEffectCellE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CMap::GetRegionalEffectCell(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN4CMap22ModifyRegionalEffectorEi14RegionEffector12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CMap::ModifyRegionalEffector(int, RegionEffector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN4CMap22RemoveRegionalEffectorE14RegionEffector12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CMap::RemoveRegionalEffector(RegionEffector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN4CMap9IsXYValidEii   # CMap::IsXYValid(int, int)
  __ZN8CMapCell11RemoveThingE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMapCell::RemoveThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN8CMapCell12RemoveLitterEv   # CMapCell::RemoveLitter()
  __ZN8CMapCell19GetNeighbouringCellEPK8MapDeltai   # CMapCell::GetNeighbouringCell(MapDelta const*, int)
  __ZN8CMapCell19GetNeighbouringCellEh   # CMapCell::GetNeighbouringCell(unsigned char)
  __ZN8CMapCell23MarkForLitterCollectionE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMapCell::MarkForLitterCollection(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN8CMapCell25MarkedForLitterCollectionEv   # CMapCell::MarkedForLitterCollection()
  __ZN8CMapCell5ResetEb   # CMapCell::Reset(bool)
  __ZN9CCellBase15GetMapCellEquivEv   # CCellBase::GetMapCellEquiv()
  __ZN9CCellBase15ModifyDirectionEh   # CCellBase::ModifyDirection(unsigned char)
  __ZN9CCellBase15RemoveDirectionEh   # CCellBase::RemoveDirection(unsigned char)
  __ZN9CCellBase15RemoveNeighbourEh   # CCellBase::RemoveNeighbour(unsigned char)
  __ZN9CCellBase16ModifyNeighboursEh   # CCellBase::ModifyNeighbours(unsigned char)
  __ZN9CCellBase17DecOverlapCounterEv   # CCellBase::DecOverlapCounter()
  __ZN9CCellBase17GetTrackCellEquivEv   # CCellBase::GetTrackCellEquiv()
  __ZN9CCellBase17SetOverlapCounterEi   # CCellBase::SetOverlapCounter(int)
  __ZN9CCellBase9SerialiseEPviij   # CCellBase::Serialise(void*, int, int, unsigned int)
  __ZNK10CTrackCell10IsHoardingEv   # CTrackCell::IsHoarding() const
  __ZNK10CTrackCell10IsStraightEv   # CTrackCell::IsStraight() const
  __ZNK10CTrackCell11IsCrossRoadEv   # CTrackCell::IsCrossRoad() const
  __ZNK10CTrackCell8IsCornerEv   # CTrackCell::IsCorner() const
  __ZNK10CTrackCell8IsTrackXEv   # CTrackCell::IsTrackX() const
  __ZNK4CMap13GetBusStopPosEi   # CMap::GetBusStopPos(int) const
  __ZNK4CMap17GetTicketBoothPosEi   # CMap::GetTicketBoothPos(int) const
  __ZNK4CMap18GetParkEntrancePosEi   # CMap::GetParkEntrancePos(int) const
  __ZNK4CMap20GetCrossingBSSidePosEi   # CMap::GetCrossingBSSidePos(int) const
  __ZNK4CMap22GetCrossingParkSidePosEi   # CMap::GetCrossingParkSidePos(int) const
  __ZNK8CMapCell16GetObjectThingIdEv   # CMapCell::GetObjectThingId() const
  __ZNK8CMapCell19HasBeenModifiedFromERKS_   # CMapCell::HasBeenModifiedFrom(CMapCell const&) const
  __ZNK8CMapCell8IsCornerEv   # CMapCell::IsCorner() const
  __ZNK8MapDeltamlEi   # MapDelta::operator*(int) const
  __ZNK9CCellBase12GetDirectionEv   # CCellBase::GetDirection() const
  __ZNK9CCellBase13GetNeighboursEv   # CCellBase::GetNeighbours() const
  __ZNK9CCellBase15GetMapCellEquivEv   # CCellBase::GetMapCellEquiv() const
  __ZNK9CCellBase17GetOverlapCounterEv   # CCellBase::GetOverlapCounter() const
  __ZNK9CCellBase17GetTrackCellEquivEv   # CCellBase::GetTrackCellEquiv() const
  __ZNK9CCellBase18NumberOfDirectionsEv   # CCellBase::NumberOfDirections() const
  __ZNK9CCellBase18NumberOfNeighboursEv   # CCellBase::NumberOfNeighbours() const
  __ZNK9CCellBase20NumberOfHVNeighboursEv   # CCellBase::NumberOfHVNeighbours() const
  _gGetObjectThingIdEnd
  _gLLCCBacktrackClick
  _gLitterNoCollector
  _gMapEmptyCellId
  _gMapTimeStamp
  _gRemoveThingNoNext
  _gRemoveThingNoPrev
  _gRemoveThingNoWho
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
