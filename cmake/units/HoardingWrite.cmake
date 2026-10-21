# Unit w208: the park's hoarding written to the theme's hoardings balance file, and the static objects of LLCellSystem.cpp
# (src/game/park/LLCellSystem.cpp). The callees outside the unit are recording fakes in tests/unit_HoardingWrite.cpp, which also defines
# the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_HoardingWrite src/game/park/LLCellSystem.cpp platform/arena.cpp)
set(TPW_UNIT_HoardingWrite_KEEP
  __Z23LLCC_WriteHoardingsLineRK23TbDynamicStringTemplateIcEii   # LLCC_WriteHoardingsLine(TbDynamicStringTemplate<char> const&, int, int)
  __Z32LLCC_WriteHoardingsToBalanceFilev   # LLCC_WriteHoardingsToBalanceFile()
  __Z40LLCC_WriteCompleteHoardingsToBalanceFilev   # LLCC_WriteCompleteHoardingsToBalanceFile()
  _gLLCC481C
  _gLLCC4820
  _gLLCCHoardingEOL
  _gLLCCHoardingFileHeader
  _gLLCCHoardingFileNote
  _gLLCCHoardingKeyPrefix
  _gLLCCHoardingKeyX
  _gLLCCHoardingKeyY
  _gLLCCHoardingSep
  _gLLCCHoardingsFile
  _gLLCCPointedOffsets
)
