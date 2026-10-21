# Unit w197: the ride's queue (src/game/Object.cpp). The callees outside the unit are fakes in tests/unit_ObjectQueue.cpp (the assert, the trace, the ride script
# variables, the world accessors and its random generator, the thing table's persons, the ride predicates, the wall test, CPerson::EvaluateQueueEdit and
# InQueueingState); the cell code (CellBase, MapDelta, MapDelta_map: GetNeighbouringCell, OPPOSITE, the time stamps) runs as translated. Serialise_basic.cpp
# supplies the LOADVAR/SAVEVAR instances those files link against; the symbols nothing calls (the map cells' mesh refresh, the save log, the file calls) are
# link fakes at the end of the unit's test file.
set(TPW_UNIT_ObjectQueue src/game/Object.cpp src/game/park/CellBase.cpp src/game/park/Map.cpp
    src/game/park/MapDelta.cpp src/game/ControlManager.cpp src/game/advisor/Advisor.cpp src/game/park/BankAccount.cpp
    src/game/park/analyser.cpp src/game/rides/FlyingRide.cpp src/game/save/FS_MasterSave.cpp platform/arena.cpp)
set(TPW_UNIT_ObjectQueue_KEEP
  __Z13ADD_DIRECTIONhh   # ADD_DIRECTION(unsigned char, unsigned char)
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
  __Z26MAP_count_walking_distance12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtES1_7NavMode   # MAP_count_walking_distance(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, NavMode)
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
  __Z8OPPOSITEh   # OPPOSITE(unsigned char)
  __ZN12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE8ByteswapEv   # tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>::Byteswap()
  __ZN12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE8ByteswapEv   # tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>::Byteswap()
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
  __ZN7CObject14GetBackOfQueueEv   # CObject::GetBackOfQueue()
  __ZN7CObject14GetQCellBehindE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CObject::GetQCellBehind(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN7CObject14IsBOQConnectedEv   # CObject::IsBOQConnected()
  __ZN7CObject16AddPersonToQueueE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CObject::AddPersonToQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN7CObject19IsExitCellConnectedEv   # CObject::IsExitCellConnected()
  __ZN7CObject21GetBackOfVirtualQueueEv   # CObject::GetBackOfVirtualQueue()
  __ZN7CObject21RemovePersonFromQueueE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CObject::RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN7CObject22GetBackOfPhysicalQueueEv   # CObject::GetBackOfPhysicalQueue()
  __ZN7CObject22RecalculateBackOfQueueEv   # CObject::RecalculateBackOfQueue()
  __ZN7CObject24ConvertQPositionToMapPosEjP12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEPhS4_   # CObject::ConvertQPositionToMapPos(unsigned int, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, unsigned char*, unsigned char*)
  __ZN7CObject31ConvertVirtualQPositionToMapPosEjP12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEPhS4_   # CObject::ConvertVirtualQPositionToMapPos(unsigned int, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, unsigned char*, unsigned char*)
  __ZN7CObject32ConvertPhysicalQPositionToMapPosEjP12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEPhS4_   # CObject::ConvertPhysicalQPositionToMapPos(unsigned int, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>*, unsigned char*, unsigned char*)
  __ZN8CMapCell11RemoveThingE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMapCell::RemoveThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN8CMapCell12RemoveLitterEv   # CMapCell::RemoveLitter()
  __ZN8CMapCell19GetNeighbouringCellEPK8MapDeltai   # CMapCell::GetNeighbouringCell(MapDelta const*, int)
  __ZN8CMapCell19GetNeighbouringCellEh   # CMapCell::GetNeighbouringCell(unsigned char)
  __ZN8CMapCell23MarkForLitterCollectionE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMapCell::MarkForLitterCollection(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN8CMapCell25MarkedForLitterCollectionEv   # CMapCell::MarkedForLitterCollection()
  __ZN9CCellBase15GetMapCellEquivEv   # CCellBase::GetMapCellEquiv()
  __ZN9CCellBase15RemoveDirectionEh   # CCellBase::RemoveDirection(unsigned char)
  __ZN9CCellBase16ModifyNeighboursEh   # CCellBase::ModifyNeighbours(unsigned char)
  __ZN9CCellBase17DecOverlapCounterEv   # CCellBase::DecOverlapCounter()
  __ZN9CCellBase17GetTrackCellEquivEv   # CCellBase::GetTrackCellEquiv()
  __ZN9CCellBase17SetOverlapCounterEi   # CCellBase::SetOverlapCounter(int)
  __ZN9CCellBase9SerialiseEPviij   # CCellBase::Serialise(void*, int, int, unsigned int)
  __ZNK4CMap13GetBusStopPosEi   # CMap::GetBusStopPos(int) const
  __ZNK4CMap17GetTicketBoothPosEi   # CMap::GetTicketBoothPos(int) const
  __ZNK4CMap18GetParkEntrancePosEi   # CMap::GetParkEntrancePos(int) const
  __ZNK4CMap20GetCrossingBSSidePosEi   # CMap::GetCrossingBSSidePos(int) const
  __ZNK4CMap22GetCrossingParkSidePosEi   # CMap::GetCrossingParkSidePos(int) const
  __ZNK7CObject18GetPositionInQueueE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CObject::GetPositionInQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>) const
  __ZNK7CObject19GetNumPeopleInQueueEv   # CObject::GetNumPeopleInQueue() const
  __ZNK7CObject23GetFrontOfPhysicalQueueEv   # CObject::GetFrontOfPhysicalQueue() const
  __ZNK8CMapCell16GetObjectThingIdEv   # CMapCell::GetObjectThingId() const
  __ZNK8CMapCell19HasBeenModifiedFromERKS_   # CMapCell::HasBeenModifiedFrom(CMapCell const&) const
  __ZNK8MapDelta6RotateEi   # MapDelta::Rotate(int) const
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
  _gLitterNoCollector
  _gMapDeltaMinusX
  _gMapDeltaMinusXMinusY
  _gMapDeltaMinusXPlusY
  _gMapDeltaMinusY
  _gMapDeltaPlusX
  _gMapDeltaPlusXMinusY
  _gMapDeltaPlusXPlusY
  _gMapDeltaPlusY
  _gMapEmptyCellId
  _gMapTimeStamp
  _gRemoveThingNoNext
  _gRemoveThingNoPrev
  _gRemoveThingNoWho
)
