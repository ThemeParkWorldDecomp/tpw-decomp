# The save file header (unit w114): CSaveLoad::LoadHeader, CSaveLoadHeader::Serialise and the legal text table of SaveLoad.cpp's static
# initialisation. The Bullfrog strings are real (the table is built with them); the compare, the file, the world's ride check and the
# six LOADVAR/SAVEVAR instances are fakes in the unit file.
set(TPW_UNIT_SaveHeader src/SaveLoadHeader.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
