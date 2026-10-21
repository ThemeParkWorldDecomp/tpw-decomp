# src files

One file per original source file of the Mac binary. The top-level folder is the library the code links into
(inventory `binary` column); the game/ subfolders are by subsystem. Evidence for each name:

- proven (__sinit): the binary has `__sinit_<name>_cpp`, emitted at the end of that file's code;
- proven (assert text): the name is in an assert or log string (analysis/binary-dev-messages.txt);
- class guess: a contiguous range of one class or subsystem, named after it;
- descriptive: a contiguous range of free functions, named after what they do;
- decomp MSL stand-ins: the decomp's hand-written stand-ins for MSL library code (MslSupport.cpp); not original source.

Main.cpp: the binary spells it Main.Cpp (__sinit_Main_Cpp).

Three behaviour changes came with the merge, each toward the original:

- game/rides/CoasterEdit.cpp declares both SetCoasterEdit(void*, int) (0x10058758) and
  SetCoasterEdit(tag_sCoasterInstance*, int) (0x100585f4) above LayoutSetCoasterEditOld (0x10058fa0), so it now
  calls the instance overload with its instance, as the original does. Its old file declared only the void*
  wrapper, which read the coaster object's word at 0x1c from the instance. LayoutSetCoasterEdit passes the object
  (void*) and still calls the wrapper.
- game/online/lobby_globe.cpp defines TPW_GLOBE_SHOWHIDE_REAL, so CLBScreenComponentPublishGlobe::Show
  (0x100aec3c) now calls the real CLBScreenComponentOnlineGlobe::Show (0x100ad788). Its old file saw the
  inline stand-in of include/lobby.h, which only called CLBScreenComponent::Show.
- The launcher now links the real GraphPanelMsg (0x101ac044, game/ui/ui_parkStatus.cpp),
  CFSOnlineSystemRideList::RequestListFromServer and StoreReceivedList (0x1017bb4c, 0x1017bbf0) and
  CFSOnlineSystemParkList_InWorldByPlayerName::RequestListFromServer (0x101796cc, all three in
  game/online/FS_OnlineList.cpp). Before, the weak stand-ins of other files (return 0, or nothing) won the static
  link, since no other symbol pulled the archive members that held the translations; the merged files are pulled.

src/OLD-PATHS.md maps the 1457 small files of before the merge to these files.

Address ranges are those of the functions present in src (main binary for game/, the shared library otherwise).
The original range column is the inventory's source file guess for the functions (analysis/function-inventory.csv,
source_file_guess): the original file whose __sinit closes the stretch of code they sit in, or "after X" for the
code after the library's last __sinit, that of X. A stretch can hold more than one original file, since a file
without static constructors has no __sinit.

src/v5_twins.cpp is not an original source file: it holds the template instances process v5 credited as byte
twins of translated instances (its first comment says how).

## src/bullfrog (42 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| AndyGLL.cpp | 60 | 0x1001de44-0x10022818 | class guess | Lbgfsave |
| Colour.cpp | 5 | 0x100193dc-0x1001a008 | proven (__sinit) | Colour |
| Draw.cpp | 92 | 0x10049590-0x100661a8 | proven (__sinit) | Draw |
| FileSystem.cpp | 249 | 0x1000a818-0x10013ca0 | class guess | LbSCBack |
| LbApp.cpp | 28 | 0x10013dec-0x10014374 | descriptive | LbSCBack |
| LbDraw.cpp | 27 | 0x1002a270-0x1002b7dc | proven (__sinit) | LbDraw |
| LbDrawText.cpp | 39 | 0x1001a2f8-0x1001be10 | descriptive | Lbgfsave |
| LbFile.cpp | 37 | 0x10000048-0x10001e04 | descriptive | LbSCBack |
| Lbgfsave.cpp | 33 | 0x1002295c-0x100240cc | proven (__sinit) | Lbgfsave |
| LbLog.cpp | 18 | 0x10014bf0-0x10015500 | descriptive | TbPixFmt |
| LbMaths.cpp | 8 | 0x100291a0-0x1002971c | proven (__sinit) | LbMaths |
| LbRect.cpp | 24 | 0x10029760-0x1002a1d4 | descriptive | LbDraw |
| LbSCBack.cpp | 15 | 0x1001462c-0x10014b4c | proven (__sinit) | LbSCBack |
| LbSemaphore.cpp | 7 | 0x10024a24-0x10024b6c | class guess | LbMaths |
| LbString.cpp | 5 | 0x10066200-0x10066360 | descriptive | WinScrn |
| LbSurface.cpp | 33 | 0x100666ac-0x10067e70 | descriptive | WinScrn |
| LbTime.cpp | 7 | 0x100468e4-0x100474cc | descriptive | Draw |
| LbZLib.cpp | 36 | 0x10024c5c-0x10025d14 | descriptive | LbMaths |
| MaskConverter.cpp | 21 | 0x10047740-0x10049430 | class guess | Draw |
| RenderContext.cpp | 71 | 0x1002b898-0x10042e64 | class guess | TbSystemInfo |
| TbBufferAccess.cpp | 17 | 0x10003e60-0x10004890 | class guess | LbSCBack |
| TbDiscAccess.cpp | 18 | 0x10004938-0x1000523c | class guess | LbSCBack |
| TbFileStorageAccess.cpp | 17 | 0x100052a8-0x10005ed0 | class guess | LbSCBack |
| TbJPEGSaver.cpp | 7 | 0x10024324-0x100249c0 | proven (__sinit) | TbJPEGSaver |
| TbMemFileAccess.cpp | 14 | 0x10005f20-0x10006730 | class guess | LbSCBack |
| TbPixFmt.cpp | 7 | 0x10016070-0x100162e8 | proven (__sinit) | TbPixFmt |
| TbQuickPosAccess.cpp | 11 | 0x100067e8-0x10006b2c | class guess | LbSCBack |
| TbRaConvert.cpp | 29 | 0x10016b10-0x1001901c | proven (__sinit) | TbRaConvert |
| TbRefpackFileAccess.cpp | 19 | 0x10006bd4-0x100072b4 | class guess | LbSCBack |
| TbRefRelease.cpp | 7 | 0x10015538-0x10015630 | class guess | TbPixFmt |
| TbRegistryKey.cpp | 32 | 0x10001e60-0x10003b04 | class guess | LbSCBack |
| TbRenderArea.cpp | 11 | 0x10016560-0x10016ad0 | class guess | TbRaConvert |
| TbSharedAccess.cpp | 12 | 0x10007360-0x1000794c | class guess | LbSCBack |
| TbStorageAccess.cpp | 10 | 0x10007a00-0x10007f2c | class guess | LbSCBack |
| TbSystemInfo.cpp | 10 | 0x100439e4-0x10043d04 | proven (__sinit) | TbSystemInfo |
| TbTimeDiff.cpp | 17 | 0x100157bc-0x10015ec4 | class guess | TbPixFmt |
| TbTimeStamp.cpp | 30 | 0x10025f0c-0x10026cc0 | class guess | LbMaths |
| TbWadFileAccess.cpp | 13 | 0x10008114-0x10008620 | class guess | LbSCBack |
| TbWadStorageAccess.cpp | 33 | 0x100086c8-0x10009fa4 | class guess | LbSCBack |
| TbZLibFileAccess.cpp | 11 | 0x1000a224-0x1000a79c | class guess | LbSCBack |
| WadNameTree.cpp | 77 | 0x10043d78-0x10046868 | class guess | Draw |
| WinScrn.cpp | 35 | 0x10068298-0x1006ab38 | proven (__sinit) | WinScrn, after WinScrn |

