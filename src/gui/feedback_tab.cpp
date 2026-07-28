#include "feedback_tab.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/menu.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/tglbtn.h>

#include <algorithm>
#include <vector>

// One mapping: an antenna value drives a target parameter.
struct FeedbackTab::Row {
	const theremini_param *target;
	theremini_feedback fb; // holds the running "last" state

	wxCheckBox *enable;
	wxSpinCtrl *low;
	wxSpinCtrl *high;
	wxSpinCtrl *sens;
	wxCheckBox *revert;
	wxSpinCtrlDouble *omin;
	wxSpinCtrlDouble *omax;
	wxWindow *panel; // the row's container, for removal
};

// One antenna's set of rows, with run state.
struct FeedbackTab::Section {
	int cc;
	bool running = false;
	wxScrolledWindow *area;
	wxBoxSizer *rows;
	std::vector<Row *> row_list;
};

namespace {
// The antenna's live value is 0..127 (7-bit); the device streams it that way.
constexpr int ANTENNA_MAX = 0x7f;

// Parameters that can be a feedback target: the real CC controls.
std::vector<const theremini_param *> targets()
{
	size_t n = 0;
	const theremini_param *params = theremini_params(&n);
	std::vector<const theremini_param *> out;
	for (size_t i = 0; i < n; i++) {
		if (params[i].cc >= 0 && params[i].kind != THEREMINI_TEXT) {
			out.push_back(&params[i]);
		}
	}
	std::sort(out.begin(), out.end(),
	          [](const theremini_param *a, const theremini_param *b) {
		          return a->order < b->order;
	          });
	return out;
}
} // namespace

FeedbackTab::FeedbackTab(wxWindow *parent, SendFn send)
	: wxPanel(parent), m_volume(new Section), m_pitch(new Section), m_send(std::move(send))
{
	auto *outer = new wxBoxSizer(wxVERTICAL);
	auto *box_v = new wxStaticBoxSizer(wxVERTICAL, this, "Volume Antenna");
	auto *box_p = new wxStaticBoxSizer(wxVERTICAL, this, "Pitch Antenna");
	SetSizer(outer);

	// build each section inside its static box
	auto build = [&](Section *s, wxStaticBoxSizer *box, int cc) {
		wxWindow *bp = box->GetStaticBox();
		s->cc = cc;

		auto *head = new wxBoxSizer(wxHORIZONTAL);
		auto *add = new wxButton(bp, wxID_ANY, "Add Row");
		auto *run = new wxToggleButton(bp, wxID_ANY, "Run");
		head->Add(add, 0, wxRIGHT, 4);
		head->Add(run, 0);
		box->Add(head, 0, wxALL, 4);

		s->area = new wxScrolledWindow(bp);
		s->area->SetScrollRate(0, 10);
		s->rows = new wxBoxSizer(wxVERTICAL);
		s->area->SetSizer(s->rows);
		box->Add(s->area, 1, wxEXPAND | wxALL, 4);

		add->Bind(wxEVT_BUTTON, [this, s](wxCommandEvent &) {
			wxMenu menu;
			const auto ts = targets();
			for (size_t i = 0; i < ts.size(); i++) {
				menu.Append(static_cast<int>(i + 1), ts[i]->name);
			}
			const int sel = GetPopupMenuSelectionFromUser(menu);
			if (sel != wxID_NONE && sel >= 1 && sel <= static_cast<int>(ts.size())) {
				AddRow(s, ts[static_cast<size_t>(sel - 1)]);
			}
		});
		run->Bind(wxEVT_TOGGLEBUTTON, [s, run](wxCommandEvent &) {
			s->running = run->GetValue();
		});
	};

	build(m_volume, box_v, THEREMINI_VOLUME_CC);
	build(m_pitch, box_p, THEREMINI_PITCH_CC);

	outer->Add(box_v, 1, wxEXPAND | wxALL, 6);
	outer->Add(box_p, 1, wxEXPAND | wxALL, 6);
}

FeedbackTab::~FeedbackTab()
{
	for (Section *s : {m_volume, m_pitch}) {
		for (Row *r : s->row_list) {
			delete r;
		}
		delete s;
	}
}

