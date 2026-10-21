# Unit v5749: std::length_error::~length_error (src/ltms/MslSupport.cpp). The callees are
# recording fakes in tests/unit_std_length_error_Dtor_length_error.cpp (which also replaces operator delete, so no arena).
set(TPW_UNIT_std_length_error_Dtor_length_error src/ltms/MslSupport.cpp)
set(TPW_UNIT_std_length_error_Dtor_length_error_KEEP
  __Z15LengthErrorDtorP11LengthErrors   # LengthErrorDtor(LengthError*, short)
  __Z25LengthErrorOperatorDeletePv   # LengthErrorOperatorDelete(void*)
  _gExceptionVtbl
  _gLengthErrorVtbl
  _gLogicErrorVtbl
)
