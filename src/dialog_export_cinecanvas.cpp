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

/// @file dialog_export_cinecanvas.cpp
/// @see dialog_export_cinecanvas.h
/// @ingroup subtitle_io export

#include "dialog_export_cinecanvas.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "compat.h"
#include "format.h"
#include "options.h"

#include <libaegisub/fs.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/filepicker.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/valgen.h>

namespace {
	/// Regex for validating ISO 639-2 language codes (2 or 3 letter codes)
	const boost::regex language_code_regex("^[a-z]{2,3}$");

	/// Validator for ISO 639-2 language codes
	class LanguageCodeValidator final : public wxValidator {
		std::string *value;

		wxTextCtrl *GetCtrl() const { return dynamic_cast<wxTextCtrl*>(GetWindow()); }

		bool TransferToWindow() override {
			wxTextCtrl *ctrl = GetCtrl();
			if (!ctrl) return false;
			ctrl->SetValue(to_wx(*value));
			return true;
		}

		bool TransferFromWindow() override {
			wxTextCtrl *ctrl = GetCtrl();
			if (!ctrl) return false;
			*value = from_wx(ctrl->GetValue());
			return true;
		}

		bool Validate(wxWindow *parent) override {
			wxTextCtrl *ctrl = GetCtrl();
			if (!ctrl) return false;

			std::string code = from_wx(ctrl->GetValue());
			if (!regex_match(code, language_code_regex)) {
				wxMessageBox(
					_("Language code must be a valid ISO 639-2 code (2-3 lowercase letters, e.g., 'en', 'fr', 'deu')."),
					_("CineCanvas XML Export"),
					wxICON_EXCLAMATION | wxOK,
					parent
				);
				return false;
			}
			return true;
		}

		wxObject *Clone() const override { return new LanguageCodeValidator(*this); }

	public:
		LanguageCodeValidator(std::string *target) : value(target) { assert(target); }
		LanguageCodeValidator(LanguageCodeValidator const& other) : wxValidator(other), value(other.value) { }
	};

	/// Custom dialog that shows validation warnings
	class CineCanvasExportDialog final : public wxDialog {
		CineCanvasExportSettings &settings;
		const AssFile *file;
		wxStaticText *warning_text;

		void UpdateWarnings() {
			std::string warnings = settings.Validate(file);
			if (!warnings.empty()) {
				warning_text->SetLabel(to_wx(warnings));
				warning_text->Show();
			} else {
				warning_text->Hide();
			}
			Layout();
			Fit();
		}

	public:
		CineCanvasExportDialog(wxWindow *parent, CineCanvasExportSettings &s, const AssFile *f)
		: wxDialog(parent, -1, _("Export to CineCanvas XML"))
		, settings(s)
		, file(f)
		{
			// Frame Rate Selection
			wxString frame_rates[] = {
				_("23.976 fps (Cinema)"),
				_("24 fps (Cinema)"),
				_("25 fps (PAL)"),
				_("29.97 fps (NTSC)"),
				_("30 fps"),
				_("48 fps (HFR Cinema)"),
				_("50 fps (HFR PAL)"),
				_("59.94 fps (HFR NTSC)"),
				_("60 fps (HFR)")
			};
			wxComboBox *frame_rate_ctrl = new wxComboBox(this, -1, frame_rates[1],
				wxDefaultPosition, wxDefaultSize, 9, frame_rates, wxCB_DROPDOWN | wxCB_READONLY);

			// Movie Title
			wxTextCtrl *movie_title_ctrl = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxSize(300, -1));

			// Reel Number
			wxSpinCtrl *reel_number_ctrl = new wxSpinCtrl(this, -1, "1",
				wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 99);

			// Language Code
			wxTextCtrl *language_code_ctrl = new wxTextCtrl(this, -1, "en", wxDefaultPosition, wxSize(60, -1));

			// Font Size
			wxSpinCtrl *font_size_ctrl = new wxSpinCtrl(this, -1, "42",
				wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 20, 100);

			// Fade Duration
			wxSpinCtrl *fade_duration_ctrl = new wxSpinCtrl(this, -1, "20",
				wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 1000);

