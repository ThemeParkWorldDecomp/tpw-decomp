# Unit v51070: FTPClient::_threadfn and small library functions (src/game/MslSupport.cpp, src/game/online/FTPClient.cpp). The callees
# outside the unit are recording fakes in tests/unit_FTPClient__threadfn.cpp, which also defines the globals of other files.
set(TPW_UNIT_FTPClient__threadfn src/game/MslSupport.cpp src/game/online/FTPClient.cpp platform/arena.cpp)
set(TPW_UNIT_FTPClient__threadfn_KEEP
  __ZN6v5107010CTokenDtorEPjs   # v51070::CTokenDtor(unsigned int*, short)
  __ZN6v5107010IPBaseCtorEPj   # v51070::IPBaseCtor(unsigned int*)
  __ZN6v5107010IPBaseDtorEPjs   # v51070::IPBaseDtor(unsigned int*, short)
  __ZN6v5107013NocaseSetFindEPjPKjS2_   # v51070::NocaseSetFind(unsigned int*, unsigned int const*, unsigned int const*)
  __ZN6v5107013StrListSpliceEPjjS0_   # v51070::StrListSplice(unsigned int*, unsigned int, unsigned int*)
  __ZN6v5107014CTokenListDtorEPjs   # v51070::CTokenListDtor(unsigned int*, short)
  __ZN6v5107014WStringSetFindEPjPKjS2_   # v51070::WStringSetFind(unsigned int*, unsigned int const*, unsigned int const*)
  __ZN6v5107015NocaseWStringEqEPKjS1_   # v51070::NocaseWStringEq(unsigned int const*, unsigned int const*)
  __ZN6v5107016EraseWideStringsEPjS0_jj   # v51070::EraseWideStrings(unsigned int*, unsigned int*, unsigned int, unsigned int)
  __ZN6v5107016StreambufSeekoffEPj   # v51070::StreambufSeekoff(unsigned int*)
  __ZN6v5107016StreambufSeekposEPj   # v51070::StreambufSeekpos(unsigned int*)
  __ZN6v5107017IstreamSentryDtorEPjs   # v51070::IstreamSentryDtor(unsigned int*, short)
  __ZN6v5107017OstreamSentryDtorEPhs   # v51070::OstreamSentryDtor(unsigned char*, short)
  __ZN6v5107018ErrorDequeIterCopyEPjPKj   # v51070::ErrorDequeIterCopy(unsigned int*, unsigned int const*)
  __ZN6v510706IosTieEPKh   # v51070::IosTie(unsigned char const*)
  __ZN6v510708SockCtorEPj   # v51070::SockCtor(unsigned int*)
  __ZN6v510708SockDtorEP4Socks   # v51070::SockDtor(Sock*, short)
  __ZN6v510709WStringEqEPKjS1_   # v51070::WStringEq(unsigned int const*, unsigned int const*)
  __ZN9FTPClient9_threadfnEPv   # FTPClient::_threadfn(void*)
  _gIPErrorCodesData
  _gIPErrorTextsData
  _gSockVtblData
)