void FeedbackTab::AddRow(Section *s, const theremini_param *target)
{
	wxWindow *parent = s->area;
	auto *r = new Row;
	r->target = target;
	r->fb = theremini_feedback{};
	r->fb.input_max = ANTENNA_MAX;

	auto *row = new wxBoxSizer(wxHORIZONTAL);
	auto *panel = new wxPanel(parent);
	panel->SetSizer(row);
	r->panel = panel;

	r->enable = new wxCheckBox(panel, wxID_ANY, "");
	r->low = new wxSpinCtrl(panel, wxID_ANY, "0", wxDefaultPosition, wxSize(60, -1),
	                        wxSP_ARROW_KEYS, 0, ANTENNA_MAX, 0);
	r->high = new wxSpinCtrl(panel, wxID_ANY, "", wxDefaultPosition, wxSize(60, -1),
	                         wxSP_ARROW_KEYS, 0, ANTENNA_MAX, ANTENNA_MAX);
	r->sens = new wxSpinCtrl(panel, wxID_ANY, "0", wxDefaultPosition, wxSize(60, -1),
	                         wxSP_ARROW_KEYS, 0, ANTENNA_MAX, 0);
	auto *name = new wxStaticText(panel, wxID_ANY, target->label ? target->label : target->name);
	name->SetMinSize(wxSize(130, -1));
	r->revert = new wxCheckBox(panel, wxID_ANY, "");
	r->omin = new wxSpinCtrlDouble(panel, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1),
	                               wxSP_ARROW_KEYS, target->min, target->max, target->min);
	r->omax = new wxSpinCtrlDouble(panel, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1),
	                               wxSP_ARROW_KEYS, target->min, target->max, target->max);
	r->omin->SetDigits(static_cast<unsigned>(target->digits));
	r->omax->SetDigits(static_cast<unsigned>(target->digits));
	auto *del = new wxButton(panel, wxID_ANY, "X", wxDefaultPosition, wxSize(28, -1));

	for (wxWindow *w : {static_cast<wxWindow *>(r->enable), static_cast<wxWindow *>(r->low),
	                    static_cast<wxWindow *>(r->high), static_cast<wxWindow *>(r->sens),
	                    static_cast<wxWindow *>(name), static_cast<wxWindow *>(r->revert),
	                    static_cast<wxWindow *>(r->omin), static_cast<wxWindow *>(r->omax),
	                    static_cast<wxWindow *>(del)}) {
		row->Add(w, 0, wxALIGN_CENTRE_VERTICAL | wxRIGHT, 4);
	}

	del->Bind(wxEVT_BUTTON, [s, r](wxCommandEvent &) {
		auto &v = s->row_list;
		v.erase(std::remove(v.begin(), v.end(), r), v.end());
		r->panel->Destroy();
		delete r;
		s->area->Layout();
		s->area->FitInside();
	});

	s->rows->Add(panel, 0, wxEXPAND | wxALL, 1);
	s->row_list.push_back(r);
	s->area->Layout();
	s->area->FitInside();
}

FeedbackTab::Section *FeedbackTab::SectionForCC(int cc)
{
	if (cc == m_volume->cc) {
		return m_volume;
	}
	if (cc == m_pitch->cc) {
		return m_pitch;
	}
	return nullptr;
}

void FeedbackTab::ProcessAntenna(int cc, int value)
{
	Section *s = SectionForCC(cc);
	if (!s || !s->running) {
		return;
	}

	for (Row *r : s->row_list) {
		// read the controls into the row's feedback config, keeping "last"
		r->fb.enabled = r->enable->GetValue();
		r->fb.low = r->low->GetValue();
		r->fb.high = r->high->GetValue();
		r->fb.sens = r->sens->GetValue();
		r->fb.revert = r->revert->GetValue();
		r->fb.out_min = r->omin->GetValue();
		r->fb.out_max = r->omax->GetValue();

		double out = 0;
		if (theremini_feedback_feed(&r->fb, value, &out) && m_send) {
			m_send(r->target, out);
		}
	}
}
