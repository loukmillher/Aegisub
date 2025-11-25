Found the issue! There's a naming mismatch:
  - meson install targets: Aegisub Cinema.app (with space)
  - osx-bundle.sh creates: Aegisub.app (different name)

    Root Cause: Naming Mismatch

  | File                          | App Name Used      |
  |-------------------------------|--------------------|
  | meson.build (line 51)         | Aegisub Cinema.app |
  | tools/osx-bundle.sh (line 18) | Aegisub.app        |
  | tools/osx-dmg.sh (line 25)    | Aegisub.app        |

  What happens:
  1. meson install puts the binary in Aegisub Cinema.app/Contents/MacOS/aegisub
  2. osx-bundle.sh creates an empty Aegisub.app folder
  3. osx-dmg.sh packages the empty Aegisub.app
  4. Result: 1.4MB DMG with no binary inside

  Solution: Update Scripts to Use "Aegisub Cinema"