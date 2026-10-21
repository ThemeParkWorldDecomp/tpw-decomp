# Unit v51052: small functions of the staff HQ, tag system, thoughts, UI receiver, Vector2d, research messages and steering behaviours
# (src/game/Thing.cpp, src/game/buildings/ResearchLab.cpp, src/game/buildings/StaffHQ.cpp, src/game/buildings/TrainingCentre.cpp, src/game/people/Thoughts.cpp, src/game/ui/vector2d.cpp). The callees outside the unit are recording fakes in
# tests/unit_CStaffHQ_PickUpStaff.cpp.
set(TPW_UNIT_CStaffHQ_PickUpStaff src/game/Thing.cpp src/game/buildings/ResearchLab.cpp src/game/buildings/StaffHQ.cpp
    src/game/buildings/TrainingCentre.cpp src/game/people/Thoughts.cpp src/game/ui/vector2d.cpp platform/arena.cpp)
set(TPW_UNIT_CStaffHQ_PickUpStaff_KEEP
  __Z13Vector2d_DtorP8Vector2ds   # Vector2d_Dtor(Vector2d*, short)
  __Z15Separation_DtorP10Separations   # Separation_Dtor(Separation*, short)
  __Z16Avoid_Walls_DtorP11Avoid_Wallss   # Avoid_Walls_Dtor(Avoid_Walls*, short)
  __Z16Follow_Path_DtorP11Follow_Paths   # Follow_Path_Dtor(Follow_Path*, short)
  __Z17__sinit_Thing_cppv   # __sinit_Thing_cpp()
  __Z18EventHistType_CtorPN9CThoughts13EventHistTypeE   # EventHistType_Ctor(CThoughts::EventHistType*)
  __Z19Avoid_Obstacle_DtorP14Avoid_Obstacles   # Avoid_Obstacle_Dtor(Avoid_Obstacle*, short)
  __Z19Repel_Obstacle_DtorP14Repel_Obstacles   # Repel_Obstacle_Dtor(Repel_Obstacle*, short)
  __Z19__sinit_StaffHQ_cppv   # __sinit_StaffHQ_cpp()
  __Z20__sinit_vector2d_cppv   # __sinit_vector2d_cpp()
  __Z23__sinit_ResearchLab_cppv   # __sinit_ResearchLab_cpp()
  __Z26__sinit_TrainingCentre_cppv   # __sinit_TrainingCentre_cpp()
  __Z32CMsgResearchCategoryUpdated_DtorP27CMsgResearchCategoryUpdateds   # CMsgResearchCategoryUpdated_Dtor(CMsgResearchCategoryUpdated*, short)
  __Z35CMsgResearchCategoryUpdated_GetTypePK27CMsgResearchCategoryUpdated   # CMsgResearchCategoryUpdated_GetType(CMsgResearchCategoryUpdated const*)
  __ZN10CTagSystem12FinalDestroyEv   # CTagSystem::FinalDestroy()
  __ZN10CTagSystem9SerialiseEPviij   # CTagSystem::Serialise(void*, int, int, unsigned int)
  __ZN14CUIMsgReceiver12FinalDestroyEv   # CUIMsgReceiver::FinalDestroy()
  __ZN14CUIMsgReceiver9SerialiseEPviij   # CUIMsgReceiver::Serialise(void*, int, int, unsigned int)
  __ZN8CStaffHQ10IsStrikingE9StaffType   # CStaffHQ::IsStriking(StaffType)
  __ZN8CStaffHQ11PickUpStaffE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CStaffHQ::PickUpStaff(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN8CStaffHQ12FinalDestroyEv   # CStaffHQ::FinalDestroy()
  __ZN8CStaffHQ20StopStrikeInProgressE9StaffType   # CStaffHQ::StopStrikeInProgress(StaffType)
  __ZN9CThoughts12FinalDestroyEv   # CThoughts::FinalDestroy()
  _gResearchLabString
  _gResearchLabStringNode
  _gStaffHQSinitHigh
  _gStaffHQSinitLow
  _gThingSinitHigh
  _gThingSinitLow
  _gTrainingCentreSinitHigh
  _gTrainingCentreSinitLow
  _gVector2dSinitHigh
  _gVector2dSinitLow
)
