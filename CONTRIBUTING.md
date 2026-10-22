# Contributing

This repository takes no pull requests and no changes of any kind. It is a pure translation of the Mac program and it stays
unchanged. A function changes only when the differential tests show that its translation differs from the Mac, and the
maintainers make that change from their private working tree.

Everything else (port work, bug fixes, other systems such as Windows or Linux, build changes, tools) goes to
[tpw-port](https://github.com/ThemeParkWorldDecomp/tpw-port): a changed game source file is a whole-file copy under overrides/ at the same path, with a row in
FIXES.md. Read its CONTRIBUTING.md first.

A pull request opened here is closed with a pointer to tpw-port.
