# Unit w145: the map half of MapDelta.cpp, CMap's validity test, region effectors, position getters, time stamps, the engine refresh walks and the
# walking distance, the direction helpers and CMapCell's litter and thing list (src/game/park/Map.cpp, src/game/park/MapDelta.cpp); src/game/park/MapDelta.cpp (w139) supplies the three
# compass offsets GetNeighbouringCell(uint8_t) reads. The callees outside the unit are recording fakes in tests/unit_MapDelta.cpp, which also
# includes calllog.cpp itself (the engine refresh walks log up to 32,768 events).
set(TPW_UNIT_MapDelta src/game/park/Map.cpp src/game/park/MapDelta.cpp platform/arena.cpp)
set(TPW_UNIT_MapDelta_KEEP
  __Z13ADD_DIRECTIONhh   # ADD_DIRECTION(unsigned char, unsigned char)
  __Z26MAP_count_walking_distance12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtES1_7NavMode   # MAP_count_walking_distance(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, NavMode)
  __Z8OPPOSITEh   # OPPOSITE(unsigned char)
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
  __ZNK4CMap13GetBusStopPosEi   # CMap::GetBusStopPos(int) const
  __ZNK4CMap17GetTicketBoothPosEi   # CMap::GetTicketBoothPos(int) const
  __ZNK4CMap18GetParkEntrancePosEi   # CMap::GetParkEntrancePos(int) const
  __ZNK4CMap20GetCrossingBSSidePosEi   # CMap::GetCrossingBSSidePos(int) const
  __ZNK4CMap22GetCrossingParkSidePosEi   # CMap::GetCrossingParkSidePos(int) const
  __ZNK8CMapCell16GetObjectThingIdEv   # CMapCell::GetObjectThingId() const
  __ZNK8CMapCell19HasBeenModifiedFromERKS_   # CMapCell::HasBeenModifiedFrom(CMapCell const&) const
  __ZNK8MapDelta6RotateEi   # MapDelta::Rotate(int) const
  __ZNK8MapDeltamlEi   # MapDelta::operator*(int) const
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
set(TPW_UNIT_MapDelta_OWN_CALLLOG ON)
