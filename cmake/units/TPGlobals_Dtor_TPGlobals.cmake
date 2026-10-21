# Unit v5692: TPGlobals::~TPGlobals (src/game/TPGlobals.cpp). The string destructors are recording fakes in
# tests/unit_TPGlobals_Dtor_TPGlobals.cpp.
set(TPW_UNIT_TPGlobals_Dtor_TPGlobals src/game/TPGlobals.cpp platform/arena.cpp)
set(TPW_UNIT_TPGlobals_Dtor_TPGlobals_KEEP
  __ZN9TPGlobalsD1Ev   # TPGlobals::~TPGlobals()
)