## src/engine (45 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| Bitmap.cpp | 11 | 0x10046db4-0x10047d24 | class guess | after tb_printf_opengl |
| Clipping.cpp | 15 | 0x1003f3bc-0x1004087c | descriptive | tb_printf_opengl |
| CsTexture.cpp | 13 | 0x10016890-0x10017270 | class guess | tb_mapzoom |
| DirectDraw.cpp | 21 | 0x10003454-0x10005098 | descriptive | tb_depthoffield |
| DrawPrimitive.cpp | 26 | 0x10006954-0x10008230 | class guess | tb_error |
| DrawPrimitiveOpenGL.cpp | 41 | 0x10042c70-0x10044fe8 | descriptive | tb_printf_opengl |
| EngineUtil.cpp | 21 | 0x100388c0-0x10039424 | descriptive | Wavelet |
| Gamut.cpp | 21 | 0x1000967c-0x1000bc70 | descriptive | tb_hfield |
| landscape.cpp | 24 | 0x1000006c-0x10002994 | proven (__sinit) | landscape |
| LightManager.cpp | 11 | 0x10014278-0x10014d38 | descriptive | tb_mapzoom |
| M3D2Load.cpp | 28 | 0x100483f4-0x1004b8d4 | descriptive | after tb_printf_opengl |
| MapWho.cpp | 27 | 0x10017318-0x10019768 | descriptive | tb_mapzoom |
| MatrixPowerPC.cpp | 17 | 0x10041918-0x100428e0 | class guess | tb_printf_opengl |
| Mesh.cpp | 33 | 0x1001ca64-0x1001f120 | class guess | tb_profiler |
| OpenGL.cpp | 19 | 0x100451a0-0x10045a90 | descriptive | tb_printf_opengl |
| OpenGLTexture.cpp | 39 | 0x10045bf0-0x10046adc | class guess | tb_printf_opengl |
| QSort.cpp | 8 | 0x1001fd38-0x10020160 | class guess | tb_resource_display |
| Rain.cpp | 29 | 0x10020458-0x100244a0 | descriptive | tb_resource_display |
| RenderList.cpp | 11 | 0x10024650-0x10026954 | descriptive | tb_resource_display |
| RenderMemory.cpp | 20 | 0x100269d4-0x10027a8c | descriptive | tb_resource_display |
| RLELoad.cpp | 16 | 0x10005104-0x10005bf8 | class guess | tb_depthoffield |
| SpriteData.cpp | 36 | 0x1002da98-0x10030b48 | class guess | tb_starfield |
| SpriteManager.cpp | 26 | 0x1003108c-0x10031fe4 | class guess | tb_starfield |
| Switch.cpp | 6 | 0x100326f0-0x10032b00 | descriptive | Wavelet |
| tb_capture.cpp | 5 | 0x10002ddc-0x10003398 | proven (__sinit) | tb_capture |
| tb_depthoffield.cpp | 15 | 0x10005c84-0x10006864 | proven (__sinit) | tb_depthoffield |
| tb_error.cpp | 7 | 0x100087e8-0x100089bc | proven (__sinit) | tb_error |
| tb_ezsprite.cpp | 15 | 0x10008b24-0x100095c0 | proven (__sinit) | tb_ezsprite |
| tb_hfield.cpp | 32 | 0x1000bca4-0x1001271c | proven (__sinit) | tb_hfield |
| tb_init.cpp | 17 | 0x1001275c-0x100141bc | proven (__sinit) | tb_init |
| tb_mapzoom.cpp | 14 | 0x10019884-0x1001a0c0 | proven (__sinit) | tb_mapzoom |
| tb_matrix.cpp | 23 | 0x1001a770-0x1001c584 | proven (__sinit) | tb_matrix |
| tb_printf_opengl.cpp | 3 | 0x10046c64-0x10046d30 | proven (__sinit) | tb_printf_opengl |
| tb_profiler.cpp | 17 | 0x1001f21c-0x1001fc8c | proven (__sinit) | tb_profiler |
| tb_resource_display.cpp | 12 | 0x10027de0-0x10028718 | proven (__sinit) | tb_resource_display |
| tb_resource_system.cpp | 18 | 0x100287a4-0x10028f38 | proven (__sinit) | tb_resource_system |
| tb_resource_texture.cpp | 6 | 0x10029078-0x10029220 | proven (__sinit) | tb_resource_texture |
| tb_shadow.cpp | 6 | 0x10029364-0x1002984c | proven (__sinit) | tb_shadow |
| tb_sky.cpp | 27 | 0x1002996c-0x1002bb04 | proven (__sinit) | tb_sky |
| tb_splash.cpp | 12 | 0x1002cb24-0x1002d7a0 | proven (__sinit) | tb_splash |
| tb_starfield.cpp | 9 | 0x1003206c-0x1003266c | proven (__sinit) | tb_starfield |
| TextureCache.cpp | 47 | 0x10032bd0-0x10038670 | class guess | Wavelet |
| TextureEntry.cpp | 16 | 0x10040d30-0x10041848 | class guess | tb_printf_opengl |
| TextureLoadManager.cpp | 18 | 0x10014e68-0x1001670c | descriptive | tb_mapzoom |
| Wavelet.cpp | 40 | 0x10039460-0x1003ed0c | proven (__sinit) | Wavelet |

