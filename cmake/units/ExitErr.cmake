# Unit w97: the exit error report (CExitError::ReportAnyError, HandleEngineErrors, ConvertEngineErrors) with SetError's file for gExitError.
# The 16-bit C library (swprintf, wcslen) is real; the UI string table's FormatString, the engine's error stack (Pop) and
# NS_MacDoze::MessageBoxW are fakes in the unit file.
set(TPW_UNIT_ExitErr src/ExitErr_report.cpp src/ExitErr.cpp platform/msl.cpp)
