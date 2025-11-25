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

/// @file subtitle_format_cinecanvas.cpp
/// @brief Reading/writing CineCanvas-style XML subtitles for Digital Cinema Packages
/// @ingroup subtitle_io

#include "subtitle_format_cinecanvas.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "ass_style.h"
#include "compat.h"
#include "dialog_export_cinecanvas.h"
#include "options.h"

#include <libaegisub/ass/time.h>
#include <libaegisub/color.h>
#include <libaegisub/fs.h>

#include <wx/xml/xml.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>

#include <algorithm>

DEFINE_EXCEPTION(CineCanvasParseError, SubtitleFormatParseError);

CineCanvasSubtitleFormat::CineCanvasSubtitleFormat()
: SubtitleFormat("CineCanvas XML")
{
}

std::vector<std::string> CineCanvasSubtitleFormat::GetReadWildcards() const {
	return {"xml"};
}

std::vector<std::string> CineCanvasSubtitleFormat::GetWriteWildcards() const {
	return {"xml"};
}

bool CineCanvasSubtitleFormat::CanReadFile(agi::fs::path const& filename, const char *) const {
	// Check extension first
	if (!agi::fs::HasExtension(filename, "xml"))
		return false;

	// Check if the XML has DCSubtitle as root element
	wxXmlDocument doc;
	if (!doc.Load(filename.wstring()))
		return false;

	wxXmlNode *root = doc.GetRoot();
	return root && root->GetName() == "DCSubtitle";
}

bool CineCanvasSubtitleFormat::CanSave(const AssFile *file) const {
	// CineCanvas format supports basic subtitle functionality
	// More validation will be added in future phases
	return true;
}

void CineCanvasSubtitleFormat::ReadFile(AssFile *target, agi::fs::path const& filename,
                                        agi::vfr::Framerate const& fps, const char *encoding) const {
	// Load default ASS structure (uses Default style)
	target->LoadDefault(false);

	// Load and validate XML
	wxXmlDocument doc;
	if (!doc.Load(filename.wstring()))
		throw CineCanvasParseError("Failed to load CineCanvas XML file");

	wxXmlNode *root = doc.GetRoot();
	if (!root || root->GetName() != "DCSubtitle")
		throw CineCanvasParseError("Invalid CineCanvas file: missing DCSubtitle root element");

	// Find Font node(s) containing Subtitle elements
	for (wxXmlNode *child = root->GetChildren(); child; child = child->GetNext()) {
		if (child->GetName() == "Font") {
			// Iterate through Subtitle elements within Font node
			for (wxXmlNode *subNode = child->GetChildren(); subNode; subNode = subNode->GetNext()) {
				if (subNode->GetName() == "Subtitle") {
					// Parse TimeIn and TimeOut attributes
					wxString timeInStr = subNode->GetAttribute("TimeIn", "00:00:00:000");
					wxString timeOutStr = subNode->GetAttribute("TimeOut", "00:00:05:000");

					agi::Time timeIn = ConvertTimeFromCineCanvas(from_wx(timeInStr));
					agi::Time timeOut = ConvertTimeFromCineCanvas(from_wx(timeOutStr));

					// Collect all Text elements with their VPosition for proper ordering
					struct TextLine {
						double vpos;
						std::string text;
					};
					std::vector<TextLine> textLines;

					for (wxXmlNode *textNode = subNode->GetChildren(); textNode; textNode = textNode->GetNext()) {
						if (textNode->GetName() == "Text") {
							wxString vposStr = textNode->GetAttribute("VPosition", "10.0");
							double vpos = 10.0;
							vposStr.ToDouble(&vpos);

							wxString content = textNode->GetNodeContent();
							if (!content.IsEmpty()) {
								textLines.push_back({vpos, from_wx(content)});
							}
						}
					}

					// Sort by VPosition descending (higher position = earlier/top line)
					std::sort(textLines.begin(), textLines.end(),
						[](const TextLine &a, const TextLine &b) { return a.vpos > b.vpos; });

					// Join text lines with \N
					std::string combinedText;
					for (size_t i = 0; i < textLines.size(); ++i) {
						if (i > 0) combinedText += "\\N";
						combinedText += textLines[i].text;
					}

					// Skip empty subtitles
					if (combinedText.empty()) continue;

					// Create AssDialogue with Default style
					auto diag = new AssDialogue;
					diag->Start = timeIn;
					diag->End = timeOut;
					diag->Text = combinedText;
					diag->Style = "Default";

					target->Events.push_back(*diag);
				}
			}
		}
	}

	// Ensure file has at least one event
	if (target->Events.empty())
		target->Events.push_back(*new AssDialogue);
}