			// Font Reference
			wxCheckBox *include_font_check = new wxCheckBox(this, -1, _("Include font reference"));
			wxFilePickerCtrl *font_uri_ctrl = new wxFilePickerCtrl(this, -1, "",
				_("Select font file"), "TrueType Font (*.ttf)|*.ttf|OpenType Font (*.otf)|*.otf",
				wxDefaultPosition, wxDefaultSize, wxFLP_OPEN | wxFLP_FILE_MUST_EXIST | wxFLP_USE_TEXTCTRL);

			// Bind font reference checkbox to enable/disable font URI picker
			include_font_check->Bind(wxEVT_CHECKBOX, [=](wxCommandEvent &) {
				font_uri_ctrl->Enable(include_font_check->GetValue());
			});
			font_uri_ctrl->Enable(settings.include_font_reference);

			// Layout: DCP Metadata section
			auto metadata_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("DCP Metadata"));

			auto movie_title_row = new wxBoxSizer(wxHORIZONTAL);
			movie_title_row->Add(new wxStaticText(this, -1, _("Movie Title:")), 0, wxALIGN_CENTRE_VERTICAL | wxRIGHT, 12);
			movie_title_row->Add(movie_title_ctrl, 1, wxEXPAND);
			metadata_sizer->Add(movie_title_row, 0, wxEXPAND | (wxALL & ~wxTOP), 6);

			auto reel_number_row = new wxBoxSizer(wxHORIZONTAL);
			reel_number_row->Add(new wxStaticText(this, -1, _("Reel Number:")), 0, wxALIGN_CENTRE_VERTICAL | wxRIGHT, 12);
			reel_number_row->Add(reel_number_ctrl, 0, 0, 0);
			metadata_sizer->Add(reel_number_row, 0, wxEXPAND | (wxALL & ~wxTOP), 6);

			auto language_row = new wxBoxSizer(wxHORIZONTAL);
			language_row->Add(new wxStaticText(this, -1, _("Language (ISO 639-2):")), 0, wxALIGN_CENTRE_VERTICAL | wxRIGHT, 12);
			language_row->Add(language_code_ctrl, 0, 0, 0);
			metadata_sizer->Add(language_row, 0, wxEXPAND | (wxALL & ~wxTOP), 6);

			// Layout: Timing section
			auto timing_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Timing"));

			auto frame_rate_row = new wxBoxSizer(wxHORIZONTAL);
			frame_rate_row->Add(new wxStaticText(this, -1, _("Frame Rate:")), 0, wxALIGN_CENTRE_VERTICAL | wxRIGHT, 12);
			frame_rate_row->Add(frame_rate_ctrl, 1, wxEXPAND);
			timing_sizer->Add(frame_rate_row, 0, wxEXPAND | (wxALL & ~wxTOP), 6);

			auto fade_row = new wxBoxSizer(wxHORIZONTAL);
			fade_row->Add(new wxStaticText(this, -1, _("Fade Duration (ms):")), 0, wxALIGN_CENTRE_VERTICAL | wxRIGHT, 12);
			fade_row->Add(fade_duration_ctrl, 0, 0, 0);
			timing_sizer->Add(fade_row, 0, wxEXPAND | (wxALL & ~wxTOP), 6);

			// Layout: Typography section
			auto typography_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Typography"));

			auto font_size_row = new wxBoxSizer(wxHORIZONTAL);
			font_size_row->Add(new wxStaticText(this, -1, _("Font Size (pt):")), 0, wxALIGN_CENTRE_VERTICAL | wxRIGHT, 12);
			font_size_row->Add(font_size_ctrl, 0, 0, 0);
			typography_sizer->Add(font_size_row, 0, wxEXPAND | (wxALL & ~wxTOP), 6);

			typography_sizer->Add(include_font_check, 0, wxEXPAND | (wxALL & ~wxTOP), 6);

			auto font_uri_row = new wxBoxSizer(wxHORIZONTAL);
			font_uri_row->Add(new wxStaticText(this, -1, _("Font File:")), 0, wxALIGN_CENTRE_VERTICAL | wxRIGHT, 12);
			font_uri_row->Add(font_uri_ctrl, 1, wxEXPAND);
			typography_sizer->Add(font_uri_row, 0, wxEXPAND | (wxALL & ~wxTOP), 6);

			// Layout: Left and Right columns
			auto left_column = new wxBoxSizer(wxVERTICAL);
			left_column->Add(metadata_sizer, 0, wxEXPAND | wxBOTTOM, 6);
			left_column->Add(timing_sizer, 0, wxEXPAND, 0);

