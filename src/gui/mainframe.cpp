#include "mainframe.h"
#include "paramcontrol.h"

#include "theremini/protocol.h"
#ifdef THEREMINI_HAVE_ALSA
#include "feedback_tab.h"
#include "theremini/alsa.h"
#include "theremini/device.h"
#include "theremini/write.h"
#include <wx/numdlg.h>
#include <cstring>
#endif

#include <wx/button.h>
#include <wx/display.h>
#include <wx/filedlg.h>
#include <wx/listbox.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace {

enum {
	ID_STORE = wxID_HIGHEST + 1,
	ID_NEW_PRESET,
	ID_COPY_PRESET,
	ID_DEL_PRESET,
	ID_CONNECT,
	ID_DISCONNECT,
	ID_SYNC_DEVICE,
	ID_SEND_DEVICE,
	ID_AUTODETECT
};

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
	: wxFrame(nullptr, wxID_ANY, "ThereMaxi", wxDefaultPosition, wxDefaultSize)
{
	auto *menu = new wxMenu();
	menu->Append(wxID_NEW, "&New Library\tCtrl+N");
	menu->Append(wxID_OPEN, "&Open Library...\tCtrl+O");
	menu->Append(wxID_SAVEAS, "Save Library &As...\tCtrl+S");
	menu->AppendSeparator();
	menu->Append(wxID_PREFERENCES, "&Preferences...");
	menu->AppendSeparator();
	menu->Append(wxID_EXIT, "&Quit\tCtrl+Q");
	auto *bar = new wxMenuBar();
	bar->Append(menu, "&File");

#ifdef THEREMINI_HAVE_ALSA
	auto *dev = new wxMenu();
	dev->Append(ID_CONNECT, "&Connect");
	dev->Append(ID_DISCONNECT, "&Disconnect");
	dev->AppendSeparator();
	dev->Append(ID_SYNC_DEVICE, "&Sync Presets from Device");
	dev->Append(ID_SEND_DEVICE, "S&end Preset to Device...");
	dev->Append(ID_AUTODETECT, "&Auto-detect Channel");
	bar->Append(dev, "&Device");
	Bind(wxEVT_MENU, &MainFrame::OnConnect, this, ID_CONNECT);
	Bind(wxEVT_MENU, &MainFrame::OnDisconnect, this, ID_DISCONNECT);
	Bind(wxEVT_MENU, &MainFrame::OnSyncDevice, this, ID_SYNC_DEVICE);
	Bind(wxEVT_MENU, &MainFrame::OnSendDevice, this, ID_SEND_DEVICE);
	Bind(wxEVT_MENU, &MainFrame::OnAutoDetect, this, ID_AUTODETECT);
	m_pump.SetOwner(this);
	Bind(wxEVT_TIMER, &MainFrame::OnPump, this);
#endif

	SetMenuBar(bar);

	m_settings = load_settings();
#ifdef THEREMINI_HAVE_ALSA
	UpdateDeviceMenu();
#endif

	Bind(wxEVT_MENU, &MainFrame::OnNewLibrary, this, wxID_NEW);
	Bind(wxEVT_MENU, &MainFrame::OnOpen, this, wxID_OPEN);
	Bind(wxEVT_MENU, &MainFrame::OnSaveAs, this, wxID_SAVEAS);
	Bind(wxEVT_MENU, &MainFrame::OnPreferences, this, wxID_PREFERENCES);
	Bind(wxEVT_MENU, [this](wxCommandEvent &) { Close(true); }, wxID_EXIT);

	// left: the preset list with New/Copy/Delete and Store; right: the editor
	auto *left = new wxBoxSizer(wxVERTICAL);
	m_presetList = new wxListBox(this, wxID_ANY);
	m_presetList->Bind(wxEVT_LISTBOX, &MainFrame::OnSelectPreset, this);
	left->Add(new wxStaticText(this, wxID_ANY, "Presets"), 0, wxALL, 4);
	left->Add(m_presetList, 1, wxEXPAND | wxALL, 4);

	auto *row = new wxBoxSizer(wxHORIZONTAL);
	auto *add = new wxButton(this, ID_NEW_PRESET, "New");
	auto *copy = new wxButton(this, ID_COPY_PRESET, "Copy");
	auto *del = new wxButton(this, ID_DEL_PRESET, "Delete");
	add->Bind(wxEVT_BUTTON, &MainFrame::OnNewPreset, this);
	copy->Bind(wxEVT_BUTTON, &MainFrame::OnCopyPreset, this);
	del->Bind(wxEVT_BUTTON, &MainFrame::OnDeletePreset, this);
	row->Add(add, 1, wxRIGHT, 2);
	row->Add(copy, 1, wxRIGHT, 2);
	row->Add(del, 1);
	left->Add(row, 0, wxEXPAND | wxALL, 4);

	auto *store = new wxButton(this, ID_STORE, "Store to Preset");
	store->Bind(wxEVT_BUTTON, &MainFrame::OnStore, this);
	left->Add(store, 0, wxEXPAND | wxALL, 4);

	auto *book = new wxNotebook(this, wxID_ANY);
	BuildPages(book);

	auto *top = new wxBoxSizer(wxHORIZONTAL);
	top->Add(left, 0, wxEXPAND);
	top->Add(book, 1, wxEXPAND);
	SetSizer(top);

#ifdef THEREMINI_HAVE_ALSA
	// message field plus one latched field per antenna
	CreateStatusBar(3);
	const int widths[] = {-1, 170, 170};
	SetStatusWidths(3, widths);
#else
	CreateStatusBar();
#endif
	SetStatusText("No library open");

	// A preferred size, but never taller or wider than the screen: on a short
	// display the old fixed 920x900 overflowed and printed wx warnings instead
	// of scrolling. The pages themselves are scrolled windows (see BuildPages),
	// so any remaining overflow degrades to scrollbars.
	const wxRect area = wxDisplay(static_cast<unsigned>(std::max(0, wxDisplay::GetFromWindow(this)))).GetClientArea();
	SetClientSize(std::min(920, area.GetWidth()), std::min(900, area.GetHeight()));
	Centre();

#ifdef THEREMINI_HAVE_ALSA
	// Try the device once the window is up; a missing one is not an error here.
	if (m_settings.auto_connect) {
		CallAfter([this] { ConnectDevice(true); });
	}
#endif
}