## src/game (45 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| ActionRecorder.cpp | 44 | 0x101439dc-0x10146e9c | proven (__sinit) | ActionRecorder |
| CommandLine.cpp | 6 | 0x10147778-0x1014798c | class guess | Game |
| ControlManager.cpp | 18 | 0x100f9ed0-0x100fa688 | class guess | Entertainer |
| Debug.cpp | 6 | 0x1006fc5c-0x1006fd80 | proven (__sinit) | Debug |
| DelayedScript.cpp | 11 | 0x100fa898-0x100fad70 | class guess | Entertainer |
| DynamicMeshSet.cpp | 8 | 0x1003013c-0x10030ae8 | descriptive | TPAdvisor |
| ExitErr.cpp | 5 | 0x1014b62c-0x1014bf38 | proven (__sinit) | ExitErr |
| FixableTime.cpp | 10 | 0x101479c0-0x10147c18 | class guess | Game |
| Game.cpp | 42 | 0x10147d5c-0x1014a768 | proven (__sinit) | Game |
| GameSys.cpp | 30 | 0x10233de0-0x10237304 | proven (__sinit) | GameSys |
| HLSprite.cpp | 55 | 0x1007a0b8-0x1007d53c | proven (__sinit) | HLSprite |
| InGame.cpp | 4 | 0x1014b2bc-0x1014b5cc | proven (__sinit) | InGame |
| keyboard.cpp | 138 | 0x1014bf74-0x101512fc | proven (__sinit) | keyboard |
| Light.cpp | 20 | 0x10007688-0x10007b04 | class guess | Advisor |
| LMatrix.cpp | 36 | 0x100c0e28-0x100c3198 | descriptive | rsse |
| Main.cpp | 4 | 0x1015139c-0x101517e8 | proven (__sinit) | Main |
| MeshLoad.cpp | 19 | 0x100b7d04-0x100b98a4 | descriptive | rsse |
| Mouse.cpp | 99 | 0x1007de84-0x10081550 | proven (__sinit) | Mouse |
| Movie.cpp | 8 | 0x100b9914-0x100b9d58 | descriptive | rsse |
| MslSupport.cpp | 257 | - | decomp MSL stand-ins | ActionRecorder, Advisor, FS_MasterSave, FS_Registry, FS_UserInfo, HLSprite, OnlinePerson, RideDB, TPMain, World, email, online, ui_ErrorDisplay, ui_IME, ui_Postcard, ui_cityLobbyController |
| Object.cpp | 155 | 0x10105d74-0x10110f6c | proven (__sinit) | Object |
| Particles.cpp | 55 | 0x100b9dd4-0x100bf490 | descriptive | rsse |
| PausableTime.cpp | 9 | 0x1015400c-0x1015445c | class guess | RideDB |
| Script.cpp | 41 | 0x100d8630-0x100da884 | descriptive | analyser |
| ScriptOps.cpp | 43 | 0x100da95c-0x100dc59c | descriptive | analyser |
| Sound.cpp | 45 | 0x100dc5e4-0x100de9ac | descriptive | analyser |
| Text.cpp | 13 | 0x1015be6c-0x1015ea40 | proven (__sinit) | Text |
| TextureTable.cpp | 10 | 0x10030c08-0x10031b10 | descriptive | TPAdvisor |
| Thing.cpp | 25 | 0x1012c57c-0x1012ec48 | proven (__sinit) | Thing |
| TPCamera.cpp | 32 | 0x10032454-0x10036218 | proven (__sinit) | TPCamera |
| TPConfig.cpp | 8 | 0x100591a4-0x1005999c | proven (__sinit) | TPConfig |
| TPEmbed.cpp | 21 | 0x101fec54-0x10200bf0 | proven (__sinit) | TPEmbed |
| TPEngine.cpp | 27 | 0x10059c70-0x1005b6c8 | descriptive | TPRide |
| TPGlobals.cpp | 6 | 0x102328bc-0x10233378 | proven (__sinit) | TPGlobals |
| TPInstance.cpp | 12 | 0x10061aec-0x10063700 | descriptive | TPRide |
| TPLevel.cpp | 19 | 0x1006375c-0x10064b30 | descriptive | TPRide |
| TPMain.cpp | 25 | 0x1000028c-0x10001b6c | proven (__sinit) | TPMain |
| TPMesh.cpp | 18 | 0x10064bd4-0x100655d8 | descriptive | TPRide |
| TPOptions.cpp | 5 | 0x1015f1a4-0x1015f30c | proven (__sinit) | TPOptions |
| TPPointer.cpp | 8 | 0x10065648-0x100672e4 | descriptive | TPRide |
| TPResource.cpp | 182 | 0x1015f464-0x10165580 | class guess | TPSettings |
| TPSettings.cpp | 31 | 0x10165ad0-0x101675b8 | proven (__sinit) | TPSettings |
| VanillaTime.cpp | 2 | 0x101686a4-0x10168770 | proven (__sinit) | VanillaTime |
| VariableTime.cpp | 7 | 0x1016886c-0x10168a3c | class guess | FS_MasterSave |
| World.cpp | 104 | 0x10139f70-0x10141c14 | proven (__sinit) | World |

## src/game/advisor (3 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| Advisor.cpp | 89 | 0x10007c68-0x1000fff0 | proven (__sinit) | Advisor |
| AdvisorScoring.cpp | 268 | 0x10010054-0x1001c34c | proven (__sinit) | AdvisorScoring |
| TPAdvisor.cpp | 9 | 0x10031bc0-0x1003224c | proven (__sinit) | TPAdvisor |