void CineCanvasSubtitleFormat::WriteFile(const AssFile *src, agi::fs::path const& filename,
                                         agi::vfr::Framerate const& fps, const char *encoding) const {
	// Load export settings from preferences
	CineCanvasExportSettings settings("Subtitle Format/CineCanvas");

	// Use the framerate from settings if provided, otherwise use the passed fps
	agi::vfr::Framerate export_fps = settings.GetFramerate();
	if (!export_fps.IsLoaded() && fps.IsLoaded())
		export_fps = fps;

	// Convert to CineCanvas-compatible format
	AssFile copy(*src);
	ConvertToCineCanvas(copy);

	// Create XML structure
	wxXmlDocument doc;
	wxXmlNode *root = new wxXmlNode(nullptr, wxXML_ELEMENT_NODE, "DCSubtitle");
	root->AddAttribute("Version", "1.0");
	doc.SetRoot(root);

	// Write header (metadata and font definitions)
	WriteHeader(root, src, settings);

	// Get the default style from the ASS file to use for export
	// This ensures we export what the user sees in the preview
	const AssStyle *defaultStyle = nullptr;
	for (auto const& style : src->Styles) {
		if (style.name == "Default") {
			defaultStyle = &style;
			break;
		}
	}
	if (!defaultStyle && !src->Styles.empty()) {
		// If no "Default" style, use the first available style
		defaultStyle = &src->Styles.front();
	}

	// Create Font container node using ASS file style properties
	wxXmlNode *fontNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Font");
	fontNode->AddAttribute("Id", "Font1");

	if (defaultStyle) {
		// Use actual style properties from the ASS file
		fontNode->AddAttribute("Size", wxString::Format("%d", static_cast<int>(defaultStyle->fontsize)));
		fontNode->AddAttribute("Weight", defaultStyle->bold ? "bold" : "normal");

		// Convert colors to CineCanvas format (RRGGBBAA)
		fontNode->AddAttribute("Color", to_wx(ConvertColorToRGBA(defaultStyle->primary, 0)));

		// Use border if outline width > 0, otherwise no effect
		if (defaultStyle->outline_w > 0) {
			fontNode->AddAttribute("Effect", "border");
			fontNode->AddAttribute("EffectColor", to_wx(ConvertColorToRGBA(defaultStyle->outline, 0)));
		} else {
			fontNode->AddAttribute("Effect", "none");
			fontNode->AddAttribute("EffectColor", "FF000000");
		}
	} else {
		// Fallback to defaults if no style found
		fontNode->AddAttribute("Size", "42");
		fontNode->AddAttribute("Weight", "normal");
		fontNode->AddAttribute("Color", "FFFFFFFF");
		fontNode->AddAttribute("Effect", "border");
		fontNode->AddAttribute("EffectColor", "FF000000");
	}

	root->AddChild(fontNode);

	// Write subtitle entries
	int spotNumber = 1;
	for (auto const& line : copy.Events) {
		if (!line.Comment) {
			WriteSubtitle(fontNode, &line, spotNumber++, export_fps, settings);
		}
	}

	// Save XML to file
	doc.Save(filename.wstring());
}

void CineCanvasSubtitleFormat::ConvertToCineCanvas(AssFile &file) const {
	// Prepare file for CineCanvas export
	file.Sort();
	StripComments(file);
	RecombineOverlaps(file);
	MergeIdentical(file);
	StripTags(file);
	// Note: We preserve \N line breaks - they will be handled in WriteSubtitle
	// by creating separate <Text> elements with different VPosition values
}

void CineCanvasSubtitleFormat::WriteHeader(wxXmlNode *root, const AssFile *src, const CineCanvasExportSettings &settings) const {
	// SubtitleID with UUID
	wxXmlNode *subIdNode = new wxXmlNode(wxXML_ELEMENT_NODE, "SubtitleID");
	root->AddChild(subIdNode);
	subIdNode->AddChild(new wxXmlNode(wxXML_TEXT_NODE, "", to_wx(GenerateUUID())));

	// MovieTitle
	wxXmlNode *movieTitleNode = new wxXmlNode(wxXML_ELEMENT_NODE, "MovieTitle");
	root->AddChild(movieTitleNode);
	movieTitleNode->AddChild(new wxXmlNode(wxXML_TEXT_NODE, "", to_wx(settings.movie_title)));

	// ReelNumber
	wxXmlNode *reelNode = new wxXmlNode(wxXML_ELEMENT_NODE, "ReelNumber");
	root->AddChild(reelNode);
	reelNode->AddChild(new wxXmlNode(wxXML_TEXT_NODE, "", wxString::Format("%d", settings.reel_number)));

	// Language
	wxXmlNode *langNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Language");
	root->AddChild(langNode);
	langNode->AddChild(new wxXmlNode(wxXML_TEXT_NODE, "", to_wx(settings.language_code)));

	// LoadFont
	wxXmlNode *loadFontNode = new wxXmlNode(wxXML_ELEMENT_NODE, "LoadFont");
	loadFontNode->AddAttribute("Id", "Font1");
	if (settings.include_font_reference && !settings.font_uri.empty()) {
		// Use just the filename from the path
		agi::fs::path font_path(settings.font_uri);
		loadFontNode->AddAttribute("URI", to_wx(font_path.filename().string()));
	} else {
		loadFontNode->AddAttribute("URI", "");
	}
	root->AddChild(loadFontNode);
}

