# Unit w33b: the engine's sprite files. The texture entry and the wavelet loader are the real ones; the unit file labels every
# operator new itself (it includes calllog.cpp).
set(TPW_UNIT_SpriteFileIO src/tb_spritefileio.cpp src/tb_textureentry.cpp src/tb_waveload.cpp)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_SpriteFileIO_OWN_CALLLOG ON)
