# Unit w83: the Bitmap pixel methods (all of src/engine/Bitmap.cpp, so clear, realloc and load link too).
set(TPW_UNIT_Bitmap src/engine/Bitmap.cpp)
set(TPW_UNIT_Bitmap_KEEP
  __ZN6Bitmap11colour_fillEhhhh   # Bitmap::colour_fill(unsigned char, unsigned char, unsigned char, unsigned char)
  __ZN6Bitmap11shadowblendERS_   # Bitmap::shadowblend(Bitmap&)
  __ZN6Bitmap12load_waveletEPvP8CWavelet   # Bitmap::load_wavelet(void*, CWavelet*)
  __ZN6Bitmap17swizzle_for_gimexEv   # Bitmap::swizzle_for_gimex()
  __ZN6Bitmap4loadEPv   # Bitmap::load(void*)
  __ZN6Bitmap5clearEv   # Bitmap::clear()
  __ZN6Bitmap7copy_toERS_   # Bitmap::copy_to(Bitmap&)
  __ZN6Bitmap7reallocEjjj   # Bitmap::realloc(unsigned int, unsigned int, unsigned int)
  __ZN6Bitmap8alphabltERS_jj   # Bitmap::alphablt(Bitmap&, unsigned int, unsigned int)
  __ZN6Bitmap9colourbltERS_hhhh   # Bitmap::colourblt(Bitmap&, unsigned char, unsigned char, unsigned char, unsigned char)
  __ZN6Bitmap9descimateERS_   # Bitmap::descimate(Bitmap&)
)
# The unit file owns the allocator (operator new[] and delete[] record events and can fail on demand).
set(TPW_UNIT_Bitmap_OWN_CALLLOG ON)
# A failing new[] returns NULL in the test library, so the translation's NULL checks must not be assumed dead.
cmake_language(DEFER CALL target_compile_options tpw_unit_Bitmap PRIVATE -fcheck-new)
