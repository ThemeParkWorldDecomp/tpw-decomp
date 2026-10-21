# Unit v51002: SAVEVARSTRUCT / LOADVARSTRUCT of CStaffHQ::StrikeType, SAVEVAR of CPerson::PersonState, LOADVAR of ResearchCat,
# CPerson::PayPricePerUseForObject (src/game/buildings/ResearchLab.cpp, src/game/buildings/StaffHQ.cpp, src/game/people/Person.cpp). The callees outside the unit are recording fakes in
# tests/unit_SAVEVARSTRUCT_3.cpp, which also defines the globals of other files.
set(TPW_UNIT_SAVEVARSTRUCT_3 src/game/buildings/ResearchLab.cpp src/game/buildings/StaffHQ.cpp
    src/game/people/Person.cpp platform/arena.cpp)
set(TPW_UNIT_SAVEVARSTRUCT_3_KEEP
  __Z13LOADVARSTRUCTIN8CStaffHQ10StrikeTypeEEiPvPT_PKci   # int LOADVARSTRUCT<CStaffHQ::StrikeType>(void*, CStaffHQ::StrikeType*, char const*, int)
  __Z13SAVEVARSTRUCTIN8CStaffHQ10StrikeTypeEEiPvPT_PKci   # int SAVEVARSTRUCT<CStaffHQ::StrikeType>(void*, CStaffHQ::StrikeType*, char const*, int)
  __Z7LOADVARI11ResearchCatEiPvPT_PKci   # int LOADVAR<ResearchCat>(void*, ResearchCat*, char const*, int)
  __Z7SAVEVARIN7CPerson11PersonStateEEiPvPT_PKci   # int SAVEVAR<CPerson::PersonState>(void*, CPerson::PersonState*, char const*, int)
  __ZN7CPerson23PayPricePerUseForObjectEP7CObject   # CPerson::PayPricePerUseForObject(CObject*)
)
