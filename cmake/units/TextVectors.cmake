# Unit w44: the text formatter's vector members (clear, push_back, resize with erase and insert) and its line processor (ProcessLine,
# FixParenthesis). Not part of TPW_UNIT_TextFormatter, whose test fakes them on both sides (a real vector frees the character blocks the
# formatter's line records still point into).
set(TPW_UNIT_TextVectors src/ltms_textformatter_rest.cpp)
