# Unit v5714: CSYS_TPFileInfo::~CSYS_TPFileInfo (src/game/ui/3DFont.cpp). The string destructor and operator delete are recording fakes in the unit's test file.
set(TPW_UNIT_CSYS_TPFileInfo_Dtor_CSYS_TPFileInfo src/game/ui/3DFont.cpp platform/arena.cpp)
set(TPW_UNIT_CSYS_TPFileInfo_Dtor_CSYS_TPFileInfo_KEEP
  __ZN15CSYS_TPFileInfodlEPv   # CSYS_TPFileInfo::operator delete(void*)
)
