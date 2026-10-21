# Unit w53: seven key tables, static entries, teardown and Search.
set(TPW_UNIT_KeyTablesInit src/game/keyboard.cpp)
set(TPW_UNIT_KeyTablesInit_KEEP
  __Z18EnableAllKeytablesv   # EnableAllKeytables()
  __Z19InitialiseKeytablesv   # InitialiseKeytables()
  __Z21DeInitialiseKeyTablesv   # DeInitialiseKeyTables()
  __ZN9CKeyTable6EnableEv   # CKeyTable::Enable()
  __ZN9CKeyTable6SearchEss   # CKeyTable::Search(short, short)
  __ZN9CKeyTable7DisableEv   # CKeyTable::Disable()
  _gKeyEntriesCamera
  _gKeyEntriesCheat
  _gKeyEntriesCoaster
  _gKeyEntriesDebug
  _gKeyEntriesGame
  _gKeyEntriesShortcuts
  _gKeyEntriesSystem
)
# The unit owns its recording allocation and string fakes.
set(TPW_UNIT_KeyTablesInit_OWN_CALLLOG ON)
# Preserve calls to the recording strncpy fake even when its source is a literal.
cmake_language(DEFER CALL target_compile_options tpw_unit_KeyTablesInit PRIVATE -fno-builtin-strncpy)
