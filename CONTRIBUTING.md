# Contributing

The project is three repositories, and every change belongs in exactly one of them:

| Repository | What it holds | Pull requests |
| --- | --- | --- |
| [tpw-decomp](https://github.com/ThemeParkWorldDecomp/tpw-decomp) | The translation: the Mac program's own functions in C++, each behaving exactly as the Mac build does, bugs included, with the differential tests that check it. | None. A function that behaves differently from the Mac is reported as an issue on tpw-port. |
| [tpw-port](https://github.com/ThemeParkWorldDecomp/tpw-port) | The original game on today's Macs, Windows PCs and Linux machines: the launcher, the platform layer, the builds, and the fixes that make it run as it did on the Mac. | Welcome, by its CONTRIBUTING.md. |
| [tpw-mods](https://github.com/ThemeParkWorldDecomp/tpw-mods) | Optional changes to how the game plays, each one off until the player turns it on. tpw-port carries it as a submodule at mods/. | Welcome, by its CONTRIBUTING.md. |

The test for where a change goes: does a park play the same with it on and with it off? If it does, the change belongs in
tpw-port. If it changes the rules, the money, the visitors, the staff, the rides, the advisor, the game's content, what a
control does or the save files, it belongs in tpw-mods, even as a setting that is off by default.

One exception: an option the original game already had in its .sam data files belongs in tpw-port, even when it
changes how a park plays, as long as it offers only the values the original accepts and its default is the original's.
Players set it in the launcher or a game menu, never by editing a .sam file.

This repository takes no pull requests and no changes of any kind. A function here changes only when the differential
tests show that its translation differs from the Mac, and the maintainers make that change in the private working tree
both tpw-decomp and tpw-port are exported from. If you find such a function, open an issue on
[tpw-port](https://github.com/ThemeParkWorldDecomp/tpw-port/issues).

Port work, bug fixes, other systems, builds and tools go to [tpw-port](https://github.com/ThemeParkWorldDecomp/tpw-port); optional changes to how the
game plays go to [tpw-mods](https://github.com/ThemeParkWorldDecomp/tpw-mods). Read the CONTRIBUTING.md there first. A pull request opened here is
closed with a pointer to the right repository.
