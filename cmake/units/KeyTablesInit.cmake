# Unit w53: seven key tables, static entries, teardown and Search.
set(TPW_UNIT_KeyTablesInit src/keytables_init.cpp src/keytables_data.cpp src/keytables_enable.cpp)
# The unit owns its recording allocation and string fakes.
set(TPW_UNIT_KeyTablesInit_OWN_CALLLOG ON)
# Preserve calls to the recording strncpy fake even when its source is a literal.
cmake_language(DEFER CALL target_compile_options tpw_unit_KeyTablesInit PRIVATE -fno-builtin-strncpy)
