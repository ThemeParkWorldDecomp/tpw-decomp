// heldpark.cpp: the port's held park (no Mac code; the port's fix heldpark, include/SaveLoad.h). A park that needs rides the
// game has not got (the Gold rides or a mod's content rides switched off) is not loaded, on the Mac as here; this tells the
// player which rides, in the game's own error dialog (AddUIError, src/game/ui/ui_ErrorDisplay.cpp), and holds the park so
// that GAMESYSTEM_Tick leaves it before its first frame, saves nothing and goes back to the menu, where the message opens.
