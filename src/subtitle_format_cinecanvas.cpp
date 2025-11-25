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

#include <libaegisub/ass/time.h>
#include <libaegisub/color.h>
#include <libaegisub/fs.h>

#include <wx/xml/xml.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>

#include <algorithm>
#include <map>
#include <regex>

DEFINE_EXCEPTION(CineCanvasParseError, SubtitleFormatParseError);

/// Helper struct to hold font properties parsed from CineCanvas XML
struct CineCanvasFontProps {
	std::string fontName = "Arial";
	int fontSize = 42;
	bool bold = false;
	bool italic = false;
	agi::Color primaryColor{255, 255, 255};
	agi::Color outlineColor{0, 0, 0};
	double outlineWidth = 2.0;
	uint8_t primaryAlpha = 0;  // ASS format: 0=opaque, 255=transparent
	uint8_t outlineAlpha = 0;
};

/// Helper struct for a styled text segment
struct StyledSegment {
	std::string text;
	bool bold = false;
	bool italic = false;
};

/// Parse ASS text with override tags into styled segments
/// @param text ASS dialogue text with override tags like {\b1}bold{\b0}
/// @param defaultBold Default bold state from style
/// @param defaultItalic Default italic state from style
/// @return Vector of styled segments
static std::vector<StyledSegment> ParseStyledSegments(const std::string &text, bool defaultBold, bool defaultItalic) {
	std::vector<StyledSegment> segments;

	bool currentBold = defaultBold;
	bool currentItalic = defaultItalic;
	std::string currentText;

	size_t i = 0;
	while (i < text.length()) {
		// Check for override tag block
		if (text[i] == '{') {
			// Save current segment if it has text
			if (!currentText.empty()) {
				segments.push_back({currentText, currentBold, currentItalic});
				currentText.clear();
			}

			// Find end of tag block
			size_t tagEnd = text.find('}', i);
			if (tagEnd == std::string::npos) {
				// Malformed - no closing brace, skip
				++i;
				continue;
			}

			// Parse tags within the block
			std::string tagBlock = text.substr(i + 1, tagEnd - i - 1);

			// Look for \b0 or \b1
			size_t bpos = 0;
			while ((bpos = tagBlock.find("\\b", bpos)) != std::string::npos) {
				if (bpos + 2 < tagBlock.length()) {
					char next = tagBlock[bpos + 2];
					if (next == '0') {
						currentBold = false;
					} else if (next == '1') {
						currentBold = true;
					}
				}
				bpos += 2;
			}

			// Look for \i0 or \i1
			size_t ipos = 0;
			while ((ipos = tagBlock.find("\\i", ipos)) != std::string::npos) {
				if (ipos + 2 < tagBlock.length()) {
					char next = tagBlock[ipos + 2];
					if (next == '0') {
						currentItalic = false;
					} else if (next == '1') {
						currentItalic = true;
					}
				}
				ipos += 2;
			}

			i = tagEnd + 1;
		} else {
			currentText += text[i];
			++i;
		}
	}

	// Add final segment
	if (!currentText.empty()) {
		segments.push_back({currentText, currentBold, currentItalic});
	}

	return segments;
}

/// Convert CineCanvas RRGGBBAA color string to agi::Color
/// @param colorStr 8-character hex string (RRGGBBAA)
/// @param outAlpha Optional pointer to receive alpha value in ASS format
/// @return agi::Color with RGB values
static agi::Color ParseCineCanvasColor(const std::string &colorStr, uint8_t *outAlpha = nullptr) {
	if (colorStr.length() < 6) {
		if (outAlpha) *outAlpha = 0;  // Fully opaque in ASS terms
		return agi::Color(255, 255, 255);  // Default white
	}

	try {
		int r = std::stoi(colorStr.substr(0, 2), nullptr, 16);
		int g = std::stoi(colorStr.substr(2, 2), nullptr, 16);
		int b = std::stoi(colorStr.substr(4, 2), nullptr, 16);

		if (outAlpha && colorStr.length() >= 8) {
			// CineCanvas alpha: FF = opaque, 00 = transparent
			// ASS alpha: 00 = opaque, FF = transparent
			int cinema_alpha = std::stoi(colorStr.substr(6, 2), nullptr, 16);
			*outAlpha = static_cast<uint8_t>(255 - cinema_alpha);
		} else if (outAlpha) {
			*outAlpha = 0;  // Fully opaque in ASS terms
		}

		return agi::Color(r, g, b);
	} catch (...) {
		if (outAlpha) *outAlpha = 0;
		return agi::Color(255, 255, 255);
	}
}

