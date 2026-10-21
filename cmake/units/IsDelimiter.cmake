# Unit v51120: the small ltms functions (src/ltms/MslSupport.cpp, src/ltms/TbIResourceIndex.cpp, src/ltms/TbVTag.cpp) and, tested where they are,
# IsDelimiter, SetHorizontalJustification (with the vectors' tear_down), TbITable::AddRef and the three resource destructors.
# The callees outside the unit are recording fakes in tests/unit_IsDelimiter.cpp.
set(TPW_UNIT_IsDelimiter src/ltms/MslSupport.cpp src/ltms/TbIResourceIndex.cpp src/ltms/TbVTag.cpp
    src/ltms/TbTextFormatter.cpp src/ltms/TbITable.cpp src/ltms/StaticUnicodeBuffer.cpp src/ltms/TbFontRenderParams.cpp
    src/ltms/TbIRLE4bitFont.cpp src/ltms/TbIUnicodeToMB.cpp)
set(TPW_UNIT_IsDelimiter_KEEP
  __Z13MslStringDtorP9MslStrings   # MslStringDtor(MslString*, short)
  __Z14LogicErrorWhatPK11LengthError   # LogicErrorWhat(LengthError const*)
  __Z16MslExceptionWhatPKv   # MslExceptionWhat(void const*)
  __Z17CCharInfoListDtorP13CCharInfoLists   # CCharInfoListDtor(CCharInfoList*, short)
  __Z20MslVectorTbPointDtorP9MslVectorI7TbPointEs   # MslVectorTbPointDtor(MslVector<TbPoint>*, short)
  __Z22MslRefCountedCharsDtorP23MslRefCountedCharsBlocks   # MslRefCountedCharsDtor(MslRefCountedCharsBlock*, short)
  __Z22MslVectorCLineInfoDtorP9MslVectorI9CLineInfoEs   # MslVectorCLineInfoDtor(MslVector<CLineInfo>*, short)
  __Z23LtmsUSwapInplace32ULongRj   # LtmsUSwapInplace32ULong(unsigned int&)
  __Z24LtmsUSwapInplace16UShortRt   # LtmsUSwapInplace16UShort(unsigned short&)
  __Z27MslVectorCCharacterInfoDtorP9MslVectorI14CCharacterInfoEs   # MslVectorCCharacterInfoDtor(MslVector<CCharacterInfo>*, short)
  __Z28__sinit_TbIResourceIndex_cppv   # __sinit_TbIResourceIndex_cpp()
  __ZN11rank_callee13FormatterDtorEPvs   # rank_callee::FormatterDtor(void*, short)
  __ZN14TbIMBToUnicode13ConvertStringEPDsPKht   # TbIMBToUnicode::ConvertString(char16_t*, unsigned char const*, unsigned short)
  __ZN14TbIMBToUnicodeD1Ev   # TbIMBToUnicode::~TbIMBToUnicode()
  __ZN14TbIRLE4bitFont11GetCharSizeEt   # TbIRLE4bitFont::GetCharSize(unsigned short)
  __ZN14TbIRLE4bitFont12GetCharCountEv   # TbIRLE4bitFont::GetCharCount()
  __ZN14TbIRLE4bitFont12GetCharIndexERPKh   # TbIRLE4bitFont::GetCharIndex(unsigned char const*&)
  __ZN14TbIRLE4bitFont14GetMBToUnicodeEv   # TbIRLE4bitFont::GetMBToUnicode()
  __ZN14TbIRLE4bitFont18GetVerticalSpacingEv   # TbIRLE4bitFont::GetVerticalSpacing()
  __ZN14TbIRLE4bitFont7GetCapsEv   # TbIRLE4bitFont::GetCaps()
  __ZN14TbIRLE4bitFont7GetTypeEv   # TbIRLE4bitFont::GetType()
  __ZN14TbIRLE4bitFontC1EPhP16TbIResourceIndex   # TbIRLE4bitFont::TbIRLE4bitFont(unsigned char*, TbIResourceIndex*)
  __ZN14TbIRLE4bitFontD1Ev   # TbIRLE4bitFont::~TbIRLE4bitFont()
  __ZN14TbIUnicodeToMB11LocateValueEDs   # TbIUnicodeToMB::LocateValue(char16_t)
  __ZN14TbIUnicodeToMB13ConvertStringEPhPKDst   # TbIUnicodeToMB::ConvertString(unsigned char*, char16_t const*, unsigned short)
  __ZN14TbIUnicodeToMBD1Ev   # TbIUnicodeToMB::~TbIUnicodeToMB()
  __ZN14TbMBStringVTag6FormatER13CStaticBufferR11TbVarFormat   # TbMBStringVTag::Format(CStaticBuffer&, TbVarFormat&)
  __ZN15TbTextFormatter11GetCharTypeEDs   # TbTextFormatter::GetCharType(char16_t)
  __ZN15TbTextFormatter11LayoutLinesEP14CCharacterInfoS1_   # TbTextFormatter::LayoutLines(CCharacterInfo*, CCharacterInfo*)
  __ZN15TbTextFormatter14CreateInfoListEPKhR18TbFontRenderParams   # TbTextFormatter::CreateInfoList(unsigned char const*, TbFontRenderParams&)
  __ZN15TbTextFormatter18EmptyStringProcessERK6TbRect   # TbTextFormatter::EmptyStringProcess(TbRect const&)
  __ZN15TbTextFormatter22GetCharIdxFromPositionE7TbPoint   # TbTextFormatter::GetCharIdxFromPosition(TbPoint)
  __ZN15TbTextFormatter24SetVerticalJustificationENS_14eJustificationE   # TbTextFormatter::SetVerticalJustification(TbTextFormatter::eJustification)
  __ZN15TbTextFormatter26SetHorizontalJustificationENS_14eJustificationE   # TbTextFormatter::SetHorizontalJustification(TbTextFormatter::eJustification)
  __ZN15TbTextFormatter4DrawERK6TbRectPKhR18TbFontRenderParamsPS0_   # TbTextFormatter::Draw(TbRect const&, unsigned char const*, TbFontRenderParams&, TbRect*)
  __ZN15TbTextFormatter6LayoutE6TbRect   # TbTextFormatter::Layout(TbRect)
  __ZN15TbTextFormatter7MeasureERK6TbRectPKhR18TbFontRenderParamsPS0_   # TbTextFormatter::Measure(TbRect const&, unsigned char const*, TbFontRenderParams&, TbRect*)
  __ZN15TbTextFormatter8DrawTextER18TbFontRenderParamsP6TbRectb   # TbTextFormatter::DrawText(TbFontRenderParams&, TbRect*, bool)
  __ZN15TbTextFormatterC1Ev   # TbTextFormatter::TbTextFormatter()
  __ZN15TbTextFormatterD1Ev   # TbTextFormatter::~TbTextFormatter()
  __ZN7TbIFontD1Ev   # TbIFont::~TbIFont()
  __ZN8TbITable6AddRefEv   # TbITable::AddRef()
  __ZN8TbITable7ReleaseEv   # TbITable::Release()
  __ZN8TbITableC1EjPvP16TbIResourceIndex   # TbITable::TbITable(unsigned int, void*, TbIResourceIndex*)
  __ZN8TbITableD1Ev   # TbITable::~TbITable()
  __ZN9MslVectorI14CCharacterInfoE9tear_downEv   # MslVector<CCharacterInfo>::tear_down()
  __ZN9MslVectorI7TbPointE9tear_downEv   # MslVector<TbPoint>::tear_down()
  __ZN9MslVectorI9CLineInfoE9tear_downEv   # MslVector<CLineInfo>::tear_down()
  __ZN9TbIMBInfoD1Ev   # TbIMBInfo::~TbIMBInfo()
  __ZNK14TbIRLE4bitFont11GetCharDataEt   # TbIRLE4bitFont::GetCharData(unsigned short) const
  __ZNK14TbIRLE4bitFont13GetCharHeaderEt   # TbIRLE4bitFont::GetCharHeader(unsigned short) const
  __ZNK15TbTextFormatter20GetCharPositionArrayEv   # TbTextFormatter::GetCharPositionArray() const
  __ZNK15TbTextFormatter24GetCharPositionArraySizeEv   # TbTextFormatter::GetCharPositionArraySize() const
  __ZNK9TbIMBInfo8GetIndexERPKh   # TbIMBInfo::GetIndex(unsigned char const*&) const
  _gMBInfoFactoryVptr
  _gMBInfoFactoryVtbl
  _gMBToUnicodeFactoryVptr
  _gMBToUnicodeFactoryVtbl
  _gRLE4bitFontFactoryVptr
  _gRLE4bitFontFactoryVtbl
  _gTableFactoryVptr
  _gTableFactoryVtbl
  _gUnicodeToMBFactoryVptr
  _gUnicodeToMBFactoryVtbl
  __ZTS20TbRefReleaseSmartPtrI14TbIMBToUnicodeE   # template struct TbRefReleaseSmartPtr<TbIMBToUnicode> (explicit instantiation)
  __ZTS20TbRefReleaseSmartPtrI14TbIUnicodeToMBE   # template struct TbRefReleaseSmartPtr<TbIUnicodeToMB> (explicit instantiation)
  tmpl_e67638e4b10d@ltms/TbITable.cpp   # ~TbRefReleaseSmartPtr<T> (a template definition src/ltms_textformatter_rest_IsDelimiter.cpp had before the regroup)
)
# The unit file includes src/ltms_textformatter_rest.cpp of before the regroup itself; it compiles copies of the files that hold their
# definitions now (TPW_UNIT_IsDelimiter_INCLUDED), sliced to those definitions (cmake/slicer.cmake).
set(TPW_UNIT_IsDelimiter_INCLUDED src/engine/tb_depthoffield.cpp src/ltms/TbTextFormatter.cpp)
set(TPW_UNIT_IsDelimiter_INCLUDED_KEEP
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