## src/game/balance (7 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| BalanceLoader.cpp | 19 | 0x1001ea28-0x10020fd0 | proven (assert text) | HoardingsBalance |
| HoardingsBalance.cpp | 5 | 0x10021074-0x100211c4 | proven (__sinit) | HoardingsBalance |
| MainBalance.cpp | 5 | 0x100212dc-0x1002136c | proven (__sinit) | MainBalance |
| OnlineBalance.cpp | 5 | 0x10021480-0x10021510 | proven (__sinit) | OnlineBalance |
| OptionsBalance.cpp | 5 | 0x10021628-0x100216c0 | proven (__sinit) | OptionsBalance |
| ResolutionBalance.cpp | 5 | 0x10237824-0x102378bc | proven (__sinit) | ResolutionBalance |
| SoundBalance.cpp | 5 | 0x100217d4-0x10021864 | proven (__sinit) | SoundBalance |

## src/game/buildings (4 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| MechanicHQ.cpp | 8 | 0x1010521c-0x10105d2c | proven (__sinit) | MechanicHQ |
| ResearchLab.cpp | 52 | 0x10121ad0-0x101247c4 | proven (__sinit) | ResearchLab |
| StaffHQ.cpp | 23 | 0x1012a7fc-0x1012c538 | proven (__sinit) | StaffHQ |
| TrainingCentre.cpp | 14 | 0x1012fb08-0x10130564 | proven (__sinit) | TrainingCentre |

## src/game/online (21 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| bthread.cpp | 12 | 0x10200c34-0x10200fb4 | descriptive | email |
| Connection.cpp | 16 | 0x10201a84-0x10202058 | class guess | email |
| email.cpp | 82 | 0x10203940-0x1020afc0 | proven (__sinit) | email |
| EmailMessage.cpp | 11 | 0x10202114-0x102031a8 | descriptive | email |
| FS_Online.cpp | 179 | 0x1016f3c8-0x1017769c | proven (__sinit) | FS_Online |
| FS_OnlineList.cpp | 174 | 0x101783a4-0x1017cbf4 | proven (__sinit) | FS_OnlineList |
| FTPClient.cpp | 26 | 0x1020b46c-0x1020c8cc | class guess | OnlinePerson |
| IPBase.cpp | 11 | 0x1020cac8-0x1020e358 | class guess | OnlinePerson |
| LBFlyingMesh.cpp | 70 | 0x100aa5d4-0x100aba0c | class guess | lobby_globe |
| LBLobbyMeshes.cpp | 56 | 0x100a7794-0x100a8aa8 | class guess | lobby_globe |
| LBLobbyView.cpp | 38 | 0x100a8b40-0x100a9b64 | class guess | lobby_globe |
| LBScreenComponentAdvisor.cpp | 33 | 0x100a6c98-0x100a7234 | class guess | lobby_globe |
| lobby.cpp | 44 | 0x100a56c8-0x100a6be0 | proven (__sinit) | lobby |
| lobby_globe.cpp | 94 | 0x100ac534-0x100afe1c | proven (__sinit) | lobby_globe |
| lobby_island.cpp | 93 | 0x100afe88-0x100b54f8 | proven (__sinit) | lobby_island |
| lobby_parser.cpp | 6 | 0x100b55b8-0x100b6ae4 | proven (__sinit) | lobby_parser |
| lobby_screen.cpp | 43 | 0x100b6ba4-0x100b7c9c | proven (__sinit) | lobby_screen |
| online.cpp | 150 | 0x10213590-0x1022a06c | proven (__sinit) | online |
| OnlineParser.cpp | 34 | 0x1020e608-0x10210d4c | class guess | OnlinePerson |
| OnlinePerson.cpp | 11 | 0x10212f58-0x10213548 | proven (__sinit) | OnlinePerson |
| Parser.cpp | 29 | 0x10210e68-0x10211eb0 | descriptive | OnlinePerson |

## src/game/park (21 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| analyser.cpp | 179 | 0x100e2a4c-0x100ef400 | proven (__sinit) | analyser |
| BankAccount.cpp | 27 | 0x100f29f4-0x100f43dc | class guess | BlockSave |
| CellBase.cpp | 23 | 0x10081608-0x100f49fc | class guess | Entertainer, LLCellMouse |
| ChallengeHQ.cpp | 42 | 0x100f5a78-0x100f93b0 | class guess | Entertainer |
| Clock.cpp | 21 | 0x10146f24-0x10147678 | proven (__sinit) | Clock |
| DynamicCell.cpp | 22 | 0x100df8fc-0x100e28b4 | descriptive | analyser |
| GoldenTickets.cpp | 24 | 0x100fbccc-0x100fda00 | proven (__sinit) | GoldenTickets |
| LineOfSight.cpp | 8 | 0x101360ac-0x10137d04 | descriptive | World |
| LLCellMouse.cpp | 36 | 0x10081a98-0x10088ff4 | proven (__sinit) | LLCellMouse |
| LLCellRide.cpp | 35 | 0x1008905c-0x10090b2c | proven (__sinit) | LLCellRide |
| LLCellSystem.cpp | 64 | 0x10090b68-0x10096bc8 | proven (__sinit) | LLCellSystem |
| Map.cpp | 29 | 0x10100890-0x1010276c | class guess | MapDelta |
| MapCell.cpp | 50 | 0x1009722c-0x1009d898 | descriptive | lobby |
| MapDelta.cpp | 15 | 0x10102948-0x10103d88 | proven (__sinit) | MapDelta |
| ParkSound.cpp | 18 | 0x100deac8-0x100df858 | descriptive | analyser |
| SkirtDeform.cpp | 8 | 0x1006d718-0x1006fbd8 | descriptive | Debug |
| SkyObjects.cpp | 14 | 0x1015b5f0-0x1015b8b0 | class guess | Text |
| TPHField.cpp | 41 | 0x1005b734-0x1005f738 | descriptive | TPRide |
| TPHoardings.cpp | 11 | 0x1005f8c4-0x100619d4 | descriptive | TPRide |
| TrackCell.cpp | 43 | 0x1009d8d0-0x100a55bc | descriptive | lobby |
| Weather.cpp | 28 | 0x10137da0-0x101399d0 | class guess | World |

