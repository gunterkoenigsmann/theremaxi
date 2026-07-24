// ThereMaxi - wxWidgets editor for the Moog Theremini.
//
// This first cut builds the parameter editor from the protocol library: a
// notebook whose pages and boxes come from each parameter's layout hints, with
// a slider paired to a wxSpinCtrl for the numeric parameters and a choice for
// the enums. No device and no library management yet - it edits values.
//
// Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.

#include "mainframe.h"

#include <wx/wx.h>

class ThereMaxiApp : public wxApp {
public:
	bool OnInit() override
	{
		auto *frame = new MainFrame();
		frame->Show(true);
		return true;
	}
};

wxIMPLEMENT_APP(ThereMaxiApp);
