# Unit v5280: TbIntegerVTag::Format (src/ltms/TbVTag.cpp). swprintf and the buffer are recording fakes in
# tests/unit_TbIntegerVTag_Format.cpp.
set(TPW_UNIT_TbIntegerVTag_Format src/ltms/TbVTag.cpp platform/arena.cpp)
set(TPW_UNIT_TbIntegerVTag_Format_KEEP
  __ZN13TbIntegerVTag6FormatER13CStaticBufferR11TbVarFormat   # TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&)
)