## src/game/people (14 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| Arrival.cpp | 10 | 0x100f1ff4-0x100f2840 | proven (__sinit) | Arrival |
| Entertainer.cpp | 14 | 0x100fb094-0x100fba44 | proven (__sinit) | Entertainer |
| guard.cpp | 21 | 0x100fda68-0x100fef90 | proven (__sinit) | guard |
| Handyman.cpp | 19 | 0x100fefd0-0x10100580 | proven (__sinit) | Handyman |
| mechanic.cpp | 16 | 0x1010409c-0x10105114 | proven (__sinit) | mechanic |
| ParkTime.cpp | 21 | 0x10111f14-0x10112c80 | class guess | Peep |
| Peep.cpp | 32 | 0x10112d70-0x10116418 | proven (__sinit) | Peep |
| Person.cpp | 42 | 0x10116634-0x1011ba64 | proven (__sinit) | Person |
| PersonQueue.cpp | 34 | 0x1011baa8-0x10120bc8 | proven (__sinit) | PersonQueue |
| Researcher.cpp | 19 | 0x10120c10-0x101219cc | proven (__sinit) | Researcher |
| staff.cpp | 57 | 0x1012482c-0x10127dfc | proven (__sinit) | staff |
| StaffDB.cpp | 21 | 0x10128ed8-0x1012a4f8 | proven (__sinit) | StaffDB |
| StaffInfo.cpp | 10 | 0x10128390-0x10128e7c | class guess | StaffDB |
| Thoughts.cpp | 9 | 0x1012ec88-0x1012f4ec | class guess | TrainingCentre |

## src/game/rides (23 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| BumperRide.cpp | 96 | 0x100218f4-0x10030020 | descriptive | TPAdvisor |
| CoasterCollision.cpp | 12 | 0x10047634-0x10049078 | descriptive | TPConfig |
| CoasterEdit.cpp | 65 | 0x10052a30-0x10059120 | descriptive | TPConfig |
| CoasterExtrude.cpp | 11 | 0x1004e60c-0x1004ff18 | descriptive | TPConfig |
| CoasterPylon.cpp | 42 | 0x1003aef4-0x10041c5c | descriptive | TPCoaster |
| CoasterQueue.cpp | 19 | 0x10049120-0x1004a328 | descriptive | TPConfig |
| CoasterSounds.cpp | 14 | 0x10045a38-0x10047510 | descriptive | TPConfig |
| CoasterSpline.cpp | 9 | 0x1004da1c-0x1004e318 | descriptive | TPConfig |
| CoasterTrack.cpp | 26 | 0x1005092c-0x10052954 | descriptive | TPConfig |
| CoasterTrain.cpp | 25 | 0x1004a4e8-0x1004d970 | descriptive | TPConfig |
| CoasterType.cpp | 28 | 0x10036564-0x1003acf0 | descriptive | TPCoaster |
| FlyingRide.cpp | 55 | 0x10074230-0x10079f8c | descriptive | HLSprite |
| RideAnim.cpp | 56 | 0x100c3234-0x100c8ca8 | descriptive | rsse |
| RideDB.cpp | 34 | 0x1015457c-0x10157010 | proven (__sinit) | RideDB |
| RideEffects.cpp | 22 | 0x100cc778-0x100cec58 | descriptive | rsse |
| RidePlacement.cpp | 23 | 0x101ed058-0x101ef4e0 | descriptive | ui_ErrorDisplay |
| RidePreview.cpp | 13 | 0x1006c198-0x1006d5fc | descriptive | Debug |
| RideSign.cpp | 31 | 0x100c8e40-0x100cbee4 | descriptive | rsse |
| rsse.cpp | 80 | 0x100cece4-0x100d8104 | proven (__sinit) | rsse |
| TPCoaster.cpp | 36 | 0x10042084-0x100459f4 | proven (__sinit) | TPCoaster |
| TPRide.cpp | 25 | 0x10067cf4-0x1006c134 | proven (__sinit) | TPRide |
| vehicle.cpp | 52 | 0x10130eb4-0x101349cc | proven (__sinit) | vehicle |
| VehicleCallbacks.cpp | 14 | 0x101356b8-0x10136060 | proven (__sinit) | VehicleCallbacks |

## src/game/save (9 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| BlockSave.cpp | 3 | 0x100f4470-0x100f4754 | proven (__sinit) | BlockSave |
| ChunkFileReader.cpp | 18 | 0x100f98c8-0x100f9c98 | class guess | Entertainer |
| FS_LoadSave.cpp | 22 | 0x101fc408-0x101fd668 | proven (__sinit) | FS_LoadSave |
| FS_MasterSave.cpp | 77 | 0x10169310-0x1016f35c | proven (__sinit) | FS_MasterSave |
| FS_Registry.cpp | 7 | 0x100038b0-0x10003b64 | proven (__sinit) | FS_Registry |
| FS_System.cpp | 24 | 0x10003fd4-0x10005090 | proven (__sinit) | FS_System |
| FS_UserInfo.cpp | 24 | 0x1017de54-0x1017fab0 | proven (__sinit) | FS_UserInfo |
| SaveLoad.cpp | 30 | 0x101570c4-0x1015b428 | proven (__sinit) | SaveLoad |
| TPQuickload.cpp | 8 | 0x1006736c-0x10067b50 | descriptive | TPRide |

