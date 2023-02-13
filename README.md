# NANO FOR WINDOWS

[![Xbuild4Windows](https://github.com/okibcn/nano-for-windows/actions/workflows/Xbuild4Windows.yml/badge.svg)](https://github.com/okibcn/nano-for-windows/actions/workflows/Xbuild4Windows.yml) [![SyncNano](https://github.com/okibcn/nano-for-windows/actions/workflows/SyncNano.yml/badge.svg)](https://github.com/okibcn/nano-for-windows/actions/workflows/SyncNano.yml)
![Version](https://img.shields.io/github/v/release/okibcn/nano-for-windows) [![Github All Releases](https://img.shields.io/github/downloads/okibcn/nano-for-windows/total.svg)]()


This is the 32/64 bits Windows terminal version of the legendary **[GNU nano text editor](https://www.nano-editor.org/)**, a small and compact CLI editor that packs a world of functionality in a small footprint (less than 300KB).

GNU nano is constantly being upgraded, but the original project is limited to providing support and functionality only for Linux. This project is meant to bring the latest updates to Windows 64/32 bits.

<div align="center"><img width="930" alt="image" src="https://user-images.githubusercontent.com/22417711/218406506-88cc1dc2-ee36-4440-94d2-61dd7dd84db6.png">
</div>

# Features ported to Windows

Pretty much everything is supported except for Linux-specific functions. We can highlight:

- Interface customization: colors, key shortcuts, line numbers, infobar, scroll bar...
- Syntax coloring for 47 different types of documents. It can be upgraded thanks to community support, for instance [here](https://github.com/scopatz/nanorc) or [here](https://github.com/mitchell486/nanorc) you can find additional syntax files for more types of documents.
- UTF-8 encoding support.
- Supplement Unicode characters support, including all the languages and emojis 😜in the CLI interface.
- multi-document editor.
- Integrated file browser.
- Mouse support for scroll and cursor location.
- Normal or regular expression (regex) search and replace.
- auto indent, syntax highlight and fast line comment for many programming languages.
- Line wrap, search, cut, copy & paste, and all the basic functions of any full-fledged editor.
- Data input through stdin support, from pipe or keyboard.
- Transparent background in Windows Terminal.

<div align="center"><img width="947" alt="image" src="https://user-images.githubusercontent.com/22417711/218406120-a7198c98-fd5f-4a1b-8793-b65c3ba68d7c.png"></div>

# Installation

The easier way to install nano is by using **[Scoop package manager](https://scoop.sh/)**.

1. To install Scoop utility open a PowerShell terminal (version 5.1 or later) and run:
```pwsh
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser # Optional: Needed to run a remote script the first time
irm get.scoop.sh | iex
```
2. Add the nano for windows app directly from its bucket:
```pwsh
scoop bucket add .oki https://github.com/okibcn/Bucket  # Optional: the changes are propagated faster this way
scoop install nano
```
To remove the app just type:
```pwsh
scoop uninstall nano
```
To update the app just type:
```pwsh
scoop update nano
```
# Usage

This repo handles only the conversion to Windows OS. The original GNU nano **[documentation](https://www.nano-editor.org/docs.php)** covers all the usage instructions. For a quick reference you can just press F1 within nano to open the integrated help.

The interface customization file is located in the user profile and has a fair description of each setting. To edit it, just open a powershell terminal type:
```pwsh
nano ~/.nanorc
```
# Notes

- When using Windows Terminal and the screen is resized within nano, after returning to the shell there could be some corruption due to the new size. To fix the issue without losing the terminal history just resize the terminal window to zero lines and return to the desired size.

- Feel free to open any issue you find, or use the [Discussions](https://github.com/okibcn/nano-for-windows/discussions) section for any other issue, suggestion, question, etc...
