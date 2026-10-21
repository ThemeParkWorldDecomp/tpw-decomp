# Unit v5694: UIMain_GetObjectState (src/game/ui/ui_main.cpp). The callees outside the unit are recording fakes in
# tests/unit_UIMain_GetObjectState.cpp, which also defines the globals of other files.
set(TPW_UNIT_UIMain_GetObjectState src/game/ui/ui_main.cpp platform/arena.cpp)
set(TPW_UNIT_UIMain_GetObjectState_KEEP
  __Z21UIMain_GetObjectStateP7CObjecti   # UIMain_GetObjectState(CObject*, int)
)