/// Parse font properties from a CineCanvas Font XML node
/// @param fontNode The Font XML node
/// @return CineCanvasFontProps filled with parsed values
static CineCanvasFontProps ParseFontNode(wxXmlNode *fontNode) {
	CineCanvasFontProps props;

	if (!fontNode) return props;

	// Parse Size
	wxString sizeStr = fontNode->GetAttribute("Size", "42");
	long size = 42;
	sizeStr.ToLong(&size);
	props.fontSize = static_cast<int>(size);

	// Parse Weight
	wxString weightStr = fontNode->GetAttribute("Weight", "normal");
	props.bold = (weightStr.Lower() == "bold");

	// Parse Italic
	wxString italicStr = fontNode->GetAttribute("Italic", "no");
	props.italic = (italicStr.Lower() == "yes");

	// Parse Color (RRGGBBAA)
	wxString colorStr = fontNode->GetAttribute("Color", "FFFFFFFF");
	props.primaryColor = ParseCineCanvasColor(from_wx(colorStr), &props.primaryAlpha);

	// Parse Effect and EffectColor
	wxString effectStr = fontNode->GetAttribute("Effect", "none");
	if (effectStr.Lower() == "border") {
		props.outlineWidth = 2.0;
		wxString effectColorStr = fontNode->GetAttribute("EffectColor", "FF000000");
		props.outlineColor = ParseCineCanvasColor(from_wx(effectColorStr), &props.outlineAlpha);
	} else if (effectStr.Lower() == "shadow") {
		props.outlineWidth = 0.0;
	} else {
		props.outlineWidth = 0.0;
	}

	return props;
}

