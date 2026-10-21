# Unit v51105: CUI2DMap::Register and 19 small functions (src/game/MslSupport.cpp, src/game/ui/ui_2dmap.cpp).
# The callees outside the unit are recording fakes in tests/unit_CUI2DMap_Register.cpp, which also defines the globals of other files.
set(TPW_UNIT_CUI2DMap_Register src/game/MslSupport.cpp src/game/ui/ui_2dmap.cpp platform/arena.cpp
    tests/standins/CUI2DMap_Register_callees.cpp)
set(TPW_UNIT_CUI2DMap_Register_KEEP
  __Z16CUI2DMapDestructP8CUI2DMaps   # CUI2DMapDestruct(CUI2DMap*, short)
  __Z18CtypeWcharDestructP13StdCtypeWchars   # CtypeWcharDestruct(StdCtypeWchar*, short)
  __Z23TUI2DMapMapGridCellCtorPv   # TUI2DMapMapGridCellCtor(void*)
  __ZN12argdeque_new12MakeIteratorEP20MslArgumentDequeIterPK16MslArgumentDequej   # argdeque_new::MakeIterator(MslArgumentDequeIter*, MslArgumentDeque const*, unsigned int)
  __ZN12argdeque_new14CdequeIterCopyEP17MslArgumentCursorPKS0_   # argdeque_new::CdequeIterCopy(MslArgumentCursor*, MslArgumentCursor const*)
  __ZN13nocase_callee10StringCopyEP13nocase_stringPKS0_   # nocase_callee::StringCopy(nocase_string*, nocase_string const*)
  __ZN13online_callee17NocaseWStringInitEPvPKDsS2_   # online_callee::NocaseWStringInit(void*, char16_t const*, char16_t const*)
  __ZN13online_callee9DequeCtorEPv   # online_callee::DequeCtor(void*)
  __ZN14CUI2DMapSprite11SetPositionERK10_D3DVECTOR   # CUI2DMapSprite::SetPosition(_D3DVECTOR const&)
  __ZN14ui2dmap_callee11ListAddTailEP7TbTListIP14CUI2DMapSpriteS2_ES2_   # ui2dmap_callee::ListAddTail(TbTList<CUI2DMapSprite*, CUI2DMapSprite*>*, CUI2DMapSprite*)
  __ZN15argdeque_callee9IterDerefEPK20MslArgumentDequeIter   # argdeque_callee::IterDeref(MslArgumentDequeIter const*)
  __ZN17argdestroy_callee12DestroyRangeEjj   # argdestroy_callee::DestroyRange(unsigned int, unsigned int)
  __ZN19wstring_less_callee4SizeEPK13MslWideString   # wstring_less_callee::Size(MslWideString const*)
  __ZN19wstring_less_callee5BeginEPK13MslWideString   # wstring_less_callee::Begin(MslWideString const*)
  __ZN8CUI2DMap14GetMapGridCellEii   # CUI2DMap::GetMapGridCell(int, int)
  __ZN8CUI2DMap8RegisterEP14CUI2DMapSprite   # CUI2DMap::Register(CUI2DMapSprite*)
  __ZN9online_lt17NocaseWStringSizeEPKv   # online_lt::NocaseWStringSize(void const*)
  __ZN9online_lt18NocaseWStringBeginEPKv   # online_lt::NocaseWStringBegin(void const*)
  __ZNK13StdCtypeWchar10do_tolowerEDs   # StdCtypeWchar::do_tolower(char16_t) const
  __ZNK13StdCtypeWchar10do_toupperEDs   # StdCtypeWchar::do_toupper(char16_t) const
  __ZNK13StdCtypeWchar9do_narrowEDsc   # StdCtypeWchar::do_narrow(char16_t, char) const
  _gArgDequeCtorBlockSize
  _gNocaseWStringTerminator
)
