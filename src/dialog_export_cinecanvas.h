// Copyright (c) 2025, Aegisub Project
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file dialog_export_cinecanvas.h
/// @see dialog_export_cinecanvas.cpp
/// @ingroup subtitle_io export

#pragma once

#include <libaegisub/vfr.h>
#include <libaegisub/fs.h>
#include <string>

class wxWindow;
class AssFile;

/// User configuration for CineCanvas XML export
/// All values are derived from the session/file context, not from stored preferences
class CineCanvasExportSettings {
public:
	/// Frame rate options for DCP subtitles
	enum FrameRate {
		FPS_23_976 = 0, ///< 23.976 fps (common for cinema)
		FPS_24     = 1, ///< 24 fps (standard cinema)
		FPS_25     = 2, ///< 25 fps (PAL)
		FPS_29_97  = 3, ///< 29.97 fps (NTSC)
		FPS_30     = 4, ///< 30 fps
		FPS_48     = 5, ///< 48 fps (HFR cinema)
		FPS_50     = 6, ///< 50 fps (HFR PAL)
		FPS_59_94  = 7, ///< 59.94 fps (HFR NTSC)
		FPS_60     = 8  ///< 60 fps (HFR)
	};

	/// Frame rate to use for timing conversion
	FrameRate frame_rate = FPS_24;

	/// Title of the movie/project (derived from filename)
	std::string movie_title;

	/// DCP reel number (usually 1-based) - DCP-specific, user must specify
	int reel_number = 1;

	/// ISO 639-2 language code (e.g., "en", "fr", "de") - DCP-specific, user must specify
	std::string language_code = "en";

	/// Include font reference in LoadFont element
	bool include_font_reference = false;

	/// Font file URI for LoadFont element (if include_font_reference is true)
	std::string font_uri;

	/// Get the agi::vfr::Framerate for the current setting
	agi::vfr::Framerate GetFramerate() const;

	/// Initialize export settings from context
	/// @param filename The output filename (used to derive movie title)
	/// @param video_fps Optional framerate from loaded video (if available)
	CineCanvasExportSettings(agi::fs::path const& filename, agi::vfr::Framerate const& video_fps = agi::vfr::Framerate());

	/// Validate settings and return warnings/errors
	/// @param file AssFile to check for compatibility issues
	/// @return Empty string if no issues, otherwise warning/error message
	std::string Validate(const AssFile *file) const;
};

/// Show a dialog box for getting an export configuration for CineCanvas XML
/// @param owner Parent window of the dialog
/// @param s Struct with initial values and to fill with the chosen settings
/// @param file AssFile to check for validation warnings
/// @return wxID_OK if user accepted, wxID_CANCEL if cancelled
int ShowCineCanvasExportDialog(wxWindow *owner, CineCanvasExportSettings &s, const AssFile *file);
