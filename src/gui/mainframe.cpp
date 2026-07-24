#include "mainframe.h"
#include "paramcontrol.h"

#include "theremini/protocol.h"

#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/listbox.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace {

enum { ID_STORE = wxID_HIGHEST + 1 };

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

} // namespace

MainFrame::MainFrame()
	: wxFrame(nullptr, wxID_ANY, "ThereMaxi", wxDefaultPosition, wxSize(920, 900))
{
	auto *menu = new wxMenu();
	menu->Append(wxID_OPEN, "&Open Library...\tCtrl+O");
	menu->Append(wxID_SAVEAS, "Save Library &As...\tCtrl+S");
	menu->AppendSeparator();
	menu->Append(wxID_EXIT, "&Quit\tCtrl+Q");
	auto *bar = new wxMenuBar();
	bar->Append(menu, "&File");
	SetMenuBar(bar);

	Bind(wxEVT_MENU, &MainFrame::OnOpen, this, wxID_OPEN);
	Bind(wxEVT_MENU, &MainFrame::OnSaveAs, this, wxID_SAVEAS);
	Bind(wxEVT_MENU, [this](wxCommandEvent &) { Close(true); }, wxID_EXIT);

	// left: the preset list and a Store button; right: the editor notebook
	auto *left = new wxBoxSizer(wxVERTICAL);
	m_presetList = new wxListBox(this, wxID_ANY);
	m_presetList->Bind(wxEVT_LISTBOX, &MainFrame::OnSelectPreset, this);
	left->Add(new wxStaticText(this, wxID_ANY, "Presets"), 0, wxALL, 4);
	left->Add(m_presetList, 1, wxEXPAND | wxALL, 4);
	auto *store = new wxButton(this, ID_STORE, "Store to Preset");
	store->Bind(wxEVT_BUTTON, &MainFrame::OnStore, this);
	left->Add(store, 0, wxEXPAND | wxALL, 4);

	auto *book = new wxNotebook(this, wxID_ANY);
	BuildPages(book);

	auto *top = new wxBoxSizer(wxHORIZONTAL);
	top->Add(left, 0, wxEXPAND);
	top->Add(book, 1, wxEXPAND);
	SetSizer(top);

	CreateStatusBar();
	SetStatusText("No library open");
}

void MainFrame::BuildPages(wxNotebook *book)
{
	for (const std::string &tab : tabs_in_order()) {
		auto *page = new wxPanel(book);
		auto *outer = new wxBoxSizer(wxVERTICAL);

		std::string current_group;
		bool have_box = false;
		wxSizer *box = nullptr;
		wxWindow *box_parent = page;

		for (const theremini_param *p : params_for_tab(tab.c_str())) {
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
			auto *ctrl = new ParamControl(box_parent, p);
			m_controls.push_back(ctrl);
			box->Add(ctrl, 0, wxEXPAND | wxALL, 2);
		}

		page->SetSizer(outer);
		book->AddPage(page, tab);
	}
}

void MainFrame::RefreshPresetList()
{
	m_presetList->Clear();
	for (size_t i = 0; i < m_library.presets.size(); i++) {
		const theremaxi::Preset &p = m_library.presets[i];
		const auto it = p.find("_ps");
		const wxString name = it != p.end() ? wxString(it->second.text) : wxString("(unnamed)");
		m_presetList->Append(wxString::Format("%02zu  %s", i + 1, name));
	}
}

void MainFrame::ApplyPreset(const theremaxi::Preset &preset)
{
	for (ParamControl *ctrl : m_controls) {
		const auto it = preset.find(ctrl->param()->id);
		if (it == preset.end()) {
			continue;
		}
		if (ctrl->param()->kind == THEREMINI_TEXT) {
			ctrl->SetText(it->second.text);
		} else {
			ctrl->SetValue(it->second.as_number());
		}
	}
}

theremaxi::Preset MainFrame::CollectPreset(int number) const
{
	theremaxi::Preset preset;
	preset["_nr"] = theremaxi::Value::num(number);
	for (ParamControl *ctrl : m_controls) {
		const theremini_param *p = ctrl->param();
		if (p->kind == THEREMINI_TEXT) {
			preset[p->id] = theremaxi::Value::str(ctrl->GetText().ToStdString());
		} else {
			preset[p->id] = theremaxi::Value::num(ctrl->GetValue());
		}
	}
	return preset;
}

void MainFrame::LoadLibrary(const wxString &path)
{
	try {
		m_library = theremaxi::load_theremaxi(path.ToStdString());
	} catch (const std::exception &e) {
		SetStatusText(wxString("Could not open: ") + e.what());
		return;
	}
	RefreshPresetList();
	if (!m_library.presets.empty()) {
		m_presetList->SetSelection(0);
		m_current = 0;
		ApplyPreset(m_library.presets[0]);
	}
	SetStatusText(wxString::Format("%zu presets", m_library.presets.size()));
}

void MainFrame::OnOpen(wxCommandEvent &)
{
	wxFileDialog dlg(this, "Open Library", "", "",
	                 "ThereMaxi libraries (*.theremaxi)|*.theremaxi|All files|*",
	                 wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if (dlg.ShowModal() != wxID_OK) {
		return;
	}
	try {
		m_library = theremaxi::load_theremaxi(dlg.GetPath().ToStdString());
	} catch (const std::exception &e) {
		wxMessageBox(e.what(), "Could not open library", wxOK | wxICON_ERROR, this);
		return;
	}
	m_current = -1;
	RefreshPresetList();
	SetStatusText(wxString::Format("%zu presets in %s", m_library.presets.size(),
	                               dlg.GetFilename()));
}

void MainFrame::OnSaveAs(wxCommandEvent &)
{
	wxFileDialog dlg(this, "Save Library As", "", "",
	                 "ThereMaxi libraries (*.theremaxi)|*.theremaxi",
	                 wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
	if (dlg.ShowModal() != wxID_OK) {
		return;
	}
	try {
		theremaxi::save_theremaxi(dlg.GetPath().ToStdString(), m_library);
	} catch (const std::exception &e) {
		wxMessageBox(e.what(), "Could not save library", wxOK | wxICON_ERROR, this);
		return;
	}
	SetStatusText("Saved " + dlg.GetFilename());
}

void MainFrame::OnSelectPreset(wxCommandEvent &)
{
	const int sel = m_presetList->GetSelection();
	if (sel < 0 || sel >= static_cast<int>(m_library.presets.size())) {
		return;
	}
	m_current = sel;
	ApplyPreset(m_library.presets[static_cast<size_t>(sel)]);
}

void MainFrame::OnStore(wxCommandEvent &)
{
	if (m_current < 0) {
		SetStatusText("Select a preset to store into first");
		return;
	}
	m_library.presets[static_cast<size_t>(m_current)] = CollectPreset(m_current);
	SetStatusText("Stored to preset - Save the library to keep it");
}
