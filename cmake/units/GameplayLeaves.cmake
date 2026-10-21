# Unit v5L07: CellIsRide, GetFirstPersonRideID and LocatorByeByeKid (src/game/Object.cpp, src/game/advisor/Advisor.cpp, src/game/park/SkirtDeform.cpp, src/game/ui/ui_staffPanel.cpp). The globals they read and the
# slide-out panel's close are defined in tests/unit_GameplayLeaves.cpp.
set(TPW_UNIT_GameplayLeaves src/game/Object.cpp src/game/advisor/Advisor.cpp src/game/park/SkirtDeform.cpp
    src/game/ui/ui_staffPanel.cpp)
set(TPW_UNIT_GameplayLeaves_KEEP
  __Z10CellIsRide10CELL_MODES   # CellIsRide(CELL_MODES)
  __Z16LocatorByeByeKidP6CThing   # LocatorByeByeKid(CThing*)
  __ZN8CAdvisor12FinalDestroyEv   # CAdvisor::FinalDestroy()
  __ZNK7CObject9IsCoasterEv   # CObject::IsCoaster() const
)
