# Unit w82: SignRenderer shell, real Bitmap storage and bullfrog strings.
# Bitmap's pixel methods (w83, w86) are logged fakes in the unit file, so src/Bitmap.cpp is compiled through a renaming wrapper.
# The unit records allocation sizes and deletes through its own calllog observer.
set(TPW_UNIT_RideSignRenderer src/SignRenderer.cpp tests/w82_bitmap_renamed.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
set(TPW_UNIT_RideSignRenderer_OWN_CALLLOG ON)
