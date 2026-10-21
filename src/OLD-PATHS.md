# src paths before the regroup

Before v5L30b, src held 1457 small files, each named after the functions it held (main a6fedea5). The regroup
merged them into one file per original source file (src/FILES.md). This table gives, for each old file, the file
or files its code is in now. 320 old files were split over several new files: for those, the number in
parentheses is how many of the old file's external definitions (functions and data) went to that file, largest
first; a 0 means only file-local helpers, stand-ins or template instances went there. The unit tests still build
exactly the definitions each old file had (devtools/slicer.py, README.md).

Paths in comments and docs outside src were rewritten through this table. Comments that tell where code came
from (the tests/standins files, the keep list notes that say "before the regroup") keep the old names.

| old file (src/) | now in |
|---|---|
| ActionRecorder.cpp | src/game/ActionRecorder.cpp |
| ActionRecorder_CActionRecorder_CActionRecorder.cpp | src/game/ActionRecorder.cpp |
| ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ProcessAction.cpp | src/game/ActionRecorder.cpp |
| ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ProcessAction_3.cpp | src/game/ui/ui_visitorStatus.cpp (12), src/game/keyboard.cpp (8), src/game/ActionRecorder.cpp (7), src/game/InGame.cpp (3), src/game/ExitErr.cpp (1) |
| ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ProcessAction_CActionRecorder_ActivateActionRecording.cpp | src/game/ActionRecorder.cpp |
| ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ProcessAction_CActionRecorder_ProcessAction_2.cpp | src/game/ActionRecorder.cpp |
| ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ReplayRecording.cpp | src/game/ActionRecorder.cpp |
| ActionRecorder_CActionRecorder_CActionRecorder_VisitorStatusMsg_2.cpp | src/game/ui/ui_visitorStatus.cpp |
| ActionRecorder_CActionRecorder_CActionRecorder_VisitorStatusMsg_2_VisitorStatusPanel_UpdateStatusWindow_2.cpp | src/game/ui/ui_visitorStatus.cpp |
| ActionRecorder_CActionRecorder_ReadAndProcessNextAction.cpp | src/game/ActionRecorder.cpp |
| AddItemToAllRidesList.cpp | src/game/ui/ui_allKidsPanel.cpp (5), src/game/ui/ui_allItemsPanel.cpp (4) |
| AddItemToAllRidesList_FillAllRidesListCallback_Upgrades.cpp | src/game/ui/ui_allKidsPanel.cpp (5), src/game/ui/ui_allItemsPanel.cpp (4) |
| AddItemToAllRidesList_FillAllRidesListCallback_Upgrades_FillAllRidesListCallback.cpp | src/game/ui/ui_StatusPanel.cpp (4), src/game/ui/ui_allKidsPanel.cpp (4), src/game/ui/ui_allStaffPanel.cpp (4), src/game/ui/ui_entryPricePanel.cpp (4), src/game/ui/ui_textmanager.cpp (3), src/game/ui/ui_FinancePanel.cpp (2), src/game/ui/ui_allItemsPanel.cpp (2), src/game/ui/ui_controlPanel.cpp (2), src/game/ui/ui_loadSavePanel.cpp (2), src/game/ui/ui_coasterPanel.cpp (1), src/game/ui/ui_hirePanel.cpp (1), src/game/ui/ui_loansPanel.cpp (1) |
| AddItemToAllRidesList_UpdateSideshowListEntries.cpp | src/game/ui/ui_allItemsPanel.cpp |
| Advisor.cpp | src/game/advisor/Advisor.cpp |
| AdvisorBalance.cpp | src/game/advisor/Advisor.cpp |
| AdvisorBalance_LOADVARSTRUCT_2.cpp | src/game/advisor/Advisor.cpp |
| AdvisorQueue.cpp | src/game/advisor/Advisor.cpp |
| AdvisorQueue_CAdvisor_HandleEvent.cpp | src/game/advisor/Advisor.cpp |
| AdvisorQueue_CAdvisor_HandleStaffInfo.cpp | src/game/advisor/Advisor.cpp |
| AdvisorQueue_CAdvisor_ReceiveMessage.cpp | src/game/advisor/Advisor.cpp |
| AdvisorRequest.cpp | src/game/advisor/Advisor.cpp |
| AdvisorScoring.cpp | src/game/advisor/AdvisorScoring.cpp (11), src/game/advisor/Advisor.cpp (1) |
| AdvisorScoring_CAdvisorScoring__ADVISOR_CONGRAT_PRICING_DRINKS_.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_CONGRAT_PRICING_DRINKS__2.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_FEATURES_DIRTY_TOILETS_.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_GOLD_TICKET_AMOUNT_VISI.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_GOLD_TICKET_NEAR_TO_XHA_2.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_IN_THE_RED_SIX_MONTHS.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_MAX_GUARDS_V2.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_STAFF_ENTERTAINER_COVER.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_STAFF_HIRE_GUARDS_3.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_STAFF_HIRE_MECHANICS_3.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_TOILETS_CLOSED.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_TRAIN_STAFF_GENERAL.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_TUTORIAL_UI_PANEL.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN_CAdvisorScoring__ADVISOR_NEW_RESEARCH_GROUP_SIDE.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN_CAdvisorScoring__ADVISOR_NEW_RESEARCH_GROUP_SIDE_CAdvisorScoring__ADVISOR_AWARD_MOVED.cpp | src/game/advisor/AdvisorScoring.cpp (16), src/game/advisor/Advisor.cpp (10), src/game/rides/BumperRide.cpp (4) |
| AdvisorScoring_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN_CAdvisorScoring__ADVISOR_NEW_RESEARCH_GROUP_SIDE_CAdvisorScoring__ADVISOR_GENERAL_WON_FIRST_GOLDE.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN_CAdvisorScoring__ADVISOR_RIDES_POOR_SELECTION_2.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN_CAdvisorScoring__ADVISOR_SHOPS_POOR_SELECTION.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN_CAdvisorScoring__ADVISOR_STAFF_HIRE_RESEARCHERS_.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_CAdvisorScoring__ADVISOR_WEALTH_HIGH_REPAY_LOAN_CAdvisorScoring__ADVISOR_STAFF_STRIKING_ENTERTAI.cpp | src/game/advisor/AdvisorScoring.cpp |
| AdvisorScoring_sinit_Advisor_cpp.cpp | src/game/rides/BumperRide.cpp (7), src/game/TPCamera.cpp (4), src/game/advisor/Advisor.cpp (4), src/game/rides/TPCoaster.cpp (4), src/game/rides/CoasterTrain.cpp (3), src/game/rides/CoasterQueue.cpp (2) |
| Advisor_AdvisorMessage_LOADVAR_SAVEVAR.cpp | src/game/advisor/Advisor.cpp |
| Advisor_turn.cpp | src/game/advisor/Advisor.cpp |
| AllocateQueue.cpp | src/game/rides/CoasterQueue.cpp |
| AllocateQueue_UnloadTrain_2.cpp | src/game/rides/CoasterQueue.cpp |
| AnalyserSystems.cpp | src/game/park/DynamicCell.cpp (14), src/game/Script.cpp (5) |
| Antialiased_2.cpp | src/game/ui/3DFont.cpp |
| Argument_Argument_2.cpp | src/game/MslSupport.cpp |
| Arrival.cpp | src/game/people/Arrival.cpp |
| Arrival_people.cpp | src/game/people/Arrival.cpp |
| Arrival_update.cpp | src/game/people/Arrival.cpp |
| AudioPlaceHolder.cpp | src/sound/AudioPlaceHolder.cpp (52), src/sound/TbAudioBody.cpp (3), src/sound/AudioPlaceHolderList.cpp (2), src/sound/TbSoundSampleCallback.cpp (1) |
| AudioPlaceHolderList.cpp | src/sound/AudioPlaceHolderList.cpp (32), src/sound/PlaceHolderTypes.cpp (1), src/sound/TbSysCommand.cpp (1) |
| AudioPlaceHolder_LbMaths_Random.cpp | src/sound/AudioPlaceHolder.cpp |
| AudioPlaceHolder_LbMaths_Random_LbMaths_RangedRandomSlow.cpp | src/sound/PlaceHolderSentence.cpp (18), src/sound/AudioPlaceHolder.cpp (6), src/sound/TbSoundCodecLoop.cpp (4), src/sound/TbSoundSampleCallback.cpp (4), src/sound/TbSoundSampleMPEG2.cpp (3), src/sound/TbSoundSampleWAV.cpp (3), src/sound/TbSoundSamplePipe.cpp (1) |
| BMP_DrawCarsForRide.cpp | src/game/rides/BumperRide.cpp |
| BMP_FixRide.cpp | src/game/rides/BumperRide.cpp |
| BMP_FixRide_BMP_CountTrackSections.cpp | src/game/rides/BumperRide.cpp |
| BMP_FixRide_BMP_CountTrackSections_BMP_CreateRide.cpp | src/game/rides/BumperRide.cpp |
| BMP_FixRide_BMP_CountTrackSections_BMP_SetRideDuration.cpp | src/game/rides/BumperRide.cpp (6), src/game/balance/HoardingsBalance.cpp (4) |
| BMP_InsideArea.cpp | src/game/rides/BumperRide.cpp |
| BMP_InsideArea_PointOutAllInverse.cpp | src/game/rides/BumperRide.cpp |
| BMP_InsideArea_PointOutAllInverse_BMP_Distance.cpp | src/game/rides/BumperRide.cpp |
| BMP_InsideArea_PointOutAllInverse_BMP_Distance_BMP_MaskCar_2.cpp | src/game/rides/BumperRide.cpp |
| BMP_InsideArea_PointOutAllInverse_PointInColObject.cpp | src/game/rides/BumperRide.cpp |
| BMP_Load.cpp | src/game/rides/BumperRide.cpp |
| BMP_Move.cpp | src/game/rides/BumperRide.cpp |
| BMP_Move_SetNavRoute.cpp | src/game/rides/BumperRide.cpp |
| BMP_REMOVEBUMP.cpp | src/game/rides/BumperRide.cpp |
| BMP_REMOVEBUMP_BMP_PollForPeeps.cpp | src/game/rides/BumperRide.cpp |
| BMP_Tick.cpp | src/game/rides/BumperRide.cpp |
| BMP_Tick_BMP_ControlSound.cpp | src/game/rides/BumperRide.cpp |
| BMP_TrackSectionPlaceSection.cpp | src/game/rides/BumperRide.cpp |
| BankAccount.cpp | src/game/park/BankAccount.cpp |
| BankAccount_ctor.cpp | src/game/park/BankAccount.cpp |
| BankAccount_loans.cpp | src/game/park/BankAccount.cpp |
| Bitmap.cpp | src/engine/Bitmap.cpp |
| BlockSave.cpp | src/game/save/BlockSave.cpp |
| BogStatusPanel_InitViewWindow.cpp | src/game/ui/ui_bogStatus.cpp |
| BogStatusPanel_InitViewWindow_BogStatusMsg.cpp | src/game/ui/ui_bogStatus.cpp (2), src/game/ui/ui_coasterPanel.cpp (2) |
| BogStatusPanel_InitViewWindow_BogStatusPanel_UpdateStatusWindow.cpp | src/game/ui/ui_bogStatus.cpp |
| CAudioComponent.cpp | src/sound/AudioComponent.cpp |
| CBalanceTable.cpp | src/game/balance/BalanceLoader.cpp |
| CCodecFactory.cpp | src/sound/FileAccess.cpp (6), src/sound/TbSoundPipe.cpp (4), src/sound/TbSoundSampleStore.cpp (2) |
| CFSOnlinePark_CFSOnlinePark.cpp | src/game/online/FS_OnlineList.cpp |
| CFSOnlineRide_CFSOnlineRide.cpp | src/game/online/FS_OnlineList.cpp |
| CFSOnlineRide_Dtor_CFSOnlineRide.cpp | src/game/online/FS_OnlineList.cpp |
| CFSOnlineServer_CFSOnlineServer.cpp | src/game/online/FS_OnlineList.cpp |
| CFSOnlineSystemList_Dtor_CFSOnlineSystemList_2.cpp | src/game/online/FS_Online.cpp |
| CFSOnlineSystemList_LoggedInGetList.cpp | src/game/online/FS_OnlineList.cpp |
| CFSOnlineSystemRideList_RequestListFromServer.cpp | src/game/online/FS_OnlineList.cpp |
| CFSOnlineSystemRideList_RequestListFromServer_CFSOnlineCity_CFSOnlineCity.cpp | src/game/online/FS_OnlineList.cpp |
| CFSOnlineSystemServerList_RequestListFromServer.cpp | src/game/online/FS_OnlineList.cpp |
| CFSOnlineSystemServerList_RequestListFromServer_CFSOnlineSystemServerList_ReturnFTPDownloadServe.cpp | src/game/online/FS_OnlineList.cpp (16), src/game/online/FS_Online.cpp (10), src/game/ui/ui_main.cpp (1) |
| CFSOnlineSystemThemeList_GetList_2.cpp | src/game/online/FS_OnlineList.cpp |
| CFSOnlineTheme_CFSOnlineTheme.cpp | src/game/online/FS_OnlineList.cpp |
| CFixedTime.cpp | src/game/FixableTime.cpp |
| CFont_set.cpp | src/game/ui/intrface.cpp |
| CLBGlobeCityFlag_CLBGlobeCityFlag.cpp | src/game/online/lobby_globe.cpp |
| CLBScreenComponentOnlineIslands_GotoNextIsland.cpp | src/game/online/lobby_island.cpp |
| CLBScreenComponentOnlineIslands_GotoNextIsland_CLBScreenComponentOnlineIslands_CreateParks.cpp | src/game/online/lobby_island.cpp |
| CLBScreenComponentOnlineIslands_GotoNextIsland_CLBScreenComponentOnlineIslands_StartCurrentIsla.cpp | src/game/online/lobby_globe.cpp (10), src/game/online/lobby_island.cpp (10) |
| CLBScreenComponentOnlineIslands_GotoNextIsland_OnlineIslandParkDownloadCompleteCallback_2.cpp | src/game/HLSprite.cpp (9), src/game/online/lobby_parser.cpp (5), src/game/online/lobby_island.cpp (3), src/game/online/lobby_globe.cpp (2), src/game/ui/intrface.cpp (1) |
| COnlineControl_COnlineControl.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_CreateVisitor.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_DoCommand.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_EarmuffsOff.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_IgnoreOff.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_NoLongerAFK.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_ProcessInitResponse.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_ProcessQueue.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_ProcessQueue_std_basic_ofstream_Dtor_basic_ofstream_3.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_ProcessResponse.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_SaveBuddy.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_SaveIgnore.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_SaveIgnore_OnlineChatData_OnlineChatData.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_SaveIgnore_std_basic_istream_Dtor_basic_istream_2.cpp | src/game/online/online.cpp |
| COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_ChatCallbackFn.cpp | src/game/online/online.cpp |
| COnlineParser_ParseGoto.cpp | src/game/online/OnlineParser.cpp |
| COnlineParser_ParseGoto_COnlineParser_COnlineParser.cpp | src/game/online/OnlineParser.cpp |
| COnlineParser_ParseGoto_COnlineParser_COnlineParser_cmp_nocase.cpp | src/game/online/Parser.cpp |
| COnlineParser_ParseGoto_COnlineParser_COnlineParser_cmp_nocase_CParser_CParser.cpp | src/game/online/Parser.cpp |
| COnlineParser_ParseGoto_COnlineParser_COnlineParser_cmp_nocase_CParser_CParser_CParser_Parse.cpp | src/game/online/Parser.cpp |
| COnlineParser_ParseGoto_COnlineParser_COnlineParser_cmp_nocase_CParser_CParser_CParser_Parse_CParser_IgnoreRemainingString.cpp | src/game/online/Parser.cpp (17), src/game/online/online.cpp (6), src/game/online/OnlinePerson.cpp (3) |
| COnlineParser_ParseGoto_COnlineParser_COnlineParser_std_vector_clear.cpp | src/game/online/OnlineParser.cpp (6), src/game/MslSupport.cpp (1) |
| COnlineParser_ParseGoto_COnlineParser_InitToken.cpp | src/game/online/OnlineParser.cpp |
| COnlineParser_ParseGoto_COnlineParser_ParseIgnore.cpp | src/game/online/OnlineParser.cpp |
| COnlineParser_ParseGoto_COnlineParser_ParseIgnore_Sock_Close.cpp | src/game/online/IPBase.cpp |
| COnlineParser_ParseGoto_COnlineParser_ParseReply_2.cpp | src/game/online/OnlineParser.cpp |
| COnlineParser_ParseGoto_std_list_clear.cpp | src/game/online/OnlineParser.cpp |
| COnlineParser_ParseGoto_std_list_insert.cpp | src/game/online/OnlineParser.cpp |
| COnlinePerson_SetDest.cpp | src/game/online/OnlinePerson.cpp |
| COnlinePerson_SetDest_GotoPark_OK.cpp | src/game/online/online.cpp |
| COnlinePerson_SetDest_GotoPark_OK_std_queue_std_vector_std_deque_std_vector_std_al.cpp | src/game/MslSupport.cpp |
| COnlinePerson_SetDest_VoteNotifyCallback.cpp | src/game/online/IPBase.cpp (9), src/game/online/FTPClient.cpp (1), src/game/online/online.cpp (1) |
| COnlinePerson_SetDest_std___tree_insert_node_at_2.cpp | src/game/MslSupport.cpp |
| CPU.cpp | src/engine/landscape.cpp |
| CPeep_IsLocalAreaUnchangedSinceBecomingStranded.cpp | src/game/people/Peep.cpp |
| CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound.cpp | src/game/people/Peep.cpp |
| CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound_CPeep_CPeep.cpp | src/game/people/Peep.cpp |
| CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound_CPeep_Dtor_CPeep.cpp | src/game/people/Peep.cpp (3), src/game/people/Person.cpp (1), src/game/advisor/AdvisorScoring.cpp (0) |
| CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound_CPeep_GetInterpolatedPeepPosition.cpp | src/game/people/Peep.cpp |
| CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound_CPeep_SetRandomDestination.cpp | src/game/people/Peep.cpp |
| CPeep_IsLocalAreaUnchangedSinceBecomingStranded_CPeep_PlaySound_CPeep_UpdatePositionInEngine.cpp | src/game/people/Peep.cpp |
| CPeep_IsLocalAreaUnchangedSinceBecomingStranded_SAVEVAR_2.cpp | src/game/people/Peep.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark.cpp | src/game/people/PersonQueue.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_CPerson.cpp | src/game/people/Person.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_DismissFromQueue.cpp | src/game/people/PersonQueue.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin.cpp | src/game/people/PersonQueue.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged.cpp | src/game/people/Person.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_CalculateOptionScore.cpp | src/game/people/Person.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_EveryTimeMods_2.cpp | src/game/people/Person.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_GetHeadSpriteID.cpp | src/game/people/Person.cpp (4), src/game/people/ParkTime.cpp (2), src/game/people/Peep.cpp (1) |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_LeaveQueue.cpp | src/game/people/PersonQueue.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_ModelState.cpp | src/game/people/Person.cpp (5), src/game/people/PersonQueue.cpp (3) |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_ProcessStateWalksToLitterBin.cpp | src/game/people/PersonQueue.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_ProcessStateWalksToMajor.cpp | src/game/people/PersonQueue.cpp (3), src/game/people/Person.cpp (1) |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_Serialise.cpp | src/game/people/Person.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_SetState.cpp | src/game/people/PersonQueue.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_CPerson_WinningMods.cpp | src/game/people/Person.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_LOADVAR.cpp | src/game/people/Person.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_FindNearestReachableLitterBin_CPerson_GetBagged_LOADVAR_CMsgPeepArrived_GetType.cpp | src/game/people/Person.cpp (8), src/game/people/PersonQueue.cpp (7), src/game/rides/VehicleCallbacks.cpp (7), src/game/buildings/MechanicHQ.cpp (4), src/game/people/guard.cpp (3), src/game/people/Peep.cpp (2), src/game/ui/MessageCentre.cpp (1) |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_ProcessStateIdle.cpp | src/game/people/PersonQueue.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_TooPricey.cpp | src/game/people/Person.cpp |
| CPerson_ProcessStateWaitingToCrossRoadOutOfPark_CPerson_UpdateTurn.cpp | src/game/people/PersonQueue.cpp |
| CSprocketsModule_Init.cpp | src/sound/CSprocketsModule.cpp |
| CStaticUnicodeBuffer.cpp | src/ltms/StaticUnicodeBuffer.cpp |
| CStaticUnicodeBuffer_CStaticMBBuffer_AppendMB.cpp | src/ltms/StaticUnicodeBuffer.cpp |
| CUI2DMap_CreateMapSprites.cpp | src/game/ui/ui_2dmap.cpp |
| CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre.cpp | src/game/ui/ui_2dmap.cpp |
| CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition.cpp | src/game/ui/ui_2dmap.cpp |
| CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMapSprite_UpdateVisibilityAndColour.cpp | src/game/ui/ui_2dmap.cpp |
| CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_MapCallback.cpp | src/game/ui/ui_2dmap.cpp |
| CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister.cpp | src/game/ui/ui_2dmap.cpp |
| CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister_CUI2DMap_CreateMapPanel.cpp | src/game/ui/ui_2dmap.cpp |
| CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister_CUI2DMap_MapAreaCallback.cpp | src/game/ui/ui_2dmap.cpp |
| CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister_CUI2DMap_Register.cpp | src/game/MslSupport.cpp (17), src/game/ui/ui_2dmap.cpp (6) |
| CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister_CUI2DMap_UpdateAllDrawRelated_2.cpp | src/game/ui/ui_2dmap.cpp |
| CVariableTime.cpp | src/game/VariableTime.cpp |
| CalculateMaxVertexHeights.cpp | src/game/park/TPHField.cpp |
| CalculateMaxVertexHeights_GetRideHeightAtPoint.cpp | src/game/park/TPHField.cpp |
| CellBase.cpp | src/game/park/CellBase.cpp |
| CellInit.cpp | src/game/park/CellBase.cpp (2), src/game/buildings/TrainingCentre.cpp (1), src/game/park/MapDelta.cpp (1), src/game/people/Researcher.cpp (1) |
| Challenge.cpp | src/game/park/ChallengeHQ.cpp |
| ChallengeBalance.cpp | src/game/park/ChallengeHQ.cpp |
| ChallengeHQ_ctor.cpp | src/game/park/ChallengeHQ.cpp |
| ChallengeHQ_init.cpp | src/game/park/ChallengeHQ.cpp |
| ChallengeHQ_offer.cpp | src/game/park/ChallengeHQ.cpp |
| ChallengeHQ_progress.cpp | src/game/park/ChallengeHQ.cpp |
| ChallengeHQ_turn.cpp | src/game/park/ChallengeHQ.cpp |
| ChangeInfoWindow.cpp | src/game/ui/ui_ridepanel.cpp |
| ChangeInfoWindow_RidePanelMsg.cpp | src/game/ui/ui_ridepanel.cpp |
| ChangeInfoWindow_UpdateInfoWindow.cpp | src/game/ui/ui_ridepanel.cpp |
| Cheat.cpp | src/game/park/Clock.cpp |
| Cheat_std_basic_string_init.cpp | src/game/MslSupport.cpp |
| ChunkFile.cpp | src/game/save/ChunkFileReader.cpp (4), src/game/World.cpp (3), src/game/park/Map.cpp (1) |
| Clock.cpp | src/game/park/Clock.cpp |
| CoasterBalance.cpp | src/game/rides/TPCoaster.cpp (3), src/game/rides/CoasterType.cpp (1) |
| CoasterDestroy.cpp | src/game/rides/TPCoaster.cpp |
| Colours.cpp | src/game/ui/Colours.cpp |
| CommandLine.cpp | src/game/CommandLine.cpp |
| ContactsLCFormat.cpp | src/game/ui/ui_onlinePanel.cpp |
| ContactsLCFormat_OnlineEditMsg.cpp | src/game/ui/ui_onlinePanel.cpp |
| ContactsLCFormat_OnlineReceiveString_2.cpp | src/game/ui/ui_onlinePanel.cpp |
| ControlManager.cpp | src/game/ControlManager.cpp |
| ControlManager_setup.cpp | src/game/ControlManager.cpp |
| CreateAllStaffPanel.cpp | src/game/ui/ui_allStaffPanel.cpp |
| CreateAllStaffPanel_LCFormat.cpp | src/game/ui/ui_allStaffPanel.cpp (5), src/game/advisor/AdvisorScoring.cpp (0) |
| CreateDirectDrawObject.cpp | src/bullfrog/WinScrn.cpp |
| CreateDirectDrawObject_LbScreen_PartSwap.cpp | src/bullfrog/WinScrn.cpp |
| CreateDirectDrawObject_LbScreen_PartSwap_LbScreen_RestoreMode.cpp | src/bullfrog/WinScrn.cpp |
| CreateDirectDrawObject_RegisterBullfrogWindowClass_2.cpp | src/bullfrog/TbSystemInfo.cpp (6), src/bullfrog/FileSystem.cpp (3), src/bullfrog/WinScrn.cpp (3), src/bullfrog/LbRect.cpp (2) |
| CreateDirectDrawObject_RegisterBullfrogWindowClass_2_LbSystemInfo_CPUGetVendor.cpp | src/bullfrog/LbRect.cpp (13), src/bullfrog/WinScrn.cpp (6), src/bullfrog/TbSystemInfo.cpp (5), src/bullfrog/LbApp.cpp (2), src/bullfrog/Draw.cpp (1), src/bullfrog/LbMaths.cpp (1), src/bullfrog/LbTime.cpp (1), src/bullfrog/TbJPEGSaver.cpp (1) |
| CreateDirectDrawObject_TbFPUSinglePrecision_Dtor_TbFPUSinglePrecision.cpp | src/bullfrog/WinScrn.cpp |
| CreateFinancePanel_2.cpp | src/game/ui/ui_FinancePanel.cpp |
| CreateOnlineParkInfoPanel.cpp | src/game/ui/ui_onlineParkInfoPanel.cpp |
| CreateOptionsPanel.cpp | src/game/ui/ui_optionsPanel.cpp |
| CreateParkStatusPanel.cpp | src/game/ui/ui_parkStatus.cpp |
| CreateResearchPanel.cpp | src/game/ui/ui_researchPanel.cpp |
| CreateResearchPanel_RideMsg.cpp | src/game/ui/ui_researchPanel.cpp |
| CreateResearchPanel_RideMsg_AnimatingPumpMsg.cpp | src/game/ui/ui_researchPanel.cpp |
| CreateRideListPanel.cpp | src/game/ui/ui_ridepanel.cpp |
| CreateRideListPanel_LCHandler.cpp | src/game/ui/ui_researchPanel.cpp |
| CreateRidePreview.cpp | src/game/rides/RidePreview.cpp |
| CreateStaffCostsPanel.cpp | src/game/ui/ui_StaffCostsPanel.cpp |
| CreateStaffCostsPanel_UpdateStaffCost.cpp | src/game/ui/ui_StaffCostsPanel.cpp (2), src/game/ui/ui_shopStatus.cpp (1) |
| CreateStaffCostsPanel_UpdateStaffCost_sinit_ui_shopStatus_cpp.cpp | src/game/ui/ui_shopStatus.cpp |
| DateMessages.cpp | src/game/people/ParkTime.cpp (3), src/game/people/Peep.cpp (3), src/game/ui/MessageCentre.cpp (3) |
| Debug.cpp | src/game/Debug.cpp |
| Debug_blendflags.cpp | src/game/park/SkirtDeform.cpp |
| Debug_blendflags_CalcSkirtTextureDirnFlags.cpp | src/game/park/SkirtDeform.cpp |
| Debug_blendflags_DoMeshSkirtDeform.cpp | src/game/park/SkirtDeform.cpp |
| Debug_overlays.cpp | src/game/rides/RidePreview.cpp |
| Debug_overlays_DecideRideShapeDeformEdges.cpp | src/game/rides/RidePreview.cpp (3), src/game/park/SkirtDeform.cpp (1) |
| Debug_overlays_DestroyRidePreview.cpp | src/game/rides/RidePreview.cpp |
| Debug_overlays_DoOrSetupSkirtDeform.cpp | src/game/park/SkirtDeform.cpp |
| Debug_pointer.cpp | src/game/rides/RidePreview.cpp |
| DelayedScript.cpp | src/game/DelayedScript.cpp |
| DestroyCoasterPylons.cpp | src/game/rides/CoasterTrack.cpp |
| DestroyCoasterPylons_CalcNumTrackSamples.cpp | src/game/rides/CoasterTrack.cpp |
| DestroyCoasterPylons_CalcNumTrackSamples_LinearSampleTrack.cpp | src/game/rides/CoasterTrack.cpp |
| DestroyCoasterPylons_SetupBraceBezierCtrlParams.cpp | src/game/rides/CoasterTrack.cpp |
| DestroyCoasterPylons_SetupBraceBezierCtrlParams_SetInitialOrientationParams.cpp | src/game/rides/CoasterTrack.cpp (9), src/game/rides/CoasterEdit.cpp (7), src/game/TPLevel.cpp (2), src/game/TPMesh.cpp (2), src/game/park/TPHField.cpp (1), src/game/park/TPHoardings.cpp (1), src/game/rides/CoasterSpline.cpp (1) |
| DisableCurrentPylonEditState.cpp | src/game/rides/CoasterEdit.cpp |
| DisableCurrentPylonEditState_SetCoasterEditState.cpp | src/game/rides/CoasterEdit.cpp |
| DisableCurrentPylonEditState_SetCoasterEditState_DeletePylon.cpp | src/game/rides/CoasterEdit.cpp |
| DisableCurrentPylonEditState_SetCoasterEditState_DoDeleteMultiPylons.cpp | src/game/rides/CoasterEdit.cpp |
| DoMovePylon.cpp | src/game/rides/CoasterEdit.cpp |
| DoScroll.cpp | src/game/ui/ui_researchPanel.cpp |
| DoScroll_NameWindow.cpp | src/game/ui/ui_researchPanel.cpp |
| Draw_colour_convert.cpp | src/bullfrog/MaskConverter.cpp (14), src/bullfrog/Colour.cpp (4) |
| Draw_colour_convert_LbPalette_GenerateGhostTable.cpp | src/bullfrog/LbTime.cpp |
| Draw_colour_convert_LbString_strnzcpy.cpp | src/bullfrog/LbString.cpp |
| Draw_renderclass_circles.cpp | src/bullfrog/Draw.cpp |
| Draw_renderclass_primitives.cpp | src/bullfrog/Draw.cpp |
| Draw_state.cpp | src/bullfrog/Draw.cpp |
| Draw_state_bfl_draw_CDraw_SetSpritePalette.cpp | src/bullfrog/Draw.cpp |
| DynamicCells.cpp | src/game/park/DynamicCell.cpp |
| DynamicCells_STATIC_WhiteOut.cpp | src/game/park/DynamicCell.cpp |
| DynamicFaces.cpp | src/game/park/DynamicCell.cpp |
| DynamicFlags.cpp | src/game/park/DynamicCell.cpp |
| DynamicMesh.cpp | src/game/park/DynamicCell.cpp (11), src/game/DynamicMeshSet.cpp (3), src/game/MeshLoad.cpp (1), src/game/park/LLCellSystem.cpp (1) |
| ESP_sprites.cpp | src/game/HLSprite.cpp |
| ESP_sprites_DrawHLSprites.cpp | src/game/HLSprite.cpp |
| ESP_sprites_ESP_SwitchToFirstPersonSprites.cpp | src/game/HLSprite.cpp |
| Email_Send.cpp | src/game/online/EmailMessage.cpp |
| Email_Send_CSock_SendData.cpp | src/game/online/Connection.cpp |
| Email_Send_CSock_SendData_Connection_RecvResponse.cpp | src/game/online/Connection.cpp |
| Email_Send_CSock_SendData_base64enc.cpp | src/game/online/Connection.cpp (10), src/game/online/EmailMessage.cpp (1), src/game/ui/UIGrapher.cpp (1) |
| Email_Send_Email_Callback_2.cpp | src/game/online/EmailMessage.cpp |
| Email_Send_Email_SendToOutbox.cpp | src/game/online/EmailMessage.cpp |
| EmbeddedData.cpp | src/game/TPEmbed.cpp |
| Entertainer.cpp | src/game/people/Entertainer.cpp |
| ExitErr.cpp | src/game/ExitErr.cpp |
| ExitErr_report.cpp | src/game/ExitErr.cpp |
| FLY_Load.cpp | src/game/rides/FlyingRide.cpp (2), src/game/HLSprite.cpp (1) |
| FLY_Load_SamsUtilities_USwapBlockStruct.cpp | src/game/HLSprite.cpp |
| FLY_SetNumberOfCars.cpp | src/game/rides/FlyingRide.cpp |
| FLY_SetNumberOfCars_FLY_Save.cpp | src/game/rides/FlyingRide.cpp |
| FS_LoadSave.cpp | src/game/save/FS_LoadSave.cpp |
| FS_LoadSave_load.cpp | src/game/save/FS_LoadSave.cpp |
| FS_MasterSave.cpp | src/game/save/FS_MasterSave.cpp |
| FS_MasterSave_easy.cpp | src/game/save/FS_MasterSave.cpp |
| FS_MasterSave_file.cpp | src/game/save/FS_MasterSave.cpp |
| FS_MasterSave_global.cpp | src/game/save/FS_MasterSave.cpp |
| FS_MasterSave_global_ctor.cpp | src/game/save/FS_MasterSave.cpp |
| FS_MasterSave_global_std___tree_std_pair_std_map_TbDynamicStringTempl.cpp | src/game/MslSupport.cpp |
| FS_MasterSave_park.cpp | src/game/save/FS_MasterSave.cpp |
| FS_MasterSave_signs.cpp | src/game/save/FS_MasterSave.cpp |
| FS_MasterSave_tickets.cpp | src/game/save/FS_MasterSave.cpp |
| FS_Online.cpp | src/game/online/FS_Online.cpp |
| FS_OnlineList.cpp | src/game/online/FS_OnlineList.cpp |
| FS_OnlineList_dtors.cpp | src/game/online/FS_Online.cpp |
| FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor.cpp | src/game/online/FS_Online.cpp (1), src/game/online/FS_OnlineList.cpp (1) |
| FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_CFSOnlineSystemParkList_InCity_GetList.cpp | src/game/online/FS_OnlineList.cpp |
| FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_CFSOnlineSystemParkList_InCity_GetList_CFSOnlineSystemCityList_StoreReceivedList.cpp | src/game/online/FS_OnlineList.cpp |
| FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_CFSOnlineSystemParkList_InCity_GetList_CFSOnlineSystemCityList_StoreReceivedList_CFSOnlineSystemCityList_ReturnCityInfo.cpp | src/game/online/FS_OnlineList.cpp (9), src/game/online/FS_Online.cpp (3), src/game/save/FS_MasterSave.cpp (1), src/game/advisor/AdvisorScoring.cpp (0) |
| FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_CFSOnlineSystemParkList_InCity_GetList_CFSOnlineSystemParkList_InCity_Dtor_CFSOnlineSys.cpp | src/game/online/FS_Online.cpp (1), src/game/online/FS_OnlineList.cpp (1) |
| FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_CFSOnlineSystemParkList_InWorldByPlayerName_Requ.cpp | src/game/online/FS_OnlineList.cpp |
| FS_Online_FSOnlineLoginRASFailureCallback.cpp | src/game/online/FS_Online.cpp |
| FS_Online_gametype.cpp | src/game/online/FS_Online.cpp |
| FS_Online_gametype_CFSOnlineSystemParkList_Dtor_CFSOnlineSystemPark.cpp | src/game/online/FS_Online.cpp |
| FS_Online_gametype_std___tree_insert_node_at_3.cpp | src/game/MslSupport.cpp (5), src/game/online/FS_Online.cpp (4) |
| FS_Online_gametype_std___tree_insert_node_at_3_sinit_FS_MasterSave_cpp.cpp | src/game/keyboard.cpp (24), src/game/online/FS_Online.cpp (5), src/game/save/FS_MasterSave.cpp (1) |
| FS_Online_gametype_std_pair_pair.cpp | src/game/MslSupport.cpp |
| FS_Online_init.cpp | src/game/online/FS_Online.cpp |
| FS_Online_init_CFSOnlineSystemList_CompleteGetList.cpp | src/game/online/FS_Online.cpp |
| FS_Online_init_CFSOnlineSystem_CityServerConnectionCallback.cpp | src/game/online/FS_Online.cpp |
| FS_Online_init_CFSOnlineSystem_CompleteFTPDownload.cpp | src/game/online/FS_Online.cpp (8), src/game/online/FS_OnlineList.cpp (2) |
| FS_Online_init_CFSOnlineSystem_CompleteFTPDownload_sinit_FS_Online_cpp.cpp | src/game/online/FS_Online.cpp |
| FS_Online_init_CFSOnlineSystem_ConnectUploadServer.cpp | src/game/online/FS_Online.cpp |
| FS_Online_init_CFSOnlineSystem_EnterChatSatellite.cpp | src/game/online/FS_Online.cpp |
| FS_Online_init_CFSOnlineSystem_GotoPlayerByIDFTPPostLoginCallba.cpp | src/game/online/FS_Online.cpp (21), src/game/save/FS_MasterSave.cpp (4), src/game/ui/ui_FeatureStatus.cpp (1), src/game/ui/ui_bogStatus.cpp (1) |
| FS_Online_init_CFSOnlineSystem_LoggedInRequestUpload.cpp | src/game/online/FS_Online.cpp |
| FS_Online_init_CFSOnlineSystem_LoggedInRequestUpload_UploadPostLoginCallbackFn.cpp | src/game/online/FS_Online.cpp (8), src/game/online/FS_OnlineList.cpp (1) |
| FS_Online_init_CFSOnlineSystem_LoginConnectionCallback.cpp | src/game/online/FS_Online.cpp |
| FS_Online_init_FSOnlineCityServerConnectionCallback.cpp | src/game/online/FS_Online.cpp |
| FS_Online_init_ccServerInfo_Dtor_ccServerInfo.cpp | src/game/online/FS_Online.cpp |
| FS_Online_tick.cpp | src/game/online/FS_Online.cpp |
| FS_Registry.cpp | src/game/save/FS_Registry.cpp |
| FS_Registry_OnlineConfig_Dtor_OnlineConfig.cpp | src/game/MslSupport.cpp |
| FS_System.cpp | src/game/save/FS_System.cpp |
| FS_System_CSYS_TPFileInfo_Dtor_CSYS_TPFileInfo.cpp | src/game/ui/3DFont.cpp |
| FS_UserInfo.cpp | src/game/save/FS_UserInfo.cpp |
| FS_UserInfo_CFSUserInfo_GameLogout.cpp | src/game/save/FS_UserInfo.cpp |
| FS_UserInfo_ctor.cpp | src/game/save/FS_UserInfo.cpp |
| FS_UserInfo_slots.cpp | src/game/save/FS_UserInfo.cpp |
| FS_login.cpp | src/game/save/FS_MasterSave.cpp (2), src/game/save/FS_UserInfo.cpp (2) |
| FTPClient_RecvData.cpp | src/game/online/FTPClient.cpp |
| FTPClient_RecvData_FTPClient_ParseFileSize.cpp | src/game/online/FTPClient.cpp |
| FTPClient_RecvData_FTPClient_ParseFileSize_FTPClient__threadfn.cpp | src/game/MslSupport.cpp (11), src/game/online/FTPClient.cpp (11) |
| FTPClient_RecvData_FTPClient__Download.cpp | src/game/online/FTPClient.cpp |
| FTPClient_RecvData_sinit_email_cpp.cpp | src/game/online/email.cpp |
| FeatureStatusPanel_UpdateStatusWindow.cpp | src/game/ui/ui_FeatureStatus.cpp |
| FeatureStatusPanel_UpdateStatusWindow_FeatureStatusMsg_2.cpp | src/game/ui/ui_FeatureStatus.cpp (2), src/game/ui/ui_FinancePanel.cpp (2) |
| FeatureStatusPanel_UpdateStatusWindow_FeatureStatusPanel_InitStatusWindow.cpp | src/game/ui/ui_FeatureStatus.cpp |
| FillLenderList.cpp | src/game/ui/ui_loansPanel.cpp |
| FillLenderList_CreateLoansPanel.cpp | src/game/ui/ui_loansPanel.cpp |
| FillRideList.cpp | src/game/ui/ui_ridepanel.cpp |
| FillUpgradeBox.cpp | src/game/ui/ui_RideStatus.cpp |
| FinanceGraphPanelMsg.cpp | src/game/ui/ui_FinancePanel.cpp |
| Game.cpp | src/game/Game.cpp |
| GameAssert.cpp | src/game/advisor/Advisor.cpp |
| GameClock.cpp | src/game/PausableTime.cpp (9), src/game/save/SaveLoad.cpp (8), src/game/FixableTime.cpp (5), src/game/VariableTime.cpp (5), src/game/ui/UI_Base.cpp (1) |
| GameClock_std___red_black_tree_balance_insert.cpp | src/game/ui/MessageCentre.cpp |
| GameSys.cpp | src/game/GameSys.cpp |
| GameSys_CreateRestartParkSaveGame.cpp | src/game/GameSys.cpp |
| GameSys_GAMESYSTEM_Load.cpp | src/game/GameSys.cpp |
| Game_exit.cpp | src/game/Game.cpp |
| Game_iterate.cpp | src/game/Game.cpp |
| Game_level.cpp | src/game/Game.cpp |
| Game_level_CGame_GetLocalisedThemeName.cpp | src/game/Game.cpp |
| Game_level_CGame_GetLocalisedThemeName_3.cpp | src/game/ActionRecorder.cpp (5), src/game/Game.cpp (2), src/game/ui/InterfaceBar.cpp (2), src/game/people/ParkTime.cpp (1), src/game/ui/UI_Base.cpp (1), src/game/ui/ui_visitorStatus.cpp (1) |
| Game_pause.cpp | src/game/Game.cpp |
| Game_theme.cpp | src/game/Game.cpp |
| Game_theme_clear.cpp | src/game/Game.cpp |
| Game_theme_resources.cpp | src/game/Game.cpp |
| Game_theme_set.cpp | src/game/Game.cpp |
| Game_world.cpp | src/game/Game.cpp |
| GetActualBraceTrackPositions.cpp | src/game/rides/CoasterPylon.cpp |
| GetActualBraceTrackPositions_RecalcBraceTangents.cpp | src/game/rides/CoasterPylon.cpp |
| GetNewBraceLoftInterval.cpp | src/game/rides/CoasterPylon.cpp |
| GetNewBraceLoftInterval_RestorePylonDetails.cpp | src/game/rides/CoasterPylon.cpp |
| GetNewBraceLoftInterval_SetupBraceInfo.cpp | src/game/rides/CoasterPylon.cpp |
| GetNextWaypoint.cpp | src/game/rides/FlyingRide.cpp |
| GetNextWaypoint_HeightAtPos_NAV.cpp | src/game/rides/FlyingRide.cpp |
| GetNextWaypoint_HeightAtPos_NAV_std___use_facet.cpp | src/game/MslSupport.cpp |
| GetNextWaypoint_HeightAtPos_NAV_std_vector_insert.cpp | src/game/MslSupport.cpp |
| GetTangentsFromZVector.cpp | src/game/rides/CoasterPylon.cpp |
| GetTangentsFromZVector_SetupBraceAnimFrameData.cpp | src/game/rides/CoasterPylon.cpp |
| GetTrackMapCellHeight.cpp | src/game/park/TrackCell.cpp |
| GlobalOrder.cpp | src/game/TPGlobals.cpp |
| GoldenTicket.cpp | src/game/park/GoldenTickets.cpp (11), src/game/save/FS_MasterSave.cpp (0) |
| GoldenTicket_check.cpp | src/game/park/GoldenTickets.cpp |
| GoldenTicket_tell.cpp | src/game/park/GoldenTickets.cpp |
| Guard.cpp | src/game/people/guard.cpp |
| HFieldTextures.cpp | src/game/TextureTable.cpp (5), src/game/MeshLoad.cpp (2), src/game/park/TPHField.cpp (1) |
| HLSprite.cpp | src/game/HLSprite.cpp |
| HandleCoasterDataChange.cpp | src/game/rides/CoasterEdit.cpp (2), src/game/rides/CoasterTrack.cpp (1) |
| HandleCoasterDataChange_ToggleLoopTurnPylonState.cpp | src/game/rides/CoasterEdit.cpp |
| Handyman.cpp | src/game/people/Handyman.cpp |
| HeightfieldLoad.cpp | src/game/park/TPHField.cpp |
| HeightfieldLoad_RestoreHFieldDataFromCopy.cpp | src/game/park/TPHField.cpp |
| HirePanelLCHandler.cpp | src/game/ui/ui_hirePanel.cpp |
| HirePanelLCHandler_CreateHireListPanel_2.cpp | src/game/ui/ui_hirePanel.cpp |
| HoardingsBalance.cpp | src/game/balance/HoardingsBalance.cpp |
| HoardingsBalance_objects.cpp | src/game/balance/HoardingsBalance.cpp |
| InGame.cpp | src/game/InGame.cpp |
| InGame_start.cpp | src/game/InGame.cpp |
| InterfaceButton.cpp | src/game/ui/InterfaceButton.cpp |
| InterfaceCheckButtons_current.cpp | src/game/ui/InterfaceButton.cpp |
| InterfaceCheckButtons_dtor.cpp | src/game/ui/InterfaceButton.cpp |
| InterfaceEditBox.cpp | src/game/ui/InterfaceEditBox.cpp (14), src/game/ui/InterfaceDrawEngine.cpp (2) |
| InterfaceFont.cpp | src/game/ui/InterfaceFont.cpp |
| InterfaceKnob.cpp | src/game/ui/InterfaceKnob.cpp |
| InterfaceKnob_InterfaceKnob_ChangeValue.cpp | src/game/ui/InterfaceKnob.cpp |
| InterfaceKnob_InterfaceKnob_GetValue.cpp | src/game/ui/InterfaceVaryBox.cpp (5), src/game/ui/ui_upgradeStatus.cpp (4), src/game/ui/InterfaceKnob.cpp (3), src/game/ui/InterfaceLightPanel.cpp (2), src/game/ui/InterfaceWindow.cpp (2), src/game/ui/InterfaceBar.cpp (1), src/game/ui/InterfaceImage.cpp (1), src/game/ui/InterfaceTextBox.cpp (1), src/game/ui/UI_Base.cpp (1), src/game/ui/ui_progressBar.cpp (1), src/game/ui/ui_yearEndPanel.cpp (1) |
| InterfaceListBox.cpp | src/game/ui/InterfaceListBox.cpp (6), src/game/ui/InterfaceDrawEngine.cpp (2) |
| InterfaceListControl2_draw.cpp | src/game/ui/InterfaceListControl2.cpp (2), src/game/ui/InterfaceDrawEngine.cpp (1) |
| InterfaceListControl2_items.cpp | src/game/ui/InterfaceListControl2.cpp |
| InterfaceListControl2_items_InterfaceListControl2_GetColumnPos.cpp | src/game/ui/InterfaceListControl2.cpp |
| InterfaceListControl2_items_InterfaceListControl2_RemoveAllItems.cpp | src/game/ui/InterfaceListControl2.cpp |
| InterfaceListControl2_items_InterfaceListControl2_SetSort.cpp | src/game/ui/InterfaceListControl2.cpp |
| InterfaceListControl2_items_InterfaceListControl2_Sort.cpp | src/game/ui/InterfaceListControl2.cpp |
| InterfaceScrollBar.cpp | src/game/ui/InterfaceScrollBar.cpp (16), src/game/ui/InterfaceDrawEngine.cpp (2) |
| InterfaceThumbTrack.cpp | src/game/ui/InterfaceScrollBar.cpp (2), src/game/ui/InterfaceWindow.cpp (2) |
| InterfaceTimer_start.cpp | src/game/ui/InterfaceTimer.cpp |
| InterfaceVaryBox.cpp | src/game/ui/InterfaceVaryBox.cpp |
| InterfaceWindow.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_anim.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_cursor.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_dims.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_draw.cpp | src/game/ui/InterfaceWindow.cpp (16), src/game/ui/InterfaceDrawEngine.cpp (1), src/game/ui/InterfaceTimer.cpp (1) |
| InterfaceWindow_draw_InterfaceWindow_GetSizeOfClientDrawObject.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_draw_InterfaceWindow_link.cpp | src/game/ui/ui_islandLobbyController.cpp (6), src/game/ui/ui_onlineOutbox.cpp (4), src/game/ui/InterfaceWindow.cpp (1), src/game/ui/ui_onlineLogin.cpp (1) |
| InterfaceWindow_dtor.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_focus.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_font.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_frame.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_mouse.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_original.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_script.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_show.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_unlink.cpp | src/game/ui/InterfaceWindow.cpp |
| InterfaceWindow_virtuals.cpp | src/game/ui/InterfaceDrawEngine.cpp (3), src/game/ui/InterfaceWindow.cpp (3) |
| InterfaceWindow_virtuals_CreateUIProgressBar.cpp | src/game/ui/ui_progressBar.cpp |
| InterfaceWindow_virtuals_UIMenu_DefaultItemCallback.cpp | src/game/ui/UIMenu.cpp |
| InterfaceWindow_z.cpp | src/game/ui/InterfaceWindow.cpp |
| LCPostFormat.cpp | src/game/ui/ui_staffRoomStatus.cpp |
| LCPostFormat_2.cpp | src/game/ui/ui_onlineOutbox.cpp |
| LCPostFormat_2_ChatPrePostCard.cpp | src/game/ui/ui_onlineOutbox.cpp (5), src/game/ui/ui_onlinePanel.cpp (4) |
| LCPostFormat_2_ChatPrePostCard_ChatBackToThirdPerson_2.cpp | src/game/ui/ui_onlinePanel.cpp |
| LCPostFormat_CreateAllShowsPanel.cpp | src/game/ui/ui_allItemsPanel.cpp |
| LCPostFormat_RideStatusPanel_UpdateStatusWindow.cpp | src/game/ui/ui_RideStatus.cpp |
| LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Condemned.cpp | src/game/ui/ui_RideStatus.cpp |
| LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Upgrade.cpp | src/game/ui/ui_RideStatus.cpp |
| LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Upgrade_RideStatusPanel_InitKnobsWindow.cpp | src/game/ui/ui_RideStatus.cpp |
| LCPostFormat_RideStatusPanel_UpdateStatusWindow_RideStatusPanel_Upgrade_RideStatusPanel_UpgradePanelByeBye.cpp | src/game/ui/ui_RideStatus.cpp (8), src/game/ui/ui_shopStatus.cpp (7), src/game/ui/ui_StaffCostsPanel.cpp (4), src/game/ui/ui_researchPanel.cpp (2), src/game/ui/ui_ridepanel.cpp (1) |
| LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom.cpp | src/game/ui/ui_staffRoomStatus.cpp |
| LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_ShowStatusMsg.cpp | src/game/ui/ui_showStatus.cpp |
| LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_StaffRoomStatusPanel_FillStaffRoomList.cpp | src/game/ui/ui_staffRoomStatus.cpp |
| LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_StaffRoomStatusPanel_InitViewWindow.cpp | src/game/ui/ui_staffStatus.cpp (9), src/game/ui/ui_showStatus.cpp (7), src/game/ui/ui_staffPanel.cpp (5), src/game/ui/ui_staffRoomStatus.cpp (5), src/game/ui/ui_upgradeStatus.cpp (3) |
| LCPostFormat_StaffRoomStatusPanel_StaffWentIntoStaffRoom_sinit_ui_staffRoomStatus_cpp.cpp | src/game/ui/ui_staffRoomStatus.cpp (9), src/game/ui/ui_staffStatus.cpp (6), src/game/ui/ui_showStatus.cpp (2) |
| LLCC_DrawKartBlock.cpp | src/game/park/LLCellRide.cpp |
| LLCC_DrawKartBlock_LLCR_PlaceRide.cpp | src/game/park/LLCellRide.cpp |
| LLCC_Kill_Them_KILL_THEM_ALL.cpp | src/game/park/LLCellRide.cpp |
| LLCC_Kill_Them_KILL_THEM_ALL_LLCC_AbortPylonMove.cpp | src/game/park/LLCellRide.cpp |
| LLCC_Kill_Them_KILL_THEM_ALL_LLCC_AbortPylonMove_LLCC_GetRideProfileInfo.cpp | src/game/park/LLCellRide.cpp |
| LLCC_Kill_Them_KILL_THEM_ALL_LLCC_AbortPylonMove_LLCC_GetRideProfileInfo_LLCR_FixBridges_2.cpp | src/game/park/LLCellRide.cpp |
| LLCC_TraceTrack.cpp | src/game/park/LLCellRide.cpp |
| LLCM_LeftMouseButtonDown.cpp | src/game/park/LLCellMouse.cpp |
| LLCM_LeftMouseButtonDown_LLCM_LeftMouseButtonUp.cpp | src/game/park/LLCellMouse.cpp |
| LLCR_FixBridgesReverse.cpp | src/game/park/LLCellRide.cpp |
| LLCR_OverRide.cpp | src/game/park/LLCellRide.cpp |
| LLCR_OverRide_LLCR_DeleteRide_2.cpp | src/game/park/LLCellRide.cpp |
| LLCR_OverRide_LLCR_OverRide_3.cpp | src/game/park/LLCellRide.cpp (9), src/game/park/LLCellMouse.cpp (1) |
| LLCR_OverRide_LLCR_OverRide_3_LLCR_GetRideCell.cpp | src/game/park/LLCellMouse.cpp (9), src/game/HLSprite.cpp (5), src/game/park/LLCellRide.cpp (5), src/game/ui/intrface.cpp (3), src/game/rides/FlyingRide.cpp (2) |
| LLCR_OverRide_LLCR_OverRide_3_LLCR_StartQueue.cpp | src/game/park/LLCellRide.cpp |
| LLCellClicks.cpp | src/game/park/LLCellSystem.cpp |
| LLCellMeshes.cpp | src/game/park/LLCellMouse.cpp |
| LLCellMeshes_LLCM_RebuildUpgradeBridges.cpp | src/game/park/LLCellMouse.cpp |
| LLCellMeshes_LLCM_RebuildUpgradeBridges_LLCM_MouseMoved.cpp | src/game/park/LLCellMouse.cpp |
| LLCellMouse.cpp | src/game/park/LLCellMouse.cpp |
| LLCellMouse_LLCM_SetCurrentObject.cpp | src/game/park/LLCellMouse.cpp |
| LLCellRide_offsets.cpp | src/game/park/LLCellRide.cpp |
| LLCellRide_offsets_LLCC_SetTrackAnimReverse.cpp | src/game/park/LLCellRide.cpp |
| LLCellRide_offsets_LLCC_SetTrackAnimReverse_LLCC_BuildTrackDB_2.cpp | src/game/park/LLCellRide.cpp |
| LLCellSystem.cpp | src/game/park/LLCellSystem.cpp |
| LLCellSystem_clickstacks.cpp | src/game/park/LLCellSystem.cpp |
| LLCellSystem_costs.cpp | src/game/park/LLCellSystem.cpp |
| LLCellSystem_hoardedit.cpp | src/game/park/LLCellSystem.cpp |
| LLCellSystem_hoardimage.cpp | src/game/park/LLCellSystem.cpp |
| LLCellSystem_hoardimage_RemoveBridge.cpp | src/game/park/LLCellSystem.cpp (1), src/game/park/MapCell.cpp (1) |
| LLCellSystem_hoarding.cpp | src/game/park/LLCellSystem.cpp |
| LLCellSystem_hoardwrite.cpp | src/game/park/LLCellSystem.cpp |
| LLCellSystem_modes.cpp | src/game/park/LLCellSystem.cpp |
| LayoutSetRotatePylon.cpp | src/game/rides/CoasterEdit.cpp |
| LayoutSetRotatePylon_DeleteLastTrackBrace.cpp | src/game/rides/CoasterEdit.cpp |
| LayoutSetRotatePylon_DeleteLastTrackBrace_SetCoasterButtonState.cpp | src/game/rides/CoasterEdit.cpp |
| LayoutSetRotatePylon_DeleteLastTrackBrace_SetCoasterButtonState_GetCoasterEditDefaultButtons.cpp | src/game/rides/CoasterEdit.cpp |
| LayoutSetRotatePylon_DeleteLastTrackBrace_UIHandleEndLoop.cpp | src/game/rides/CoasterEdit.cpp |
| LbDraw_primitives.cpp | src/bullfrog/LbDraw.cpp (22), src/bullfrog/LbDrawText.cpp (4), src/bullfrog/TbSystemInfo.cpp (2) |
| LbDraw_text.cpp | src/bullfrog/LbDrawText.cpp (28), src/bullfrog/LbRect.cpp (2) |
| LbExeKey.cpp | src/bullfrog/LbLog.cpp |
| LbFile.cpp | src/bullfrog/LbFile.cpp (22), src/bullfrog/LbApp.cpp (2) |
| LbFile_LbApp_GetWindowsScreenMode.cpp | src/bullfrog/LbApp.cpp (8), src/bullfrog/LbSCBack.cpp (2) |
| LbFile_LbApp_GetWindowsScreenMode_LbApp_SetWindowIcon.cpp | src/bullfrog/LbApp.cpp (20), src/bullfrog/LbSCBack.cpp (4), src/bullfrog/TbJPEGSaver.cpp (4), src/bullfrog/AndyGLL.cpp (2), src/bullfrog/Draw.cpp (2), src/bullfrog/Colour.cpp (1), src/bullfrog/LbZLib.cpp (1) |
| LbFile_LbApp_GetWindowsScreenMode_LbSystemEventCallback_Dtor__LbSystemEventCallbac.cpp | src/bullfrog/LbSCBack.cpp |
| LbFile_LbApp_GetWindowsScreenMode_LbSystemEventCallback_Dtor__LbSystemEventCallbac_CritLock_Dtor_CritLock.cpp | src/bullfrog/FileSystem.cpp |
| LbFile_more.cpp | src/bullfrog/LbFile.cpp (15), src/bullfrog/LbApp.cpp (1) |
| LbFile_stamp.cpp | src/bullfrog/LbFile.cpp |
| LbList.cpp | src/bullfrog/LbLog.cpp |
| LbLog.cpp | src/bullfrog/LbLog.cpp |
| LbScreen_Initialise.cpp | src/bullfrog/WinScrn.cpp |
| LbScreen_Initialise_LbScreen_Terminate.cpp | src/bullfrog/WinScrn.cpp |
| LbScreen_Initialise_LbScreen_Terminate_LbScreen_SetMode_2.cpp | src/bullfrog/WinScrn.cpp |
| LbSurface.cpp | src/bullfrog/LbSurface.cpp |
| LbSurface_LbScreen_IsModeAvailable_2.cpp | src/bullfrog/WinScrn.cpp (9), src/bullfrog/LbSurface.cpp (1) |
| LbTime.cpp | src/bullfrog/LbTime.cpp |
| LbTime_LbTime_GetTime.cpp | src/bullfrog/LbTime.cpp |
| Lbgfsave_bmppcx.cpp | src/bullfrog/AndyGLL.cpp (16), src/bullfrog/Lbgfsave.cpp (6) |
| Lbgfsave_iffgiftim.cpp | src/bullfrog/AndyGLL.cpp (29), src/bullfrog/Lbgfsave.cpp (9) |
| Lbgfsave_iffgiftim_TbJPEGLoader_ReadHeaderInfo.cpp | src/bullfrog/TbJPEGSaver.cpp |
| LevelInit.cpp | src/game/TPLevel.cpp |
| LevelInit2.cpp | src/game/TPLevel.cpp (10), src/game/World.cpp (4) |
| LevelLeaves.cpp | src/game/DynamicMeshSet.cpp (6), src/game/rides/CoasterExtrude.cpp (4), src/game/rides/RidePlacement.cpp (1) |
| LevelLeaves_TrackRideIdToThingId.cpp | src/game/rides/BumperRide.cpp |
| LevelLights.cpp | src/game/Light.cpp (2), src/game/TPLevel.cpp (2) |
| LevelLights_tick.cpp | src/game/TPLevel.cpp |
| LevelSystems.cpp | src/game/park/TPHField.cpp (7), src/game/rides/CoasterType.cpp (4), src/game/rides/RidePreview.cpp (4), src/game/rides/TPCoaster.cpp (3), src/game/MeshLoad.cpp (1), src/game/TPLevel.cpp (1), src/game/park/TPHoardings.cpp (1) |
| LevelSystems_Destroy3DObjectFromAIWorld.cpp | src/game/park/TPHField.cpp |
| LevelSystems_GenerateHoardingMeshes.cpp | src/game/park/TPHoardings.cpp |
| LevelSystems_GenerateHoardingMeshes_HoardingCentreAngle.cpp | src/game/park/TPHoardings.cpp |
| LevelSystems_GetNodePositionSimple.cpp | src/game/park/TPHField.cpp |
| LevelSystems_GetRequiredBraceTangents_2.cpp | src/game/rides/CoasterPylon.cpp |
| LevelSystems_PlaceEndPylons.cpp | src/game/rides/TPCoaster.cpp |
| LevelSystems_RecalculateParkDarkening.cpp | src/game/park/TPHField.cpp |
| LevelSystems_SetHoardingYOffsets.cpp | src/game/park/TPHoardings.cpp |
| LevelSystems_SetLightColourExplicit.cpp | src/game/TPLevel.cpp |
| LevelSystems_SetRideHoardingLevel.cpp | src/game/park/TPHoardings.cpp |
| Light.cpp | src/game/Light.cpp |
| Light_CLightParallel_Dtor_CLightParallel.cpp | src/game/advisor/Advisor.cpp (5), src/game/ui/InterfaceImage.cpp (1) |
| Light_sinit_3DFont_cpp.cpp | src/game/ui/3DFont.cpp |
| Light_unnamed_IntensifyPix.cpp | src/game/ui/3DFont.cpp |
| LinkPreviewBrace.cpp | src/game/rides/CoasterPylon.cpp |
| LinkPreviewBrace_GetPylonUnderCursor.cpp | src/game/rides/CoasterPylon.cpp |
| LoadCoasters.cpp | src/game/rides/TPCoaster.cpp |
| LoadCoasters_HACKRestoreCoasterQueues.cpp | src/game/rides/CoasterSounds.cpp (1), src/game/rides/CoasterTrack.cpp (1), src/game/rides/CoasterTrain.cpp (1), src/game/rides/TPCoaster.cpp (1) |
| LoadPanelMsg.cpp | src/game/ui/ui_loadSavePanel.cpp (5), src/game/ui/ui_hirePanel.cpp (3) |
| LoadPanelMsg_LCFormat_5.cpp | src/game/ui/ui_loadSavePanel.cpp |
| MacroAI.cpp | src/game/park/Map.cpp |
| Main.cpp | src/game/Main.cpp |
| MainBalance.cpp | src/game/balance/MainBalance.cpp |
| MainWindowCallback.cpp | src/game/ui/ui_allItemsPanel.cpp |
| MainWindowCallback_CreateAllItemsPanel.cpp | src/game/ui/ui_allItemsPanel.cpp (4), src/game/ui/ui_allStaffPanel.cpp (4) |
| MainWindowCallback_CreateAllItemsPanel_AllStaffPanelMsg.cpp | src/game/ui/ui_FinancePanel.cpp (1), src/game/ui/ui_StatusPanel.cpp (1), src/game/ui/ui_allStaffPanel.cpp (1) |
| MainWindowCallback_CreateAllRidesPanel.cpp | src/game/ui/ui_allItemsPanel.cpp |
| MapCell_corner.cpp | src/game/park/MapCell.cpp |
| MapCell_flags.cpp | src/game/park/MapCell.cpp |
| MapCell_kinds.cpp | src/game/park/MapCell.cpp |
| MapCell_kinds_LLCC_DrawLine_2.cpp | src/game/park/MapCell.cpp (2), src/game/online/lobby_globe.cpp (1) |
| MapCell_tile.cpp | src/game/park/MapCell.cpp |
| MapDelta.cpp | src/game/park/MapDelta.cpp |
| MapDelta_map.cpp | src/game/park/Map.cpp (22), src/game/park/MapDelta.cpp (14) |
| MapDelta_map_CMapCell_CalculateNeighbours.cpp | src/game/park/MapCell.cpp |
| MapDelta_map_CMapCell_ConnectionType.cpp | src/game/park/MapCell.cpp |
| MapDelta_map_CMapCell_Delete.cpp | src/game/park/MapCell.cpp |
| MapDelta_map_CMapCell_IsHoarding.cpp | src/game/park/MapCell.cpp |
| MapDelta_map_CMapCell_IsQueue.cpp | src/game/park/MapCell.cpp (20), src/game/park/TrackCell.cpp (6), src/game/park/LLCellRide.cpp (3), src/game/online/lobby.cpp (1) |
| MapDelta_map_CMapCell_Modify.cpp | src/game/park/MapCell.cpp |
| MapDelta_map_CMapCell_ModifyType.cpp | src/game/park/MapCell.cpp |
| MapDelta_save.cpp | src/game/park/Map.cpp (4), src/game/park/MapDelta.cpp (2) |
| MapDelta_wall.cpp | src/game/park/Map.cpp |
| MapReset.cpp | src/game/park/CellBase.cpp (2), src/game/park/LLCellSystem.cpp (1), src/game/park/MapCell.cpp (1), src/game/park/TPHField.cpp (1), src/game/park/TrackCell.cpp (1) |
| Map_init.cpp | src/game/park/Map.cpp |
| MatrixPowerPC.cpp | src/engine/MatrixPowerPC.cpp |
| Mechanic.cpp | src/game/people/mechanic.cpp |
| Mechanic_CMechanicHQ_GetNewJob.cpp | src/game/buildings/MechanicHQ.cpp |
| MeshDestroy.cpp | src/game/TPInstance.cpp (1), src/game/TPMesh.cpp (1) |
| MeshDraw.cpp | src/game/TPMesh.cpp |
| MeshLoad.cpp | src/game/MeshLoad.cpp (16), src/game/TPMesh.cpp (2) |
| MessageCentre_post.cpp | src/game/ui/MessageCentre.cpp (3), src/game/advisor/Advisor.cpp (1), src/game/park/BankAccount.cpp (1), src/game/save/BlockSave.cpp (1) |
| MessageCentre_post_CSubscriptionHeader_Load.cpp | src/game/ui/MessageCentre.cpp |
| MinAnalyserValue.cpp | src/game/ui/ui_graph.cpp |
| MinAnalyserValue_SetupGraph.cpp | src/game/ui/ui_graph.cpp |
| MinAnalyserValue_std___cdeque_iterator_operator_3.cpp | src/game/MslSupport.cpp |
| Mouse.cpp | src/game/Mouse.cpp |
| Mouse_handlers.cpp | src/game/Mouse.cpp |
| Mouse_modes.cpp | src/game/Mouse.cpp |
| Mouse_modes_AbortMovePylon.cpp | src/game/rides/CoasterTrack.cpp (4), src/game/rides/CoasterEdit.cpp (2), src/game/park/TPHField.cpp (1) |
| Mouse_staff.cpp | src/game/Mouse.cpp |
| MsgAdvisor.cpp | src/game/park/GoldenTickets.cpp (2), src/game/ui/MessageCentre.cpp (1) |
| MsgCheatMode.cpp | src/game/park/Clock.cpp |
| MsgDeath.cpp | src/game/Thing.cpp (2), src/game/ui/MessageCentre.cpp (1) |
| MsgDeath_CThoughts_Serialise.cpp | src/game/people/Thoughts.cpp |
| MsgDeath_CThoughts_Serialise_LOADVARSTRUCT_3.cpp | src/game/Thing.cpp (3), src/game/buildings/TrainingCentre.cpp (2) |
| MsgTag.cpp | src/game/ui/MessageCentre.cpp |
| NewTrackBrace.cpp | src/game/rides/CoasterPylon.cpp |
| NodeGamuts.cpp | src/game/MeshLoad.cpp |
| NodeWrapping.cpp | src/game/rides/RidePlacement.cpp |
| Object.cpp | src/game/Object.cpp |
| Object_admit.cpp | src/game/Object.cpp |
| Object_info.cpp | src/game/Object.cpp |
| Object_opening.cpp | src/game/Object.cpp |
| Object_queries.cpp | src/game/Object.cpp |
| Object_queue.cpp | src/game/Object.cpp |
| Object_serialise.cpp | src/game/Object.cpp |
| Object_takings.cpp | src/game/Object.cpp |
| Object_turn.cpp | src/game/Object.cpp |
| Object_upkeep.cpp | src/game/Object.cpp |
| OnlineBalance.cpp | src/game/balance/OnlineBalance.cpp |
| OnlinePanelMsg.cpp | src/game/ui/ui_onlinePanel.cpp |
| OnlinePanelMsg_DisableVoteButton.cpp | src/game/ui/ui_onlinePanel.cpp |
| OnlineParkFindPanelMsg.cpp | src/game/ui/ui_onlinePanel.cpp (8), src/game/ui/ui_onlineParkFindPanel.cpp (4) |
| OnlineParkFindPanelMsg_ContactMenu.cpp | src/game/ui/ui_onlinePanel.cpp |
| OnlineParkFindPanelMsg_ContactMenu_ButtonsUnderlayMsg_2.cpp | src/game/ui/ui_onlineParkInfoPanel.cpp (7), src/game/ui/ui_onlineParkListPanel.cpp (6), src/game/ui/UIInterface.cpp (4), src/game/ui/ui_onlinePanel.cpp (2) |
| OnlineParkFindPanelMsg_ContactMenu_ButtonsUnderlayMsg_2_UIInterface_PreLevelLoad.cpp | src/game/ui/UIInterface.cpp |
| OnlineParkFindPanelMsg_LCPostFormat_7.cpp | src/game/ui/ui_onlineParkFindPanel.cpp |
| OnlineParkInfoPanelMsg.cpp | src/game/ui/ui_onlineParkInfoPanel.cpp (10), src/game/ui/ui_onlineParkFindPanel.cpp (8) |
| OnlineParkInfoPanelMsg_CreateOnlineParkFindPanel.cpp | src/game/ui/ui_onlineParkFindPanel.cpp |
| OptionsBalance.cpp | src/game/balance/OptionsBalance.cpp |
| ParkAnalyser_cells.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_ctor.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_gates.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_history.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_init.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_month.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_objects.cpp | src/game/park/analyser.cpp (26), src/game/World.cpp (0) |
| ParkAnalyser_rating.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_readers.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_rides.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_save.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_serialise.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_shops.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_staff.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_turn.cpp | src/game/park/analyser.cpp |
| ParkAnalyser_visitors.cpp | src/game/park/analyser.cpp |
| ParkInfoLines.cpp | src/game/ui/ui_parkStatus.cpp |
| ParkInfoLines_GraphPanelMsg.cpp | src/game/ui/ui_parkStatus.cpp |
| ParkMessages.cpp | src/game/ui/MessageCentre.cpp (3), src/game/buildings/ResearchLab.cpp (2), src/game/buildings/StaffHQ.cpp (2), src/game/park/analyser.cpp (2), src/game/park/BankAccount.cpp (1) |
| ParkMessages_CStaffHQ_DropStaff.cpp | src/game/buildings/StaffHQ.cpp |
| ParkMessages_CStaffHQ_DropStaff_CStaffHQ_ConsiderStriking.cpp | src/game/buildings/StaffHQ.cpp |
| ParkMessages_CStaffHQ_DropStaff_CStaffHQ_Serialise.cpp | src/game/buildings/StaffHQ.cpp |
| ParkMessages_CStaffHQ_DropStaff_CStaffHQ_Serialise_CStaffHQ_PickUpStaff.cpp | src/game/buildings/ResearchLab.cpp (10), src/game/buildings/StaffHQ.cpp (7), src/game/ui/vector2d.cpp (6), src/game/Thing.cpp (5), src/game/buildings/TrainingCentre.cpp (4), src/game/people/Thoughts.cpp (1) |
| ParkMessages_CStaffHQ_DropStaff_CStaffHQ_Serialise_CStaffHQ_StaffTypeHasAGrievance.cpp | src/game/buildings/StaffHQ.cpp |
| ParkMessages_SAVEVAR.cpp | src/game/buildings/ResearchLab.cpp |
| ParkMessages_SAVEVARSTRUCT_3.cpp | src/game/buildings/StaffHQ.cpp (2), src/game/people/Person.cpp (2), src/game/buildings/ResearchLab.cpp (1) |
| ParkTime.cpp | src/game/people/ParkTime.cpp |
| ParkTime_funny.cpp | src/game/people/ParkTime.cpp |
| ParkTime_season.cpp | src/game/people/ParkTime.cpp |
| ParkTime_update.cpp | src/game/people/ParkTime.cpp |
| ParkTime_update_CParkTime_Serialise.cpp | src/game/people/ParkTime.cpp |
| PlaceHolderHelpers.cpp | src/sound/PlaceHolderSentence.cpp (15), src/sound/TbMapStreamer.cpp (2), src/sound/AudioPlaceHolder.cpp (1) |
| PlaceHolderTypes.cpp | src/sound/PlaceHolderSentence.cpp (42), src/sound/PlaceHolderTypes.cpp (17), src/sound/ListSentence.cpp (8), src/game/advisor/AdvisorScoring.cpp (0) |
| Postcard.cpp | src/game/ui/Postcard.cpp |
| Postcard_Postcard_save.cpp | src/game/MeshLoad.cpp (2), src/game/ui/Postcard.cpp (1) |
| ProcessGetChatSatellitePlayers.cpp | src/game/ui/ui_worldLobbyController.cpp |
| ProcessGetChatSatellitePlayers_CreateWorldLobbyController.cpp | src/game/ui/ui_worldLobbyController.cpp (4), src/game/ui/InterfaceTextObjectSprite.cpp (1) |
| ProcessGetChatSatellitePlayers_sinit_online_cpp.cpp | src/game/online/online.cpp (7), src/game/ui/ui_worldLobbyController.cpp (1) |
| PublishPanelMsg.cpp | src/game/ui/ui_publishParkPanel.cpp (8), src/game/ui/ui_onlineParkListPanel.cpp (4) |
| PublishPanelMsg_GetRideRequiredYOffset.cpp | src/game/rides/RidePlacement.cpp |
| PublishPanelMsg_LCFormat.cpp | src/game/ui/ui_onlineParkListPanel.cpp |
| PublishPanelMsg_OnlineParkListPostGetAllParksInCityCallbackFn.cpp | src/game/ui/ui_onlineCityInfoOverlay.cpp (7), src/game/ui/ui_onlineNewsPanel.cpp (5), src/game/ui/ui_cityLobbyController.cpp (4), src/game/ui/ui_onlinePanel.cpp (4), src/game/ui/ui_onlineLogin.cpp (3), src/game/ui/ui_onlineOutbox.cpp (1), src/game/ui/ui_onlineParkFindPanel.cpp (1), src/game/ui/ui_onlineParkInfoPanel.cpp (1), src/game/ui/ui_onlineParkListPanel.cpp (1) |
| Quickload.cpp | src/game/save/TPQuickload.cpp (13), src/game/rides/TPRide.cpp (1) |
| RenderContext8_circle.cpp | src/bullfrog/RenderContext.cpp |
| RenderContext_depth_circles.cpp | src/bullfrog/RenderContext.cpp (61), src/bullfrog/LbTime.cpp (3) |
| RenderContext_depth_circles_sinit_LbDraw_cpp.cpp | src/bullfrog/LbDraw.cpp |
| ResearchLab_init.cpp | src/game/buildings/ResearchLab.cpp |
| ResearchLab_projects.cpp | src/game/buildings/ResearchLab.cpp (26), src/game/World.cpp (0) |
| ResearchLab_projects_CResearchCategory_Serialise.cpp | src/game/buildings/ResearchLab.cpp |
| ResearchLab_projects_CResearchLab_ReceiveMessage.cpp | src/game/buildings/ResearchLab.cpp |
| ResearchLab_projects_CResearchProject_Serialise.cpp | src/game/buildings/ResearchLab.cpp |
| Researcher.cpp | src/game/people/Researcher.cpp |
| ResolutionBalance.cpp | src/game/balance/ResolutionBalance.cpp |
| RestoreAnimTime.cpp | src/game/rides/TPRide.cpp |
| RestoreAnimTime_LoadRides_2.cpp | src/game/rides/TPRide.cpp |
| RideAnimChange.cpp | src/game/rides/RideAnim.cpp |
| RideAnimChange_RATick.cpp | src/game/rides/RideAnim.cpp |
| RideAnimInit.cpp | src/game/rides/RideAnim.cpp (4), src/game/rides/TPRide.cpp (1) |
| RideAnimInit_GetAnimTimeStats.cpp | src/game/rides/RideAnim.cpp |
| RideAnimInterp.cpp | src/game/rides/RideAnim.cpp (12), src/game/LMatrix.cpp (3) |
| RideAnimLoad.cpp | src/game/rides/TPRide.cpp |
| RideAnimStep.cpp | src/game/TPEngine.cpp (1), src/game/park/TPHoardings.cpp (1), src/game/rides/RideAnim.cpp (1), src/game/rides/TPRide.cpp (1) |
| RideAnimTick.cpp | src/game/rides/RideAnim.cpp (5), src/game/TPEmbed.cpp (2), src/game/TPMesh.cpp (1) |
| RideAnimTick_StaticifyCreateOnlyAnimNodes.cpp | src/game/rides/RideAnim.cpp |
| RideDB_balance.cpp | src/game/rides/RideDB.cpp |
| RideDB_clear.cpp | src/game/rides/RideDB.cpp |
| RideDB_effects.cpp | src/game/rides/RideDB.cpp |
| RideDB_helpers.cpp | src/game/rides/RideDB.cpp |
| RideDB_info.cpp | src/game/rides/RideDB.cpp |
| RideDB_load.cpp | src/game/rides/RideDB.cpp |
| RideDB_lookup.cpp | src/game/rides/RideDB.cpp |
| RideDB_names.cpp | src/game/rides/RideDB.cpp (6), src/game/save/SaveLoad.cpp (4) |
| RideDB_scan.cpp | src/game/rides/RideDB.cpp |
| RideDB_shape.cpp | src/game/rides/RideDB.cpp |
| RideDB_walk.cpp | src/game/rides/RideDB.cpp |
| RideDestroy.cpp | src/game/DynamicMeshSet.cpp (3), src/game/TPMesh.cpp (2), src/game/rides/TPRide.cpp (2), src/game/TPInstance.cpp (1), src/game/park/SkirtDeform.cpp (1), src/game/park/TPHField.cpp (1) |
| RideDestroy_CalculateSkirtVertices.cpp | src/game/park/SkirtDeform.cpp |
| RideDestroy_LoadFileIntoMemory.cpp | src/game/park/TPHField.cpp |
| RideDestroy_LoadFileIntoMemory_GenerateRideHeightMap.cpp | src/game/park/TPHField.cpp |
| RideDestroy_LoadFileIntoMemory_SaveRideHeightMap.cpp | src/game/park/TPHField.cpp |
| RideInstance.cpp | src/game/TPInstance.cpp (5), src/game/TPEmbed.cpp (3), src/game/rides/TPRide.cpp (3), src/engine/EngineUtil.cpp (2), src/game/Debug.cpp (1), src/game/rides/RideAnim.cpp (1), src/game/rides/RidePlacement.cpp (1) |
| RideInstance_RegenerateRideSign.cpp | src/game/rides/RidePlacement.cpp |
| RideInstance_std_basic_ofstream_Dtor_basic_ofstream_2.cpp | src/game/MslSupport.cpp |
| RideInstance_std_basic_ostream_Dtor_basic_ostream.cpp | src/game/MslSupport.cpp |
| RideSign.cpp | src/game/rides/RidePlacement.cpp |
| RideTick.cpp | src/game/rides/RideAnim.cpp |
| RideTypes.cpp | src/game/rides/TPRide.cpp |
| RideTypes_CalculateRootNodeFaceNormals.cpp | src/game/rides/TPRide.cpp |
| RideTypes_OutputAnimTimeStats.cpp | src/game/rides/TPRide.cpp |
| RideTypes_OutputAnimTimeStats_SaveRides_2.cpp | src/game/rides/TPRide.cpp |
| RideWrapping.cpp | src/game/rides/RidePlacement.cpp |
| SamsUtilities_UCommandLoadSound_UCommandLoadSoun.cpp | src/sound/SprocketsChannel.cpp |
| SaveLoad.cpp | src/game/save/SaveLoad.cpp |
| SaveLoadHeader.cpp | src/game/save/SaveLoad.cpp (3), src/game/park/SkyObjects.cpp (2) |
| SaveLoad_CSaveLoad_LoadModules.cpp | src/game/save/SaveLoad.cpp |
| SaveLoad_CSaveLoad_SaveHeader_2.cpp | src/game/save/SaveLoad.cpp |
| SaveLoad_CSaveLoad_SaveHeader_2_CSaveLoad_Save.cpp | src/game/save/SaveLoad.cpp |
| SaveLoad_CSaveLoad_SaveModules.cpp | src/game/save/SaveLoad.cpp |
| ScriptBlocks.cpp | src/game/park/analyser.cpp |
| ScriptCheckTable.cpp | src/game/Script.cpp |
| ScriptFlow.cpp | src/game/ScriptOps.cpp (19), src/game/Script.cpp (2) |
| ScriptHostSprites.cpp | src/game/park/analyser.cpp |
| ScriptInterp.cpp | src/game/Script.cpp (21), src/game/ScriptOps.cpp (10) |
| ScriptSave.cpp | src/game/Script.cpp |
| ScriptSprites.cpp | src/game/ScriptOps.cpp (13), src/game/Script.cpp (5) |
| ScriptTick.cpp | src/game/Script.cpp |
| Serialise_basic.cpp | src/game/advisor/Advisor.cpp (10), src/game/ControlManager.cpp (7), src/game/park/analyser.cpp (6), src/game/Object.cpp (2), src/game/park/BankAccount.cpp (2), src/game/save/FS_MasterSave.cpp (2), src/game/rides/FlyingRide.cpp (1) |
| Serialise_basic_SAVEVARSTRUCT_2.cpp | src/game/advisor/Advisor.cpp (2), src/game/advisor/AdvisorScoring.cpp (1) |
| Serialise_basic_SAVEVARSTRUCT_2_CAdvisorRequest_Play.cpp | src/game/advisor/Advisor.cpp |
| Serialise_basic_std___tree_std_pair_std_map_TbDynamicStringTempl_2.cpp | src/game/MslSupport.cpp |
| Serialise_basic_std_list_std_basic_string_std_allocator_std_basi_4.cpp | src/game/ui/InterfaceFont.cpp (7), src/game/ui/ui_main.cpp (5), src/game/online/OnlineParser.cpp (3), src/game/online/FS_OnlineList.cpp (2) |
| Serialise_float.cpp | src/game/park/analyser.cpp |
| ServerListPostLoginCallbackFn.cpp | src/game/online/FS_OnlineList.cpp |
| SetupHeadIDs.cpp | src/game/rides/BumperRide.cpp |
| SetupHeadIDs_SetControlType_2.cpp | src/game/rides/BumperRide.cpp |
| ShopStatusPanel_InitWindow.cpp | src/game/ui/ui_shopStatus.cpp |
| ShopStatusPanel_InitWindow_ShopStatusPanel_ApplyChanges.cpp | src/game/ui/ui_shopStatus.cpp |
| ShopStatusPanel_InitWindow_ShopStatusPanel_ApplyChanges_ShopStatusPanel_InitStatusWindow.cpp | src/game/ui/ui_shopStatus.cpp |
| ShowEditParkNamePanel.cpp | src/game/ui/ui_entryPricePanel.cpp |
| ShowEditParkNamePanel_CreateEntryPricePanel_2.cpp | src/game/ui/ui_entryPricePanel.cpp |
| ShowStatusPanel_UpdateStatusWindow.cpp | src/game/ui/ui_showStatus.cpp |
| ShowStatusPanel_UpdateStatusWindow_ShowStatusPanel_InitStatusWindow.cpp | src/game/ui/ui_showStatus.cpp |
| ShowStatusPanel_UpdateStatusWindow_ShowStatusPanel_UpdateWindow_2.cpp | src/game/ui/ui_yearEndPanel.cpp (2), src/game/ui/ui_showStatus.cpp (1), src/game/ui/ui_upgradeStatus.cpp (1) |
| SignRenderer.cpp | src/game/rides/RideSign.cpp |
| SkyObjects_ctor.cpp | src/game/park/SkyObjects.cpp |
| SkyObjects_destroy.cpp | src/game/park/SkyObjects.cpp |
| SkyObjects_destroy_CSkyObjects_InitSkyObjects_2.cpp | src/game/rides/RideDB.cpp (8), src/game/keyboard.cpp (6), src/game/park/SkyObjects.cpp (4), src/game/ui/InterfaceFont.cpp (2), src/game/ui/MessageCentre.cpp (1) |
| SoundBalance.cpp | src/game/balance/SoundBalance.cpp |
| SoundPark.cpp | src/game/park/ParkSound.cpp (8), src/game/Sound.cpp (2) |
| SoundRegister.cpp | src/game/park/ParkSound.cpp (8), src/game/Sound.cpp (5) |
| SoundRuntime.cpp | src/game/Sound.cpp |
| SoundRuntime_SOUND_SetRect.cpp | src/game/Sound.cpp |
| SpeedTimerFudge.cpp | src/game/ui/ui_researchPanel.cpp |
| SpeedTimerFudge_sinit_ui_Postcard_cpp.cpp | src/game/ui/ui_Postcard.cpp |
| Staff.cpp | src/game/people/staff.cpp |
| StaffDB_ctor.cpp | src/game/people/StaffDB.cpp (1), src/game/people/StaffInfo.cpp (1) |
| StaffDB_dtor.cpp | src/game/people/StaffDB.cpp |
| StaffDB_pool.cpp | src/game/people/StaffDB.cpp (14), src/game/people/StaffInfo.cpp (10) |
| StaffDB_update.cpp | src/game/people/StaffDB.cpp |
| StaffStatusPanel_FireMe.cpp | src/game/ui/ui_staffStatus.cpp |
| StaffStatusPanel_FireMe_StaffStatusPanel_UpdateStatusWindow.cpp | src/game/ui/ui_staffStatus.cpp |
| StaffStatusPanel_FireMe_StaffStatusPanel_UpdateStatusWindow_StaffStatusMsg.cpp | src/game/ui/ui_staffStatus.cpp |
| Staff_behaviour.cpp | src/game/people/staff.cpp |
| Staff_queries.cpp | src/game/people/staff.cpp |
| SwearFilter_Filter.cpp | src/game/online/IPBase.cpp |
| TPAdvisor.cpp | src/game/TextureTable.cpp |
| TPAdvisor_model.cpp | src/game/advisor/TPAdvisor.cpp (9), src/game/TPCamera.cpp (1) |
| TPAdvisor_model_sAdvisorStore_Dtor_sAdvisorStore.cpp | src/game/TPCamera.cpp |
| TPCamera.cpp | src/game/TPCamera.cpp |
| TPCamera_callees.cpp | src/game/TPCamera.cpp |
| TPCamera_handle.cpp | src/game/TPCamera.cpp |
| TPCamera_init.cpp | src/game/TPCamera.cpp |
| TPCamera_messages.cpp | src/game/TPCamera.cpp |
| TPCamera_modes.cpp | src/game/TPCamera.cpp |
| TPCoaster_data.cpp | src/game/rides/CoasterType.cpp |
| TPCoaster_instances.cpp | src/game/rides/TPCoaster.cpp (12), src/game/rides/CoasterPylon.cpp (11), src/game/rides/CoasterType.cpp (1) |
| TPCoaster_instances_DestroyTrackBrace.cpp | src/game/rides/CoasterPylon.cpp |
| TPCoaster_instances_IsValidPylonPosition.cpp | src/game/rides/CoasterPylon.cpp |
| TPCoaster_load.cpp | src/game/rides/CoasterType.cpp |
| TPCoaster_loaders.cpp | src/game/rides/CoasterType.cpp (9), src/game/rides/CoasterPylon.cpp (1) |
| TPCoaster_loaders_GetActualBraceParams.cpp | src/game/rides/CoasterPylon.cpp |
| TPCoaster_loaders_NewPylon.cpp | src/game/rides/CoasterPylon.cpp |
| TPCoaster_sam.cpp | src/game/rides/CoasterType.cpp |
| TPCoaster_ticks.cpp | src/game/rides/CoasterEdit.cpp (33), src/game/rides/TPCoaster.cpp (6) |
| TPCoaster_ticks_HandleMultiBracePreviewChange.cpp | src/game/rides/CoasterEdit.cpp |
| TPCoaster_trains.cpp | src/game/rides/TPCoaster.cpp (8), src/game/rides/BumperRide.cpp (1) |
| TPCoaster_trains_SaveCoasters.cpp | src/game/rides/TPCoaster.cpp |
| TPCoaster_trains_SetPotentialPylonArea.cpp | src/game/rides/CoasterPylon.cpp |
| TPCoaster_xsect.cpp | src/game/rides/CoasterType.cpp |
| TPConfig.cpp | src/game/TPConfig.cpp |
| TPConfig_LayoutReplayCoasterSpeed.cpp | src/game/rides/CoasterEdit.cpp |
| TPConfig_LayoutSetMovePylon.cpp | src/game/rides/CoasterEdit.cpp |
| TPConfig_LayoutSetMovePylon_SetCoasterEdit.cpp | src/game/rides/CoasterEdit.cpp |
| TPConfig_LayoutSetMovePylon_SetCoasterEdit_CalculateBraceCost.cpp | src/game/rides/CoasterEdit.cpp |
| TPConfig_bend.cpp | src/game/rides/CoasterSounds.cpp (6), src/game/rides/CoasterSpline.cpp (1), src/game/rides/CoasterTrain.cpp (0) |
| TPConfig_bend_GetAlignmentParamsQuick.cpp | src/game/rides/CoasterSpline.cpp |
| TPConfig_bend_LayoutStartAllCoasterSounds.cpp | src/game/rides/CoasterSounds.cpp |
| TPConfig_bend_StopTrainSounds.cpp | src/game/rides/CoasterSounds.cpp |
| TPConfig_collision.cpp | src/game/rides/CoasterCollision.cpp (25), src/game/rides/CoasterPylon.cpp (1) |
| TPConfig_extrude.cpp | src/game/rides/CoasterExtrude.cpp (24), src/game/rides/CoasterSpline.cpp (1) |
| TPConfig_extrude_GenerateTrackSubdivisions_2.cpp | src/game/rides/CoasterTrack.cpp (2), src/game/rides/CoasterTrain.cpp (2), src/game/rides/CoasterExtrude.cpp (1) |
| TPConfig_extrude_GetCrossSectionFromParams.cpp | src/game/rides/CoasterExtrude.cpp |
| TPConfig_extrude_GetCrossSectionFromParams_GenerateExtrusions.cpp | src/game/rides/CoasterExtrude.cpp |
| TPConfig_ridetick.cpp | src/game/rides/CoasterTrain.cpp (7), src/game/rides/CoasterSounds.cpp (2), src/game/rides/CoasterExtrude.cpp (1) |
| TPConfig_ridetick_DeactivateCoasterTrain.cpp | src/game/rides/CoasterTrain.cpp |
| TPConfig_stats.cpp | src/game/rides/CoasterTrain.cpp (30), src/game/rides/CoasterQueue.cpp (1) |
| TPConfig_stats_RecalcDistancesFromCentreCar.cpp | src/game/rides/CoasterTrain.cpp (2), src/game/rides/CoasterQueue.cpp (1) |
| TPConfig_stats_RoundUp.cpp | src/game/rides/CoasterSpline.cpp (1), src/game/rides/CoasterTrain.cpp (1) |
| TPDialog.cpp | src/game/ui/TPDialog.cpp |
| TPEmbed_destroy.cpp | src/game/TPEmbed.cpp |
| TPEmbed_heads.cpp | src/game/TPEmbed.cpp |
| TPEmbed_ime.cpp | src/game/ui/IME.cpp |
| TPEngine.cpp | src/game/TPEngine.cpp (32), src/game/rides/TPRide.cpp (5), src/game/TPInstance.cpp (4), src/game/TPMesh.cpp (4), src/game/park/TPHField.cpp (2) |
| TPEngine_SetRideMobile.cpp | src/game/rides/TPRide.cpp |
| TPEngine_pointedcell.cpp | src/game/TPPointer.cpp |
| TPGlobals.cpp | src/game/TPGlobals.cpp |
| TPGlobals_Dtor_TPGlobals.cpp | src/game/TPGlobals.cpp |
| TPMain.cpp | src/game/TPMain.cpp |
| TPMain_std_basic_string_assign.cpp | src/game/MslSupport.cpp |
| TPMain_std_basic_string_basic_string.cpp | src/game/MslSupport.cpp |
| TPMain_std_exception_Dtor_exception_2.cpp | src/game/MslSupport.cpp |
| TPMain_std_logic_error_Dtor_logic_error.cpp | src/game/MslSupport.cpp (4), src/game/ui/3DFont.cpp (1) |
| TPMovie.cpp | src/game/Movie.cpp (8), src/game/MeshLoad.cpp (1) |
| TPOptions.cpp | src/game/TPOptions.cpp |
| TPOptions_CTimeSig_GetTimeSecond.cpp | src/game/ui/InterfaceFont.cpp (13), src/game/ui/MessageCentre.cpp (5), src/game/save/SaveLoad.cpp (4), src/game/park/SkyObjects.cpp (3) |
| TPOptions_CTimeSig_Serialise.cpp | src/game/TPOptions.cpp |
| TPRide_height.cpp | src/game/park/TPHField.cpp |
| TPRide_pickup.cpp | src/game/TPPointer.cpp |
| TPRide_pointed.cpp | src/game/TPPointer.cpp |
| TPRide_pointer.cpp | src/game/TPPointer.cpp |
| TPRide_proctex.cpp | src/game/park/TPHField.cpp |
| TPSettings.cpp | src/game/TPResource.cpp (65), src/game/TPSettings.cpp (31) |
| TPSettings_itemlist.cpp | src/game/TPResource.cpp |
| TPSettings_itemres.cpp | src/game/TPResource.cpp |
| TPSettings_itemres_CTPItemListResource_OpenFile.cpp | src/game/TPResource.cpp |
| TPSound.cpp | src/game/Sound.cpp (13), src/game/park/ParkSound.cpp (1) |
| TPString.cpp | src/game/ui/3DFont.cpp |
| TPString_Antialiased_3.cpp | src/game/ui/3DFont.cpp |
| TailSeven.cpp | src/game/ui/InterfaceDrawEngine.cpp (1), src/game/ui/ui_main.cpp (1) |
| TbAudioBody.cpp | src/sound/TbAudioBody.cpp (18), src/sound/AudioComponent.cpp (2), src/sound/TbSysCommand.cpp (2) |
| TbAudioBody_create.cpp | src/sound/SprocketsChannel.cpp |
| TbAudioBody_create_CSprocketsChannel_Callback.cpp | src/sound/PlaceHolderSentence.cpp |
| TbAudioBody_create_SamsUtilities_UCommand_SetImmediate.cpp | src/sound/CSprocketsModule.cpp (23), src/sound/SprocketsChannel.cpp (4) |
| TbColour.cpp | src/game/ui/intrface.cpp |
| TbFPUPrecision.cpp | src/bullfrog/TbPixFmt.cpp |
| TbIMBToUnicode.cpp | src/ltms/TbIResourceIndex.cpp (2), src/ltms/StaticUnicodeBuffer.cpp (1), src/ltms/TbFontRenderParams.cpp (1) |
| TbIMBToUnicode_TbIMBInfo_Dtor_TbIMBInfo.cpp | src/ltms/StaticUnicodeBuffer.cpp (3), src/ltms/TbFontRenderParams.cpp (2) |
| TbIRLE4bitFont.cpp | src/ltms/TbIRLE4bitFont.cpp (11), src/ltms/TbFontRenderParams.cpp (1) |
| TbIRLE4bitFont_TbIRLE4bitFont_TbIRLE4bitFont.cpp | src/ltms/TbIRLE4bitFont.cpp |
| TbIResourceIndex.cpp | src/ltms/TbIResourceIndex.cpp |
| TbIResourceIndex_TbIResourceIndex_ReleaseResource.cpp | src/ltms/TbIResourceIndex.cpp |
| TbIResourceIndex_bfl_FileSystem_CDiscFileStorage_Dtor_CDiscFileSt.cpp | src/ltms/TbFontRenderParams.cpp |
| TbITable.cpp | src/ltms/TbITable.cpp |
| TbITableResource.cpp | src/ltms/TbITable.cpp |
| TbITable_TbITable_FormatString.cpp | src/ltms/TbITable.cpp |
| TbIUnicodeToMB.cpp | src/ltms/TbIUnicodeToMB.cpp |
| TbMapStreamer.cpp | src/sound/TbMapStreamer.cpp (23), src/sound/TbAudioBody.cpp (2), src/sound/TbSoundManager.cpp (1) |
| TbMapStreamer_TbMapStreamer_CCatFixupTable_Load.cpp | src/sound/TbMapStreamer.cpp |
| TbMapStreamer_TbMapStreamer_CCatFixupTable_Load_CBankContainer_Dtor_CBankContainer.cpp | src/sound/CSprocketsModule.cpp |
| TbMapStreamer_TbMapStreamer_FixUpCatagoryIndex.cpp | src/sound/TpBody.cpp (7), src/sound/CSprocketsModule.cpp (4), src/sound/TbMapStreamer.cpp (3), src/sound/TbSoundCore.cpp (3), src/sound/TbSoundSampleStore.cpp (1) |
| TbMapStreamer_TbMapStreamer_ReShuffleSoundForDroppable.cpp | src/sound/TbMapStreamer.cpp (8), src/sound/CSprocketsModule.cpp (1) |
| TbMpegBase.cpp | src/sound/LbMpeg.cpp (23), src/sound/MpegDecode.cpp (19), src/sound/FileAccess.cpp (1) |
| TbMpegBase_CFormat_FillWaveFormatEx.cpp | src/sound/FileAccess.cpp |
| TbMpegBase_CFormat_operator_tWAVEFORMATEX.cpp | src/sound/TbSoundPipe.cpp |
| TbMpegFilePlayer.cpp | src/sound/LbMpeg.cpp (12), src/sound/MpegDecode.cpp (4), src/sound/CSprocketsModule.cpp (3), src/sound/StreamableMpeg2.cpp (3) |
| TbMpegFilePlayer_SamsUtilities_UManager_Allocate.cpp | src/sound/CSprocketsModule.cpp |
| TbMpegSynth.cpp | src/sound/MpegDecode.cpp |
| TbPixFmt.cpp | src/bullfrog/TbPixFmt.cpp |
| TbRaConvert.cpp | src/bullfrog/TbRaConvert.cpp |
| TbRefRelease.cpp | src/bullfrog/TbRefRelease.cpp |
| TbRegistryKey.cpp | src/bullfrog/TbRegistryKey.cpp |
| TbResource.cpp | src/ltms/TbResource.cpp |
| TbResourceFactories.cpp | src/ltms/TbFontRenderParams.cpp (2), src/ltms/TbIRLE4bitFont.cpp (1), src/ltms/TbITable.cpp (1), src/ltms/TbIUnicodeToMB.cpp (1) |
| TbResource_CLineList_AddLine.cpp | src/ltms/TbTextFormatter.cpp |
| TbSoundBank.cpp | src/sound/TbMMFileBank.cpp (16), src/sound/TbBank.cpp (9), src/sound/Bank.cpp (7), src/sound/TbSoundBankInfo.cpp (7), src/sound/TbSoundPipeChannel.cpp (3), src/sound/TbSoundManager.cpp (2), src/sound/ListSentence.cpp (1), src/sound/TbSoundSampleInfo.cpp (1) |
| TbSoundCodecs.cpp | src/game/advisor/AdvisorScoring.cpp (24), src/sound/TbSoundSampleWAV.cpp (10), src/sound/TbSoundSampleWAVFile.cpp (9), src/sound/TimerCodec.cpp (8), src/sound/TbSoundCodecLoop.cpp (7), src/sound/TbSoundPipe.cpp (2) |
| TbSoundCore.cpp | src/sound/TbSoundCore.cpp (14), src/sound/ChannelState.cpp (5), src/sound/ServiceQueue.cpp (2) |
| TbSoundCore_TbSpeaker_Mono_operator_int.cpp | src/sound/ChannelState.cpp (11), src/sound/TbSoundSampleWAVFile.cpp (7), src/sound/Bank.cpp (4), src/sound/TimerCodec.cpp (4), src/sound/UserCallback.cpp (3), src/sound/TbBank.cpp (1) |
| TbSoundInfo.cpp | src/sound/TbSoundPosition.cpp (15), src/sound/TbSoundSampleInfo.cpp (13), src/sound/TbSoundSystemInfo.cpp (12), src/sound/SystemCommand.cpp (8), src/sound/AudioPlaceHolder.cpp (4), src/sound/TbSoundManager.cpp (3), src/sound/TbSoundPipe.cpp (3), src/sound/TbSoundCore.cpp (2), src/sound/TbSoundCodecLoop.cpp (1), src/sound/TbSoundPipeChannel.cpp (1), src/sound/TbSoundSampleStore.cpp (1), src/sound/TbSysCommand.cpp (1) |
| TbSoundManager.cpp | src/sound/TbSoundManager.cpp (11), src/sound/TbSysCommand.cpp (6), src/sound/AudioPlaceHolder.cpp (5), src/sound/TbAudioBody.cpp (4) |
| TbSoundPipe.cpp | src/sound/TbSoundSamplePipe.cpp (19), src/sound/TbSoundSampleCodec.cpp (12), src/sound/TbSoundCodecLoop.cpp (10), src/sound/TbSoundPipe.cpp (10), src/sound/StreamableMpeg2.cpp (5), src/sound/TbSoundSampleCallback.cpp (1), src/sound/TbSoundSystemInfo.cpp (1) |
| TbSoundPipeChannel.cpp | src/sound/TbSoundPipeChannel.cpp (17), src/sound/CSprocketsModule.cpp (2), src/sound/TbSoundCore.cpp (2), src/sound/TbSoundPipe.cpp (2), src/sound/TbSoundSampleStore.cpp (2), src/sound/TbSoundSampleWAV.cpp (1) |
| TbSoundPipeChannel_CSprocketsModule_SetChannelVolume.cpp | src/sound/CSprocketsModule.cpp |
| TbSoundPipeChannel_CSprocketsModule_SetChannelVolume_CSprocketsModule_SetListenerDirection_2.cpp | src/sound/CSprocketsModule.cpp |
| TbSoundPipeChannel_CSprocketsModule_SetChannelVolume_SamsUtilities_UCommand3dPosition_SetOrientation.cpp | src/sound/CSprocketsModule.cpp |
| TbSoundSampleCallback.cpp | src/game/advisor/AdvisorScoring.cpp (18), src/sound/TbSoundSampleCallback.cpp (13), src/sound/TbSoundPipe.cpp (7), src/sound/UserCallback.cpp (7), src/sound/TbSoundSamplePipe.cpp (5), src/sound/TbSoundBankInfo.cpp (1), src/sound/TbSoundSampleCodec.cpp (1), src/sound/TbSoundSampleStore.cpp (1) |
| TbSoundSampleMPEG2.cpp | src/sound/StreamableMpeg2.cpp (16), src/sound/TbSoundSampleMPEG2.cpp (16), src/game/advisor/AdvisorScoring.cpp (14), src/sound/TbBank.cpp (1), src/sound/TbSoundSamplePipe.cpp (1) |
| TbSoundSampleMPEG2_CServiceQueue_CServiceQueue.cpp | src/sound/ServiceQueue.cpp |
| TbSoundSampleStore.cpp | src/sound/TbSoundSampleStore.cpp (19), src/sound/TbSoundCore.cpp (2) |
| TbSoundSystemModule.cpp | src/sound/TbSoundSystemModule.cpp (32), src/sound/TbSoundCore.cpp (2), src/sound/ServiceQueue.cpp (1) |
| TbString.cpp | src/bullfrog/FileSystem.cpp |
| TbStringBuffer.cpp | src/bullfrog/FileSystem.cpp |
| TbStringList.cpp | src/bullfrog/FileSystem.cpp |
| TbStringMatch.cpp | src/bullfrog/FileSystem.cpp (3), src/bullfrog/TbWadStorageAccess.cpp (2) |
| TbString_more.cpp | src/bullfrog/FileSystem.cpp |
| TbSysCommand.cpp | src/sound/TbSysCommand.cpp (42), src/sound/TpBody.cpp (10) |
| TbSysCommand_TpBody_Load.cpp | src/sound/CSprocketsModule.cpp (5), src/sound/SprocketsChannel.cpp (2), src/sound/TpBody.cpp (2) |
| TbSysCommand_TpBody_Load_CSprocketsChannel_SetDescription_2.cpp | src/sound/TbMapStreamer.cpp (6), src/sound/TimerCodec.cpp (3) |
| TbSysCommand_TpBody_Load_CSprocketsChannel_SetDescription_2_TbStaticFileBank_RegisterBank.cpp | src/sound/TbSoundSamplePipe.cpp (9), src/sound/StreamableMpeg2.cpp (7), src/sound/TbMMFileBank.cpp (1) |
| TbTextResourceManager.cpp | src/ltms/TbTextResourceManager.cpp |
| TbTimeDiff.cpp | src/bullfrog/TbTimeDiff.cpp |
| TbTimeStamp_calendar.cpp | src/bullfrog/TbTimeStamp.cpp |
| TbTimeStamp_format.cpp | src/bullfrog/TbTimeStamp.cpp |
| TbTimeStamp_ops.cpp | src/bullfrog/TbTimeStamp.cpp |
| TbUniStringVTag.cpp | src/ltms/TbVTag.cpp |
| TbUniStringVTag_TbIntegerVTag_Format.cpp | src/ltms/TbVTag.cpp |
| TbUniStringVTag_TbIntegerVTag_Format_std__RefCountedPtr__RefCountedPtr.cpp | src/ltms/TbVTag.cpp |
| Text.cpp | src/game/Text.cpp |
| ThingCtors.cpp | src/game/Thing.cpp (2), src/game/advisor/Advisor.cpp (1), src/game/buildings/MechanicHQ.cpp (1), src/game/buildings/ResearchLab.cpp (1), src/game/buildings/StaffHQ.cpp (1), src/game/park/Weather.cpp (1), src/game/ui/vector2d.cpp (1) |
| ThingLoop.cpp | src/game/Thing.cpp (3), src/game/advisor/Advisor.cpp (1), src/game/park/MapDelta.cpp (1), src/game/park/Weather.cpp (1), src/game/ui/MessageCentre.cpp (1) |
| ThingLoop_ADV_Load.cpp | src/game/advisor/Advisor.cpp |
| ThingLoop_CMessageCentre_Save.cpp | src/game/ui/MessageCentre.cpp |
| ThingLoop_CTagSystem_FormatTagText.cpp | src/game/Thing.cpp |
| ThingMessages.cpp | src/game/park/Weather.cpp (4), src/game/ui/MessageCentre.cpp (3), src/game/park/ChallengeHQ.cpp (2) |
| ThingMessages2.cpp | src/game/Object.cpp (12), src/game/ui/MessageCentre.cpp (6) |
| Thing_receive.cpp | src/game/Thing.cpp |
| Thing_receive_CThing_Move_2.cpp | src/game/Thing.cpp |
| Things2_small.cpp | src/game/advisor/Advisor.cpp (2), src/game/park/analyser.cpp (2), src/game/park/BankAccount.cpp (1), src/game/park/Weather.cpp (1) |
| TrackCell.cpp | src/game/buildings/TrainingCentre.cpp |
| TrackCell_CTrainingCentre_Serialise.cpp | src/game/buildings/TrainingCentre.cpp |
| TrackCell_SAVEVARSTRUCT.cpp | src/game/buildings/TrainingCentre.cpp |
| TrackCell_SAVEVARSTRUCT_SAVEVAR_4.cpp | src/game/buildings/TrainingCentre.cpp |
| TrackCell_SAVEVARSTRUCT_SAVEVAR_4_LOADVAR_2.cpp | src/game/buildings/TrainingCentre.cpp |
| TrackCell_flags.cpp | src/game/park/TrackCell.cpp |
| TrackCell_flags_CTrackCell_CalculateDirections.cpp | src/game/park/TrackCell.cpp |
| TrackCell_flags_CTrackCell_CalculateNeighbours.cpp | src/game/park/TrackCell.cpp |
| TrackCell_flags_CTrackCell_ConnectionType.cpp | src/game/park/TrackCell.cpp |
| TrackCell_flags_CTrackCell_Delete.cpp | src/game/park/TrackCell.cpp |
| TrackCell_flags_CTrackCell_IsTrackStartBlock.cpp | src/game/park/TrackCell.cpp |
| TrackCell_flags_CTrackCell_IsTrackStartBlock_LLCC_DrawTrackLine.cpp | src/game/online/lobby.cpp (6), src/game/park/TrackCell.cpp (1) |
| TrackCell_flags_CTrackCell_Modify.cpp | src/game/park/TrackCell.cpp |
| TrackCell_flags_CTrackCell_SetHFieldTile.cpp | src/game/park/TrackCell.cpp |
| TrackCell_flags_CTrackCell_SetHFieldTile_LLCC_BreakTrack.cpp | src/game/park/TrackCell.cpp |
| TrackCell_isflag.cpp | src/game/park/TrackCell.cpp |
| TrackCell_modify.cpp | src/game/park/TrackCell.cpp |
| TrackCell_shape.cpp | src/game/park/TrackCell.cpp |
| TrueTypeRender.cpp | src/game/rides/RideSign.cpp |
| UIInterface_context.cpp | src/game/ui/UIInterface.cpp |
| UI_Base.cpp | src/game/ui/CoolRect.cpp (11), src/game/ui/UI_Base.cpp (7), src/game/ui/CoolRegion.cpp (6) |
| UI_Base_highestz.cpp | src/game/ui/UI_Base.cpp |
| UI_Base_mouse.cpp | src/game/ui/UI_Base.cpp |
| UI_Base_std_basic_string_compare.cpp | src/game/MslSupport.cpp |
| UI_IslandLobby.cpp | src/game/ui/ui_islandLobbyController.cpp (9), src/game/ui/ui_onlineOutbox.cpp (1) |
| UI_IslandLobby_CalcEmailAddressScrollBar.cpp | src/game/ui/ui_onlineOutbox.cpp |
| UI_IslandLobby_CalcEmailAddressScrollBar_OutboxSendingMsg.cpp | src/game/ui/ui_onlineOutbox.cpp |
| UI_IslandLobby_CalcEmailAddressScrollBar_OutboxSendingMsg_ColumnClick.cpp | src/game/ui/ui_onlineOutbox.cpp |
| UI_IslandLobby_CalcEmailAddressScrollBar_OutboxSendingMsg_OutboxPanelMsg.cpp | src/game/ui/ui_onlineOutbox.cpp |
| UI_IslandLobby_TextPanelMsg.cpp | src/game/ui/ui_onlineNewsPanel.cpp |
| UI_IslandLobby_TextPanelMsg_NewsPanelMsg.cpp | src/game/ui/ui_onlineLogin.cpp (2), src/game/ui/ui_onlineNewsPanel.cpp (1) |
| UI_IslandLobby_UpdateOnlineCityInfoOverlay.cpp | src/game/ui/ui_onlineCityInfoOverlay.cpp (5), src/game/ui/ui_onlineLogin.cpp (4) |
| UI_IslandLobby_UpdateOnlineCityInfoOverlay_CreateOnlineLoginPanel.cpp | src/game/ui/ui_onlineLogin.cpp |
| UI_IslandLobby_msg.cpp | src/game/ui/ui_islandLobbyController.cpp |
| UI_IslandLobby_msg_sinit_ui_cityLobbyController_cpp.cpp | src/game/ui/ui_cityLobbyController.cpp |
| UI_IslandLobby_msg_sinit_ui_cityLobbyController_cpp_UpdateCityLobbyUI.cpp | src/game/ui/ui_cityLobbyController.cpp |
| UI_activate.cpp | src/game/ui/UI_Base.cpp |
| UI_close.cpp | src/game/ui/InterfaceMessageQueue.cpp (2), src/game/ui/InterfaceTimer.cpp (1), src/game/ui/UI_Base.cpp (1) |
| UI_drawtext.cpp | src/game/ui/ui_utils.cpp (5), src/game/ui/InterfaceFont.cpp (3), src/game/ui/ui_main.cpp (1) |
| UI_focus.cpp | src/game/ui/UI_Base.cpp |
| UI_init.cpp | src/game/ui/UI_Base.cpp (38), src/game/ui/InterfaceMessageQueue.cpp (8), src/game/ui/InterfaceTimer.cpp (5) |
| UI_invalidate.cpp | src/game/ui/UI_Base.cpp (4), src/game/ui/InterfaceMessageQueue.cpp (2) |
| UI_pause.cpp | src/game/ui/UIInterface.cpp |
| UI_post.cpp | src/game/ui/UI_Base.cpp |
| UI_pump.cpp | src/game/ui/UI_Base.cpp (9), src/game/ui/InterfaceMessageQueue.cpp (6), src/game/ui/ui_ErrorDisplay.cpp (4), src/game/ui/InterfaceWindow.cpp (3), src/game/ui/ui_Postcard.cpp (3), src/game/ui/ui_main.cpp (3), src/game/ui/InterfaceTimer.cpp (2), src/game/ui/UIInterface.cpp (2) |
| UI_pump_DeInitOnlineUI.cpp | src/game/ui/ui_main.cpp |
| UI_pump_UIInterface_CamcorderMode.cpp | src/game/ui/UIInterface.cpp (6), src/game/ui/ui_ErrorDisplay.cpp (2), src/game/ui/ui_mainpanel2.cpp (1) |
| UI_pump_UIInterface_SetGameWindowCursor.cpp | src/game/ui/UIInterface.cpp |
| UI_pump_sinit_UI_Base_cpp.cpp | src/game/ui/UI_Base.cpp |
| UI_pump_std___cdeque_pop_front.cpp | src/game/ui/UIInterface.cpp |
| UI_pump_std___cdeque_swap.cpp | src/game/MslSupport.cpp |
| UI_pump_std_deque_pop_front.cpp | src/game/MslSupport.cpp |
| UI_pump_std_deque_push_back.cpp | src/game/MslSupport.cpp |
| UI_send.cpp | src/game/ui/InterfaceWindow.cpp (1), src/game/ui/UI_Base.cpp (1) |
| UI_timers_remove.cpp | src/game/ui/InterfaceTimer.cpp (5), src/game/advisor/AdvisorScoring.cpp (0) |
| UpdateSatellite.cpp | src/game/ui/ui_worldLobbyController.cpp (2), src/game/ui/ui_2dmap.cpp (1) |
| UpdateSatellite_UI_2DMap_MapAreaCallback.cpp | src/game/ui/ui_2dmap.cpp |
| UpgradeStatusMsg.cpp | src/game/ui/ui_staffStatus.cpp (3), src/game/ui/ui_upgradeStatus.cpp (1) |
| UpgradeStatusMsg_UpgradeStatusPanel_InitStatusWindow_2.cpp | src/game/ui/ui_upgradeStatus.cpp |
| UpgradeStatusMsg_UpgradeStatusPanel_InitStatusWindow_2_CreateYearEndPanel.cpp | src/game/ui/ui_yearEndPanel.cpp |
| VanillaTime.cpp | src/game/VanillaTime.cpp |
| VehicleCallbacks_map.cpp | src/game/rides/VehicleCallbacks.cpp |
| VehicleCallbacks_nav.cpp | src/game/rides/VehicleCallbacks.cpp |
| VehicleCallbacks_nav_FixedSizeArray_Dtor_FixedSizeArray.cpp | src/game/rides/VehicleCallbacks.cpp |
| Vehicle_path.cpp | src/game/rides/vehicle.cpp |
| Vehicle_serialise.cpp | src/game/rides/vehicle.cpp |
| Vehicle_steer.cpp | src/game/rides/vehicle.cpp |
| Vehicle_walls.cpp | src/game/rides/vehicle.cpp |
| Wavelet.cpp | src/engine/EngineUtil.cpp (20), src/engine/Wavelet.cpp (9) |
| Wavelet_decode.cpp | src/engine/Wavelet.cpp (16), src/engine/EngineUtil.cpp (3) |
| Wavelet_decode_v12.cpp | src/engine/Wavelet.cpp |
| Wavelet_fast.cpp | src/engine/Wavelet.cpp |
| Weather.cpp | src/game/park/Weather.cpp |
| Weather_turn.cpp | src/game/park/Weather.cpp |
| WorldSavvy.cpp | src/game/ui/ui_visitorStatus.cpp |
| WorldSavvy_VisitorStatusPanel_InitStatusWindow.cpp | src/game/ui/ui_visitorStatus.cpp (3), src/game/ui/InterfaceTextObjectSprite.cpp (1) |
| World_GetAdvisor.cpp | src/game/World.cpp |
| World_alloc.cpp | src/game/World.cpp |
| World_death.cpp | src/game/World.cpp |
| World_exit.cpp | src/game/World.cpp |
| World_gates.cpp | src/game/World.cpp |
| World_getters.cpp | src/game/World.cpp |
| World_init.cpp | src/game/World.cpp |
| World_load.cpp | src/game/rides/rsse.cpp (2), src/game/Thing.cpp (1), src/game/World.cpp (1), src/game/people/Thoughts.cpp (0) |
| World_register.cpp | src/game/World.cpp |
| World_rides.cpp | src/game/World.cpp |
| World_save.cpp | src/game/World.cpp (6), src/game/Thing.cpp (1) |
| World_save_sinit_World_cpp.cpp | src/game/World.cpp |
| World_sequences.cpp | src/game/World.cpp |
| World_setup.cpp | src/game/World.cpp |
| World_sight.cpp | src/game/park/LineOfSight.cpp |
| World_turn.cpp | src/game/World.cpp |
| World_unregister.cpp | src/game/World.cpp |
| World_wallhug.cpp | src/game/park/LineOfSight.cpp |
| advisor_leaves.cpp | src/game/TPEmbed.cpp (3), src/game/rides/RideAnim.cpp (2), src/game/TPMesh.cpp (1) |
| advisor_leaves_CountDataNodesByType.cpp | src/game/TPEmbed.cpp (8), src/game/online/bthread.cpp (3), src/game/people/Person.cpp (1) |
| advisor_leaves_RayPolyCheckOnRide.cpp | src/game/TPPointer.cpp |
| advisor_leaves_UpdateRideAnimSpeedAllChannels.cpp | src/game/online/lobby_island.cpp (8), src/game/rides/RideAnim.cpp (7), src/game/Debug.cpp (4), src/game/keyboard.cpp (3), src/game/online/lobby_globe.cpp (3), src/game/online/lobby_screen.cpp (3), src/game/park/GoldenTickets.cpp (3), src/game/rides/TPRide.cpp (3), src/game/rides/FlyingRide.cpp (2), src/game/save/BlockSave.cpp (1) |
| analyser_kill.cpp | src/game/Script.cpp |
| bfl_Access.cpp | src/bullfrog/TbBufferAccess.cpp (12), src/bullfrog/TbSharedAccess.cpp (8), src/bullfrog/TbQuickPosAccess.cpp (7) |
| bfl_Colour.cpp | src/bullfrog/Colour.cpp |
| bfl_DiscAccess.cpp | src/bullfrog/FileSystem.cpp (35), src/bullfrog/TbDiscAccess.cpp (15), src/bullfrog/TbFileStorageAccess.cpp (12), src/bullfrog/TbStorageAccess.cpp (5), src/bullfrog/LbSemaphore.cpp (2) |
| bfl_FileSystem.cpp | src/bullfrog/FileSystem.cpp (19), src/bullfrog/LbMaths.cpp (3), src/bullfrog/TbBufferAccess.cpp (2), src/bullfrog/TbFileStorageAccess.cpp (2), src/bullfrog/TbStorageAccess.cpp (2), src/bullfrog/MaskConverter.cpp (1), src/bullfrog/TbQuickPosAccess.cpp (1), src/bullfrog/TbSharedAccess.cpp (1) |
| bfl_FileSystem_more.cpp | src/bullfrog/FileSystem.cpp (11), src/bullfrog/TbMemFileAccess.cpp (11), src/bullfrog/TbZLibFileAccess.cpp (10), src/bullfrog/LbMaths.cpp (1), src/bullfrog/TbWadStorageAccess.cpp (1) |
| bfl_FileSystem_more_LbMaths_ArcTan.cpp | src/bullfrog/LbZLib.cpp |
| bfl_Refpack.cpp | src/bullfrog/FileSystem.cpp (4), src/bullfrog/TbRefpackFileAccess.cpp (2) |
| bfl_Strings.cpp | src/bullfrog/LbString.cpp |
| bfl_WadAccess.cpp | src/bullfrog/TbRefpackFileAccess.cpp (10), src/bullfrog/TbWadFileAccess.cpp (10) |
| bfl_WadFile.cpp | src/bullfrog/WadNameTree.cpp (19), src/bullfrog/TbWadStorageAccess.cpp (1) |
| bfl_WadStorage.cpp | src/bullfrog/TbWadStorageAccess.cpp (13), src/bullfrog/FileSystem.cpp (2), src/bullfrog/Lbgfsave.cpp (1) |
| bfl_WadSwap.cpp | src/bullfrog/Draw.cpp |
| bfl_graphicfile.cpp | src/bullfrog/Lbgfsave.cpp (11), src/bullfrog/AndyGLL.cpp (9), src/bullfrog/TbRenderArea.cpp (8), src/bullfrog/LbDrawText.cpp (1) |
| bfl_lbdraw_leaves.cpp | src/bullfrog/LbDraw.cpp (14), src/bullfrog/LbRect.cpp (3), src/bullfrog/LbDrawText.cpp (1) |
| bfl_tbrect.cpp | src/bullfrog/LbRect.cpp |
| bfl_tbrect_TbRect_Intersects.cpp | src/bullfrog/LbRect.cpp |
| bullfrog_CPU.cpp | src/bullfrog/LbLog.cpp |
| effect_manager.cpp | src/game/HLSprite.cpp |
| email.cpp | src/game/online/email.cpp (5), src/game/online/bthread.cpp (2) |
| email_Argument_Argument.cpp | src/game/online/email.cpp |
| email_bthread_mutex_Dtor_bthread_mutex.cpp | src/game/online/email.cpp |
| email_load.cpp | src/game/online/email.cpp |
| email_load_AddressBook_Save.cpp | src/game/online/email.cpp |
| email_load_AddressBook_Save_AddressBook_Add.cpp | src/game/online/email.cpp |
| email_load_AddressBook_Save_AddressBook_Add_AddressBook_First.cpp | src/game/online/email.cpp |
| email_load_EmailInfo_Dtor_EmailInfo.cpp | src/game/online/email.cpp |
| email_load_Outbox_ReadMailData.cpp | src/game/online/email.cpp |
| email_load_Outbox_Send.cpp | src/game/online/email.cpp |
| email_load_Outbox_Send_Outbox_WriteMailData.cpp | src/game/online/email.cpp |
| email_load_Outbox_Send_Outbox_WriteMailData_Outbox_DeleteMail.cpp | src/game/online/email.cpp |
| email_load_Outbox_Send_Outbox_WriteMailData_Outbox_First.cpp | src/game/online/email.cpp |
| email_load_Outbox_Send_Outbox_WriteMailData_std_basic_ifstream_Dtor_basic_ifstream_2.cpp | src/game/MslSupport.cpp |
| email_load_std_operator_2.cpp | src/game/MslSupport.cpp |
| fixmath.cpp | src/game/park/GoldenTickets.cpp |
| gameplay_leaves.cpp | src/game/Object.cpp (1), src/game/advisor/Advisor.cpp (1), src/game/park/SkirtDeform.cpp (1), src/game/ui/ui_staffPanel.cpp (1) |
| glue_callees.cpp | src/game/online/online.cpp |
| hlsprite_flies.cpp | src/game/rides/FlyingRide.cpp |
| hlsprite_flies_FLY_ForceExtractPeep.cpp | src/game/rides/FlyingRide.cpp |
| hlsprite_flies_init.cpp | src/game/rides/FlyingRide.cpp |
| hlsprite_flies_init_FLY_CreateBase.cpp | src/game/rides/FlyingRide.cpp |
| hlsprite_flybase.cpp | src/game/rides/FlyingRide.cpp |
| hlsprite_flybase_FLY_Draw.cpp | src/game/rides/FlyingRide.cpp |
| hlsprite_highlight.cpp | src/game/HLSprite.cpp |
| hlsprite_remove.cpp | src/game/HLSprite.cpp |
| hlsprite_zone.cpp | src/game/rides/FlyingRide.cpp |
| hlsprite_zone_FLY_Tick.cpp | src/game/rides/FlyingRide.cpp |
| host_leaves.cpp | src/game/rides/BumperRide.cpp (4), src/game/advisor/Advisor.cpp (1) |
| host_leaves_BMP_InitSystem.cpp | src/game/rides/BumperRide.cpp |
| host_leaves_Write_BumpToChunk.cpp | src/game/rides/BumperRide.cpp |
| host_leaves_Write_BumpToChunk_BMP_Save.cpp | src/game/rides/BumperRide.cpp |
| host_leaves_Write_BumpToChunk_BMP_Save_Read_BumpFromChunk.cpp | src/game/rides/BumperRide.cpp |
| host_ui2dmap_draw_ops.cpp | src/game/ui/ui_2dmap.cpp |
| intrface_current.cpp | src/game/ui/intrface.cpp |
| intrface_pointed.cpp | src/game/ui/intrface.cpp |
| intrface_process.cpp | src/game/ui/intrface.cpp |
| keytables_data.cpp | src/game/keyboard.cpp |
| keytables_disable.cpp | src/game/keyboard.cpp |
| keytables_enable.cpp | src/game/keyboard.cpp |
| keytables_enable_AnaglyphToggle.cpp | src/game/keyboard.cpp |
| keytables_init.cpp | src/game/keyboard.cpp |
| keytables_keys.cpp | src/game/keyboard.cpp |
| keytables_state.cpp | src/game/keyboard.cpp |
| landscape.cpp | src/engine/landscape.cpp |
| lb_semalock.cpp | src/bullfrog/LbSemaphore.cpp |
| lb_zlib.cpp | src/bullfrog/LbZLib.cpp |
| lb_zlib_compress.cpp | src/bullfrog/LbZLib.cpp |
| lb_zlib_size.cpp | src/bullfrog/LbZLib.cpp |
| lobby.cpp | src/game/online/lobby.cpp |
| lobby_advisor.cpp | src/game/online/LBScreenComponentAdvisor.cpp |
| lobby_exit.cpp | src/game/TPCamera.cpp (1), src/game/ui/UIInterface.cpp (1), src/game/ui/ui_islandLobbyController.cpp (1), src/game/ui/ui_main.cpp (1) |
| lobby_exit_InitOnlineUI.cpp | src/game/ui/ui_main.cpp |
| lobby_exit_IsRideRideable.cpp | src/game/TPCamera.cpp |
| lobby_exit_IsRideRideable_GetThingAtPosnIfEntrance.cpp | src/game/rides/BumperRide.cpp (3), src/game/TPCamera.cpp (2) |
| lobby_flyingmesh.cpp | src/game/online/LBFlyingMesh.cpp (17), src/game/Particles.cpp (1), src/game/online/lobby.cpp (1) |
| lobby_globe.cpp | src/game/online/lobby_globe.cpp |
| lobby_globe_CLBGlobe_dtor.cpp | src/game/online/lobby_globe.cpp |
| lobby_globe_CLBScreenComponentDownloadGlobe_Dtor_CLBScreenCo.cpp | src/game/online/lobby_globe.cpp |
| lobby_globe_CLBScreenComponentPublishGlobe_WindowMessagesFn.cpp | src/game/online/lobby_globe.cpp |
| lobby_globe_close.cpp | src/game/online/lobby_globe.cpp |
| lobby_globe_close_CLBScreenComponentOnlineGlobe_Show_2.cpp | src/game/online/lobby_globe.cpp |
| lobby_globe_close_CLBScreenComponentOnlineGlobe_Show_2_CLBScreenComponentOnlineGlobe_GlobeRight.cpp | src/game/HLSprite.cpp (10), src/game/online/lobby_globe.cpp (9), src/game/rides/RideAnim.cpp (2), src/game/park/LLCellMouse.cpp (1), src/game/park/TPHField.cpp (1) |
| lobby_globe_close_CLBScreenComponentOnlineGlobe_Tick.cpp | src/game/online/lobby_globe.cpp |
| lobby_globe_close_PostGetCityListCallbackFunction.cpp | src/game/online/lobby_globe.cpp |
| lobby_islands.cpp | src/game/online/lobby_island.cpp |
| lobby_islands_login.cpp | src/game/online/lobby_island.cpp |
| lobby_islands_rest.cpp | src/game/online/lobby_island.cpp (14), src/game/online/lobby_parser.cpp (2), src/game/online/LBFlyingMesh.cpp (1) |
| lobby_islands_tick.cpp | src/game/online/lobby_island.cpp (5), src/game/online/LBFlyingMesh.cpp (3) |
| lobby_mailicon.cpp | src/game/online/lobby.cpp |
| lobby_meshes.cpp | src/game/online/LBLobbyMeshes.cpp |
| lobby_parser.cpp | src/game/online/lobby_parser.cpp |
| lobby_screen.cpp | src/game/online/lobby_screen.cpp (15), src/game/online/LBScreenComponentAdvisor.cpp (1) |
| lobby_sendquit.cpp | src/game/online/lobby.cpp |
| lobby_start_leaves.cpp | src/game/ui/InterfaceStringHeap.cpp (10), src/game/rides/RidePlacement.cpp (9), src/game/ui/InterfaceFont.cpp (1) |
| lobby_view.cpp | src/game/online/LBLobbyView.cpp |
| lt__3stdFRCQ23std58basic_stringRCQ23std58basic_s.cpp | src/game/MslSupport.cpp |
| ltms_fontrender.cpp | src/ltms/TbFontRenderParams.cpp (9), src/ltms/TbFontRender.cpp (4) |
| ltms_fontrender_method.cpp | src/ltms/StaticUnicodeBuffer.cpp (4), src/ltms/TbFontRender.cpp (4) |
| ltms_fontrender_methods.cpp | src/ltms/TbOneColourNoBlend4BitFontRenderMethod.cpp (9), src/ltms/TbOneColour4BitFontRenderMethod.cpp (2), src/ltms/TbTextured4BitFontRenderMethod.cpp (2), src/ltms/TbTexturedNoBlend4BitFontRenderMethod.cpp (2) |
| ltms_fontrender_onecolour.cpp | src/ltms/TbOneColour4BitFontRenderMethod.cpp |
| ltms_fontrender_onecolour_noblend.cpp | src/ltms/TbOneColourNoBlend4BitFontRenderMethod.cpp |
| ltms_fontrender_textured.cpp | src/ltms/TbTextured4BitFontRenderMethod.cpp |
| ltms_fontrender_textured_noblend.cpp | src/ltms/TbTexturedNoBlend4BitFontRenderMethod.cpp |
| ltms_textformatter.cpp | src/ltms/TbTextFormatter.cpp |
| ltms_textformatter_rest.cpp | src/ltms/TbTextFormatter.cpp (10), src/engine/tb_depthoffield.cpp (0) |
| ltms_textformatter_rest_InvertLine.cpp | src/ltms/TbTextFormatter.cpp |
| ltms_textformatter_rest_IsDelimiter.cpp | src/ltms/TbIResourceIndex.cpp (11), src/ltms/MslSupport.cpp (10), src/ltms/TbVTag.cpp (1), src/ltms/TbITable.cpp (0) |
| ltms_textformatter_std_exception_Dtor_exception.cpp | src/ltms/MslSupport.cpp |
| ltms_textformatter_std_length_error_Dtor_length_error.cpp | src/ltms/MslSupport.cpp |
| m3d2_load.cpp | src/engine/M3D2Load.cpp (27), src/engine/EngineUtil.cpp (2), src/game/park/TPHField.cpp (1) |
| msl_red_black_tree.cpp | src/game/MslSupport.cpp |
| msl_tree_insert.cpp | src/game/MslSupport.cpp |
| msl_tree_subscription.cpp | src/game/MslSupport.cpp |
| rsse.cpp | src/game/LMatrix.cpp |
| rsse_close.cpp | src/game/rides/rsse.cpp |
| rsse_commands.cpp | src/game/rides/rsse.cpp |
| rsse_init.cpp | src/game/rides/rsse.cpp (3), src/game/rides/RideAnim.cpp (1) |
| rsse_math.cpp | src/game/LMatrix.cpp |
| rsse_matrix.cpp | src/game/LMatrix.cpp |
| rsse_normals.cpp | src/game/rides/RideAnim.cpp |
| rsse_objects.cpp | src/game/rides/rsse.cpp (16), src/game/rides/RideEffects.cpp (1) |
| rsse_objects_ProcessInstruction.cpp | src/game/rides/rsse.cpp |
| rsse_objects_RSSE_UpgradeRideEffect.cpp | src/game/rides/rsse.cpp |
| rsse_orientation.cpp | src/game/LMatrix.cpp |
| rsse_particles.cpp | src/game/Particles.cpp |
| rsse_particles_init.cpp | src/game/Particles.cpp |
| rsse_particles_save.cpp | src/game/Particles.cpp |
| rsse_particles_state.cpp | src/game/rides/rsse.cpp |
| rsse_peeps.cpp | src/game/rides/RideEffects.cpp |
| rsse_proctex.cpp | src/game/MeshLoad.cpp |
| rsse_ride_tick.cpp | src/game/rides/rsse.cpp |
| rsse_rotate.cpp | src/game/LMatrix.cpp |
| rsse_save.cpp | src/game/rides/rsse.cpp |
| rsse_scream.cpp | src/game/rides/RideEffects.cpp (10), src/game/rides/rsse.cpp (2) |
| rsse_script_life.cpp | src/game/rides/rsse.cpp |
| rsse_sign_filters.cpp | src/game/rides/RideSign.cpp |
| rsse_sign_shaders.cpp | src/game/rides/RideSign.cpp |
| rsse_spawn.cpp | src/game/Particles.cpp |
| rsse_tick.cpp | src/game/Particles.cpp |
| sinit_AdvisorScoring_cpp.cpp | src/game/advisor/AdvisorScoring.cpp |
| sinit_ui_2dmap_cpp.cpp | src/game/ui/ui_2dmap.cpp |
| sinit_ui_ErrorDisplay_cpp.cpp | src/game/ui/ui_ErrorDisplay.cpp (8), src/game/MslSupport.cpp (1), src/game/ui/UIInterface.cpp (1) |
| sinit_ui_ErrorDisplay_cpp_std_deque_tear_down.cpp | src/game/MslSupport.cpp |
| sinit_ui_FinancePanel_cpp.cpp | src/game/ui/ui_FinancePanel.cpp |
| sinit_ui_FinancePanel_cpp_UpdateViewWindow_2.cpp | src/game/ui/ui_hirePanel.cpp (6), src/game/ui/ui_FinancePanel.cpp (1) |
| sinit_ui_RideStatus_cpp.cpp | src/game/ui/ui_RideStatus.cpp (2), src/game/ui/ui_StatusPanel.cpp (1) |
| sinit_ui_RideStatus_cpp_RideStatusMsg.cpp | src/game/ui/ui_RideStatus.cpp |
| snd_commands.cpp | src/sound/TbSysCommand.cpp (6), src/sound/TbSoundCore.cpp (4), src/sound/AudioPlaceHolder.cpp (3) |
| snd_commands_execute.cpp | src/sound/SystemCommand.cpp |
| snd_containers.cpp | src/sound/TbMapStreamer.cpp |
| snd_handles.cpp | src/sound/ChannelState.cpp (6), src/sound/TbSoundCodecLoop.cpp (5), src/sound/TbSoundPipeChannel.cpp (5), src/sound/TbSoundPipe.cpp (4), src/sound/TbSoundSampleStore.cpp (2), src/sound/AudioComponent.cpp (1), src/sound/SystemCommand.cpp (1), src/sound/TbSoundCore.cpp (1), src/sound/TbSoundSampleWAV.cpp (1) |
| std__RefCountedPtr_std___locale_imp_std__Single_.cpp | src/game/MslSupport.cpp |
| std___cdeque_do_assign_std___cdeque_iterator.cpp | src/game/MslSupport.cpp |
| std___cdeque_erase.cpp | src/game/MslSupport.cpp |
| std___cdeque_insert_2.cpp | src/game/MslSupport.cpp |
| std___cdeque_iterator_operator_2.cpp | src/game/MslSupport.cpp |
| std___destroy.cpp | src/game/MslSupport.cpp |
| std___putnumber_char_std_ostreambuf_iterator.cpp | src/game/MslSupport.cpp |
| std___putnumber_wchar_t_std_ostreambuf_iterator.cpp | src/game/MslSupport.cpp |
| std___tree_erase_2.cpp | src/game/MslSupport.cpp |
| std___tree_std_basic_string_std_less_std_basic_s_2.cpp | src/game/MslSupport.cpp |
| std___tree_std_basic_string_std_less_std_basic_s_3.cpp | src/game/MslSupport.cpp |
| std___tree_std_pair_std_map_nocase_string_BuddyI.cpp | src/game/MslSupport.cpp (16), src/game/ui/UIInterface.cpp (8), src/game/ui/ui_FeatureStatus.cpp (3) |
| std___tree_std_pair_std_map_nocase_string_BuddyI_2.cpp | src/game/online/online.cpp |
| std_basic_filebuf_basic_filebuf.cpp | src/game/MslSupport.cpp |
| std_basic_filebuf_basic_filebuf_3.cpp | src/game/MslSupport.cpp |
| std_basic_filebuf_close.cpp | src/game/MslSupport.cpp |
| std_basic_filebuf_seekoff.cpp | src/game/MslSupport.cpp |
| std_basic_filebuf_seekpos.cpp | src/game/MslSupport.cpp |
| std_basic_filebuf_uflow.cpp | src/game/MslSupport.cpp |
| std_basic_filebuf_unshift.cpp | src/game/MslSupport.cpp |
| std_basic_ifstream_Dtor_basic_ifstream.cpp | src/game/MslSupport.cpp |
| std_basic_ifstream_basic_ifstream_2.cpp | src/game/MslSupport.cpp |
| std_basic_istream_basic_istream.cpp | src/game/ui/ui_2dmap.cpp (5), src/game/save/FS_LoadSave.cpp (2), src/game/ui/InterfaceKnob.cpp (2), src/game/ui/UIGrapher.cpp (2) |
| std_basic_istream_read.cpp | src/game/MslSupport.cpp |
| std_basic_ofstream_basic_ofstream.cpp | src/game/MslSupport.cpp |
| std_basic_ostream_flush.cpp | src/game/MslSupport.cpp |
| std_basic_ostream_flush_2.cpp | src/game/MslSupport.cpp |
| std_basic_ostream_sentry_Dtor_sentry.cpp | src/game/MslSupport.cpp |
| std_basic_streambuf_uflow.cpp | src/game/MslSupport.cpp |
| std_basic_streambuf_xsgetn.cpp | src/game/MslSupport.cpp |
| std_basic_streambuf_xsputn.cpp | src/game/MslSupport.cpp |
| std_basic_string_compare_2.cpp | src/game/MslSupport.cpp |
| std_basic_string_compare_3.cpp | src/game/MslSupport.cpp |
| std_basic_string_operator.cpp | src/game/MslSupport.cpp |
| std_basic_string_operator_3.cpp | src/game/MslSupport.cpp |
| std_basic_string_replace_2.cpp | src/game/MslSupport.cpp |
| std_ctype_do_is.cpp | src/game/MslSupport.cpp |
| std_deque_std_vector_std_allocator_std_vector_gr.cpp | src/game/MslSupport.cpp |
| std_deque_std_vector_std_allocator_std_vector_pu.cpp | src/game/MslSupport.cpp |
| std_exception_what.cpp | src/game/MslSupport.cpp (22), src/game/park/TPHField.cpp (3), src/game/online/FS_Online.cpp (2), src/game/ui/ui_FeatureStatus.cpp (1) |
| std_ios_base_setstate.cpp | src/game/MslSupport.cpp |
| std_less_std_basic_string_operator.cpp | src/game/online/online.cpp (6), src/game/ui/ui_worldLobbyController.cpp (3), src/game/ui/ui_2dmap.cpp (1) |
| std_less_std_basic_string_operator_ShowWorldLobbyController.cpp | src/game/MslSupport.cpp (12), src/game/ui/ui_worldLobbyController.cpp (7), src/game/ui/ui_publishParkPanel.cpp (5), src/game/TPGlobals.cpp (3), src/game/online/OnlinePerson.cpp (3), src/game/ui/ui_2dmap.cpp (1), src/game/ui/ui_ErrorDisplay.cpp (1) |
| std_list_erase.cpp | src/game/MslSupport.cpp |
| std_list_insert_2.cpp | src/game/MslSupport.cpp |
| std_list_std_basic_string_std_allocator_std_basi_2.cpp | src/game/MslSupport.cpp |
| std_list_unique.cpp | src/game/MslSupport.cpp |
| std_locale_Dtor_locale.cpp | src/game/HLSprite.cpp (4), src/game/TPInstance.cpp (2), src/game/TPPointer.cpp (2), src/game/advisor/Advisor.cpp (1) |
| std_operator.cpp | src/game/MslSupport.cpp |
| std_use_facet.cpp | src/game/rides/FlyingRide.cpp (23), src/game/MslSupport.cpp (2) |
| std_vector_Dtor_vector.cpp | src/game/MslSupport.cpp |
| std_vector_vector.cpp | src/game/MslSupport.cpp |
| tb_capture.cpp | src/engine/tb_capture.cpp |
| tb_capture_ConvertSquareFrom32.cpp | src/engine/DirectDraw.cpp (1), src/engine/Switch.cpp (1), src/engine/tb_hfield.cpp (1), src/engine/tb_printf_opengl.cpp (1) |
| tb_capture_ConvertSquareFrom32_CSwitch_ModeSwitch.cpp | src/engine/tb_hfield.cpp (4), src/engine/Switch.cpp (3), src/engine/DirectDraw.cpp (2), src/engine/DrawPrimitiveOpenGL.cpp (2), src/engine/tb_init.cpp (2), src/engine/EngineUtil.cpp (1) |
| tb_clipping.cpp | src/engine/Clipping.cpp |
| tb_clipping_edges.cpp | src/engine/Clipping.cpp |
| tb_ddraw.cpp | src/engine/DirectDraw.cpp |
| tb_decode_wavelet.cpp | src/engine/tb_depthoffield.cpp |
| tb_drawprimitive.cpp | src/engine/DrawPrimitive.cpp |
| tb_drawprimitive_addsprite.cpp | src/engine/tb_error.cpp (4), src/engine/DrawPrimitive.cpp (1) |
| tb_drawprimitive_buffer.cpp | src/engine/DrawPrimitive.cpp |
| tb_drawprimitive_sprite.cpp | src/engine/DrawPrimitive.cpp |
| tb_error.cpp | src/engine/tb_error.cpp |
| tb_ezsprite.cpp | src/engine/tb_ezsprite.cpp |
| tb_fontrenderparams.cpp | src/ltms/TbFontRenderParams.cpp |
| tb_fpu.cpp | src/engine/EngineUtil.cpp |
| tb_gamut.cpp | src/engine/Gamut.cpp |
| tb_hfield.cpp | src/engine/tb_hfield.cpp (7), src/engine/Gamut.cpp (5) |
| tb_hfield_adjust.cpp | src/engine/tb_hfield.cpp |
| tb_hfield_conquad.cpp | src/engine/tb_hfield.cpp |
| tb_hfield_execute.cpp | src/engine/tb_hfield.cpp |
| tb_hfield_frame.cpp | src/engine/tb_hfield.cpp (3), src/engine/Gamut.cpp (1) |
| tb_hfield_gamut.cpp | src/engine/Gamut.cpp |
| tb_hfield_helpers.cpp | src/engine/tb_hfield.cpp (7), src/engine/DrawPrimitive.cpp (1) |
| tb_hfield_init.cpp | src/engine/tb_hfield.cpp |
| tb_hfield_proctex.cpp | src/engine/tb_hfield.cpp |
| tb_hfield_range.cpp | src/engine/tb_hfield.cpp |
| tb_hfield_shadow.cpp | src/engine/tb_hfield.cpp |
| tb_hfield_texrange.cpp | src/engine/tb_hfield.cpp |
| tb_init.cpp | src/engine/tb_init.cpp |
| tb_init_SMachineParameters_Output.cpp | src/engine/tb_init.cpp |
| tb_init_SMachineParameters_Output_SMachineParameters_Return.cpp | src/engine/tb_init.cpp |
| tb_light.cpp | src/engine/LightManager.cpp |
| tb_lightmanager_frame.cpp | src/engine/LightManager.cpp (8), src/engine/MapWho.cpp (1) |
| tb_loaders.cpp | src/engine/TextureLoadManager.cpp |
| tb_loadmanager.cpp | src/engine/CsTexture.cpp (12), src/engine/TextureLoadManager.cpp (4) |
| tb_loadsurface.cpp | src/engine/CsTexture.cpp |
| tb_map.cpp | src/engine/MapWho.cpp |
| tb_map_render.cpp | src/engine/MapWho.cpp (10), src/engine/Gamut.cpp (1) |
| tb_mapzoom.cpp | src/engine/MapWho.cpp (11), src/engine/LightManager.cpp (6), src/game/Light.cpp (1) |
| tb_mapzoom_class.cpp | src/engine/tb_mapzoom.cpp |
| tb_matrix.cpp | src/engine/tb_matrix.cpp (34), src/engine/M3D2Load.cpp (1) |
| tb_mesh.cpp | src/engine/Mesh.cpp |
| tb_mesh_CMesh_GenerateRideBase.cpp | src/engine/Mesh.cpp |
| tb_mesh_frame.cpp | src/engine/Mesh.cpp (4), src/engine/DirectDraw.cpp (1), src/engine/RenderList.cpp (1) |
| tb_mesh_matrix.cpp | src/engine/Mesh.cpp |
| tb_meshdelay.cpp | src/engine/Mesh.cpp |
| tb_opengl_draw.cpp | src/engine/DrawPrimitiveOpenGL.cpp |
| tb_opengl_state.cpp | src/engine/OpenGLTexture.cpp (12), src/engine/DrawPrimitiveOpenGL.cpp (8), src/engine/OpenGL.cpp (4) |
| tb_opengl_texture_download.cpp | src/engine/DrawPrimitiveOpenGL.cpp (1), src/engine/OpenGLTexture.cpp (1) |
| tb_particle.cpp | src/engine/tb_profiler.cpp |
| tb_particle_frame.cpp | src/engine/tb_profiler.cpp |
| tb_polygon.cpp | src/engine/Gamut.cpp |
| tb_printf_opengl.cpp | src/engine/OpenGL.cpp (12), src/engine/DrawPrimitiveOpenGL.cpp (5), src/engine/OpenGLTexture.cpp (4), src/engine/Clipping.cpp (3), src/engine/tb_printf_opengl.cpp (1) |
| tb_printf_opengl_sinit_tb_printf_opengl_cpp.cpp | src/engine/tb_printf_opengl.cpp |
| tb_profiler.cpp | src/engine/tb_profiler.cpp |
| tb_qsort.cpp | src/engine/QSort.cpp |
| tb_rain.cpp | src/engine/Rain.cpp |
| tb_rain_frame.cpp | src/engine/Rain.cpp |
| tb_rain_lightning.cpp | src/engine/Rain.cpp |
| tb_rain_render.cpp | src/engine/Rain.cpp (4), src/engine/EngineUtil.cpp (1) |
| tb_renderentry.cpp | src/engine/RenderList.cpp |
| tb_renderentry_setup.cpp | src/engine/RenderList.cpp |
| tb_renderlist.cpp | src/engine/RenderList.cpp |
| tb_rendermemory.cpp | src/engine/RenderMemory.cpp |
| tb_rendermemory_execute.cpp | src/engine/RenderMemory.cpp |
| tb_rendermemory_frame.cpp | src/engine/RenderMemory.cpp |
| tb_resource_display.cpp | src/engine/tb_resource_display.cpp (12), src/engine/RenderList.cpp (3) |
| tb_resource_system.cpp | src/engine/tb_resource_system.cpp |
| tb_resource_texture.cpp | src/engine/tb_resource_texture.cpp |
| tb_rleload.cpp | src/engine/RLELoad.cpp |
| tb_rotatevertex.cpp | src/engine/EngineUtil.cpp |
| tb_shadow.cpp | src/engine/tb_shadow.cpp |
| tb_sky.cpp | src/engine/tb_sky.cpp |
| tb_sky_render.cpp | src/engine/tb_sky.cpp |
| tb_splash.cpp | src/engine/tb_splash.cpp |
| tb_spritefileio.cpp | src/engine/SpriteData.cpp (15), src/engine/RLELoad.cpp (4), src/engine/SpriteManager.cpp (3), src/engine/tb_depthoffield.cpp (1) |
| tb_spritelist_rest.cpp | src/engine/SpriteData.cpp (5), src/engine/SpriteManager.cpp (4) |
| tb_sprites.cpp | src/engine/SpriteManager.cpp |
| tb_sprites_draw.cpp | src/engine/SpriteData.cpp |
| tb_sprites_partial.cpp | src/engine/SpriteData.cpp |
| tb_sprites_shadow.cpp | src/engine/SpriteData.cpp |
| tb_starfield.cpp | src/engine/SpriteManager.cpp |
| tb_starfield_frame.cpp | src/engine/tb_starfield.cpp |
| tb_switch.cpp | src/engine/Switch.cpp |
| tb_switch_sinit_tb_starfield_cpp.cpp | src/engine/tb_starfield.cpp |
| tb_texturecache.cpp | src/engine/TextureCache.cpp (14), src/engine/Gamut.cpp (2) |
| tb_texturecache_balance.cpp | src/engine/TextureCache.cpp (5), src/engine/Gamut.cpp (1) |
| tb_texturecache_frame.cpp | src/engine/TextureCache.cpp |
| tb_texturecache_frame2.cpp | src/engine/TextureCache.cpp |
| tb_texturecache_helpers.cpp | src/engine/TextureCache.cpp |
| tb_texturecache_pack.cpp | src/engine/TextureCache.cpp |
| tb_texturecache_sort.cpp | src/engine/TextureCache.cpp |
| tb_texturecache_sort_CTextureCache_TestUploadSpeed.cpp | src/engine/TextureCache.cpp |
| tb_textureentry.cpp | src/engine/TextureEntry.cpp |
| tb_waveload.cpp | src/engine/tb_depthoffield.cpp (8), src/engine/RLELoad.cpp (2) |
| tb_waveload_sinit_tb_depthoffield_cpp.cpp | src/engine/tb_depthoffield.cpp |
| turn_object_counterclockwise.cpp | src/game/keyboard.cpp |
| turn_object_counterclockwise_TestAdvisor3.cpp | src/game/keyboard.cpp |
| turn_object_counterclockwise_TestAdvisor3_backtrack.cpp | src/game/keyboard.cpp |
| turn_object_counterclockwise_clear_patrol_area.cpp | src/game/keyboard.cpp |
| ui_DrawObject.cpp | src/game/ui/InterfaceCursor.cpp (16), src/game/ui/InterfaceDrawEngine.cpp (13), src/game/ui/InterfaceDrawObject.cpp (12) |
| ui_DrawObjectEngineMesh.cpp | src/game/ui/InterfaceDrawEngine.cpp (6), src/game/TPMesh.cpp (1) |
| ui_DrawObjectEngineMesh_InterfaceDrawEngineHGradient_InterfaceDrawEngine.cpp | src/game/ui/InterfaceDrawEngine.cpp |
| ui_DrawObjectGradient.cpp | src/game/ui/InterfaceDrawEngine.cpp |
| ui_DrawObjectNumber.cpp | src/game/ui/InterfaceTextObjectSprite.cpp |
| ui_DrawObjectResizable.cpp | src/game/ui/InterfaceDrawEngine.cpp |
| ui_DrawObjectSprite.cpp | src/game/ui/InterfaceDrawObjectSprite.cpp |
| ui_DrawObjectText.cpp | src/game/ui/InterfaceTextObjectSprite.cpp (9), src/game/ui/ui_textmanager.cpp (3) |
| ui_DrawObjectText_ctor.cpp | src/game/ui/InterfaceTextObjectSprite.cpp |
| ui_DrawObject_find.cpp | src/game/ui/InterfaceDrawObject.cpp |
| ui_Postcard_data.cpp | src/game/ui/ui_Postcard.cpp |
| ui_Postcard_panel.cpp | src/game/ui/ui_Postcard.cpp (22), src/game/keyboard.cpp (1), src/game/ui/UIInterface.cpp (1) |
| ui_Postcard_panel_CreateAddressBook.cpp | src/game/ui/ui_Postcard.cpp |
| ui_Postcard_panel_CreateAddressBook_DeleteEntryOK.cpp | src/game/ui/ui_Postcard.cpp (5), src/game/ui/ui_RideStatus.cpp (4), src/game/ui/InterfaceMessageQueue.cpp (1), src/game/ui/ui_researchPanel.cpp (1) |
| ui_Postcard_panel_SendPostcard.cpp | src/game/ui/ui_Postcard.cpp |
| ui_Postcard_panel_spawn_handyman.cpp | src/game/keyboard.cpp |
| ui_Postcard_panel_spawn_handyman_WeatherUpLots.cpp | src/game/keyboard.cpp |
| ui_Postcard_panel_spawn_handyman_WeatherUpLots_open_park.cpp | src/game/keyboard.cpp |
| ui_Postcard_panel_spawn_handyman_apply_for_loan_3.cpp | src/game/keyboard.cpp |
| ui_Postcard_panel_spawn_handyman_place_litter.cpp | src/game/keyboard.cpp |
| ui_Postcard_panel_std_basic_string_assign_2.cpp | src/game/online/FS_OnlineList.cpp (9), src/game/MslSupport.cpp (1), src/game/ui/InterfaceDrawEngine.cpp (1) |
| ui_Postcard_panel_std_list_std_basic_string_std_allocator_std_basi.cpp | src/game/MslSupport.cpp |
| ui_Postcard_panel_win_ticket_1.cpp | src/game/keyboard.cpp |
| ui_Postcard_panel_win_ticket_1_show_person_history_info.cpp | src/game/keyboard.cpp |
| ui_Postcard_panel_win_ticket_1_show_person_history_info_enter_hoarding_mode.cpp | src/game/keyboard.cpp |
| ui_Postcard_panel_win_ticket_1_show_person_history_info_enter_hoarding_mode_context_help.cpp | src/game/keyboard.cpp (17), src/game/ui/ui_mainpanel2.cpp (4), src/game/ui/ui_Postcard.cpp (1), src/game/ui/ui_loansPanel.cpp (1), src/game/ui/ui_optionsPanel.cpp (1), src/game/ui/ui_parkStatus.cpp (1) |
| ui_StatusPanel.cpp | src/game/ui/ui_tags.cpp |
| ui_StatusPanel_FindStaticTag.cpp | src/game/ui/ui_tags.cpp |
| ui_StatusPanel_FindStaticTag_OverrideTag.cpp | src/game/ui/ui_tags.cpp |
| ui_StatusPanel_SaveTags.cpp | src/game/ui/ui_tags.cpp |
| ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject.cpp | src/game/ui/ui_StatusPanel.cpp |
| ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_ShowVersionNumberTag.cpp | src/game/ui/ui_tags.cpp (2), src/game/ui/ui_StatusPanel.cpp (1) |
| ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_UIStatusPanel_EnableNameClick.cpp | src/game/ui/ui_StatusPanel.cpp (22), src/game/ui/InterfaceTextObjectSprite.cpp (4), src/game/Particles.cpp (2) |
| ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_UIStatusPanel_ShowEditNamePanel.cpp | src/game/ui/ui_StatusPanel.cpp |
| ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_ViewWindowCallback.cpp | src/game/ui/ui_StatusPanel.cpp |
| ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_ViewWindowCallback_DefaultStatusPanelMsg.cpp | src/game/ui/ui_StatusPanel.cpp |
| ui_StatusPanel_SetButton.cpp | src/game/ui/ui_tags.cpp |
| ui_StatusPanel_SetButton_ShowTagPanel.cpp | src/game/ui/ui_tags.cpp |
| ui_StatusPanel_tables.cpp | src/game/ui/ui_StatusPanel.cpp |
| ui_TextObjects.cpp | src/game/ui/InterfaceTextObjectSprite.cpp |
| ui_TextObjects_InterfaceCashTextObject_Change.cpp | src/game/ui/InterfaceTextObjectSprite.cpp |
| ui_TextObjects_InterfacePasswordTextObject_InterfacePasswordTex.cpp | src/game/ui/InterfaceTextObjectSprite.cpp (11), src/game/ui/ui_bogStatus.cpp (4), src/game/ui/InterfaceDrawEngine.cpp (3), src/game/ui/UIMenu.cpp (1), src/game/ui/ui_main.cpp (1), src/game/ui/ui_textmanager.cpp (1) |
| ui_allKidsPanel.cpp | src/game/ui/ui_allKidsPanel.cpp |
| ui_allKidsPanel_AllKidsPanelMsg.cpp | src/game/ui/ui_allKidsPanel.cpp |
| ui_allKidsPanel_AllKidsPanelMsg_LCPostFormat_6.cpp | src/game/ui/ui_allItemsPanel.cpp |
| ui_allKidsPanel_AllKidsPanelMsg_LoansPanelMsg.cpp | src/game/ui/ui_loansPanel.cpp |
| ui_allKidsPanel_AllKidsPanelMsg_LoansPanelMsg_CreateLoadPanel_2.cpp | src/game/ui/ui_loadSavePanel.cpp |
| ui_allKidsPanel_AllKidsPanelMsg_LoansPanelMsg_TakeLoanOK.cpp | src/game/ui/ui_loansPanel.cpp |
| ui_allKidsPanel_select.cpp | src/game/ui/ui_allKidsPanel.cpp |
| ui_allStaffPanel_script.cpp | src/game/ui/ui_allStaffPanel.cpp |
| ui_coasteredit_kb.cpp | src/game/ui/ui_coasterPanel.cpp |
| ui_coasteredit_kb_EndCoasterEditMode.cpp | src/game/ui/ui_coasterPanel.cpp |
| ui_controls_create.cpp | src/game/ui/InterfaceBar.cpp (1), src/game/ui/InterfaceButton.cpp (1), src/game/ui/InterfaceEditBox.cpp (1), src/game/ui/InterfaceImage.cpp (1), src/game/ui/InterfaceKnob.cpp (1), src/game/ui/InterfaceLightPanel.cpp (1), src/game/ui/InterfaceListBox.cpp (1), src/game/ui/InterfaceListControl2.cpp (1), src/game/ui/InterfaceTextBox.cpp (1), src/game/ui/InterfaceVaryBox.cpp (1), src/game/ui/InterfaceWindow.cpp (1) |
| ui_controls_dtors.cpp | src/game/ui/InterfaceBar.cpp (1), src/game/ui/InterfaceTextBox.cpp (1) |
| ui_controls_small.cpp | src/game/ui/InterfaceListControl2.cpp (4), src/game/ui/CoolRegion.cpp (3), src/game/ui/InterfaceKnob.cpp (1), src/game/ui/InterfaceListBox.cpp (1) |
| ui_cool_shapes.cpp | src/game/ui/CoolRegion.cpp |
| ui_cursor_classes.cpp | src/game/ui/InterfaceBFCursor.cpp |
| ui_cursor_classes_CUIGrapher_DrawGraph.cpp | src/game/ui/UIGrapher.cpp |
| ui_cursor_classes_CUIGrapher_DrawGraph_CUIGrapher_SetGridStyle.cpp | src/game/ui/ui_mainmenu.cpp (9), src/game/ui/UIGrapher.cpp (7), src/game/online/bthread.cpp (6), src/game/TPEmbed.cpp (3), src/game/save/FS_LoadSave.cpp (3) |
| ui_cursor_classes_CUIGrapher_DrawGraph_sinit_FS_LoadSave_cpp.cpp | src/game/save/FS_LoadSave.cpp |
| ui_cursors.cpp | src/game/ui/InterfaceBFCursor.cpp (5), src/game/ui/ui_main.cpp (5), src/game/ui/UIInterface.cpp (1) |
| ui_dialog.cpp | src/game/ui/ui_ErrorDisplay.cpp |
| ui_dialogbox.cpp | src/game/ui/ui_main.cpp (3), src/game/ui/InterfaceTimer.cpp (1), src/game/ui/ui_ErrorDisplay.cpp (1) |
| ui_dialogbox_callback.cpp | src/game/ui/ui_ErrorDisplay.cpp (2), src/game/ui/InterfaceTimer.cpp (1), src/game/ui/ui_main.cpp (1) |
| ui_dialogbox_data.cpp | src/game/ui/ui_ErrorDisplay.cpp |
| ui_editbox_handler.cpp | src/game/ui/InterfaceEditBox.cpp |
| ui_entryPricePanel.cpp | src/game/ui/ui_cashPanel.cpp (3), src/game/ui/ui_controlPanel.cpp (3), src/game/ui/ui_coasterPanel.cpp (2) |
| ui_entryPricePanel_CoasterEditMode.cpp | src/game/ui/ui_coasterPanel.cpp |
| ui_entryPricePanel_CoasterPanelEndTrack.cpp | src/game/ui/ui_coasterPanel.cpp |
| ui_entryPricePanel_CoasterPanelMsg.cpp | src/game/ui/ui_coasterPanel.cpp |
| ui_entryPricePanel_msg.cpp | src/game/ui/ui_cashPanel.cpp (1), src/game/ui/ui_controlPanel.cpp (1) |
| ui_fonttable.cpp | src/game/ui/ui_main.cpp |
| ui_game_panels_data.cpp | src/game/ui/ui_entryPricePanel.cpp (3), src/game/ui/ui_StatusPanel.cpp (1), src/game/ui/ui_main.cpp (1) |
| ui_handlers_small.cpp | src/game/ui/InterfaceButton.cpp (2), src/game/ui/InterfaceKnob.cpp (1), src/game/ui/InterfaceTextBox.cpp (1), src/game/ui/InterfaceVaryBox.cpp (1) |
| ui_handlers_small_InterfaceTextBox_operator.cpp | src/game/ui/InterfaceTextBox.cpp |
| ui_helptext.cpp | src/game/ui/InterfaceHelpText.cpp (17), src/game/ui/InterfaceTimer.cpp (1), src/game/ui/UI_Base.cpp (1) |
| ui_image_dtor.cpp | src/game/ui/InterfaceImage.cpp |
| ui_ime.cpp | src/game/ui/ui_IME.cpp |
| ui_ime_lifetime.cpp | src/game/ui/ui_graph.cpp (2), src/game/ui/ui_iterators.cpp (1) |
| ui_iterators.cpp | src/game/ui/ui_iterators.cpp |
| ui_iterators_MainGameMenuConfirmPublish_OK.cpp | src/game/ui/ui_mainmenu.cpp |
| ui_iterators_MainGameMenuConfirmPublish_OK_MainGameMenuConfirmQuit_OK.cpp | src/game/ui/ui_mainmenu.cpp (3), src/game/save/FS_LoadSave.cpp (2) |
| ui_iterators_sinit_ui_IME_cpp.cpp | src/game/ui/ui_IME.cpp |
| ui_iterators_std_basic_string_replace.cpp | src/game/MslSupport.cpp |
| ui_loginpanel.cpp | src/game/ui/ui_playerLogin.cpp (7), src/game/ui/ui_main.cpp (1), src/game/ui/ui_utils.cpp (1) |
| ui_loginpanel_UI_DrawPostcardText_2.cpp | src/game/ui/ui_utils.cpp (4), src/game/ui/InterfaceDrawEngine.cpp (1), src/game/ui/InterfaceTextObjectSprite.cpp (1) |
| ui_loginpanel_data.cpp | src/game/ui/ui_playerLogin.cpp |
| ui_loginpanel_msg.cpp | src/game/ui/ui_playerLogin.cpp |
| ui_loginpanel_msg_sinit_ui_parkStatus_cpp.cpp | src/game/ui/ui_parkStatus.cpp |
| ui_main.cpp | src/game/ui/ui_main.cpp |
| ui_main_click.cpp | src/game/ui/ui_main.cpp |
| ui_main_context.cpp | src/game/ui/ui_main.cpp |
| ui_main_game.cpp | src/game/ui/ui_main.cpp (6), src/game/ui/UIInterface.cpp (2) |
| ui_main_gamewin.cpp | src/game/ui/ui_main.cpp |
| ui_main_kbcapture.cpp | src/game/ui/ui_main.cpp (9), src/game/ui/UIMenu.cpp (1), src/game/ui/ui_mainmenu.cpp (1) |
| ui_main_kbcapture_HideMainMenu.cpp | src/game/ui/ui_mainmenu.cpp |
| ui_main_kbcapture_InitialiseLobbyMenu.cpp | src/game/ui/ui_mainmenu.cpp |
| ui_main_kbcapture_UIMain_ShowRidePreview.cpp | src/game/ui/ui_main.cpp |
| ui_main_kbcapture_UIMain_ShowRidePreview_UIMain_GotoStatusPanelForThing.cpp | src/game/ui/ui_main.cpp |
| ui_main_kbcapture_UIMain_ShowRidePreview_UIMain_GotoStatusPanelForThing_UIMain_ZoomToMapId.cpp | src/game/ui/ui_main.cpp |
| ui_main_kbcapture_UIMenu_AddItem.cpp | src/game/ui/UIMenu.cpp (10), src/game/ui/UI_Base.cpp (2), src/game/ui/InterfaceTextObjectSprite.cpp (1), src/game/ui/InterfaceWindow.cpp (1) |
| ui_main_kbcapture_UIMenu_AddItem_UIMenu_Show.cpp | src/game/ui/UIMenu.cpp |
| ui_main_leaves.cpp | src/game/ui/ui_main.cpp |
| ui_main_meshes.cpp | src/game/ui/ui_main.cpp (17), src/game/ui/InterfaceDrawEngine.cpp (2), src/game/ui/UIInterface.cpp (2) |
| ui_main_panel.cpp | src/game/ui/ui_main.cpp |
| ui_main_panel_UIMain_GetObjectState.cpp | src/game/ui/ui_main.cpp |
| ui_main_panel_UI_Congratulations.cpp | src/game/ui/ui_main.cpp |
| ui_main_panel_UI_Congratulations_SamsUtilities_UVersion_Dtor_UVersion_2.cpp | src/game/ui/ui_main.cpp (6), src/game/ui/ui_parkStatus.cpp (3), src/game/ui/ui_loansPanel.cpp (2), src/game/ui/InterfaceTextObjectSprite.cpp (1), src/game/ui/ui_Postcard.cpp (1) |
| ui_main_panel_UI_Congratulations_UI_BankruptAfterLoad.cpp | src/game/ui/ui_main.cpp |
| ui_main_pause.cpp | src/game/ui/ui_main.cpp |
| ui_main_peek.cpp | src/game/ui/ui_main.cpp |
| ui_main_preview_tick.cpp | src/game/ui/ui_main.cpp |
| ui_mainpanel2_create.cpp | src/game/ui/ui_mainpanel2.cpp |
| ui_mainpanel2_msg.cpp | src/game/ui/ui_mainpanel2.cpp |
| ui_mainpanel2_msg_GotoLastUsedPanel.cpp | src/game/ui/ui_mainpanel2.cpp |
| ui_mainpanel2_msg_HideIfUnder.cpp | src/game/ui/ui_mainpanel2.cpp |
| ui_mainpanel2_msg_UI_MainPanelDateChange_2.cpp | src/game/ui/ui_mainpanel2.cpp (7), src/game/ui/ui_loansPanel.cpp (1) |
| ui_mainpanel2_pause.cpp | src/game/ui/ui_mainpanel2.cpp |
| ui_mainpanel2_window.cpp | src/game/ui/ui_mainpanel2.cpp |
| ui_mainpanel2_window_LinkSlideoutPanel.cpp | src/game/ui/ui_mainpanel2.cpp |
| ui_mainpanel2_window_bumhole.cpp | src/game/ui/ui_mainpanel2.cpp (3), src/game/ui/ui_optionsPanel.cpp (2) |
| ui_mainpanel2_window_bumhole_DoScroll_3.cpp | src/game/ui/ui_parkStatus.cpp |
| ui_mainpanel2_window_bumhole_DoScroll_3_DoButton_2.cpp | src/game/ui/ui_optionsPanel.cpp |
| ui_mainpanel2_window_bumhole_UI_SwitchGameWindow.cpp | src/game/ui/ui_mainpanel2.cpp |
| ui_mainpanel_data.cpp | src/game/ui/ui_staffPanel.cpp (2), src/game/ui/ui_mainpanel2.cpp (1) |
| ui_matrix.cpp | src/game/LMatrix.cpp (1), src/game/ui/InterfaceDrawEngine.cpp (1) |
| ui_measuretext.cpp | src/game/ui/ui_utils.cpp |
| ui_newname.cpp | src/game/ui/ui_playerLogin.cpp |
| ui_newname_AddressBookMsg.cpp | src/game/ui/ui_Postcard.cpp |
| ui_newname_AddressBookMsg_sinit_ui_playerLogin_cpp.cpp | src/game/ui/ui_playerLogin.cpp |
| ui_newname_data.cpp | src/game/ui/ui_playerLogin.cpp |
| ui_newname_msg.cpp | src/game/ui/ui_playerLogin.cpp |
| ui_pause_data.cpp | src/game/ui/ui_mainpanel2.cpp |
| ui_progress_cursor.cpp | src/game/ui/InterfaceCursor.cpp (2), src/game/ui/ui_progressBar.cpp (2) |
| ui_ridepanel_remove.cpp | src/game/ui/ui_ridepanel.cpp |
| ui_staffPanel.cpp | src/game/ui/ui_staffPanel.cpp |
| ui_staffPanel_ShowKidLocatorPanel.cpp | src/game/ui/ui_staffPanel.cpp |
| ui_staffPanel_init.cpp | src/game/ui/ui_staffPanel.cpp |
| ui_staffPanel_msg.cpp | src/game/ui/ui_staffPanel.cpp |
| ui_staffPanel_msg_hackfollowmode.cpp | src/game/ui/ui_staffPanel.cpp |
| ui_staffPanel_msg_hackfollowmode_GetNextStaffMember.cpp | src/game/ui/ui_showStatus.cpp (3), src/game/ui/ui_staffPanel.cpp (1) |
| ui_tooltip.cpp | src/game/ui/ui_tooltip.cpp |
| ui_whatami.cpp | src/game/ui/InterfaceWindow.cpp (3), src/game/ui/InterfaceBar.cpp (1), src/game/ui/InterfaceButton.cpp (1), src/game/ui/InterfaceEditBox.cpp (1), src/game/ui/InterfaceImage.cpp (1), src/game/ui/InterfaceKnob.cpp (1), src/game/ui/InterfaceLightPanel.cpp (1), src/game/ui/InterfaceListBox.cpp (1), src/game/ui/InterfaceListControl2.cpp (1), src/game/ui/InterfaceTextBox.cpp (1), src/game/ui/InterfaceVaryBox.cpp (1) |
| ui_whatami_InterfaceLightPanel_SetValue.cpp | src/game/ui/InterfaceLightPanel.cpp |
| ui_whatami_InterfaceTextBox_CalcTextPos_2.cpp | src/game/ui/InterfaceTextBox.cpp (5), src/game/ui/InterfaceStringHeap.cpp (2) |
| ui_whatami_InterfaceVaryBox_ChangeValue.cpp | src/game/ui/InterfaceVaryBox.cpp |
| ui_window_scripts.cpp | src/game/ui/ui_allStaffPanel.cpp (1), src/game/ui/ui_cityLobbyController.cpp (1), src/game/ui/ui_onlineCityInfoOverlay.cpp (1), src/game/ui/ui_onlineOutbox.cpp (1), src/game/ui/ui_onlineParkInfoPanel.cpp (1), src/game/ui/ui_parkStatus.cpp (1) |
| ui_window_surface.cpp | src/game/ui/InterfaceWindow.cpp (2), src/game/ui/InterfaceTimer.cpp (1) |
| v5_twins.cpp | src/v5_twins.cpp |
| vector2d.cpp | src/game/ui/vector2d.cpp |
| vector2d_CUIMsgReceiver_ReceiveMessage.cpp | src/game/ui/vector2d.cpp |
