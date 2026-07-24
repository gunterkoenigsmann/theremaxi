#include "mainframe.h"
#include "paramcontrol.h"

#include "theremini/protocol.h"

#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/statbox.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace {

// The parameters that belong on a page, in editor order.
std::vector<const theremini_param *> params_for_tab(const char *tab)
{
	size_t count = 0;
	const theremini_param *params = theremini_params(&count);

	std::vector<const theremini_param *> out;
	for (size_t i = 0; i < count; i++) {
		if (params[i].tab && std::string(params[i].tab) == tab) {
			out.push_back(&params[i]);
		}
	}
	std::sort(out.begin(), out.end(),
	          [](const theremini_param *a, const theremini_param *b) {
		          return a->order < b->order;
	          });
	return out;
}

// The tab names, in the order the editor shows them.
std::vector<std::string> tabs_in_order()
{
	size_t count = 0;
	const theremini_param *params = theremini_params(&count);

	std::vector<std::string> tabs;
	std::map<std::string, int> first_order;
	for (size_t i = 0; i < count; i++) {
		if (!params[i].tab) {
			continue;
		}
		const std::string tab = params[i].tab;
		if (first_order.find(tab) == first_order.end()) {
			first_order[tab] = params[i].order;
			tabs.push_back(tab);
		}
	}
	std::sort(tabs.begin(), tabs.end(),
	          [&](const std::string &a, const std::string &b) {
		          return first_order[a] < first_order[b];
	          });
	return tabs;
}

// A page: parameters grouped into boxes by their group hint.
wxPanel *make_page(wxNotebook *book, const char *tab)
{
	auto *page = new wxPanel(book);
	auto *outer = new wxBoxSizer(wxVERTICAL);

	std::string current_group;
	bool have_box = false;
	wxSizer *box = nullptr;
	wxWindow *box_parent = page; // a grouped control belongs to its static box

	for (const theremini_param *p : params_for_tab(tab)) {
		const std::string group = p->group ? p->group : "";
		if (!have_box || group != current_group) {
			current_group = group;
			have_box = true;
			if (!group.empty()) {
				auto *sb = new wxStaticBoxSizer(wxVERTICAL, page, group);
				box = sb;
				box_parent = sb->GetStaticBox();
			} else {
				box = new wxBoxSizer(wxVERTICAL);
				box_parent = page;
			}
			outer->Add(box, 0, wxEXPAND | wxALL, 4);
		}
		box->Add(new ParamControl(box_parent, p), 0, wxEXPAND | wxALL, 2);
	}

	page->SetSizer(outer);
	return page;
}

} // namespace

MainFrame::MainFrame()
	: wxFrame(nullptr, wxID_ANY, "ThereMaxi", wxDefaultPosition, wxSize(760, 900))
{
	auto *book = new wxNotebook(this, wxID_ANY);
	for (const std::string &tab : tabs_in_order()) {
		book->AddPage(make_page(book, tab.c_str()), tab);
	}

	auto *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(book, 1, wxEXPAND);
	SetSizer(sizer);
}