## src/game/ui (92 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| 3DFont.cpp | 19 | 0x10005354-0x1000757c | proven (__sinit) | 3DFont |
| Colours.cpp | 4 | 0x1007d66c-0x1007d66c | proven (__sinit) | Colours |
| CoolRect.cpp | 15 | 0x101c7448-0x101c7a78 | class guess | UI_Base |
| CoolRegion.cpp | 32 | 0x101d5a3c-0x101d6200 | class guess | ui_cityLobbyController |
| IME.cpp | 9 | 0x101fe934-0x101feb18 | class guess | TPEmbed |
| InterfaceBar.cpp | 9 | 0x101c86c4-0x101c89a0 | class guess | UI_Base |
| InterfaceBFCursor.cpp | 11 | 0x101fe208-0x101fe52c | class guess | TPEmbed |
| InterfaceButton.cpp | 30 | 0x101cad20-0x101cc27c | class guess | ui_cityLobbyController |
| InterfaceCursor.cpp | 22 | 0x101cc6a0-0x101ccc8c | class guess | ui_cityLobbyController |
| InterfaceDrawEngine.cpp | 63 | 0x10187994-0x1018aa0c | class guess | ui_utils |
| InterfaceDrawObject.cpp | 16 | 0x101cccc0-0x101cd2d4 | class guess | ui_cityLobbyController |
| InterfaceDrawObjectSprite.cpp | 13 | 0x1018c978-0x1018d0bc | class guess | ui_textmanager |
| InterfaceEditBox.cpp | 21 | 0x101cd468-0x101cf1ec | class guess | ui_cityLobbyController |
| InterfaceFont.cpp | 36 | 0x10168ae0-0x101692d4 | class guess | FS_MasterSave |
| InterfaceHelpText.cpp | 20 | 0x101cf874-0x101cfdb0 | class guess | ui_cityLobbyController |
| InterfaceImage.cpp | 8 | 0x101cfe3c-0x101d0054 | class guess | ui_cityLobbyController |
| InterfaceKnob.cpp | 15 | 0x101d008c-0x101d07ac | class guess | ui_cityLobbyController |
| InterfaceLightPanel.cpp | 8 | 0x101d08f0-0x101d0b5c | class guess | ui_cityLobbyController |
| InterfaceListBox.cpp | 12 | 0x101d0b98-0x101d1210 | class guess | ui_cityLobbyController |
| InterfaceListControl2.cpp | 43 | 0x101d154c-0x101d51f0 | class guess | ui_cityLobbyController |
| InterfaceMessageQueue.cpp | 20 | 0x101c7ad8-0x101c85f0 | class guess | UI_Base |
| InterfaceScrollBar.cpp | 21 | 0x101d6280-0x101d7348 | class guess | ui_cityLobbyController |
| InterfaceStringHeap.cpp | 6 | 0x101d7808-0x101d7b60 | class guess | ui_cityLobbyController |
| InterfaceTextBox.cpp | 14 | 0x101d7bf0-0x101d8134 | class guess | ui_cityLobbyController |
| InterfaceTextObjectSprite.cpp | 79 | 0x1018d11c-0x1018f3b4 | class guess | ui_textmanager |
| InterfaceTimer.cpp | 17 | 0x101d819c-0x101d8c2c | class guess | ui_cityLobbyController |
| InterfaceVaryBox.cpp | 15 | 0x101d8c94-0x101d9674 | class guess | ui_cityLobbyController |
| InterfaceWindow.cpp | 108 | 0x101d987c-0x101dff2c | class guess | ui_cityLobbyController |
| intrface.cpp | 8 | 0x1007d9d4-0x1007de28 | proven (__sinit) | intrface |
| MessageCentre.cpp | 64 | 0x10151870-0x10152bb4 | class guess | RideDB |
| Postcard.cpp | 20 | 0x100bf7c4-0x100c0c20 | class guess | rsse |
| TPDialog.cpp | 3 | 0x10237950-0x102379c8 | proven (__sinit) | TPDialog |
| ui_2dmap.cpp | 69 | 0x1022bb44-0x10231134 | proven (__sinit) | ui_2dmap |
| ui_allItemsPanel.cpp | 23 | 0x10195978-0x1019862c | descriptive | ui_allStaffPanel |
| ui_allKidsPanel.cpp | 19 | 0x101946f8-0x1019592c | proven (__sinit) | ui_allKidsPanel |
| ui_allStaffPanel.cpp | 11 | 0x10198690-0x10199e84 | proven (__sinit) | ui_allStaffPanel |
| UI_Base.cpp | 54 | 0x101c8a5c-0x101cac94 | proven (__sinit) | UI_Base |
| ui_bogStatus.cpp | 16 | 0x10199ed0-0x1019ad70 | proven (__sinit) | ui_bogStatus |
| ui_cashPanel.cpp | 3 | 0x1019ade8-0x1019afac | descriptive | ui_entryPricePanel |
| ui_cityLobbyController.cpp | 12 | 0x101e0388-0x101e0cec | proven (__sinit) | ui_cityLobbyController |
| ui_coasterPanel.cpp | 17 | 0x1019b018-0x1019d014 | descriptive | ui_entryPricePanel |
| ui_controlPanel.cpp | 5 | 0x1019d070-0x1019d2f0 | descriptive | ui_entryPricePanel |
| ui_entryPricePanel.cpp | 8 | 0x1019d350-0x1019e260 | proven (__sinit) | ui_entryPricePanel |
| ui_ErrorDisplay.cpp | 10 | 0x101f12c8-0x101f499c | proven (__sinit) | ui_ErrorDisplay |
| ui_FeatureStatus.cpp | 14 | 0x1019e2d4-0x1019eea0 | proven (__sinit) | ui_FeatureStatus |
| ui_FinancePanel.cpp | 11 | 0x1019ef1c-0x101a0dbc | proven (__sinit) | ui_FinancePanel |
| ui_graph.cpp | 16 | 0x101f5e3c-0x101f6bec | descriptive | ui_IME |
| ui_hirePanel.cpp | 17 | 0x101a0fc8-0x101a2a14 | descriptive | ui_mainpanel2 |
| ui_IME.cpp | 5 | 0x101f6c84-0x101f7c74 | proven (__sinit) | ui_IME |
| ui_islandLobbyController.cpp | 13 | 0x101e0d98-0x101e1e3c | proven (__sinit) | ui_islandLobbyController |
| ui_iterators.cpp | 25 | 0x101f7d18-0x101fa800 | proven (__sinit) | ui_iterators |
| ui_loadSavePanel.cpp | 14 | 0x101a2bdc-0x101a3be4 | descriptive | ui_mainpanel2 |
| ui_loansPanel.cpp | 21 | 0x101a3c44-0x101a577c | descriptive | ui_mainpanel2 |
| ui_main.cpp | 85 | 0x1017fdf8-0x101877b8 | proven (__sinit) | ui_main |
| ui_mainmenu.cpp | 26 | 0x101fa8d4-0x101fc260 | proven (__sinit) | ui_mainmenu |
| ui_mainpanel2.cpp | 35 | 0x101a57fc-0x101a8df4 | proven (__sinit) | ui_mainpanel2 |
| ui_onlineCityInfoOverlay.cpp | 8 | 0x101e1ee8-0x101e2450 | proven (__sinit) | ui_onlineCityInfoOverlay |
| ui_onlineLogin.cpp | 5 | 0x101e24c8-0x101e2c78 | proven (__sinit) | ui_onlineLogin |
| ui_onlineNewsPanel.cpp | 6 | 0x101e2ce8-0x101e3628 | proven (__sinit) | ui_onlineNewsPanel |
| ui_onlineOutbox.cpp | 24 | 0x101e3698-0x101e551c | proven (__sinit) | ui_onlineOutbox |
| ui_onlinePanel.cpp | 42 | 0x101e568c-0x101e92c4 | descriptive | ui_onlineParkFindPanel |
| ui_onlineParkFindPanel.cpp | 18 | 0x101e9324-0x101ea5d8 | proven (__sinit) | ui_onlineParkFindPanel |
| ui_onlineParkInfoPanel.cpp | 9 | 0x101ea684-0x101ebae0 | proven (__sinit) | ui_onlineParkInfoPanel |
| ui_onlineParkListPanel.cpp | 9 | 0x101ebba8-0x101ec90c | proven (__sinit) | ui_onlineParkListPanel |
| ui_optionsPanel.cpp | 8 | 0x101a8e60-0x101ab904 | descriptive | ui_parkStatus |
| ui_parkStatus.cpp | 24 | 0x101ab968-0x101adbe4 | proven (__sinit) | ui_parkStatus |
| ui_playerLogin.cpp | 13 | 0x101add48-0x101afa90 | proven (__sinit) | ui_playerLogin |
| ui_Postcard.cpp | 26 | 0x101afb34-0x101b30c8 | proven (__sinit) | ui_Postcard |
| ui_progressBar.cpp | 5 | 0x101dff64-0x101e0300 | descriptive | ui_cityLobbyController |
| ui_publishParkPanel.cpp | 5 | 0x101ec9b8-0x101ecfe8 | proven (__sinit) | ui_publishParkPanel |
| ui_researchPanel.cpp | 23 | 0x101b3398-0x101b5b4c | descriptive | ui_ridepanel |
| ui_ridepanel.cpp | 13 | 0x101b5c14-0x101b8c8c | proven (__sinit) | ui_ridepanel |
| ui_RideStatus.cpp | 30 | 0x101b8cc8-0x101bc3ec | proven (__sinit) | ui_RideStatus |
| ui_shopStatus.cpp | 17 | 0x101bc468-0x101bdfb8 | proven (__sinit) | ui_shopStatus |
| ui_showStatus.cpp | 24 | 0x101c0d24-0x101c2b10 | proven (__sinit) | ui_showStatus |
| ui_StaffCostsPanel.cpp | 8 | 0x101be030-0x101bf2dc | proven (__sinit) | ui_StaffCostsPanel |
| ui_staffPanel.cpp | 16 | 0x101bf32c-0x101c0cdc | proven (__sinit) | ui_staffPanel |
| ui_staffRoomStatus.cpp | 24 | 0x101c2b88-0x101c40f0 | proven (__sinit) | ui_staffRoomStatus |
| ui_staffStatus.cpp | 25 | 0x101c41a0-0x101c5bec | proven (__sinit) | ui_staffStatus |
| ui_StatusPanel.cpp | 178 | 0x10192a3c-0x101946ac | proven (__sinit) | ui_StatusPanel |
| ui_tags.cpp | 20 | 0x10190350-0x101928dc | descriptive | ui_StatusPanel |
| ui_textmanager.cpp | 11 | 0x1018f93c-0x101902e8 | proven (__sinit) | ui_textmanager |
| ui_tooltip.cpp | 17 | 0x1018be38-0x1018c7a4 | proven (__sinit) | ui_tooltip |
| ui_upgradeStatus.cpp | 13 | 0x101c5c80-0x101c6738 | proven (__sinit) | ui_upgradeStatus |
| ui_utils.cpp | 8 | 0x1018b2c8-0x1018bda8 | proven (__sinit) | ui_utils |
| ui_visitorStatus.cpp | 31 | 0x10141f1c-0x101435c4 | proven (__sinit) | ui_visitorStatus |
| ui_worldLobbyController.cpp | 16 | 0x1022a3b4-0x1022bacc | proven (__sinit) | ui_worldLobbyController |
| ui_yearEndPanel.cpp | 6 | 0x101c67b4-0x101c73e4 | descriptive | UI_Base |
| UIGrapher.cpp | 12 | 0x101fd798-0x101fddbc | class guess | TPEmbed |
| UIInterface.cpp | 88 | 0x101ef5a8-0x101f1270 | class guess | ui_ErrorDisplay |
| UIMenu.cpp | 15 | 0x1018aa48-0x1018b298 | descriptive | ui_utils |
| vector2d.cpp | 9 | 0x101305b0-0x10130e70 | proven (__sinit) | vector2d |

