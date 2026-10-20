# The ltms text formatter and font renderer (unit w35a). The parameters' constructors and destructor are real (the formatter's render
# runs them); the fonts, the render methods, the parameters' other members, the vectors' other members and the line processor are fakes
# in the unit file.
set(TPW_UNIT_TextFormatter src/ltms_textformatter.cpp src/ltms_fontrender.cpp src/tb_fontrenderparams.cpp)