void CineCanvasSubtitleFormat::WriteSubtitle(wxXmlNode *fontNode, const AssDialogue *line,
                                             int spotNumber, const agi::vfr::Framerate &fps,
                                             const CineCanvasExportSettings &settings) const {
	// Create Subtitle element
	wxXmlNode *subtitleNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Subtitle");
	subtitleNode->AddAttribute("SpotNumber", wxString::Format("%d", spotNumber));

	// Convert timing to CineCanvas format with frame-accurate timing
	std::string timeIn = ConvertTimeToCineCanvas(line->Start, fps);
	std::string timeOut = ConvertTimeToCineCanvas(line->End, fps);

	subtitleNode->AddAttribute("TimeIn", to_wx(timeIn));
	subtitleNode->AddAttribute("TimeOut", to_wx(timeOut));

	// Use fade duration from settings
	subtitleNode->AddAttribute("FadeUpTime", wxString::Format("%d", settings.fade_duration));
	subtitleNode->AddAttribute("FadeDownTime", wxString::Format("%d", settings.fade_duration));

	fontNode->AddChild(subtitleNode);

	// Split text by \N (ASS line break marker) to create separate Text elements
	std::string text = line->Text.get();
	std::vector<std::string> lines;

	// Split on \N (case-insensitive: both \N and \n are used in ASS)
	size_t pos = 0;
	size_t prev = 0;
	while ((pos = text.find("\\N", prev)) != std::string::npos) {
		lines.push_back(text.substr(prev, pos - prev));
		prev = pos + 2;
	}
	// Also check for lowercase \n
	if (lines.empty()) {
		prev = 0;
		while ((pos = text.find("\\n", prev)) != std::string::npos) {
			lines.push_back(text.substr(prev, pos - prev));
			prev = pos + 2;
		}
	}
	lines.push_back(text.substr(prev));

	// Remove empty lines and trim whitespace
	std::vector<std::string> cleanedLines;
	for (const auto& l : lines) {
		std::string trimmed = l;
		// Trim leading/trailing whitespace
		size_t start = trimmed.find_first_not_of(" \t");
		if (start != std::string::npos) {
			size_t end = trimmed.find_last_not_of(" \t");
			trimmed = trimmed.substr(start, end - start + 1);
			if (!trimmed.empty()) {
				cleanedLines.push_back(trimmed);
			}
		}
	}

	// If no valid lines found, use original text
	if (cleanedLines.empty()) {
		cleanedLines.push_back(text);
	}

	// Base VPosition for bottom line, and line spacing (matching reference XML)
	const double baseVPosition = 10.0;
	const double lineSpacing = 6.5;

	// Create Text elements for each line
	// Lines are ordered top-to-bottom in the ASS text, but we need to position
	// them with higher VPosition for upper lines
	int numLines = static_cast<int>(cleanedLines.size());
	for (int i = 0; i < numLines; ++i) {
		wxXmlNode *textNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Text");
		textNode->AddAttribute("VAlign", "bottom");
		textNode->AddAttribute("HAlign", "center");

		// Calculate VPosition: bottom line gets baseVPosition,
		// each line above gets progressively higher position
		// Line order in cleanedLines: [0]=top, [numLines-1]=bottom
		// So line 0 should have highest VPosition, last line has baseVPosition
		double vpos = baseVPosition + (numLines - 1 - i) * lineSpacing;
		textNode->AddAttribute("VPosition", wxString::Format("%.1f", vpos));
		textNode->AddAttribute("HPosition", "0.0");
		textNode->AddAttribute("Direction", "horizontal");

		subtitleNode->AddChild(textNode);
		textNode->AddChild(new wxXmlNode(wxXML_TEXT_NODE, "", to_wx(cleanedLines[i])));
	}
}

