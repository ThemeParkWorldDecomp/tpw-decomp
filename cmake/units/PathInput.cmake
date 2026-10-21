# Unit v5L06: the path tool's pointer and selection accessors (UI_GetMouseXY, SetPotentiallyHighlightedRide, GetPointedAtRide,
# LockPointedAtRide, CIntrface::SetCurrentThing, the four ui_allKidsPanel selection functions; unit v5L16 adds UI_SetSelectedPeep).
set(TPW_UNIT_PathInput src/game/ui/UI_Base.cpp src/game/rides/RidePreview.cpp src/game/TPPointer.cpp
    src/game/ui/intrface.cpp src/game/ui/ui_allKidsPanel.cpp)
set(TPW_UNIT_PathInput_KEEP
  __Z13UI_GetMouseXYRsS_   # UI_GetMouseXY(short&, short&)
  __Z16GetPointedAtRidev   # GetPointedAtRide()
  __Z16UI_CheckSelectedv   # UI_CheckSelected()
  __Z17LockPointedAtRidei   # LockPointedAtRide(int)
  __Z18UI_GetSelectedPeepv   # UI_GetSelectedPeep()
  __Z18UI_SetSelectedPeepP5CPeep   # UI_SetSelectedPeep(CPeep*)
  __Z20UI_GetSelectedObjectv   # UI_GetSelectedObject()
  __Z20UI_SetSelectedObjectPv   # UI_SetSelectedObject(void*)
  __Z29SetPotentiallyHighlightedRideP16tag_sRideAnimSet   # SetPotentiallyHighlightedRide(tag_sRideAnimSet*)
  __ZN9CIntrface15SetCurrentThingEP6CThing   # CIntrface::SetCurrentThing(CThing*)
)
