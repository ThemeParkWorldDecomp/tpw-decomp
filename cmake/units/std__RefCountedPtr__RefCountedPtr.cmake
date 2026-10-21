# Unit v51035: _RefCountedPtr ctor, vector<CLineInfo>::tear_down, logic_error dtor, TbDoubleVTag::Format. Fakes in tests/unit_std__RefCountedPtr__RefCountedPtr.cpp.
set(TPW_UNIT_std__RefCountedPtr__RefCountedPtr src/ltms/TbVTag.cpp platform/arena.cpp)
set(TPW_UNIT_std__RefCountedPtr__RefCountedPtr_KEEP
  __Z14LogicErrorDtorP11LengthErrors   # LogicErrorDtor(LengthError*, short)
  __Z27MslRefCountedCharsBlockCtorP23MslRefCountedCharsBlockPc   # MslRefCountedCharsBlockCtor(MslRefCountedCharsBlock*, char*)
  __ZN12TbDoubleVTag6FormatER13CStaticBufferR11TbVarFormat   # TbDoubleVTag::Format(CStaticBuffer&, TbVarFormat&)
)
