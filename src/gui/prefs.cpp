#include "prefs.h"

#include <wx/choice.h>
#include <wx/config.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

namespace {

void load_one(wxConfigBase *cfg, const wxString &key, AntennaInput &in)
{
	in.channel = cfg->ReadLong(key + "/channel", in.channel);
	in.cc = cfg->ReadLong(key + "/cc", in.cc);
	in.wide = cfg->ReadBool(key + "/wide", in.wide);
}

void save_one(wxConfigBase *cfg, const wxString &key, const AntennaInput &in)
{
	cfg->Write(key + "/channel", static_cast<long>(in.channel));
	cfg->Write(key + "/cc", static_cast<long>(in.cc));
	cfg->Write(key + "/wide", in.wide);
}

} // namespace

Settings load_settings()
{
	Settings s;
	wxConfigBase *cfg = wxConfigBase::Get();
	load_one(cfg, "midi_input/volume", s.volume);
	load_one(cfg, "midi_input/pitch", s.pitch);
	return s;
}

void save_settings(const Settings &s)
{
	wxConfigBase *cfg = wxConfigBase::Get();
	save_one(cfg, "midi_input/volume", s.volume);
	save_one(cfg, "midi_input/pitch", s.pitch);
	cfg->Flush();
}

// One antenna's row of controls: channel, controller, bit width.
struct PrefsDialog::Row {
	wxSpinCtrl *channel;
	wxSpinCtrl *cc;
	wxChoice *width;

	Row(wxWindow *parent, wxSizer *grid, const wxString &label, const AntennaInput &in)
	{
		grid->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTRE_VERTICAL);
		channel = new wxSpinCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                         wxDefaultSize, wxSP_ARROW_KEYS, 0, 15, in.channel);
		cc = new wxSpinCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
		                    wxDefaultSize, wxSP_ARROW_KEYS, 0, 127, in.cc);
		width = new wxChoice(parent, wxID_ANY);
		width->Append("7-bit");
		width->Append("14-bit");
		width->SetSelection(in.wide ? 1 : 0);
		grid->Add(channel);
		grid->Add(cc);
		grid->Add(width);
	}

	AntennaInput get() const
	{
		return { channel->GetValue(), cc->GetValue(), width->GetSelection() == 1 };
	}
};

PrefsDialog::PrefsDialog(wxWindow *parent, const Settings &settings)
	: wxDialog(parent, wxID_ANY, "Preferences")
{
	auto *box = new wxStaticBoxSizer(wxVERTICAL, this, "MIDI Input");
	wxWindow *bp = box->GetStaticBox();

	auto *grid = new wxFlexGridSizer(4, 6, 6);
	grid->Add(new wxStaticText(bp, wxID_ANY, ""));
	grid->Add(new wxStaticText(bp, wxID_ANY, "Channel"));
	grid->Add(new wxStaticText(bp, wxID_ANY, "Controller"));
	grid->Add(new wxStaticText(bp, wxID_ANY, "Resolution"));
	m_volume = new Row(bp, grid, "Volume", settings.volume);
	m_pitch = new Row(bp, grid, "Pitch", settings.pitch);
	box->Add(grid, 0, wxALL, 8);

	auto *top = new wxBoxSizer(wxVERTICAL);
	top->Add(box, 0, wxEXPAND | wxALL, 10);
	top->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
	SetSizerAndFit(top);
}

Settings PrefsDialog::GetSettings() const
{
	Settings s;
	s.volume = m_volume->get();
	s.pitch = m_pitch->get();
	return s;
}