std::string CineCanvasSubtitleFormat::ConvertColorToRGBA(const agi::Color &color, uint8_t alpha) const {
	// ASS stores colors internally in RGB format (not BGR)
	// The agi::Color struct uses r, g, b members in RGB order

	// ASS alpha: 0x00 = opaque, 0xFF = transparent
	// CineCanvas alpha: 0xFF = opaque, 0x00 = transparent
	// Therefore, we must invert the alpha channel
	uint8_t cinema_alpha = 255 - alpha;

	// Format as RRGGBBAA for CineCanvas
	return (boost::format("%02X%02X%02X%02X")
		% static_cast<int>(color.r)
		% static_cast<int>(color.g)
		% static_cast<int>(color.b)
		% static_cast<int>(cinema_alpha)).str();
}

std::string CineCanvasSubtitleFormat::GenerateUUID() const {
	// Generate a simple UUID for SubtitleID
	// Full UUID implementation will use Boost.UUID in later phases
	// For now, use a placeholder format
	return "urn:uuid:00000000-0000-0000-0000-000000000000";
}

void CineCanvasSubtitleFormat::ParseFontAttributes(const AssStyle *style, wxXmlNode *fontNode) const {
	// Font attribute parsing will be implemented in later phases
	// This is a placeholder for the basic structure
}

void CineCanvasSubtitleFormat::ParseTextPosition(const AssDialogue *line, wxXmlNode *textNode) const {
	// Position parsing will be implemented in later phases
	// This is a placeholder for the basic structure
}

std::string CineCanvasSubtitleFormat::ConvertTimeToCineCanvas(const agi::Time &time, const agi::vfr::Framerate &fps) const {
	// Get time in milliseconds
	int ms = static_cast<int>(time);

	// For frame-accurate timing, convert through frames if we have a valid FPS
	if (fps.IsLoaded() && fps.FPS() > 0) {
		// Convert time to frame number
		int frame = fps.FrameAtTime(ms, agi::vfr::START);
		// Convert frame back to time for frame-accurate timing
		ms = fps.TimeAtFrame(frame, agi::vfr::START);
	}

	// Calculate time components
	int hours = ms / 3600000;
	ms %= 3600000;
	int minutes = ms / 60000;
	ms %= 60000;
	int seconds = ms / 1000;
	int milliseconds = ms % 1000;

	// Format as HH:MM:SS:mmm
	return (boost::format("%02d:%02d:%02d:%03d")
		% hours
		% minutes
		% seconds
		% milliseconds).str();
}

int CineCanvasSubtitleFormat::GetFadeTime(const AssDialogue *line, bool isFadeIn) const {
	// Default fade duration (20ms is DCP standard)
	// In future phases, this could parse ASS fade overrides (\fad or \fade tags)
	// and extract actual fade times from the dialogue line

	// For now, use 20ms standard
	// Configuration option will be added in Phase 2.3
	return 20;
}

agi::Time CineCanvasSubtitleFormat::ConvertTimeFromCineCanvas(const std::string &timeStr) const {
	// Parse CineCanvas time format: HH:MM:SS:mmm (colons throughout)
	int hours = 0, minutes = 0, seconds = 0, milliseconds = 0;

	// Use sscanf for simple parsing of the format
	if (sscanf(timeStr.c_str(), "%d:%d:%d:%d", &hours, &minutes, &seconds, &milliseconds) != 4) {
		// Try alternative format with period for milliseconds (HH:MM:SS.mmm)
		if (sscanf(timeStr.c_str(), "%d:%d:%d.%d", &hours, &minutes, &seconds, &milliseconds) != 4) {
			// If parsing fails, return 0
			return agi::Time(0);
		}
	}

	// Convert to total milliseconds
	int totalMs = hours * 3600000 + minutes * 60000 + seconds * 1000 + milliseconds;
	return agi::Time(totalMs);
}

int CineCanvasSubtitleFormat::ConvertAlignmentToASS(const std::string &vAlign, const std::string &hAlign) const {
	// ASS alignment codes use numpad layout:
	// 7 8 9  (top)
	// 4 5 6  (middle)
	// 1 2 3  (bottom)

	int base = 2; // Default to bottom-center

	// Determine vertical position
	if (vAlign == "top") {
		base = 8; // top row
	} else if (vAlign == "center") {
		base = 5; // middle row
	} else {
		base = 2; // bottom row (default)
	}

	// Adjust for horizontal alignment
	if (hAlign == "left") {
		return base - 1; // 1, 4, or 7
	} else if (hAlign == "right") {
		return base + 1; // 3, 6, or 9
	} else {
		return base; // center: 2, 5, or 8
	}
}
