// Byte twins: template instances whose machine code is a byte twin of a
// translated instance of the same template in the same program. Each is credited only after the twin's own
// differential test passes with this instance's original code in the twin's place (tests/v5twin.py).
#include "TbTList.h"

#include "WorldThings.h"
class CFSOnlineCity;
class CFSOnlinePark;
class CFSOnlineRide;
class CFSOnlineServer;
class CFSOnlineTheme;
class CLBGlobe;
class CLBGlobeCityFlag;
class CLBIsland;
class CTPItemResource;
class CUI2DMapSprite;
class TFSLoadSaveSaveFileDetails;
class _LbSEFunction;

// main: ~TbPtrList 0x10170344
template class TbPtrList<CFSOnlineCity>;
// main: ~TbPtrList 0x1016fea8
template class TbPtrList<CFSOnlinePark>;
// main: ~TbPtrList 0x1017b404
template class TbPtrList<CFSOnlineRide>;
// main: ~TbPtrList 0x1017a864
template class TbPtrList<CFSOnlineServer>;
// main: ~TbPtrList 0x10179c88
template class TbPtrList<CFSOnlineTheme>;
// main: ~TbPtrList 0x100acbfc
template class TbPtrList<CLBGlobe>;
// main: ~TbPtrList 0x100acb58
template class TbPtrList<CLBGlobeCityFlag>;
// main: ~TbPtrList 0x100b09b8
template class TbPtrList<CLBIsland>;
// main: ~TbPtrList 0x101623cc
template class TbPtrList<CTPItemResource>;
// main: ~TbPtrList 0x102308d4
template class TbPtrList<CUI2DMapSprite>;
// main: ~TbPtrList 0x101fc2c8
template class TbPtrList<TFSLoadSaveSaveFileDetails>;
// main: ~TbPtrList 0x101704b0
template class TbPtrList<TbDynamicStringTemplate<char16_t>>;
// main: ~TbPtrList 0x10147cb0
template class TbPtrList<TbDynamicStringTemplate<char>>;
// main: TbTList 0x10177c6c, ~TbTList 0x10177cd0, RemoveAll 0x10177d7c, RemoveHead 0x10177e2c, Find 0x1017d7d8, RemoveAt 0x1017d858, AddTail 0x1017d93c
template class TbTList<CFSOnlineCity*, CFSOnlineCity*>;
// main: TbTList 0x101779d8, ~TbTList 0x10177a3c, RemoveAll 0x10177ae8, RemoveHead 0x10177b98, Find 0x1017d674, RemoveAt 0x1017d6f4, AddTail 0x1017da20
template class TbTList<CFSOnlinePark*, CFSOnlinePark*>;
// main: RemoveHead 0x10177758, TbTList 0x1017cce4, ~TbTList 0x1017cd48, Find 0x1017cdf4, RemoveAll 0x1017ce74, RemoveAt 0x1017cf24, AddTail 0x1017dcd0
template class TbTList<CFSOnlineRide*, CFSOnlineRide*>;
// main: RemoveHead 0x1017782c, TbTList 0x1017d008, ~TbTList 0x1017d070, Find 0x1017d120, RemoveAll 0x1017d1a4, RemoveAt 0x1017d258, AddTail 0x1017dbe8
template class TbTList<CFSOnlineServer*, CFSOnlineServer*>;
// main: RemoveHead 0x10177904, TbTList 0x1017d340, ~TbTList 0x1017d3a8, Find 0x1017d458, RemoveAll 0x1017d4dc, RemoveAt 0x1017d590, AddTail 0x1017db04
template class TbTList<CFSOnlineTheme*, CFSOnlineTheme*>;
// main: TbTList 0x100af8b8, ~TbTList 0x100af910, Find 0x100af9b0, RemoveAll 0x100afa1c, RemoveAt 0x100afac0, RemoveHead 0x100afb98, AddTail 0x100afc60
template class TbTList<CLBGlobe*, CLBGlobe*>;
// main: TbTList 0x100af498, ~TbTList 0x100af504, Find 0x100af5b8, RemoveAll 0x100af640, RemoveAt 0x100af6f8, RemoveHead 0x100af7e0, AddTail 0x100afd30
template class TbTList<CLBGlobeCityFlag*, CLBGlobeCityFlag*>;
// main: TbTList 0x10164610, ~TbTList 0x10164678, RemoveAll 0x10164728, RemoveHead 0x101647dc, AddHead 0x10164d9c
template class TbTList<CTPItemResource*, CTPItemResource*>;
// main: TbTList 0x102314e8, ~TbTList 0x10231550, Find 0x10231600, RemoveAll 0x10231684, RemoveAt 0x10231738, RemoveHead 0x1023181c, AddTail 0x102318f0
template class TbTList<CUI2DMapSprite*, CUI2DMapSprite*>;
// main: ~TbTList 0x101fd130, Find 0x101fd1f8, RemoveAll 0x101fd2a0, RemoveHead 0x101fd36c, AddTail 0x101fd458, AddHead 0x101fd560
template class TbTList<TFSLoadSaveSaveFileDetails*, TFSLoadSaveSaveFileDetails*>;
// main: TbTList 0x10177f00, ~TbTList 0x10177f80, RemoveAll 0x10178048, RemoveHead 0x10178114, AddTail 0x10178200
template class TbTList<TbDynamicStringTemplate<char16_t>*, TbDynamicStringTemplate<char16_t>*>;
// main: ~TbTList 0x1014aa0c, Find 0x1014aad4, RemoveAll 0x1014ab7c, RemoveHead 0x1014ac48, AddTail 0x1014af08, AddHead 0x1014b010
template class TbTList<TbDynamicStringTemplate<char>*, TbDynamicStringTemplate<char>*>;
// bullfrog: TbTList 0x100148b8
template class TbTList<_LbSEFunction*, _LbSEFunction*>;
// main: TbTList 0x10141d94
template TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>::TbTList();
