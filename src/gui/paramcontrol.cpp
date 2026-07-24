#include "paramcontrol.h"

#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <cmath>

// A slider carries integer positions, so map the display range onto
// [0, steps] at the parameter's finest resolution.
static int steps_for(const theremini_param *p, double increment)
{
	return static_cast<int>(std::lround((p->max - p->min) / increment));
}

ParamControl::ParamControl(wxWindow *parent, const theremini_param *param)
	: wxPanel(parent), m_param(param)
{
	auto *row = new wxBoxSizer(wxHORIZONTAL);

	auto *label = new wxStaticText(this, wxID_ANY, param->label ? param->label : param->name);
	label->SetMinSize(wxSize(140, -1));
	row->Add(label, 0, wxALIGN_CENTRE_VERTICAL | wxRIGHT, 6);

	switch (param->kind) {
	case THEREMINI_NUMERIC: {
		m_increment = std::pow(10.0, -param->digits);
		const int steps = steps_for(param, m_increment);

		m_slider = new wxSlider(this, wxID_ANY, 0, 0, steps > 0 ? steps : 1);
		m_spin = new wxSpinCtrlDouble(this, wxID_ANY, wxEmptyString,
		                              wxDefaultPosition, wxSize(110, -1),
		                              wxSP_ARROW_KEYS, param->min, param->max,
		                              param->min, m_increment);
		m_spin->SetDigits(static_cast<unsigned>(param->digits));

		row->Add(m_slider, 1, wxALIGN_CENTRE_VERTICAL | wxRIGHT, 6);
		row->Add(m_spin, 0, wxALIGN_CENTRE_VERTICAL);

		m_slider->Bind(wxEVT_SLIDER, &ParamControl::OnSlider, this);
		m_spin->Bind(wxEVT_SPINCTRLDOUBLE, &ParamControl::OnSpin, this);
		break;
	}
	case THEREMINI_ENUM: {
		m_choice = new wxChoice(this, wxID_ANY);
		for (int i = 0; i < param->value_count; i++) {
			m_choice->Append(param->values[i]);
		}
		m_choice->SetSelection(0);
		row->Add(m_choice, 1, wxALIGN_CENTRE_VERTICAL);
		break;
	}
	case THEREMINI_TEXT: {
		m_text = new wxTextCtrl(this, wxID_ANY);
		m_text->SetMaxLength(THEREMINI_NAME_BYTES);
		row->Add(m_text, 1, wxALIGN_CENTRE_VERTICAL);
		break;
	}
	}

	SetSizer(row);
}

void ParamControl::OnSlider(wxCommandEvent &)
{
	if (m_syncing) {
		return;
	}
	m_syncing = true;
	m_spin->SetValue(m_param->min + m_slider->GetValue() * m_increment);
	m_syncing = false;
}

void ParamControl::OnSpin(wxSpinDoubleEvent &)
{
	if (m_syncing) {
		return;
	}
	m_syncing = true;
	m_slider->SetValue(
		static_cast<int>(std::lround((m_spin->GetValue() - m_param->min) / m_increment)));
	m_syncing = false;
}

double ParamControl::GetValue() const
{
	if (m_spin) {
		return m_spin->GetValue();
	}
	if (m_choice) {
		return m_choice->GetSelection();
	}
	return 0.0;
}

wxString ParamControl::GetText() const
{
	return m_text ? m_text->GetValue() : wxString();
}

void ParamControl::SetValue(double value)
{
	// A programmatic change does not fire the control's event, so move the
	// paired widget here rather than relying on the echo handlers.
	if (m_spin) {
		m_spin->SetValue(value);
		m_slider->SetValue(
			static_cast<int>(std::lround((value - m_param->min) / m_increment)));
	} else if (m_choice) {
		m_choice->SetSelection(static_cast<int>(std::lround(value)));
	}
}

void ParamControl::SetText(const wxString &text)
{
	if (m_text) {
		m_text->SetValue(text);
	}
}
