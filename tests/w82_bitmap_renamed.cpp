// Unit w82: the real src/Bitmap.cpp (clear, realloc, load) without the seven methods this unit fakes in unit_RideSignRenderer.cpp
// (colour_fill, copy_to, swizzle_for_gimex, load_wavelet, alphablt, shadowblend, colourblt). Their real bodies (units w83 and w86)
// land in this copy under other names, so the logged fakes keep the call log the PPC side models.
#define colour_fill real_colour_fill
#define copy_to real_copy_to
#define swizzle_for_gimex real_swizzle_for_gimex
#define load_wavelet real_load_wavelet
#define alphablt real_alphablt
#define shadowblend real_shadowblend
#define colourblt real_colourblt
#include "../src/Bitmap.cpp"
