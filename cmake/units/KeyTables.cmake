# The key tables' KeyDown and KeyUp (unit w50): the first matching entry's action and state.
set(TPW_UNIT_KeyTables src/game/keyboard.cpp)
set(TPW_UNIT_KeyTables_KEEP
  __ZN9CKeyTable5KeyUpEss   # CKeyTable::KeyUp(short, short)
  __ZN9CKeyTable7KeyDownEss   # CKeyTable::KeyDown(short, short)
)
