# Unit v5297: CLineList::AddLine and its destructor (src/ltms/TbTextFormatter.cpp). The callees are recording fakes in
# tests/unit_CLineList_AddLine.cpp.
set(TPW_UNIT_CLineList_AddLine src/ltms/TbTextFormatter.cpp platform/arena.cpp)
set(TPW_UNIT_CLineList_AddLine_KEEP
  __ZN9CLineList7AddLineEP14CCharacterInfoS1_S1_j   # CLineList::AddLine(CCharacterInfo*, CCharacterInfo*, CCharacterInfo*, unsigned int)
  __ZN9CLineList8DestructEs   # CLineList::Destruct(short)
)
