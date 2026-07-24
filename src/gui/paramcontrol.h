// One editable parameter: a labelled row that edits a theremini_param.
//
// Numeric parameters get a slider and a wxSpinCtrlDouble that stay in sync, so
// the value can be dragged, stepped or typed. Enums get a choice, names a text
// field.
//
// Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.

#ifndef THEREMAXI_PARAMCONTROL_H
#define THEREMAXI_PARAMCONTROL_H

#include "theremini/protocol.h"

#include <wx/panel.h>
#include <wx/spinctrl.h> // wxSpinCtrlDouble and wxSpinDoubleEvent

class wxSlider;
class wxChoice;
class wxTextCtrl;

class ParamControl : public wxPanel {
public:
	ParamControl(wxWindow *parent, const theremini_param *param);

	// The current value in display units (0 for a name).
	double GetValue() const;

private:
	void OnSlider(wxCommandEvent &);
	void OnSpin(wxSpinDoubleEvent &);

	const theremini_param *m_param;
	double m_increment = 1.0; // finest step, 10^-digits

	wxSlider *m_slider = nullptr;
	wxSpinCtrlDouble *m_spin = nullptr;
	wxChoice *m_choice = nullptr;
	wxTextCtrl *m_text = nullptr;

	bool m_syncing = false; // guards the slider<->spin echo
};

#endif
