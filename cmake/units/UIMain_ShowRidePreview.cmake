# Unit v5013: the ride and sprite previews, MAKECOLUMNBUTTON, UIMain_ZoomToThing (src/game/ui/ui_main.cpp). The callees outside the unit are
# recording fakes in tests/unit_UIMain_ShowRidePreview.cpp, which also defines the globals of other files.
set(TPW_UNIT_UIMain_ShowRidePreview src/game/ui/ui_main.cpp platform/arena.cpp)
set(TPW_UNIT_UIMain_ShowRidePreview_KEEP
  __Z16MAKECOLUMNBUTTONP15InterfaceWindowPDsiiiii   # MAKECOLUMNBUTTON(InterfaceWindow*, char16_t*, int, int, int, int, int)
  __Z18UIMain_ZoomToThing12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # UIMain_ZoomToThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __Z22UIMain_ShowRidePreviewR8CoolRectji   # UIMain_ShowRidePreview(CoolRect&, unsigned int, int)
  __Z24UIMain_ShowSpritePreviewR8CoolRectP5CPeepif   # UIMain_ShowSpritePreview(CoolRect&, CPeep*, int, float)
  _gRidePreview
  _gSpritePreviewHandle
  _gSpritePreviewPeep
  _gZoomSkipThing
)
