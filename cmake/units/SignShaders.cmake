# The ride sign's image filters and shaders (unit w84): erodebuffer_erode, make_gaussian_filter_2d, copy_offset_alpha, dilate, erode,
# blur, and PlasticShader and ShadowGlow. Bitmap's clear, realloc and descimate, TPFile_Read and _USwapBlock32 are fakes in the unit
# file; the allocation observer needs the unit file to include the call log itself.
set(TPW_UNIT_SignShaders src/rsse_sign_filters.cpp src/rsse_sign_shaders.cpp)
set(TPW_UNIT_SignShaders_OWN_CALLLOG ON)
