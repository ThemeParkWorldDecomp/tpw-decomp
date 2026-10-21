# The ride sign's image filters and shaders (unit w84): erodebuffer_erode, make_gaussian_filter_2d, copy_offset_alpha, dilate, erode,
# blur, and PlasticShader and ShadowGlow. Bitmap's clear, realloc and descimate, TPFile_Read and _USwapBlock32 are fakes in the unit
# file; the allocation observer needs the unit file to include the call log itself.
set(TPW_UNIT_SignShaders src/game/rides/RideSign.cpp)
set(TPW_UNIT_SignShaders_KEEP
  __Z17copy_offset_alphaR6BitmapRKS_ii   # copy_offset_alpha(Bitmap&, Bitmap const&, int, int)
  __Z17erodebuffer_erodeR6BitmapS0_hS0_   # erodebuffer_erode(Bitmap&, Bitmap&, unsigned char, Bitmap&)
  __Z23make_gaussian_filter_2dPdid   # make_gaussian_filter_2d(double*, int, double)
  __Z4blurR6Bitmapd   # blur(Bitmap&, double)
  __Z5erodeR6Bitmapi   # erode(Bitmap&, int)
  __Z6dilateR6Bitmapi   # dilate(Bitmap&, int)
  __ZN10ShadowGlowD1Ev   # ShadowGlow::~ShadowGlow()
  __ZN13PlasticShader13load_settingsEPN3bfl10FileSystem5CFileE   # PlasticShader::load_settings(bfl::FileSystem::CFile*)
  __ZN13PlasticShader13plastic_shadeER6Bitmap   # PlasticShader::plastic_shade(Bitmap&)
  __ZN13PlasticShader14blur_with_maskER6BitmapS1_f   # PlasticShader::blur_with_mask(Bitmap&, Bitmap&, float)
  __ZN13PlasticShader5bevelER6BitmapjS1_   # PlasticShader::bevel(Bitmap&, unsigned int, Bitmap&)
  __ZN13PlasticShader5clearEv   # PlasticShader::clear()
  __ZN13PlasticShaderD1Ev   # PlasticShader::~PlasticShader()
  __ZNK10ShadowGlow11shadow_glowERK6BitmapRS0_   # ShadowGlow::shadow_glow(Bitmap const&, Bitmap&) const
)
set(TPW_UNIT_SignShaders_OWN_CALLLOG ON)
