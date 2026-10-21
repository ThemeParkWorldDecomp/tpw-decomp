# Unit v5732: CritLock::~CritLock (src/bullfrog/FileSystem.cpp).
# The callees outside the unit are recording fakes in tests/unit_CritLock_Dtor_CritLock.cpp.
set(TPW_UNIT_CritLock_Dtor_CritLock src/bullfrog/FileSystem.cpp platform/arena.cpp)
set(TPW_UNIT_CritLock_Dtor_CritLock_KEEP
  __Z17CritLock_DtorBodyP8CritLocks   # CritLock_DtorBody(CritLock*, short)
)
