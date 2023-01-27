# NANO FOR WINDOWS

This is the Windows version of the legendary **[GNU nano text editor](https://www.nano-editor.org/)**, a small and compact CLI editor that packs a world of functionality in a small footprint (less than 300KB) based on the pico editor.

GNU nano is constantly being upgraded, but the original project is limited to providing support and functionality only for Linux. This project is meant to bring the latest updates to Windows 64/32.

<div align="center"><img width="640" alt="image" src="https://user-images.githubusercontent.com/22417711/215018921-9b0ae50c-e63e-424c-85bb-6b722c5944b0.png"></div>

# Features ported to Windows

Pretty much everything is supported except for Linux-specific functions. We can highlight:

- interface customization: colors, key shortcuts, line numbers, scroll bar...
- Syntax coloring for 47 different types of documents. It can be upgraded thanks to community support, for instance [here](https://github.com/scopatz/nanorc) or [here](https://github.com/mitchell486/nanorc).
- multi-document editor.
- UTF-8 encoding support.
- Supplement Unicode characters support, including all the languages and emojis 😜in the CLI interface.
- Mouse support for scroll and cursor location.
- Line wrap, search, cut, copy & paste, and all the basic functions of any full-fledged editor.

<div align="center"><img width="640" alt="Screenshot 2023-01-26 212422" src="https://user-images.githubusercontent.com/22417711/215018756-09651357-8412-4b2c-a9af-c8dd2f0a20f0.png"></div>

# Installation

The easier way to install nano is by using **[Scoop package manager](https://scoop.sh/)**.

1. Open a PowerShell terminal (version 5.1 or later) and run:
```pwsh
> Set-ExecutionPolicy RemoteSigned -Scope CurrentUser # Optional: Needed to run a remote script the first time
> irm get.scoop.sh | iex
```
2. Add the nano for windows app directly from its bucket:
```pwsh
> scoop install https://raw.githubusercontent.com/okibcn/Bucket/master/bucket/nano.json
```
# Known Issues

- Pipes in Windows are not working yet.
