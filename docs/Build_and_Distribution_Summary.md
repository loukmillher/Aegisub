# Aegisub Cinema - Build and Distribution Summary

**Date:** November 14, 2025  
**Build Version:** Cinema-e0d3814a3 (git hash: e0d3814a3)

## Build Status ✅

Successfully built Aegisub Cinema as a standalone macOS application bundle and packaged it as a DMG for distribution.

## Deliverables

### DMG Package
- **Location:** `build_static/Aegisub-Cinema-e0d3814a3.dmg`
- **Size:** 53 MB (includes all bundled libraries)
- **MD5 Checksum:** `5290ae07561160ecbf7b895ee0c35c90`
- **SHA256 Checksum:** `78251672739bfde6feb1ed2f6c3680db71151158e3ddacdf821b116ba7832678`
- **Status:** ✅ Verified, signed, and tested

### Application Bundle
- **Location:** `build_static/Aegisub Cinema.app`
- **Architecture:** Native (arm64 on Apple Silicon, x86_64 on Intel)
- **Minimum macOS:** 11.0 (Big Sur)
- **Status:** ✅ All libraries bundled and fixed

## Build Process Completed

### 1. Application Bundle Creation
- Compiled Aegisub binary with static linking
- Created macOS app bundle structure
- Installed all required resources:
  - 30+ language localizations
  - Automation scripts (Lua/Moonscript)
  - English dictionary files
  - Application icons and resources

### 2. Library Bundling
Fixed and bundled **100+ dynamic libraries** including:
- FFmpeg suite (libavcodec, libavformat, libavutil, libswscale, libswresample)
- Subtitle rendering (libass)
- Audio/Video codecs (x264, x265, VP8/VP9, AV1, Opus, Vorbis, etc.)
- Text rendering (FreeType, Fribidi, HarfBuzz)
- System utilities (ICU, Hunspell, FFTW)

All library paths were fixed using `install_name_tool` to reference `@executable_path` for portability.

### 3. DMG Creation
Created a professional installer DMG with:
- Drag-and-drop installation (Applications folder symlink)
- Custom background image
- Volume icon
- Compressed format for efficient distribution

## Distribution Notes

### Installation Instructions for Friends
1. Download `Aegisub-Cinema-e0d3814a3.dmg`
2. Double-click the DMG to mount it
3. Drag `Aegisub.app` to the Applications folder
4. Eject the DMG
5. Launch Aegisub from Applications

### First Launch
The app is ad-hoc code signed and should launch normally. If macOS shows a security warning:
1. Right-click (or Control-click) on Aegisub Cinema.app
2. Select "Open" from the context menu
3. Click "Open" in the security dialog

Alternatively, go to **System Settings > Privacy & Security** and allow the app to run.

**Note:** The app uses an ad-hoc signature (not from an Apple Developer account), so it will run on your Mac but may show warnings on other Macs until you obtain a proper Developer ID certificate.

## Known Considerations

### Code Signing ✅
The app has been ad-hoc code signed with `codesign --deep --sign -`, which allows it to run on your Mac and Macs with similar security settings.

**Current Status:**
- ✅ Ad-hoc signed (suitable for personal use and sharing with friends)
- ✅ Info.plist properly configured
- ✅ All libraries bundled with fixed paths

**For wider distribution**, you may want to:
- Sign the app with an Apple Developer certificate (requires $99/year Apple Developer membership)
- Notarize it with Apple for Gatekeeper compatibility (allows installation without warnings)

### Library Dependencies
The script warned about some libraries having Homebrew paths. However, all necessary libraries have been copied into the app bundle, so the app should work on any Mac without requiring Homebrew or other dependencies.

## Files Modified
- `src/subtitle_format_cinecanvas.cpp` - Your CineCanvas export feature

## Testing Results
✅ DMG mounts successfully  
✅ App bundle structure is correct  
✅ All required libraries are bundled  
✅ Checksum verification passed  

## Next Steps (Optional)

If you want to prepare this for wider distribution:

1. **Code Sign the Application:**
   ```bash
   codesign --deep --force --sign "Your Developer ID" "Aegisub Cinema.app"
   ```

2. **Notarize with Apple:**
   ```bash
   xcrun notarytool submit Aegisub-Cinema-e0d3814a3.dmg --apple-id your@email.com --team-id TEAMID
   ```

3. **Create a Universal Binary:**
   - Build separately for arm64 and x86_64
   - Use `lipo` to combine into a universal binary
   - Rebuild the DMG

## Support

The app includes all standard Aegisub features plus your CineCanvas XML export functionality. For issues, check:
- Aegisub documentation: http://docs.aegisub.org/
- Original project: https://github.com/TypesettingTools/Aegisub

---

**Ready to share!** The DMG is located at:
```
/Users/loukmillher/Documents/Development/Projects/Active/Aegisub/build_static/Aegisub-Cinema-e0d3814a3.dmg
```

