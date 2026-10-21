# Unit v5723: seven small ui_main functions (src/game/ui/ui_main.cpp).
# The callees outside the unit are recording fakes in tests/unit_UIMain_ZoomToMapId.cpp, which also defines the globals of other files.
set(TPW_UNIT_UIMain_ZoomToMapId src/game/ui/ui_main.cpp platform/arena.cpp)
set(TPW_UNIT_UIMain_ZoomToMapId_KEEP
  __Z17UIMain_ZoomToPeepP5CPeep   # UIMain_ZoomToPeep(CPeep*)
  __Z18UIMain_ZoomToMapIdR12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # UIMain_ZoomToMapId(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>&)
  __Z23UIMain_GetShiftKeyStatev   # UIMain_GetShiftKeyState()
  __Z25UIMain_DestroyRidePreviewv   # UIMain_DestroyRidePreview()
  __Z25UIMain_MidWindowParticlesiP15InterfaceWindow   # UIMain_MidWindowParticles(int, InterfaceWindow*)
  __Z26UIMain_UpdateSpritePreviewv   # UIMain_UpdateSpritePreview()
  __Z27UIMain_DestroySpritePreviewv   # UIMain_DestroySpritePreview()
)
