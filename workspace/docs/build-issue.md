Found the issue! There's a naming mismatch:
  - meson install targets: Aegisub Cinema.app (with space)
  - osx-bundle.sh creates: Aegisub.app (different name)

    Root Cause: Naming Mismatch
```
  | File                          | App Name Used      |
  |-------------------------------|--------------------|
  | meson.build (line 51)         | Aegisub Cinema.app |
  | tools/osx-bundle.sh (line 18) | Aegisub.app        |
  | tools/osx-dmg.sh (line 25)    | Aegisub.app        |
```
  What happens:
  1. meson install puts the binary in Aegisub Cinema.app/Contents/MacOS/aegisub
  2. osx-bundle.sh creates an empty Aegisub.app folder
  3. osx-dmg.sh packages the empty Aegisub.app
  4. Result: 1.4MB DMG with no binary inside

  Solution: Update Scripts to Use "Aegisub Cinema"


```
   sudo python3 "/Users/loukmillher/Documents/Development/Projects/Active/Aegisub/tools/osx-fix-libs.py"
  "/Users/loukmillher/Documents/Development/Projects/Active/Aegisub/build_static/Aegisub Cinema.app/Contents/MacOS/aegisub"
```

  ---
  Summary of Build Issue Fix

  Root cause: Naming mismatch between meson config and shell scripts

  | File                | Before             | After              |
  |---------------------|--------------------|--------------------|
  | meson.build         | Aegisub Cinema.app | (unchanged)        |
  | tools/osx-bundle.sh | Aegisub.app        | Aegisub Cinema.app |
  | tools/osx-dmg.sh    | Aegisub.app        | Aegisub Cinema.app |

  Additional fixes: Made SetFile and DS_Store copy steps non-fatal (cosmetic features that can fail due to macOS permissions)

  ---
  Changes Summary for Ticket

  Line break fix (src/subtitle_format_cinecanvas.cpp):
  - Modified WriteSubtitle to split text on \N delimiters
  - Creates separate <Text> elements for each line with different VPosition values (10.0 for bottom, +6.5 for each line above)
  - Matches reference CineCanvas XML format

  Build system fix:
  - tools/osx-bundle.sh: Changed app name from Aegisub.app to Aegisub Cinema.app
  - tools/osx-dmg.sh: Changed app name from Aegisub.app to Aegisub Cinema.app
  - Made cosmetic DMG steps (volume icon, DS_Store) non-fatal
