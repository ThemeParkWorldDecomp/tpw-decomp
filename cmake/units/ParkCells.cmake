# Unit w156: the park analyser's cell and map queries (src/game/park/analyser.cpp). The callees outside the unit are fakes in tests/unit_ParkCells.cpp
# (the assert, the world accessors, the used-thing walk, the analyser value's Get and Set, CountNeighboursOfType, the eleven CMapCell predicates,
# CStaff::HasPatrolArea and GetPatrolArea, CMap::GetRegionalEffectCell), which also defines gThingArray, gWorld and gWorldMap.
set(TPW_UNIT_ParkCells src/game/park/analyser.cpp platform/arena.cpp)
set(TPW_UNIT_ParkCells_KEEP
  __ZN13CParkAnalyser12ClearMapDataEv   # CParkAnalyser::ClearMapData()
  __ZN13CParkAnalyser19IsNearbyThingOfTypeEN6CThing10ThingModelE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEi   # CParkAnalyser::IsNearbyThingOfType(CThing::ThingModel, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, int)
  __ZN13CParkAnalyser19MarkAreaPatrolledByEN6CThing10ThingModelE   # CParkAnalyser::MarkAreaPatrolledBy(CThing::ThingModel)
  __ZN13CParkAnalyser21GetNearbyLitteredCellE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtES0_I22IDXTYPEUniqueNameMapIdtEi   # CParkAnalyser::GetNearbyLitteredCell(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, int)
  __ZN13CParkAnalyser27PercentageOfAreaPatrolledByEN6CThing10ThingModelENS_8AreaTypeE   # CParkAnalyser::PercentageOfAreaPatrolledBy(CThing::ThingModel, CParkAnalyser::AreaType)
  __ZN13CParkAnalyser29NumberOfCellsCoveredByObjectsEv   # CParkAnalyser::NumberOfCellsCoveredByObjects()
  __ZN13CParkAnalyser32PercentageOfParkCoveredByCamerasEv   # CParkAnalyser::PercentageOfParkCoveredByCameras()
  __ZNK13CParkAnalyser15GetAnalyserCellE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CParkAnalyser::GetAnalyserCell(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>) const
  __ZNK13CParkAnalyser19GetNumLitteredCellsEj   # CParkAnalyser::GetNumLitteredCells(unsigned int) const
  __ZNK13CParkAnalyser19GetNumWalkableCellsEj   # CParkAnalyser::GetNumWalkableCells(unsigned int) const
  __ZNK13CParkAnalyser20GetNumDevelopedCellsEj   # CParkAnalyser::GetNumDevelopedCells(unsigned int) const
  __ZNK13CParkAnalyser26GetPercentageLitteredCellsEv   # CParkAnalyser::GetPercentageLitteredCells() const
  __ZNK17CParkAnalyserCell19GetNumLitteredCellsEj   # CParkAnalyserCell::GetNumLitteredCells(unsigned int) const
  __ZNK17CParkAnalyserCell19GetNumWalkableCellsEj   # CParkAnalyserCell::GetNumWalkableCells(unsigned int) const
  __ZNK17CParkAnalyserCell20GetNumDevelopedCellsEj   # CParkAnalyserCell::GetNumDevelopedCells(unsigned int) const
)