			auto right_column = new wxBoxSizer(wxVERTICAL);
			right_column->Add(typography_sizer, 0, wxEXPAND, 0);

			auto columns_sizer = new wxBoxSizer(wxHORIZONTAL);
			columns_sizer->Add(left_column, 1, wxRIGHT | wxEXPAND, 6);
			columns_sizer->Add(right_column, 1, wxEXPAND, 0);

			// Warning text (initially hidden)
			warning_text = new wxStaticText(this, -1, "");
			warning_text->SetForegroundColour(*wxRED);
			warning_text->Wrap(500);
			warning_text->Hide();

			// Buttons
			auto buttons_sizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL | wxHELP);

			// Main layout
			auto main_sizer = new wxBoxSizer(wxVERTICAL);
			main_sizer->Add(columns_sizer, 0, wxEXPAND | wxALL, 12);
			main_sizer->Add(warning_text, 0, wxEXPAND | (wxALL & ~wxTOP), 12);
			main_sizer->Add(buttons_sizer, 0, wxEXPAND | (wxALL & ~wxTOP), 12);

			SetSizerAndFit(main_sizer);
			CenterOnParent();

			// Set up validators
			frame_rate_ctrl->SetValidator(wxGenericValidator((int*)&settings.frame_rate));
			movie_title_ctrl->SetValidator(wxGenericValidator(&settings.movie_title));
			reel_number_ctrl->SetValidator(wxGenericValidator(&settings.reel_number));
			language_code_ctrl->SetValidator(LanguageCodeValidator(&settings.language_code));
			font_size_ctrl->SetValidator(wxGenericValidator(&settings.font_size));
			fade_duration_ctrl->SetValidator(wxGenericValidator(&settings.fade_duration));
			include_font_check->SetValidator(wxGenericValidator(&settings.include_font_reference));
			font_uri_ctrl->SetValidator(wxGenericValidator(&settings.font_uri));

			// Show initial warnings
			Bind(wxEVT_INIT_DIALOG, [=](wxInitDialogEvent &e) {
				e.Skip();
				UpdateWarnings();
			});

			// Update warnings when settings change
			frame_rate_ctrl->Bind(wxEVT_COMBOBOX, [=](wxCommandEvent &) { UpdateWarnings(); });
			movie_title_ctrl->Bind(wxEVT_TEXT, [=](wxCommandEvent &) { UpdateWarnings(); });
		}
	};

} // namespace

int ShowCineCanvasExportDialog(wxWindow *owner, CineCanvasExportSettings &s, const AssFile *file) {
	CineCanvasExportDialog d(owner, s, file);
	return d.ShowModal();
}

agi::vfr::Framerate CineCanvasExportSettings::GetFramerate() const {
	switch (frame_rate) {
		case FPS_23_976: return agi::vfr::Framerate(24000, 1001, false);
		case FPS_24:     return agi::vfr::Framerate(24, 1);
		case FPS_25:     return agi::vfr::Framerate(25, 1);
		case FPS_29_97:  return agi::vfr::Framerate(30000, 1001, false);
		case FPS_30:     return agi::vfr::Framerate(30, 1);
		case FPS_48:     return agi::vfr::Framerate(48, 1);
		case FPS_50:     return agi::vfr::Framerate(50, 1);
		case FPS_59_94:  return agi::vfr::Framerate(60000, 1001, false);
		case FPS_60:     return agi::vfr::Framerate(60, 1);
		default:         return agi::vfr::Framerate(24, 1);
	}
}

CineCanvasExportSettings::CineCanvasExportSettings(std::string const& prefix)
: prefix(prefix)
{
	// Load settings from options with defaults
	int fps_option = OPT_GET(prefix + "/Default Frame Rate")->GetInt();

	// Map the old simple FPS value to the new enum
	// Default was 24, so we map common values
	switch (fps_option) {
		case 23: frame_rate = FPS_23_976; break;
		case 24: frame_rate = FPS_24; break;
		case 25: frame_rate = FPS_25; break;
		case 29: frame_rate = FPS_29_97; break;
		case 30: frame_rate = FPS_30; break;
		case 48: frame_rate = FPS_48; break;
		case 50: frame_rate = FPS_50; break;
		case 59: frame_rate = FPS_59_94; break;
		case 60: frame_rate = FPS_60; break;
		default: frame_rate = FPS_24; break;
	}

	movie_title = OPT_GET(prefix + "/Movie Title")->GetString();
	reel_number = OPT_GET(prefix + "/Reel Number")->GetInt();
	language_code = OPT_GET(prefix + "/Language Code")->GetString();
	font_size = OPT_GET(prefix + "/Default Font Size")->GetInt();
	fade_duration = OPT_GET(prefix + "/Fade Duration")->GetInt();
	include_font_reference = OPT_GET(prefix + "/Include Font Reference")->GetBool();

	// Font URI is not in the original config, so we use an empty default
	font_uri = "";
}

