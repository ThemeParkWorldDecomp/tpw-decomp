# Unit v5120: bthread_mutex::~bthread_mutex (src/game/online/email.cpp). The callees outside the unit are recording
# fakes in tests/unit_bthread_mutex_Dtor_bthread_mutex.cpp.
set(TPW_UNIT_bthread_mutex_Dtor_bthread_mutex src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_bthread_mutex_Dtor_bthread_mutex_KEEP
  __ZN13bthread_mutex7DestroyEPS_s   # bthread_mutex::Destroy(bthread_mutex*, short)
)