/// Extract effective font properties from a dialogue line
/// @param line The dialogue line
/// @param style The style assigned to this line (may be nullptr)
/// @return Effective font properties (style + override tags)
static CineCanvasFontProps GetEffectiveFontProps(const AssDialogue *line, const AssStyle *style) {
	CineCanvasFontProps props;

	// Start with style properties (or defaults if no style)
	if (style) {
		props.fontName = style->font;
		props.fontSize = static_cast<int>(style->fontsize);
		props.bold = style->bold;
		props.italic = style->italic;
		props.primaryColor = style->primary;
		props.outlineColor = style->outline;
		props.outlineWidth = style->outline_w;
	}

	// Parse override tags from the text for font name, size, color (not bold/italic)
	// Bold and italic are handled per-segment in WriteSubtitle using ParseStyledSegments
	std::string text = line->Text.get();

	// Variables for regex parsing
	std::smatch match;
	std::string::const_iterator searchStart;

	// Parse \fn (font name): \fnArial
	std::regex fontNameRegex(R"(\\fn([^\\}]+))");
	searchStart = text.cbegin();
	while (std::regex_search(searchStart, text.cend(), match, fontNameRegex)) {
		props.fontName = match[1].str();
		searchStart = match.suffix().first;
	}

	// Parse \fs (font size): \fs42
	std::regex fontSizeRegex(R"(\\fs(\d+))");
	searchStart = text.cbegin();
	while (std::regex_search(searchStart, text.cend(), match, fontSizeRegex)) {
		props.fontSize = std::stoi(match[1].str());
		searchStart = match.suffix().first;
	}

	// Parse \1c (primary color): \1c&HFFFFFF& or \c&HFFFFFF&
	// ASS color format: &HBBGGRR& (BGR order)
	std::regex colorRegex(R"(\\1?c&H([0-9A-Fa-f]{6})&?)");
	searchStart = text.cbegin();
	while (std::regex_search(searchStart, text.cend(), match, colorRegex)) {
		std::string hex = match[1].str();
		// Parse BGR
		int b = std::stoi(hex.substr(0, 2), nullptr, 16);
		int g = std::stoi(hex.substr(2, 2), nullptr, 16);
		int r = std::stoi(hex.substr(4, 2), nullptr, 16);
		props.primaryColor = agi::Color(r, g, b);
		searchStart = match.suffix().first;
	}

	// Parse \3c (outline color): \3c&H000000&
	std::regex outlineColorRegex(R"(\\3c&H([0-9A-Fa-f]{6})&?)");
	searchStart = text.cbegin();
	while (std::regex_search(searchStart, text.cend(), match, outlineColorRegex)) {
		std::string hex = match[1].str();
		int b = std::stoi(hex.substr(0, 2), nullptr, 16);
		int g = std::stoi(hex.substr(2, 2), nullptr, 16);
		int r = std::stoi(hex.substr(4, 2), nullptr, 16);
		props.outlineColor = agi::Color(r, g, b);
		searchStart = match.suffix().first;
	}

	// Parse \1a (primary alpha): \1a&HFF&
	std::regex alphaRegex(R"(\\1?a&H([0-9A-Fa-f]{2})&?)");
	searchStart = text.cbegin();
	while (std::regex_search(searchStart, text.cend(), match, alphaRegex)) {
		props.primaryAlpha = std::stoi(match[1].str(), nullptr, 16);
		searchStart = match.suffix().first;
	}

	return props;
}

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
	// Load default ASS structure
	target->LoadDefault(false);

	// Load and validate XML
	wxXmlDocument doc;
	if (!doc.Load(filename.wstring()))
		throw CineCanvasParseError("Failed to load CineCanvas XML file");

	wxXmlNode *root = doc.GetRoot();
	if (!root || root->GetName() != "DCSubtitle")
		throw CineCanvasParseError("Invalid CineCanvas file: missing DCSubtitle root element");

	// Parse metadata from root element children
	std::string movieTitle;
	std::string language;
	wxXmlNode *containerFontNode = nullptr;

	for (wxXmlNode *child = root->GetChildren(); child; child = child->GetNext()) {
		wxString nodeName = child->GetName();
		if (nodeName == "MovieTitle") {
			movieTitle = from_wx(child->GetNodeContent());
		} else if (nodeName == "Language") {
			language = from_wx(child->GetNodeContent());
		} else if (nodeName == "Font") {
			// Remember the first/main Font container node
			if (!containerFontNode) {
				containerFontNode = child;
			}
		}
	}

	// Store metadata in ASS script info
	if (!movieTitle.empty()) {
		target->SetScriptInfo("Title", movieTitle);
	}
	if (!language.empty()) {
		// Store language in a custom field (ASS doesn't have a standard language field)
		target->SetScriptInfo("Language", language);
	}

	// Parse font properties from container Font node and create a CineCanvas style
	CineCanvasFontProps containerFontProps;
	if (containerFontNode) {
		containerFontProps = ParseFontNode(containerFontNode);
	}

	// Create or update the "CineCanvas" style with parsed font properties
	// First, remove the Default style that was created by LoadDefault
	for (auto it = target->Styles.begin(); it != target->Styles.end(); ) {
		if (it->name == "Default") {
			it = target->Styles.erase(it);
		} else {
			++it;
		}
	}

	// Create the CineCanvas style
	auto *style = new AssStyle();
	style->name = "CineCanvas";
	style->font = containerFontProps.fontName;
	style->fontsize = containerFontProps.fontSize;
	style->bold = containerFontProps.bold;
	style->italic = containerFontProps.italic;
	style->primary = containerFontProps.primaryColor;
	style->outline = containerFontProps.outlineColor;
	style->outline_w = containerFontProps.outlineWidth;
	style->alignment = 2;  // Bottom center (default for subtitles)
	style->Margin = {10, 10, 10};  // Default margins
	style->UpdateData();
	target->Styles.push_back(*style);

	// Process all Font nodes to find Subtitle elements
	for (wxXmlNode *fontChild = root->GetChildren(); fontChild; fontChild = fontChild->GetNext()) {
		if (fontChild->GetName() != "Font") continue;

		// Parse this Font node's properties (may differ from container)
		CineCanvasFontProps fontProps = ParseFontNode(fontChild);

		// Iterate through children - could be Subtitle elements or nested Font elements
		for (wxXmlNode *subNode = fontChild->GetChildren(); subNode; subNode = subNode->GetNext()) {
			if (subNode->GetName() == "Subtitle") {
				// Parse TimeIn and TimeOut attributes
				wxString timeInStr = subNode->GetAttribute("TimeIn", "00:00:00:000");
				wxString timeOutStr = subNode->GetAttribute("TimeOut", "00:00:05:000");

				agi::Time timeIn = ConvertTimeFromCineCanvas(from_wx(timeInStr));
				agi::Time timeOut = ConvertTimeFromCineCanvas(from_wx(timeOutStr));

				// Parse FadeUpTime and FadeDownTime
				wxString fadeUpStr = subNode->GetAttribute("FadeUpTime", "0");
				wxString fadeDownStr = subNode->GetAttribute("FadeDownTime", "0");
				long fadeUp = 0, fadeDown = 0;
				fadeUpStr.ToLong(&fadeUp);
				fadeDownStr.ToLong(&fadeDown);

				// Look for nested Font element inside Subtitle (for inline font overrides)
				wxXmlNode *inlineFontNode = nullptr;
				for (wxXmlNode *subChild = subNode->GetChildren(); subChild; subChild = subChild->GetNext()) {
					if (subChild->GetName() == "Font") {
						inlineFontNode = subChild;
						break;
					}
				}

				// Determine which node contains Text elements
				wxXmlNode *textContainer = inlineFontNode ? inlineFontNode : subNode;

				// Collect all Text elements with their VPosition for proper ordering
				struct TextLine {
					double vpos;
					std::string text;
					std::string valign;
					std::string halign;
				};
				std::vector<TextLine> textLines;

				for (wxXmlNode *textNode = textContainer->GetChildren(); textNode; textNode = textNode->GetNext()) {
					if (textNode->GetName() == "Text") {
						wxString vposStr = textNode->GetAttribute("VPosition", "10.0");
						double vpos = 10.0;
						vposStr.ToDouble(&vpos);

						wxString valign = textNode->GetAttribute("VAlign", "bottom");
						wxString halign = textNode->GetAttribute("HAlign", "center");

						wxString content = textNode->GetNodeContent();
						if (!content.IsEmpty()) {
							textLines.push_back({vpos, from_wx(content), from_wx(valign), from_wx(halign)});
						}
					}
				}

				// Also check for Text directly under Subtitle (without Font wrapper)
				if (textLines.empty()) {
					for (wxXmlNode *textNode = subNode->GetChildren(); textNode; textNode = textNode->GetNext()) {
						if (textNode->GetName() == "Text") {
							wxString vposStr = textNode->GetAttribute("VPosition", "10.0");
							double vpos = 10.0;
							vposStr.ToDouble(&vpos);

							wxString valign = textNode->GetAttribute("VAlign", "bottom");
							wxString halign = textNode->GetAttribute("HAlign", "center");

							wxString content = textNode->GetNodeContent();
							if (!content.IsEmpty()) {
								textLines.push_back({vpos, from_wx(content), from_wx(valign), from_wx(halign)});
							}
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

				// Prepend fade tag if fades are specified
				std::string finalText;
				if (fadeUp > 0 || fadeDown > 0) {
					finalText = "{\\fad(" + std::to_string(fadeUp) + "," + std::to_string(fadeDown) + ")}" + combinedText;
				} else {
					finalText = combinedText;
				}

				// Create AssDialogue with CineCanvas style
				auto *diag = new AssDialogue;
				diag->Start = timeIn;
				diag->End = timeOut;
				diag->Text = finalText;
				diag->Style = "CineCanvas";

				target->Events.push_back(*diag);
			}
		}
	}

	// Ensure file has at least one event
	if (target->Events.empty())
		target->Events.push_back(*new AssDialogue);
}

void CineCanvasSubtitleFormat::WriteFile(const AssFile *src, agi::fs::path const& filename,
                                         agi::vfr::Framerate const& fps, const char *encoding) const {
	// Initialize export settings from filename and video framerate
	CineCanvasExportSettings settings(filename, fps);

	// Get the export framerate from settings (which may have been auto-detected or will be user-selected)
	agi::vfr::Framerate export_fps = settings.GetFramerate();

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

	// Build style lookup map
	std::map<std::string, const AssStyle*> styleMap;
	for (auto const& style : src->Styles) {
		styleMap[style.name] = &style;
	}

	// Get default style for container Font node
	const AssStyle *defaultStyle = nullptr;
	auto it = styleMap.find("Default");
	if (it != styleMap.end()) {
		defaultStyle = it->second;
	} else if (!src->Styles.empty()) {
		defaultStyle = &src->Styles.front();
	}

	// Create container Font node with default/fallback properties
	// Per-line differences will use inline Font elements
	wxXmlNode *fontNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Font");
	fontNode->AddAttribute("Id", "Font1");

	// Set container defaults (will be overridden per-line as needed)
	if (defaultStyle) {
		fontNode->AddAttribute("Script", to_wx(defaultStyle->font));  // Font family name
		fontNode->AddAttribute("Size", wxString::Format("%d", static_cast<int>(defaultStyle->fontsize)));
		fontNode->AddAttribute("Weight", defaultStyle->bold ? "bold" : "normal");
		fontNode->AddAttribute("Italic", defaultStyle->italic ? "yes" : "no");
		fontNode->AddAttribute("Color", to_wx(ConvertColorToRGBA(defaultStyle->primary, 0)));

		if (defaultStyle->outline_w > 0) {
			fontNode->AddAttribute("Effect", "border");
			fontNode->AddAttribute("EffectColor", to_wx(ConvertColorToRGBA(defaultStyle->outline, 0)));
		} else {
			fontNode->AddAttribute("Effect", "none");
			fontNode->AddAttribute("EffectColor", "FF000000");
		}
	} else {
		fontNode->AddAttribute("Script", "Arial");  // Default font family
		fontNode->AddAttribute("Size", "42");
		fontNode->AddAttribute("Weight", "normal");
		fontNode->AddAttribute("Italic", "no");
		fontNode->AddAttribute("Color", "FFFFFFFF");
		fontNode->AddAttribute("Effect", "border");
		fontNode->AddAttribute("EffectColor", "FF000000");
	}

	root->AddChild(fontNode);

	// Write subtitle entries with per-line style lookup
	int spotNumber = 1;
	for (auto const& line : copy.Events) {
		if (!line.Comment) {
			// Look up the style for this line
			const AssStyle *lineStyle = nullptr;
			auto styleIt = styleMap.find(line.Style.get());
			if (styleIt != styleMap.end()) {
				lineStyle = styleIt->second;
			} else {
				lineStyle = defaultStyle;  // Fall back to default
			}
			WriteSubtitle(fontNode, &line, lineStyle, spotNumber++, export_fps, settings);
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
	// Note: We do NOT call StripTags here - tags are preserved so WriteSubtitle
	// can extract \fad fade times from each line before stripping
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
                                             const AssStyle *style, int spotNumber,
                                             const agi::vfr::Framerate &fps,
                                             const CineCanvasExportSettings &settings) const {
	// Get effective font properties (style + override tags)
	CineCanvasFontProps fontProps = GetEffectiveFontProps(line, style);

	// Create Subtitle element
	wxXmlNode *subtitleNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Subtitle");
	subtitleNode->AddAttribute("SpotNumber", wxString::Format("%d", spotNumber));

	// Convert timing to CineCanvas format with frame-accurate timing
	std::string timeIn = ConvertTimeToCineCanvas(line->Start, fps);
	std::string timeOut = ConvertTimeToCineCanvas(line->End, fps);

	subtitleNode->AddAttribute("TimeIn", to_wx(timeIn));
	subtitleNode->AddAttribute("TimeOut", to_wx(timeOut));

	// Extract fade times from ASS \fad tags (must be done before stripping tags)
	int fadeIn = GetFadeTime(line, true);
	int fadeOut = GetFadeTime(line, false);
	subtitleNode->AddAttribute("FadeUpTime", wxString::Format("%d", fadeIn));
	subtitleNode->AddAttribute("FadeDownTime", wxString::Format("%d", fadeOut));

	fontNode->AddChild(subtitleNode);

	// Get the raw text with override tags
	std::string rawText = line->Text.get();

	// Get default bold/italic from style
	bool defaultBold = style ? style->bold : false;
	bool defaultItalic = style ? style->italic : false;

	// Split raw text on \N (preserving override tags)
	std::vector<std::string> rawLines;
	size_t pos = 0;
	size_t prev = 0;
	while ((pos = rawText.find("\\N", prev)) != std::string::npos) {
		rawLines.push_back(rawText.substr(prev, pos - prev));
		prev = pos + 2;
	}
	// Also check for lowercase \n
	if (rawLines.empty()) {
		prev = 0;
		while ((pos = rawText.find("\\n", prev)) != std::string::npos) {
			rawLines.push_back(rawText.substr(prev, pos - prev));
			prev = pos + 2;
		}
	}
	rawLines.push_back(rawText.substr(prev));

	// Base VPosition for bottom line, and line spacing
	const double baseVPosition = 10.0;
	const double lineSpacing = 6.5;

	// Process each line
	int numLines = static_cast<int>(rawLines.size());
	int validLineIndex = 0;

	// First pass: count non-empty lines for positioning
	int nonEmptyLines = 0;
	for (const auto& rawLine : rawLines) {
		auto segments = ParseStyledSegments(rawLine, defaultBold, defaultItalic);
		std::string lineText;
		for (const auto& seg : segments) {
			lineText += seg.text;
		}
		// Trim whitespace
		size_t start = lineText.find_first_not_of(" \t");
		if (start != std::string::npos) {
			nonEmptyLines++;
		}
	}

	if (nonEmptyLines == 0) nonEmptyLines = 1;

	// Second pass: create XML elements
	for (int lineIdx = 0; lineIdx < numLines; ++lineIdx) {
		const std::string& rawLine = rawLines[lineIdx];

		// Parse this line into styled segments
		auto segments = ParseStyledSegments(rawLine, defaultBold, defaultItalic);

		// Combine segment texts to check if line is empty
		std::string lineText;
		for (const auto& seg : segments) {
			lineText += seg.text;
		}

		// Trim whitespace
		size_t start = lineText.find_first_not_of(" \t");
		if (start == std::string::npos) {
			continue;  // Skip empty lines
		}
		size_t end = lineText.find_last_not_of(" \t");
		lineText = lineText.substr(start, end - start + 1);

		// Check if all segments have the same styling
		bool allSameStyle = true;
		bool firstBold = segments.empty() ? defaultBold : segments[0].bold;
		bool firstItalic = segments.empty() ? defaultItalic : segments[0].italic;
		for (const auto& seg : segments) {
			if (seg.bold != firstBold || seg.italic != firstItalic) {
				allSameStyle = false;
				break;
			}
		}

		// Calculate VPosition for this line
		double vpos = baseVPosition + (nonEmptyLines - 1 - validLineIndex) * lineSpacing;
		validLineIndex++;

		if (allSameStyle) {
			// Simple case: uniform styling for the line
			// Create Font element with this line's style
			wxXmlNode *lineFontNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Font");
			lineFontNode->AddAttribute("Script", to_wx(fontProps.fontName));
			lineFontNode->AddAttribute("Size", wxString::Format("%d", fontProps.fontSize));
			lineFontNode->AddAttribute("Weight", firstBold ? "bold" : "normal");
			lineFontNode->AddAttribute("Italic", firstItalic ? "yes" : "no");
			lineFontNode->AddAttribute("Color", to_wx(ConvertColorToRGBA(fontProps.primaryColor, fontProps.primaryAlpha)));

			if (fontProps.outlineWidth > 0) {
				lineFontNode->AddAttribute("Effect", "border");
				lineFontNode->AddAttribute("EffectColor", to_wx(ConvertColorToRGBA(fontProps.outlineColor, 0)));
			} else {
				lineFontNode->AddAttribute("Effect", "none");
			}

			subtitleNode->AddChild(lineFontNode);

			wxXmlNode *textNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Text");
			textNode->AddAttribute("VAlign", "bottom");
			textNode->AddAttribute("HAlign", "center");
			textNode->AddAttribute("VPosition", wxString::Format("%.1f", vpos));
			textNode->AddAttribute("HPosition", "0.0");
			textNode->AddAttribute("Direction", "horizontal");

			lineFontNode->AddChild(textNode);
			textNode->AddChild(new wxXmlNode(wxXML_TEXT_NODE, "", to_wx(lineText)));
		} else {
			// Mixed styling: need inline Font elements for each segment
			// Create base Font element with default (normal) style
			wxXmlNode *lineFontNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Font");
			lineFontNode->AddAttribute("Script", to_wx(fontProps.fontName));
			lineFontNode->AddAttribute("Size", wxString::Format("%d", fontProps.fontSize));
			lineFontNode->AddAttribute("Weight", "normal");
			lineFontNode->AddAttribute("Italic", "no");
			lineFontNode->AddAttribute("Color", to_wx(ConvertColorToRGBA(fontProps.primaryColor, fontProps.primaryAlpha)));

			if (fontProps.outlineWidth > 0) {
				lineFontNode->AddAttribute("Effect", "border");
				lineFontNode->AddAttribute("EffectColor", to_wx(ConvertColorToRGBA(fontProps.outlineColor, 0)));
			} else {
				lineFontNode->AddAttribute("Effect", "none");
			}

			subtitleNode->AddChild(lineFontNode);

			wxXmlNode *textNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Text");
			textNode->AddAttribute("VAlign", "bottom");
			textNode->AddAttribute("HAlign", "center");
			textNode->AddAttribute("VPosition", wxString::Format("%.1f", vpos));
			textNode->AddAttribute("HPosition", "0.0");
			textNode->AddAttribute("Direction", "horizontal");

			lineFontNode->AddChild(textNode);

			// Add each segment with appropriate inline Font if styled differently
			for (const auto& seg : segments) {
				std::string segText = seg.text;
				// Trim only leading whitespace from first segment, trailing from last
				// But preserve internal spacing
				if (segText.empty()) continue;

				if (seg.bold || seg.italic) {
					// Need inline Font element for styled text
					wxXmlNode *inlineFont = new wxXmlNode(wxXML_ELEMENT_NODE, "Font");
					if (seg.bold) {
						inlineFont->AddAttribute("Weight", "bold");
					}
					if (seg.italic) {
						inlineFont->AddAttribute("Italic", "yes");
					}
					textNode->AddChild(inlineFont);
					inlineFont->AddChild(new wxXmlNode(wxXML_TEXT_NODE, "", to_wx(segText)));
				} else {
					// Normal text - just add as text node
					textNode->AddChild(new wxXmlNode(wxXML_TEXT_NODE, "", to_wx(segText)));
				}
			}
		}
	}

	// If no lines were added (empty subtitle), add placeholder
	if (validLineIndex == 0) {
		wxXmlNode *lineFontNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Font");
		lineFontNode->AddAttribute("Script", to_wx(fontProps.fontName));
		lineFontNode->AddAttribute("Size", wxString::Format("%d", fontProps.fontSize));
		lineFontNode->AddAttribute("Weight", "normal");
		lineFontNode->AddAttribute("Italic", "no");
		lineFontNode->AddAttribute("Color", to_wx(ConvertColorToRGBA(fontProps.primaryColor, fontProps.primaryAlpha)));
		lineFontNode->AddAttribute("Effect", fontProps.outlineWidth > 0 ? "border" : "none");
		if (fontProps.outlineWidth > 0) {
			lineFontNode->AddAttribute("EffectColor", to_wx(ConvertColorToRGBA(fontProps.outlineColor, 0)));
		}
		subtitleNode->AddChild(lineFontNode);

		wxXmlNode *textNode = new wxXmlNode(wxXML_ELEMENT_NODE, "Text");
		textNode->AddAttribute("VAlign", "bottom");
		textNode->AddAttribute("HAlign", "center");
		textNode->AddAttribute("VPosition", wxString::Format("%.1f", baseVPosition));
		textNode->AddAttribute("HPosition", "0.0");
		textNode->AddAttribute("Direction", "horizontal");
		lineFontNode->AddChild(textNode);
		textNode->AddChild(new wxXmlNode(wxXML_TEXT_NODE, "", ""));
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
	// Parse \fad(fadeIn,fadeOut) tag from dialogue text
	// Returns fade time in milliseconds, or 0 if no fade tag present
	std::string text = line->Text.get();

	// Look for \fad(fadeIn,fadeOut) pattern
	size_t fadPos = text.find("\\fad(");
	if (fadPos == std::string::npos) {
		// Also check for \fade( which is the extended format
		fadPos = text.find("\\fade(");
	}

	if (fadPos != std::string::npos) {
		size_t startParen = text.find('(', fadPos);
		size_t endParen = text.find(')', startParen);

		if (startParen != std::string::npos && endParen != std::string::npos) {
			std::string params = text.substr(startParen + 1, endParen - startParen - 1);

			// Parse the fade values - \fad(fadeIn,fadeOut)
			int fadeIn = 0, fadeOut = 0;
			if (sscanf(params.c_str(), "%d,%d", &fadeIn, &fadeOut) >= 2) {
				return isFadeIn ? fadeIn : fadeOut;
			} else if (sscanf(params.c_str(), "%d", &fadeIn) == 1) {
				// Single value - use for both
				return fadeIn;
			}
		}
	}

	// No fade tag found - return 0 (no fade)
	return 0;
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
