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

/// @file cinecanvas_config.cpp
/// @brief CineCanvas configuration validation and access implementation
/// @ingroup subtitle_io

#include "cinecanvas_config.h"

#include <algorithm>
#include <cctype>
#include <set>

// Static member initialization
const std::vector<int> CineCanvasConfig::SUPPORTED_FRAME_RATES = {24, 25, 30};
const std::string CineCanvasConfig::DEFAULT_MOVIE_TITLE = "Untitled";
const std::string CineCanvasConfig::DEFAULT_LANGUAGE_CODE = "en";

// Common ISO 639-2 language codes for cinema
// This is not exhaustive but covers the most common codes
static const std::set<std::string> VALID_LANGUAGE_CODES = {
	// Common 2-letter codes (ISO 639-1, but widely used)
	"en", "fr", "de", "es", "it", "pt", "ru", "ja", "zh", "ko",
	"ar", "he", "hi", "nl", "pl", "sv", "da", "no", "fi", "cs",
	"el", "tr", "th", "vi", "id", "ms", "tl", "uk", "ro", "hu",

	// ISO 639-2/T codes (terminology)
	"eng", "fra", "deu", "spa", "ita", "por", "rus", "jpn", "zho", "kor",
	"ara", "heb", "hin", "nld", "pol", "swe", "dan", "nor", "fin", "ces",
	"ell", "tur", "tha", "vie", "ind", "msa", "tgl", "ukr", "ron", "hun",

	// ISO 639-2/B codes (bibliographic)
	"ger", // German (bibliographic)
	"fre", // French (bibliographic)
	"chi", // Chinese (bibliographic)
	"cze", // Czech (bibliographic)
	"dut", // Dutch (bibliographic)
	"gre", // Greek (bibliographic)
	"per", // Persian (bibliographic)
	"rum", // Romanian (bibliographic)
	"slo", // Slovak (bibliographic)
	"wel"  // Welsh (bibliographic)
};

int CineCanvasConfig::ValidateFrameRate(int fps) {
	auto it = std::find(SUPPORTED_FRAME_RATES.begin(), SUPPORTED_FRAME_RATES.end(), fps);
	if (it != SUPPORTED_FRAME_RATES.end()) {
		return fps;
	}
	return DEFAULT_FRAME_RATE;
}

std::string CineCanvasConfig::ValidateMovieTitle(const std::string& title) {
	// Remove leading/trailing whitespace
	auto start = title.find_first_not_of(" \t\n\r");
	auto end = title.find_last_not_of(" \t\n\r");

	if (start == std::string::npos || end == std::string::npos) {
		return DEFAULT_MOVIE_TITLE;
	}

	std::string trimmed = title.substr(start, end - start + 1);
	return trimmed.empty() ? DEFAULT_MOVIE_TITLE : trimmed;
}

int CineCanvasConfig::ValidateReelNumber(int reel) {
	return (reel >= MIN_REEL_NUMBER) ? reel : DEFAULT_REEL_NUMBER;
}

std::string CineCanvasConfig::ValidateLanguageCode(const std::string& code) {
	// Convert to lowercase for comparison
	std::string lowerCode = code;
	std::transform(lowerCode.begin(), lowerCode.end(), lowerCode.begin(),
	               [](unsigned char c) { return std::tolower(c); });

	if (IsValidLanguageCode(lowerCode)) {
		return lowerCode;
	}
	return DEFAULT_LANGUAGE_CODE;
}

int CineCanvasConfig::ValidateFontSize(int size) {
	if (size >= MIN_FONT_SIZE && size <= MAX_FONT_SIZE) {
		return size;
	}
	return DEFAULT_FONT_SIZE;
}

int CineCanvasConfig::ValidateFadeDuration(int duration) {
	return (duration >= MIN_FADE_DURATION) ? duration : DEFAULT_FADE_DURATION;
}

bool CineCanvasConfig::IsValidLanguageCode(const std::string& code) {
	// Check if it's in our known valid codes
	if (VALID_LANGUAGE_CODES.find(code) != VALID_LANGUAGE_CODES.end()) {
		return true;
	}

	// Also accept any 2-3 letter alphabetic code as potentially valid
	// (to allow for codes we might not have in our list)
	if (code.length() < 2 || code.length() > 3) {
		return false;
	}

	return std::all_of(code.begin(), code.end(),
	                   [](unsigned char c) { return std::isalpha(c); });
}
