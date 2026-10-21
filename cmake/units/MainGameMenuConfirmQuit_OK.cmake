# Unit v5958: the main menu's quit answer, item callback and fade, and CFSLoadSave::SaveInternal / GetFirstSaveFileDetails
# (src/game/save/FS_LoadSave.cpp, src/game/ui/ui_mainmenu.cpp). The callees outside the unit are recording fakes in
# tests/unit_MainGameMenuConfirmQuit_OK.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_MainGameMenuConfirmQuit_OK src/game/save/FS_LoadSave.cpp src/game/ui/ui_mainmenu.cpp platform/arena.cpp)
set(TPW_UNIT_MainGameMenuConfirmQuit_OK_KEEP
  __Z11itemOnFrameP15InterfaceWindow   # itemOnFrame(InterfaceWindow*)
  __Z14MainMenuItemCBP15InterfaceWindowiii   # MainMenuItemCB(InterfaceWindow*, int, int, int)
  __Z26MainGameMenuConfirmQuit_OKPv   # MainGameMenuConfirmQuit_OK(void*)
  __ZN11CFSLoadSave12SaveInternalERK23TbDynamicStringTemplateIDsES3_   # CFSLoadSave::SaveInternal(TbDynamicStringTemplate<char16_t> const&, TbDynamicStringTemplate<char16_t> const&)
  __ZN11CFSLoadSave23GetFirstSaveFileDetailsEv   # CFSLoadSave::GetFirstSaveFileDetails()
)
