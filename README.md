# Build Menu Framework

Register modded items in the normal Build menu.

## Install

Install [Moon Sweepers Mod Loader](https://github.com/swooping-mods/moon-sweepers-mod-loader) first.

Download [build-menu-framework-0.2.2.msmod](content/build-menu-framework-0.2.2.msmod), import it with **Install mod package**, then select the mod and its required frameworks.

## Controls

- Shared registration for native Build menu items; no player shortcuts.

## Compatibility

Version 0.2.2. Targets the pinned Steam Windows x64 IL2CPP build of Moon Sweepers (Steam app 5335510). On Linux, the game runs through Proton. Experimental: back up saves before using save-affecting mods.

## Build

Run `python3 build.py --check` to verify source hashes, then `python3 build.py` to write a build under `.local/build/`. Requires installed Clang, lld-link and llvm-dlltool.

## Credits

See [CREDITS.txt](CREDITS.txt) and [third-party notices](vendored/THIRD_PARTY_NOTICES.md).