## src/ltms (16 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| MslSupport.cpp | 13 | - | decomp MSL stand-ins | after TbIResourceIndex |
| StaticUnicodeBuffer.cpp | 34 | 0x10000024-0x100018d8 | class guess | TbFontRender |
| TbFontRender.cpp | 11 | 0x10001d30-0x100021fc | proven (__sinit) | TbFontRender |
| TbFontRenderParams.cpp | 28 | 0x10002350-0x10002fbc | class guess | TbIResourceIndex |
| TbIResourceIndex.cpp | 23 | 0x100032a8-0x10004068 | proven (__sinit) | TbIResourceIndex |
| TbIRLE4bitFont.cpp | 17 | 0x100040d8-0x100048ec | class guess | after TbIResourceIndex |
| TbITable.cpp | 17 | 0x100049f0-0x100053fc | class guess | after TbIResourceIndex |
| TbIUnicodeToMB.cpp | 6 | 0x10005674-0x100059ac | class guess | after TbIResourceIndex |
| TbOneColour4BitFontRenderMethod.cpp | 11 | 0x1000dff0-0x1000f558 | class guess | after TbIResourceIndex |
| TbOneColourNoBlend4BitFontRenderMethod.cpp | 12 | 0x1000d020-0x1000df64 | descriptive | after TbIResourceIndex |
| TbResource.cpp | 4 | 0x10005ae4-0x10005c68 | class guess | after TbIResourceIndex |
| TbTextFormatter.cpp | 41 | 0x10005cc0-0x10007c64 | class guess | after TbIResourceIndex |
| TbTextResourceManager.cpp | 12 | 0x100094bc-0x10009fd8 | class guess | after TbIResourceIndex |
| TbTextured4BitFontRenderMethod.cpp | 11 | 0x1000a0c0-0x1000b944 | class guess | after TbIResourceIndex |
| TbTexturedNoBlend4BitFontRenderMethod.cpp | 12 | 0x1000b9c8-0x1000cf98 | class guess | after TbIResourceIndex |
| TbVTag.cpp | 6 | 0x1000923c-0x10009408 | class guess | after TbIResourceIndex |

