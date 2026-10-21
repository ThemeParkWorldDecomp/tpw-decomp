# Unit v51056: CUIGrapher setters, CFSLoadSave wrappers and destructor, ui_mainmenu / TPEmbed small functions, thread and destructor helpers
# (src/game/TPEmbed.cpp, src/game/online/bthread.cpp, src/game/save/FS_LoadSave.cpp, src/game/ui/UIGrapher.cpp, src/game/ui/ui_mainmenu.cpp). The callees outside the unit are recording fakes in
# tests/unit_CUIGrapher_SetGridStyle.cpp, which also defines the globals of other files.
set(TPW_UNIT_CUIGrapher_SetGridStyle src/game/TPEmbed.cpp src/game/online/bthread.cpp src/game/save/FS_LoadSave.cpp
    src/game/ui/UIGrapher.cpp src/game/ui/ui_mainmenu.cpp platform/arena.cpp)
set(TPW_UNIT_CUIGrapher_SetGridStyle_KEEP
  __Z12operator_newjPv   # operator_new(unsigned int, void*)
  __Z14bthread_createPFPvS_ES_   # bthread_create(void* (*)(void*), void*)
  __Z19__sinit_TPEmbed_cppv   # __sinit_TPEmbed_cpp()
  __Z23__sinit_ui_mainmenu_cppv   # __sinit_ui_mainmenu_cpp()
  __Z27MainLobbyMenuConfirmQuit_OKPv   # MainLobbyMenuConfirmQuit_OK(void*)
  __Z29MainGameMenuConfirmRestart_OKPv   # MainGameMenuConfirmRestart_OK(void*)
  __Z33BankruptGameMenuConfirmRestart_OKPv   # BankruptGameMenuConfirmRestart_OK(void*)
  __Z38MainLobbyMenuPostOnlineLoginCallbackFniPKv   # MainLobbyMenuPostOnlineLoginCallbackFn(int, void const*)
  __ZN10CUIGrapher10SetBGStyleE8TbColour   # CUIGrapher::SetBGStyle(TbColour)
  __ZN10CUIGrapher11SetDrawAreaEiiii   # CUIGrapher::SetDrawArea(int, int, int, int)
  __ZN10CUIGrapher12SetGridStyleE8TbColour   # CUIGrapher::SetGridStyle(TbColour)
  __ZN10CUIGrapher13SetXAxisStyleE8TbColour   # CUIGrapher::SetXAxisStyle(TbColour)
  __ZN10CUIGrapher13SetYAxisStyleE8TbColour   # CUIGrapher::SetYAxisStyle(TbColour)
  __ZN10CUIGrapher9SetXRangeEiii   # CUIGrapher::SetXRange(int, int, int)
  __ZN10CUIGrapher9SetYRangeEiii   # CUIGrapher::SetYRange(int, int, int)
  __ZN10Connection7DestroyEPS_s   # Connection::Destroy(Connection*, short)
  __ZN11CFSLoadSave10LoadHiddenERK23TbDynamicStringTemplateIDsE   # CFSLoadSave::LoadHidden(TbDynamicStringTemplate<char16_t> const&)
  __ZN11CFSLoadSave10SaveHiddenERK23TbDynamicStringTemplateIDsE   # CFSLoadSave::SaveHidden(TbDynamicStringTemplate<char16_t> const&)
  __ZN11CFSLoadSave4SaveERK23TbDynamicStringTemplateIDsE   # CFSLoadSave::Save(TbDynamicStringTemplate<char16_t> const&)
  __ZN11CFSLoadSave7DestroyEPS_s   # CFSLoadSave::Destroy(CFSLoadSave*, short)
  __ZN12bthread_cond6signalEv   # bthread_cond::signal()
  __ZN13bthread_mutex9ConstructEPS_   # bthread_mutex::Construct(bthread_mutex*)
  __ZN15argdeque_callee10VectorDtorEPvs   # argdeque_callee::VectorDtor(void*, short)
  _gMainMenuLiteral
  _gMainMenuText
  _gMainMenuTextNode
  _gTPEmbedWordA
  _gTPEmbedWordB
)
