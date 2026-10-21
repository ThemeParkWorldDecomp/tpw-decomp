# Unit v5949: exception destructors, char ref-counted pointer, Antialiased<2> (src/game/MslSupport.cpp, src/game/ui/3DFont.cpp). The callees
# outside the unit are recording fakes in tests/unit_std_logic_error_Dtor_logic_error.cpp.
set(TPW_UNIT_std_logic_error_Dtor_logic_error src/game/MslSupport.cpp src/game/ui/3DFont.cpp platform/arena.cpp)
set(TPW_UNIT_std_logic_error_Dtor_logic_error_KEEP
  __ZN13tpw_antialias11AntialiasedILi2EEEvRKN13tpw_intensify7CTraitsIXT_EEERK12TbRenderAreaRK6TbRectRKN3bfl4draw7CColourERK7TbPointSJ_   # void tpw_antialias::Antialiased<2>(tpw_intensify::CTraits<2> const&, TbRenderArea const&, TbRect const&, bfl::draw::CColour const&, TbPoint const&, TbPoint const&)
  __ZN3IME17MslLogicErrorDtorEPNS_18MslExceptionObjectEs   # IME::MslLogicErrorDtor(IME::MslExceptionObject*, short)
  __ZN3IME17MslOutOfRangeDtorEPNS_18MslExceptionObjectEs   # IME::MslOutOfRangeDtor(IME::MslExceptionObject*, short)
  __ZN3IME22MslRefCountedCharsCtorEPNS_18MslRefCountedCharsEPc   # IME::MslRefCountedCharsCtor(IME::MslRefCountedChars*, char*)
  __ZN3IME26MslRefCountedCharsDtor25e0EPNS_18MslRefCountedCharsEs   # IME::MslRefCountedCharsDtor25e0(IME::MslRefCountedChars*, short)
)
