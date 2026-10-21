# Unit v5032: TbITable::FormatString, multibyte overload (src/ltms/TbITable.cpp). DoFormat, the buffer's Close and the
# converter's AddRef/Release are recording fakes in tests/unit_TbITable_FormatString.cpp.
set(TPW_UNIT_TbITable_FormatString src/ltms/TbITable.cpp platform/arena.cpp)
set(TPW_UNIT_TbITable_FormatString_KEEP
  __ZNK8TbITable12FormatStringEPhjjz   # TbITable::FormatString(unsigned char*, unsigned int, unsigned int, ...) const
)
