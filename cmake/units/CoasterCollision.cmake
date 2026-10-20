# Unit v5L20: the coaster collision system (src/TPConfig_collision.cpp). The callees outside the unit (NormaliseVector,
# MReleaseAllocF, MReleaseFreeF) are recording fakes in tests/unit_CoasterCollision.cpp, which also defines the globals of other files.
set(TPW_UNIT_CoasterCollision src/TPConfig_collision.cpp platform/arena.cpp)
