// The MidiFeedbackLoop tab: map the live antenna movement onto preset
// parameters. Two sections, one per antenna; each holds rows that each drive one
// target parameter, with a gate, sensitivity, invert and an output range. When a
// section is running, incoming antenna values are fed through its rows and the
// results are sent to the device via the callback.
//
// Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.

#ifndef THEREMAXI_FEEDBACK_TAB_H
#define THEREMAXI_FEEDBACK_TAB_H

#include "theremini/device.h"
#include "theremini/protocol.h"

#include <wx/panel.h>

#include <functional>

class FeedbackTab : public wxPanel {
public:
	// Called when a row produces a value: send this display value for this
	// parameter to the device.
	using SendFn = std::function<void(const theremini_param *param, double value)>;

	FeedbackTab(wxWindow *parent, SendFn send);
	~FeedbackTab() override;

	// Route one live antenna control-change (its controller number and value).
	void ProcessAntenna(int cc, int value);

private:
	struct Section;
	struct Row;

	Section *SectionForCC(int cc);
	void BuildSection(Section *s, const char *title, int cc);
	void AddRow(Section *s, const theremini_param *target);

	Section *m_volume;
	Section *m_pitch;
	SendFn m_send;
};

#endif
