# Unit w44: the text formatter's vector members (clear, push_back, resize with erase and insert) and its line processor (ProcessLine,
# FixParenthesis). Not part of TPW_UNIT_TextFormatter, whose test fakes them on both sides (a real vector frees the character blocks the
# formatter's line records still point into).
set(TPW_UNIT_TextVectors src/ltms/TbTextFormatter.cpp)
set(TPW_UNIT_TextVectors_KEEP
  __Z11ProcessLineP14CCharacterInfoS0_   # ProcessLine(CCharacterInfo*, CCharacterInfo*)
  __Z14FixParenthesisP14CCharacterInfoS0_   # FixParenthesis(CCharacterInfo*, CCharacterInfo*)
  __ZN9MslVectorI14CCharacterInfoE5clearEv   # MslVector<CCharacterInfo>::clear()
  __ZN9MslVectorI14CCharacterInfoE9push_backERKS0_   # MslVector<CCharacterInfo>::push_back(CCharacterInfo const&)
  __ZN9MslVectorI7TbPointE5clearEv   # MslVector<TbPoint>::clear()
  __ZN9MslVectorI7TbPointE5eraseEPS0_S2_   # MslVector<TbPoint>::erase(TbPoint*, TbPoint*)
  __ZN9MslVectorI7TbPointE6insertEPS0_jRKS0_   # MslVector<TbPoint>::insert(TbPoint*, unsigned int, TbPoint const&)
  __ZN9MslVectorI7TbPointE6resizeEj   # MslVector<TbPoint>::resize(unsigned int)
  __ZN9MslVectorI9CLineInfoE5clearEv   # MslVector<CLineInfo>::clear()
  __ZN9MslVectorI9CLineInfoE9push_backERKS0_   # MslVector<CLineInfo>::push_back(CLineInfo const&)
)