## src/sound (41 files)

| file | functions | address range | evidence | original range |
|---|---:|---|---|---|
| AudioComponent.cpp | 22 | 0x10001364-0x10001c60 | class guess | LbMpeg |
| AudioPlaceHolder.cpp | 78 | 0x100179a8-0x1001bae4 | proven (__sinit) | AudioPlaceHolder |
| AudioPlaceHolderList.cpp | 38 | 0x1001bb40-0x1001da8c | class guess | CSprocketsModule |
| Bank.cpp | 16 | 0x100143e8-0x10014884 | class guess | ChannelState |
| ChannelState.cpp | 23 | 0x1001493c-0x10014d6c | proven (__sinit) | ChannelState |
| CSprocketsModule.cpp | 102 | 0x1002b2d0-0x1002dd30 | proven (__sinit) | CSprocketsModule |
| FileAccess.cpp | 12 | 0x10001cf0-0x100022e4 | class guess | LbMpeg |
| LbMpeg.cpp | 48 | 0x10002310-0x100039a0 | proven (__sinit) | LbMpeg |
| ListSentence.cpp | 14 | 0x1001e720-0x1001f09c | class guess | CSprocketsModule |
| MpegDecode.cpp | 45 | 0x10003b98-0x10006f88 | class guess | StreamableMpeg2 |
| PlaceHolderSentence.cpp | 107 | 0x10023d38-0x10026c24 | class guess | CSprocketsModule |
| PlaceHolderTypes.cpp | 32 | 0x1001ddbc-0x1001e694 | class guess | CSprocketsModule |
| ServiceQueue.cpp | 13 | 0x10006ff8-0x10007590 | class guess | StreamableMpeg2 |
| SprocketsChannel.cpp | 12 | 0x1002a18c-0x1002b158 | descriptive | CSprocketsModule |
| StreamableMpeg2.cpp | 33 | 0x1000764c-0x10008760 | proven (__sinit) | StreamableMpeg2 |
| SystemCommand.cpp | 25 | 0x10014dc8-0x10016050 | class guess | AudioPlaceHolder |
| TbAudioBody.cpp | 30 | 0x100161a0-0x10017898 | class guess | AudioPlaceHolder |
| TbBank.cpp | 16 | 0x1000881c-0x10008c28 | class guess | TbMMFileBank |
| TbMapStreamer.cpp | 69 | 0x1001f0dc-0x10023a78 | class guess | CSprocketsModule |
| TbMMFileBank.cpp | 22 | 0x10008cc0-0x10009934 | proven (__sinit) | TbMMFileBank |
| TbSoundBankInfo.cpp | 11 | 0x10013f2c-0x100143b0 | class guess | ChannelState |
| TbSoundCodecLoop.cpp | 32 | 0x10009ba8-0x1000b530 | proven (__sinit) | TbSoundCodecLoop |
| TbSoundCore.cpp | 40 | 0x1000b618-0x1000c53c | class guess | TbSoundSampleCallback |
| TbSoundManager.cpp | 22 | 0x10026d24-0x100279c4 | class guess | CSprocketsModule |
| TbSoundPipe.cpp | 65 | 0x10000054-0x10001324 | class guess | LbMpeg |
| TbSoundPipeChannel.cpp | 32 | 0x100099d8-0x1000ab10 | class guess | TbSoundCodecLoop |
| TbSoundPosition.cpp | 17 | 0x1000c580-0x1000cba4 | class guess | TbSoundSampleCallback |
| TbSoundSampleCallback.cpp | 21 | 0x1000cc68-0x1000d5a4 | proven (__sinit) | TbSoundSampleCallback |
| TbSoundSampleCodec.cpp | 16 | 0x1000d608-0x1000e818 | class guess | TbSoundSampleMPEG2 |
| TbSoundSampleInfo.cpp | 14 | 0x1000dc60-0x1000e068 | class guess | TbSoundSampleMPEG2 |
| TbSoundSampleMPEG2.cpp | 18 | 0x1000e120-0x1000eccc | proven (__sinit) | TbSoundSampleMPEG2 |
| TbSoundSamplePipe.cpp | 22 | 0x1000ed30-0x1000f8d4 | proven (__sinit) | TbSoundSamplePipe |
| TbSoundSampleStore.cpp | 37 | 0x1000f948-0x10010da4 | class guess | TbSoundSampleWAV |
| TbSoundSampleWAV.cpp | 15 | 0x1000fd34-0x1001144c | proven (__sinit) | TbSoundSampleWAV |
| TbSoundSampleWAVFile.cpp | 17 | 0x10011534-0x10011be4 | proven (__sinit) | TbSoundSampleWAVFile |
| TbSoundSystemInfo.cpp | 14 | 0x10011cd4-0x100122a0 | class guess | TimerCodec |
| TbSoundSystemModule.cpp | 32 | 0x100122f0-0x10013074 | class guess | TimerCodec |
| TbSysCommand.cpp | 66 | 0x10027a64-0x1002a0c4 | class guess | CSprocketsModule |
| TimerCodec.cpp | 19 | 0x10013278-0x1001396c | proven (__sinit) | TimerCodec |
| TpBody.cpp | 19 | 0x1002de28-0x1002e8a4 | class guess | after CSprocketsModule |
| UserCallback.cpp | 8 | 0x10013a4c-0x10013ed0 | proven (__sinit) | UserCallback |

Total: 383 files.
