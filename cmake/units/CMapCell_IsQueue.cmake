# Unit v51060: small predicates and accessors of CMapCell, CTrackCell, CLBLobby and the cell system (src/game/online/lobby.cpp, src/game/park/LLCellRide.cpp, src/game/park/MapCell.cpp, src/game/park/TrackCell.cpp).
# The callees outside the unit are recording fakes in tests/unit_CMapCell_IsQueue.cpp, which also defines the globals of other files.
set(TPW_UNIT_CMapCell_IsQueue src/game/online/lobby.cpp src/game/park/LLCellRide.cpp src/game/park/MapCell.cpp
    src/game/park/TrackCell.cpp platform/arena.cpp)
set(TPW_UNIT_CMapCell_IsQueue_KEEP
  __Z19LLCC_ResetPylonMovev   # LLCC_ResetPylonMove()
  __Z22__sinit_LLCellRide_cppv   # __sinit_LLCellRide_cpp()
  __Z23LLCR_GetHoardingExtentsPiS_S_S_   # LLCR_GetHoardingExtents(int*, int*, int*, int*)
  __ZN10CTrackCell10DeleteMeshEv   # CTrackCell::DeleteMesh()
  __ZN10CTrackCell14ModifyParentIDE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CTrackCell::ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN10CTrackCell16SetSegmentNumberEs   # CTrackCell::SetSegmentNumber(short)
  __ZN10CTrackCell18RemoveInstanceMeshEv   # CTrackCell::RemoveInstanceMesh()
  __ZN8CLBLobby9IsKeyDownEj   # CLBLobby::IsKeyDown(unsigned int)
  __ZN8CMapCell13SetPylonIndexEs   # CMapCell::SetPylonIndex(short)
  __ZN8CMapCell14ModifyParentIDE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CMapCell::ModifyParentID(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN8CMapCell18RemoveInstanceMeshEv   # CMapCell::RemoveInstanceMesh()
  __ZN8CMapCell7GetFlagEv   # CMapCell::GetFlag()
  __ZN8CMapCell7SetFlagEt   # CMapCell::SetFlag(unsigned short)
  __ZNK10CTrackCell11IsJustTrackEv   # CTrackCell::IsJustTrack() const
  __ZNK10CTrackCell7IsTrackEv   # CTrackCell::IsTrack() const
  __ZNK8CMapCell10IsEntranceEv   # CMapCell::IsEntrance() const
  __ZNK8CMapCell10IsStraightEv   # CMapCell::IsStraight() const
  __ZNK8CMapCell10IsWalkableEv   # CMapCell::IsWalkable() const
  __ZNK8CMapCell13GetPylonIndexEv   # CMapCell::GetPylonIndex() const
  __ZNK8CMapCell14IsCoasterPylonEv   # CMapCell::IsCoasterPylon() const
  __ZNK8CMapCell23IsWalkableInFirstPersonEv   # CMapCell::IsWalkableInFirstPerson() const
  __ZNK8CMapCell6IsExitEv   # CMapCell::IsExit() const
  __ZNK8CMapCell6IsLakeEv   # CMapCell::IsLake() const
  __ZNK8CMapCell6IsNoGoEv   # CMapCell::IsNoGo() const
  __ZNK8CMapCell6IsPathEv   # CMapCell::IsPath() const
  __ZNK8CMapCell6IsRideEv   # CMapCell::IsRide() const
  __ZNK8CMapCell7IsGrassEv   # CMapCell::IsGrass() const
  __ZNK8CMapCell7IsQueueEv   # CMapCell::IsQueue() const
  __ZNK8CMapCell8IsBridgeEv   # CMapCell::IsBridge() const
  __ZNK8CMapCell8IsTrackXEv   # CMapCell::IsTrackX() const
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
