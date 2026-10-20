# Unit w85: TrueTypeRender (constructor, destructor shell, load_settings, render_font_unicode).
set(TPW_UNIT_TrueTypeRender src/TrueTypeRender.cpp)
# The unit file has its own event log and allocator (operator new[] and delete[] record events and can fail on demand).
set(TPW_UNIT_TrueTypeRender_OWN_CALLLOG ON)
# A failing new[] returns NULL in the test library, so the translation's NULL checks are live there.
cmake_language(DEFER CALL target_compile_options tpw_unit_TrueTypeRender PRIVATE -fcheck-new)
