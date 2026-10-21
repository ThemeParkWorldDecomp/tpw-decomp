# Unit v5052: CPerson::CPerson and CPerson::ReceiveMessage (src/game/people/Person.cpp). The callees outside the unit are
# recording fakes in tests/unit_CPerson_CPerson.cpp, which also defines the globals of other files.
set(TPW_UNIT_CPerson_CPerson src/game/people/Person.cpp platform/arena.cpp)
set(TPW_UNIT_CPerson_CPerson_KEEP
  __ZN7CPerson14ReceiveMessageERK4CMsg   # CPerson::ReceiveMessage(CMsg const&)
  __ZN7CPersonC1E12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # CPerson::CPerson(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  _gPersonCellA
  _gPersonCellB
  _gPersonTable
)