void CineCanvasExportSettings::Save() const {
	// Map enum back to simple FPS value for storage
	int fps_value = 24;
	switch (frame_rate) {
		case FPS_23_976: fps_value = 23; break;
		case FPS_24:     fps_value = 24; break;
		case FPS_25:     fps_value = 25; break;
		case FPS_29_97:  fps_value = 29; break;
		case FPS_30:     fps_value = 30; break;
		case FPS_48:     fps_value = 48; break;
		case FPS_50:     fps_value = 50; break;
		case FPS_59_94:  fps_value = 59; break;
		case FPS_60:     fps_value = 60; break;
	}

	OPT_SET(prefix + "/Default Frame Rate")->SetInt(fps_value);
	OPT_SET(prefix + "/Movie Title")->SetString(movie_title);
	OPT_SET(prefix + "/Reel Number")->SetInt(reel_number);
	OPT_SET(prefix + "/Language Code")->SetString(language_code);
	OPT_SET(prefix + "/Default Font Size")->SetInt(font_size);
	OPT_SET(prefix + "/Fade Duration")->SetInt(fade_duration);
	OPT_SET(prefix + "/Include Font Reference")->SetBool(include_font_reference);
}

std::string CineCanvasExportSettings::Validate(const AssFile *file) const {
	if (!file) return "";

	std::vector<wxString> warnings;

	// Check for unsupported features
	bool has_animations = false;
	bool has_complex_effects = false;
	bool has_drawings = false;
	size_t subtitle_count = 0;
	size_t max_line_length = 0;

	for (auto const& line : file->Events) {
		if (line.Comment) continue;

		subtitle_count++;

		// Check for complex ASS features
		std::string text = line.Text.get();

		// Check for animations (\t, \move)
		if (text.find("\\t") != std::string::npos || text.find("\\move") != std::string::npos) {
			has_animations = true;
		}

		// Check for complex effects (\blur, \be, \fscx, \fscy, etc.)
		if (text.find("\\blur") != std::string::npos ||
		    text.find("\\be") != std::string::npos ||
		    text.find("\\fscx") != std::string::npos ||
		    text.find("\\fscy") != std::string::npos) {
			has_complex_effects = true;
		}

		// Check for vector drawings
		if (text.find("\\p") != std::string::npos) {
			has_drawings = true;
		}

		// Check line length (rough estimate, tags will be stripped)
		if (text.length() > max_line_length) {
			max_line_length = text.length();
		}
	}

	// Generate warnings
	if (subtitle_count > 500) {
		warnings.push_back(fmt_wx(
			_("Warning: File contains %d subtitles. DCP typically limits to ~500 per reel."),
			static_cast<int>(subtitle_count)
		));
	}

	if (has_animations) {
		warnings.push_back(_("Warning: Animations (\\t, \\move) will be lost in export."));
	}

	if (has_complex_effects) {
		warnings.push_back(_("Warning: Complex effects (\\blur, \\be, scaling) will be lost in export."));
	}

	if (has_drawings) {
		warnings.push_back(_("Warning: Vector drawings (\\p) are not supported and will be lost."));
	}

	if (max_line_length > 80) {
		warnings.push_back(_("Warning: Some lines are very long. Cinema subtitles typically use 40-50 characters per line."));
	}

	if (include_font_reference && font_uri.empty()) {
		warnings.push_back(_("Warning: Font reference enabled but no font file selected."));
	}

	// Color space warning (always show)
	warnings.push_back(_("Note: DCP uses XYZ color space. Color appearance may differ from ASS preview."));

	// Join warnings with newlines
	std::string result;
	for (size_t i = 0; i < warnings.size(); ++i) {
		result += from_wx(warnings[i]);
		if (i < warnings.size() - 1) result += "\n";
	}

	return result;
}
