// Copyright (c) 2025, Aegisub Project
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

/// @file cinecanvas_config.h
/// @brief CineCanvas configuration validation and access
/// @ingroup subtitle_io

#pragma once

#include <string>
#include <vector>

namespace agi { class OptionValue; }

/// @class CineCanvasConfig
/// @brief Validates and provides access to CineCanvas export configuration
class CineCanvasConfig {
public:
	/// Supported frame rates for DCP subtitles
	static const std::vector<int> SUPPORTED_FRAME_RATES;

	/// Minimum and maximum font size values
	static const int MIN_FONT_SIZE = 10;
	static const int MAX_FONT_SIZE = 72;

	/// Minimum reel number
	static const int MIN_REEL_NUMBER = 1;

	/// Minimum fade duration (milliseconds)
	static const int MIN_FADE_DURATION = 0;

	/// Default values
	static const int DEFAULT_FRAME_RATE = 24;
	static const std::string DEFAULT_MOVIE_TITLE;
	static const int DEFAULT_REEL_NUMBER = 1;
	static const std::string DEFAULT_LANGUAGE_CODE;
	static const int DEFAULT_FONT_SIZE = 42;
	static const int DEFAULT_FADE_DURATION = 20;
	static const bool DEFAULT_INCLUDE_FONT_REFERENCE = false;

	/// Validates frame rate value
	/// @param fps Frame rate to validate
	/// @return Validated frame rate (falls back to default if invalid)
	static int ValidateFrameRate(int fps);

	/// Validates movie title
	/// @param title Movie title to validate
	/// @return Validated title (falls back to default if empty)
	static std::string ValidateMovieTitle(const std::string& title);

	/// Validates reel number
	/// @param reel Reel number to validate
	/// @return Validated reel number (falls back to default if invalid)
	static int ValidateReelNumber(int reel);

	/// Validates ISO 639-2 language code
	/// @param code Language code to validate
	/// @return Validated language code (falls back to default if invalid)
	static std::string ValidateLanguageCode(const std::string& code);

	/// Validates font size
	/// @param size Font size to validate
	/// @return Validated font size (falls back to default if invalid)
	static int ValidateFontSize(int size);

	/// Validates fade duration
	/// @param duration Fade duration in milliseconds to validate
	/// @return Validated fade duration (falls back to default if invalid)
	static int ValidateFadeDuration(int duration);

	/// Checks if a language code is valid ISO 639-2 format
	/// @param code Language code to check
	/// @return true if valid, false otherwise
	static bool IsValidLanguageCode(const std::string& code);
};
