# Unit w83: the Bitmap pixel methods (all of src/Bitmap.cpp, so clear, realloc and load link too).
set(TPW_UNIT_Bitmap src/Bitmap.cpp)
# The unit file owns the allocator (operator new[] and delete[] record events and can fail on demand).
set(TPW_UNIT_Bitmap_OWN_CALLLOG ON)
# A failing new[] returns NULL in the test library, so the translation's NULL checks must not be assumed dead.
cmake_language(DEFER CALL target_compile_options tpw_unit_Bitmap PRIVATE -fcheck-new)
