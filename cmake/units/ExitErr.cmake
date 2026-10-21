# Unit w97: the exit error report (CExitError::ReportAnyError, HandleEngineErrors, ConvertEngineErrors) with SetError's file for gExitError.
# The 16-bit C library (swprintf, wcslen) is real; the UI string table's FormatString, the engine's error stack (Pop) and
# NS_MacDoze::MessageBoxW are fakes in the unit file.
set(TPW_UNIT_ExitErr src/game/ExitErr.cpp platform/msl.cpp)
set(TPW_UNIT_ExitErr_KEEP
  __ZN10CExitError14ReportAnyErrorEv   # CExitError::ReportAnyError()
  __ZN10CExitError18HandleEngineErrorsEv   # CExitError::HandleEngineErrors()
  __ZN10CExitError8SetErrorENS_9ErrorTypeEjPKc   # CExitError::SetError(CExitError::ErrorType, unsigned int, char const*)
  __ZNK10CExitError19ConvertEngineErrorsEi   # CExitError::ConvertEngineErrors(int) const
  _gExitError
)
