# Unit w79h: the Execute bodies of the sound library's system commands (the 47 TbSysCommand classes) and the five payload accessors of
# their base classes, run against the real audio object of w79f (TbAudioBody: GetSoundManager, GetPlaceHolderList, SystemCommand,
# with two of its virtuals hooked), the info and sample-info records (w77) and the library's own command constructors (w79b).
# Fakes in the unit file, each recording what the original's callee is given: the sound manager and the core under it, the
# placeholder list and a placeholder (w79g1, w79g2), TpBody::Tick/Save/Load (untranslated), CvtDblUl and the streamer calls the
# body links.
set(TPW_UNIT_SysCommands src/TbSysCommand.cpp src/TbAudioBody.cpp src/CAudioComponent.cpp src/snd_commands.cpp src/TbSoundInfo.cpp
    src/TbRefRelease.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp platform/arena.cpp)
