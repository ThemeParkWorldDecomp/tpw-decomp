# Unit v5710: std::exception::~exception of the main app (src/game/MslSupport.cpp). OperatorDelete and the vtable
# global are defined in tests/unit_std_exception_Dtor_exception_2.cpp.
set(TPW_UNIT_std_exception_Dtor_exception_2 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_exception_Dtor_exception_2_KEEP
  __ZN9TPMainExc13ExceptionDtorEPNS_9ExceptionEs   # TPMainExc::ExceptionDtor(TPMainExc::Exception*, short)
)
