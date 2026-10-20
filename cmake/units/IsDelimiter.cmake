# Unit v51120: the small ltms functions (src/ltms_textformatter_rest_IsDelimiter.cpp) and, tested where they are,
# IsDelimiter, SetHorizontalJustification (with the vectors' tear_down), TbITable::AddRef and the three resource destructors.
# The callees outside the unit are recording fakes in tests/unit_IsDelimiter.cpp.
set(TPW_UNIT_IsDelimiter src/ltms_textformatter_rest_IsDelimiter.cpp src/ltms_textformatter.cpp src/TbITableResource.cpp
    src/TbIMBToUnicode.cpp src/TbIRLE4bitFont.cpp src/TbIUnicodeToMB.cpp)