void MainFrame::BuildPages(wxNotebook *book)
{
	for (const std::string &tab : tabs_in_order()) {
		// a scrolled window so a page taller than the frame gets a scrollbar
		// rather than being clipped
		auto *page = new wxScrolledWindow(book);
		page->SetScrollRate(0, 10);
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
		page->FitInside(); // set the virtual size so scrolling knows its bounds
		book->AddPage(page, tab);
	}

#ifdef THEREMINI_HAVE_ALSA
	// the MidiFeedbackLoop tab drives parameters from the live antennas; sending
	// needs a device, so a row's output goes out as a control-change on channel 0
	m_feedback = new FeedbackTab(book, [this](const theremini_param *p, double v) {
		if (!m_seq) {
			return;
		}
		theremini_wire w;
		if (theremini_value_export(p, v, &w)) {
			theremini_alsa_send_cc(m_seq, 0, p->cc, w.bytes[0]);
			if (w.count == 2) {
				theremini_alsa_send_cc(m_seq, 0, p->lsb_cc, w.bytes[1]);
			}
		}
	});
	book->AddPage(m_feedback, "MidiFeedbackLoop");
#endif
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

void MainFrame::SelectPreset(int index)
{
	if (index < 0 || index >= static_cast<int>(m_library.presets.size())) {
		m_current = -1;
		return;
	}
	m_current = index;
	m_presetList->SetSelection(index);
	ApplyPreset(m_library.presets[static_cast<size_t>(index)]);
}

void MainFrame::OnSelectPreset(wxCommandEvent &)
{
	SelectPreset(m_presetList->GetSelection());
}

void MainFrame::OnStore(wxCommandEvent &)
{
	if (m_current < 0) {
		SetStatusText("Select a preset to store into first");
		return;
	}
	m_library.presets[static_cast<size_t>(m_current)] = CollectPreset(m_current);
	RefreshPresetList(); // the name may have changed
	m_presetList->SetSelection(m_current);
	SetStatusText("Stored to preset - Save the library to keep it");
}

void MainFrame::OnNewLibrary(wxCommandEvent &)
{
	m_library = theremaxi::Library{};
	m_current = -1;
	RefreshPresetList();
	SetStatusText("New library");
}

void MainFrame::OnPreferences(wxCommandEvent &)
{
	PrefsDialog dlg(this, m_settings);
	if (dlg.ShowModal() == wxID_OK) {
		m_settings = dlg.GetSettings();
		save_settings(m_settings);
		SetStatusText("Preferences saved");
	}
}

void MainFrame::OnNewPreset(wxCommandEvent &)
{
	wxTextEntryDialog dlg(this, "Name for the new preset:", "New Preset", "NEW");
	if (dlg.ShowModal() != wxID_OK) {
		return;
	}
	// capture the current editor into a new preset with the given name
	const size_t index = m_library.presets.size();
	theremaxi::Preset preset = CollectPreset(static_cast<int>(index));
	preset["_ps"] = theremaxi::Value::str(dlg.GetValue().ToStdString());
	m_library.presets.push_back(std::move(preset));
	theremaxi::renumber(m_library);
	RefreshPresetList();
	SelectPreset(static_cast<int>(index));
	SetStatusText("New preset - Save the library to keep it");
}

void MainFrame::OnCopyPreset(wxCommandEvent &)
{
	if (m_current < 0) {
		SetStatusText("Select a preset to copy first");
		return;
	}
	const size_t index = theremaxi::copy_preset(m_library, static_cast<size_t>(m_current));
	RefreshPresetList();
	SelectPreset(static_cast<int>(index));
	SetStatusText("Copied preset - Save the library to keep it");
}

void MainFrame::OnDeletePreset(wxCommandEvent &)
{
	if (m_current < 0) {
		SetStatusText("Select a preset to delete first");
		return;
	}
	theremaxi::remove_preset(m_library, static_cast<size_t>(m_current));
	RefreshPresetList();
	SelectPreset(m_library.presets.empty()
	                 ? -1
	                 : std::min(m_current, static_cast<int>(m_library.presets.size()) - 1));
	SetStatusText("Deleted preset - Save the library to keep it");
}

// --------------------------------------------------------------- device I/O

#ifdef THEREMINI_HAVE_ALSA

namespace {
void antenna_trampoline(int channel, int cc, int value, void *user)
{
	static_cast<MainFrame *>(user)->OnAntenna(channel, cc, value);
}
} // namespace

void MainFrame::UpdateDeviceMenu()
{
	wxMenuBar *bar = GetMenuBar();
	const bool on = m_seq != nullptr;
	bar->Enable(ID_CONNECT, !on);
	bar->Enable(ID_DISCONNECT, on);
	bar->Enable(ID_SYNC_DEVICE, on);
	bar->Enable(ID_SEND_DEVICE, on);
	bar->Enable(ID_AUTODETECT, on);
}

bool MainFrame::ConnectDevice(bool silent)
{
	m_seq = theremini_alsa_open("ThereMaxi");
	if (!m_seq) {
		if (!silent) {
			SetStatusText("Cannot open the ALSA sequencer");
		}
		return false;
	}
	const char *name = theremini_alsa_discover(m_seq);
	if (!name) {
		theremini_alsa_close(m_seq);
		m_seq = nullptr;
		if (!silent) {
			SetStatusText("No Theremini found");
		}
		return false;
	}

	theremini_alsa_on_cc(m_seq, antenna_trampoline, this);

	// identity, for the firmware version
	uint8_t req[THEREMINI_MESSAGE_MAX];
	uint8_t reply[64];
	size_t len = theremini_msg_identity_request(req, sizeof req);
	theremini_alsa_send(m_seq, req, len);
	long n = theremini_alsa_read_sysex(m_seq, reply, sizeof reply, 1500);
	wxString fw;
	if (n >= 7 && reply[3] == 0x06 && reply[4] == 0x02) {
		fw = wxString::Format(" (firmware %d.%d.%d)", reply[n - 4], reply[n - 3], reply[n - 2]);
	}

	SetStatusText(wxString::Format("Connected to %s%s", name, fw));
	m_pump.Start(50); // keep up with the antenna stream
	UpdateDeviceMenu();
	return true;
}

void MainFrame::OnConnect(wxCommandEvent &)
{
	ConnectDevice(false);
}

void MainFrame::OnDisconnect(wxCommandEvent &)
{
	m_pump.Stop();
	if (m_seq) {
		theremini_alsa_close(m_seq);
		m_seq = nullptr;
	}
	SetStatusText("Disconnected");
	UpdateDeviceMenu();
}

void MainFrame::OnSyncDevice(wxCommandEvent &)
{
	if (!m_seq) {
		return;
	}
	m_pump.Stop(); // do not compete for the input while reading the dump

	uint8_t req[THEREMINI_MESSAGE_MAX];
	static uint8_t reply[16384];
	size_t len = theremini_msg_request_all_presets(req, sizeof req);
	theremini_alsa_send(m_seq, req, len);
	long n = theremini_alsa_read_sysex(m_seq, reply, sizeof reply, 3000);

	if (n <= 0) {
		SetStatusText("No preset dump from the device");
		m_pump.Start(50);
		return;
	}

	theremini_dump dump;
	if (theremini_sysex_decode(reply, static_cast<size_t>(n), &dump) != THEREMINI_SYSEX_OK) {
		SetStatusText("Could not decode the preset dump");
		m_pump.Start(50);
		return;
	}

	// turn the decoded presets into a library
	theremaxi::Library lib;
	size_t count = 0;
	const theremini_param *params = theremini_params(&count);
	for (size_t i = 0; i < dump.count; i++) {
		theremaxi::Preset preset;
		for (size_t j = 0; j < count; j++) {
			const theremini_value *v = &dump.presets[i].values[j];
			if (!v->present) {
				continue;
			}
			if (params[j].kind == THEREMINI_TEXT && v->text[0]) {
				preset[params[j].id] = theremaxi::Value::str(v->text);
			} else {
				preset[params[j].id] = theremaxi::Value::num(v->number);
			}
		}
		lib.presets.push_back(std::move(preset));
	}
	theremaxi::renumber(lib);

	m_library = std::move(lib);
	m_current = -1;
	RefreshPresetList();
	if (!m_library.presets.empty()) {
		SelectPreset(0);
	}
	SetStatusText(wxString::Format("Synced %zu presets from the device", m_library.presets.size()));
	m_pump.Start(50);
}

void MainFrame::OnSendDevice(wxCommandEvent &)
{
	if (!m_seq) {
		return;
	}

	// which slot? default to the selected preset's position
	const long deflt = m_current >= 0 ? m_current + 1 : 1;
	const long slot = wxGetNumberFromUser(
		"Send the current settings to which slot on the device?\n"
		"This overwrites that slot on the Theremini.",
		"Slot (1-32):", "Send Preset to Device", deflt, 1, 32, this);
	if (slot < 1) {
		return; // cancelled
	}
	if (wxMessageBox(
		    wxString::Format("Overwrite slot %ld on the Theremini with the current settings?", slot),
		    "Send Preset to Device", wxYES_NO | wxICON_WARNING, this) != wxYES) {
		return;
	}

	// build a device preset from the editor's controls
	theremini_preset preset;
	std::memset(&preset, 0, sizeof preset);
	for (ParamControl *c : m_controls) {
		const size_t idx = theremini_param_index(c->param());
		theremini_value &v = preset.values[idx];
		v.present = true;
		if (c->param()->kind == THEREMINI_TEXT) {
			const std::string t = c->GetText().ToStdString();
			std::strncpy(v.text, t.c_str(), THEREMINI_TEXT_MAX - 1);
		} else {
			v.number = c->GetValue();
		}
	}

	m_pump.Stop(); // do not compete for the port during the write
	// the device receives on channel 0, as the reference implementation sends
	const bool ok = theremini_write_preset(m_seq, 0, static_cast<int>(slot - 1), &preset);
	m_pump.Start(50);

	SetStatusText(ok ? wxString::Format("Sent current settings to slot %ld", slot)
	                 : "Send failed");
}

void MainFrame::OnAutoDetect(wxCommandEvent &)
{
	if (!m_seq) {
		return;
	}
	m_pump.Stop();
	const int ch = theremini_alsa_detect_channel(m_seq, 2000);
	m_pump.Start(50);

	if (ch < 0) {
		SetStatusText("Could not detect the channel - move a hand near the antennas");
		return;
	}
	m_settings.volume.channel = ch;
	m_settings.pitch.channel = ch;
	save_settings(m_settings);
	SetStatusText(wxString::Format("Antennas on channel %d - saved to preferences", ch));
}

void MainFrame::OnPump(wxTimerEvent &)
{
	if (m_seq) {
		theremini_alsa_pump(m_seq, 0); // non-blocking; drives OnAntenna
	}
}

void MainFrame::ShowAntenna(int field, const char *which, int channel, int value)
{
	SetStatusText(wxString::Format("%s: ch %d  %d", which, channel, value), field);
}

void MainFrame::OnAntenna(int channel, int cc, int value)
{
	// Latch each antenna in its own field so both stay visible at once rather
	// than one line flipping between volume and pitch.
	if (cc == THEREMINI_VOLUME_CC && value != m_last_volume) {
		m_last_volume = value;
		ShowAntenna(1, "vol", channel, value);
	} else if (cc == THEREMINI_PITCH_CC && value != m_last_pitch) {
		m_last_pitch = value;
		ShowAntenna(2, "pitch", channel, value);
	}
	if (m_feedback) {
		m_feedback->ProcessAntenna(cc, value);
	}
}

#endif // THEREMINI_HAVE_ALSA

MainFrame::~MainFrame()
{
#ifdef THEREMINI_HAVE_ALSA
	m_pump.Stop();
	if (m_seq) {
		theremini_alsa_close(m_seq);
	}
#endif
}
