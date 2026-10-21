# Unit v5125: std::exception::~exception (src/ltms/MslSupport.cpp). operator delete is a recording fake in
# tests/unit_std_exception_Dtor_exception.cpp.
set(TPW_UNIT_std_exception_Dtor_exception src/ltms/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_exception_Dtor_exception_KEEP
  __Z20LtmsStdExceptionDtorPvs   # LtmsStdExceptionDtor(void*, short)
  _gLtmsStdExceptionVtbl
)
