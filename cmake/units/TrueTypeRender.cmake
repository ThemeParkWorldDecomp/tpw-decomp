# Unit w85: TrueTypeRender (constructor, destructor shell, load_settings, render_font_unicode).
set(TPW_UNIT_TrueTypeRender src/game/rides/RideSign.cpp)
set(TPW_UNIT_TrueTypeRender_KEEP
  __ZN14TrueTypeRender13load_settingsEPN3bfl10FileSystem5CFileE   # TrueTypeRender::load_settings(bfl::FileSystem::CFile*)
  __ZN14TrueTypeRender19render_font_unicodeEPN10NS_MacDoze14InternalHandleEPKDsR6Bitmap   # TrueTypeRender::render_font_unicode(NS_MacDoze::InternalHandle*, char16_t const*, Bitmap&)
  __ZN14TrueTypeRenderC1Ev   # TrueTypeRender::TrueTypeRender()
  __ZN14TrueTypeRenderD1Ev   # TrueTypeRender::~TrueTypeRender()
)
# The unit file has its own event log and allocator (operator new[] and delete[] record events and can fail on demand).
set(TPW_UNIT_TrueTypeRender_OWN_CALLLOG ON)
# A failing new[] returns NULL in the test library, so the translation's NULL checks are live there.
cmake_language(DEFER CALL target_compile_options tpw_unit_TrueTypeRender PRIVATE -fcheck-new)
