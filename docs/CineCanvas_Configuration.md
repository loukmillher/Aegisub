# CineCanvas Export Configuration Options

This document describes the configuration options available for CineCanvas XML export in Aegisub.

## Overview

CineCanvas export settings are stored in Aegisub's configuration system and can be customized to match your Digital Cinema Package (DCP) workflow requirements.

## Configuration Options

### Default Frame Rate

- **Option Path**: `Subtitle Format/CineCanvas/Default Frame Rate`
- **Type**: Integer
- **Default**: `24`
- **Valid Values**: `24`, `25`, `30`
- **Description**: Sets the default frame rate for CineCanvas subtitle exports. This determines how timing information is interpreted during export.

**Supported Frame Rates:**
- **24 fps**: Standard cinema frame rate (most common for theatrical releases)
- **25 fps**: PAL standard, used in many European territories
- **30 fps**: Used for some digital cinema applications

**Note**: If an invalid frame rate is provided, the system will fall back to 24 fps.

### Movie Title

- **Option Path**: `Subtitle Format/CineCanvas/Movie Title`
- **Type**: String
- **Default**: `"Untitled"`
- **Description**: The default movie title to use in CineCanvas XML exports. This appears in the `<MovieTitle>` element of the exported XML.

**Validation:**
- Leading and trailing whitespace is automatically removed
- Empty or whitespace-only titles fall back to "Untitled"

### Reel Number

- **Option Path**: `Subtitle Format/CineCanvas/Reel Number`
- **Type**: Integer
- **Default**: `1`
- **Valid Range**: `1` to `999`
- **Description**: The reel number for the DCP. For multi-reel DCPs, this identifies which reel the subtitles belong to.

**Note**: If a value less than 1 is provided, it will fall back to 1.

### Language Code

- **Option Path**: `Subtitle Format/CineCanvas/Language Code`
- **Type**: String
- **Default**: `"en"`
- **Description**: ISO 639-2 language code for the subtitle track. This is used in the `<Language>` element of the CineCanvas XML.

**Common Language Codes:**
- `en` / `eng` - English
- `fr` / `fra` - French
- `de` / `deu` - German
- `es` / `spa` - Spanish
- `it` / `ita` - Italian
- `pt` / `por` - Portuguese
- `ja` / `jpn` - Japanese
- `zh` / `zho` - Chinese
- `ko` / `kor` - Korean

**Validation:**
- Accepts both 2-letter (ISO 639-1) and 3-letter (ISO 639-2) codes
- Automatically converts to lowercase
- Invalid codes fall back to "en"

### Default Font Size

- **Option Path**: `Subtitle Format/CineCanvas/Default Font Size`
- **Type**: Integer
- **Default**: `42`
- **Valid Range**: `10` to `72`
- **Description**: Default font size (in points) for CineCanvas subtitle text. This is used in the `Size` attribute of the `<Font>` element.

**Recommended Values:**
- **42-48 points**: Standard for cinema subtitles
- **36-42 points**: For smaller screens or dense text
- **48-60 points**: For large venues or accessibility requirements

**Note**: Values outside the valid range will fall back to 42 points.

### Fade Duration

- **Option Path**: `Subtitle Format/CineCanvas/Fade Duration`
- **Type**: Integer
- **Default**: `20` (milliseconds)
- **Valid Range**: `0` to `∞`
- **Description**: Default fade-in and fade-out duration in milliseconds. This is used for the `FadeUpTime` and `FadeDownTime` attributes of subtitle elements.

**Common Values:**
- **0 ms**: No fade (instant appearance/disappearance)
- **20 ms**: Quick fade (default, subtle)
- **100 ms**: Medium fade (more noticeable)
- **250 ms**: Slow fade (smooth, cinematic)

**Note**: Negative values will fall back to 20 ms.

### Include Font Reference

- **Option Path**: `Subtitle Format/CineCanvas/Include Font Reference`
- **Type**: Boolean
- **Default**: `false`
- **Description**: When enabled, includes `<LoadFont>` elements in the exported CineCanvas XML to reference external font files.

**Values:**
- `false`: No font references included (default)
- `true`: Include `<LoadFont>` elements for fonts used in the subtitles

## Accessing Configuration Options

### Via Aegisub Preferences

Configuration options can be accessed through Aegisub's preferences dialog:

1. Open Aegisub
2. Go to **View** → **Options** (or **Aegisub** → **Preferences** on macOS)
3. Navigate to the **Subtitle Format** section
4. Select **CineCanvas** settings

### Via Configuration File

Configuration options are stored in JSON format in Aegisub's configuration file:

**Location:**
- **macOS**: `~/Library/Application Support/Aegisub/config.json`
- **Linux**: `~/.config/aegisub/config.json`
- **Windows**: `%APPDATA%/Aegisub/config.json`

**Example Configuration:**
```json
{
  "Subtitle Format": {
    "CineCanvas": {
      "Default Frame Rate": 24,
      "Movie Title": "My Feature Film",
      "Reel Number": 1,
      "Language Code": "en",
      "Default Font Size": 48,
      "Fade Duration": 20,
      "Include Font Reference": false
    }
  }
}
```

## Validation Behavior

All configuration options include validation to ensure valid values:

1. **Invalid values** are automatically replaced with safe defaults
2. **Type mismatches** (e.g., string instead of integer) fall back to defaults
3. **Out-of-range values** are clamped or replaced with defaults
4. **Validation occurs** both when loading configuration and when values are used during export

## Best Practices

### Frame Rate Selection

- Use **24 fps** for theatrical releases and standard DCP mastering
- Use **25 fps** for PAL territories or European broadcast DCPs
- Use **30 fps** only when specifically required by the distribution spec

### Movie Title

- Use descriptive titles that match the DCP's Content Title
- Avoid special characters that might cause XML parsing issues
- Keep titles concise (under 50 characters recommended)

### Font Size

- **Test on actual cinema screens** when possible
- Larger venues may need larger font sizes (48-60 pt)
- Smaller art-house theaters can use smaller sizes (36-42 pt)
- Consider audience accessibility requirements

### Fade Duration

- **Keep fades subtle** (0-50 ms) for professional cinema subtitles
- **Avoid long fades** (>250 ms) as they can feel sluggish
- **Use 0 ms** for precise, frame-accurate subtitle timing

## See Also

- [CineCanvas Export Guide](CineCanvas_Export_Guide.md) - Complete export workflow
- [CineCanvas Technical Specification](CineCanvas_Technical_Spec.md) - XML format details
- [ASS to CineCanvas Mapping](ASS_to_CineCanvas_Mapping.md) - Conversion details
